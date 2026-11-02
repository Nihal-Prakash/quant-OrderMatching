#include "engine.h"
#include <algorithm>
#include <deque>
#include <functional>
#include <map>

using namespace mebench;

namespace {
struct Resting {
    OrderRef ref;
    uint32_t qty;
};

class Engine final : public IMatchingEngine {
    using Bids = std::map<int32_t, std::deque<Resting>, std::greater<int32_t>>;
    using Asks = std::map<int32_t, std::deque<Resting>>;
    Bids bids;
    Asks asks;
    uint64_t last_seq = 0, total_qty = 0;
    uint32_t order_count = 0;

    static bool crosses(const Order& o, int32_t price) {
        return o.tif == TIF::Market || (o.side == Side::Buy ? price <= o.price : price >= o.price);
    }

    template<class Book>
    static bool fillable(const Book& book, const Order& o) {
        uint64_t qty = 0;
        for (const auto& [price, queue] : book) {
            if (!crosses(o, price)) break;
            for (const auto& r : queue)
                if (r.ref.participant_id != o.participant_id) qty += r.qty;
            if (qty >= o.qty) return true;
        }
        return false;
    }

    template<class Book>
    void match(Book& book, const Order& o, uint32_t& remaining, OutSink& out) {
        while (remaining && !book.empty()) {
            auto level = book.begin();
            if (!crosses(o, level->first)) break;
            auto& queue = level->second;
            auto& r = queue.front();
            if (r.ref.participant_id == o.participant_id) {
                out.emit({o.seq, r.ref, o.ref(), 0, r.qty, OutType::CancelAck, o.side, RejectReason::None, {}});
                total_qty -= r.qty;
                --order_count;
                queue.pop_front();
            } else {
                auto qty = std::min(remaining, r.qty);
                out.emit({o.seq, r.ref, o.ref(), level->first, qty, OutType::Trade, o.side, RejectReason::None, {}});
                remaining -= qty;
                r.qty -= qty;
                total_qty -= qty;
                if (!r.qty) {
                    --order_count;
                    queue.pop_front();
                }
            }
            if (queue.empty()) book.erase(level);
        }
    }

    template<class Book>
    bool cancel(Book& book, OrderRef ref, uint64_t seq, Side side, OutSink& out) {
        for (auto level = book.begin(); level != book.end(); ++level) {
            auto& queue = level->second;
            for (auto it = queue.begin(); it != queue.end(); ++it) {
                if (it->ref.session_id != ref.session_id || it->ref.client_order_id != ref.client_order_id) continue;
                out.emit({seq, {}, it->ref, 0, it->qty, OutType::CancelAck, side, RejectReason::None, {}});
                total_qty -= it->qty;
                --order_count;
                queue.erase(it);
                if (queue.empty()) book.erase(level);
                return true;
            }
        }
        return false;
    }

    template<class Book>
    static void levels(const Book& book, LevelVec& out) {
        out.clear();
        for (const auto& [price, queue] : book) {
            uint64_t qty = 0;
            for (const auto& r : queue) qty += r.qty;
            out.emplace_back(price, qty);
        }
    }

public:
    void on_new(const Order& o, OutSink& out) noexcept override {
        last_seq = o.seq;
        if (o.tif == TIF::FOK && !(o.side == Side::Buy ? fillable(asks, o) : fillable(bids, o))) {
            out.emit({o.seq, {}, o.ref(), 0, 0, OutType::Reject, o.side, RejectReason::FokUnfillable, {}});
            return;
        }
        out.emit({o.seq, {}, o.ref(), o.price, 0, OutType::Ack, o.side, RejectReason::None, {}});
        uint32_t remaining = o.qty;
        if (o.side == Side::Buy) match(asks, o, remaining, out);
        else match(bids, o, remaining, out);
        if (o.tif == TIF::Limit && remaining) {
            if (o.side == Side::Buy) bids[o.price].push_back({o.ref(), remaining});
            else asks[o.price].push_back({o.ref(), remaining});
            total_qty += remaining;
            ++order_count;
        } else if (remaining && (o.tif == TIF::IOC || o.tif == TIF::Market)) {
            out.emit({o.seq, {}, o.ref(), 0, remaining, OutType::Expired, o.side, RejectReason::None, {}});
        }
    }

    void on_cancel(OrderRef ref, uint64_t seq, OutSink& out) noexcept override {
        last_seq = seq;
        if (cancel(bids, ref, seq, Side::Buy, out) || cancel(asks, ref, seq, Side::Sell, out)) return;
        out.emit({seq, {}, ref, 0, 0, OutType::Reject, Side::Buy, RejectReason::UnknownOrder, {}});
    }

    void snapshot(BookSnapshot& snap) const override {
        snap = {};
        snap.at_seq = last_seq;
        bid_levels(snap.bids);
        ask_levels(snap.asks);
        snap.n_bids = snap.bids.size();
        snap.n_asks = snap.asks.size();
        snap.resting_qty_total = total_qty;
        snap.resting_order_count = order_count;
    }
    void bid_levels(LevelVec& out) const override { levels(bids, out); }
    void ask_levels(LevelVec& out) const override { levels(asks, out); }
};
}

extern "C" IMatchingEngine* create_engine() { return new Engine; }
