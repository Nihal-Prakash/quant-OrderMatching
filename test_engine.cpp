#include "engine.h"
#include <cassert>
#include <memory>
#include <vector>

using namespace mebench;

struct Sink : OutSink {
    std::vector<OutEvent> events;
    void emit(const OutEvent& event) noexcept override { events.push_back(event); }
};

Order order(uint64_t seq, uint64_t id, int32_t price, uint32_t qty, Side side, TIF tif = TIF::Limit, uint16_t participant = 1) {
    return {seq, id, price, qty, 1, participant, side, tif, 0};
}

int main() {
    static_assert(sizeof(OrderRef) == 16 && sizeof(Order) == 32 && sizeof(OutEvent) == 56);
    std::unique_ptr<IMatchingEngine> engine(create_engine());
    Sink out;
    BookSnapshot book;
    engine->on_new(order(1, 1, 101, 5, Side::Sell, TIF::Limit, 2), out);
    engine->on_new(order(2, 2, 100, 7, Side::Sell, TIF::Limit, 3), out);
    engine->on_new(order(3, 3, 101, 8, Side::Buy), out);
    assert(out.events.size() == 5);
    assert(out.events[3].type == OutType::Trade && out.events[3].price == 100 && out.events[3].qty == 7);
    assert(out.events[4].type == OutType::Trade && out.events[4].price == 101 && out.events[4].qty == 1);
    engine->snapshot(book);
    assert(book.at_seq == 3 && book.n_asks == 1 && book.asks[0].second == 4);
    assert(book.resting_qty_total == 4 && book.resting_order_count == 1);
    engine->on_new(order(4, 4, 101, 5, Side::Buy, TIF::FOK, 2), out);
    assert(out.events.back().type == OutType::Reject && out.events.back().reason == RejectReason::FokUnfillable);
    engine->snapshot(book);
    assert(book.resting_qty_total == 4 && book.at_seq == 4);
    engine->on_new(order(5, 5, 101, 4, Side::Buy, TIF::FOK, 3), out);
    assert(out.events.back().type == OutType::Trade && out.events.back().qty == 4);
    engine->on_new(order(6, 6, 99, 3, Side::Buy), out);
    engine->on_new(order(7, 7, 99, 2, Side::Sell, TIF::IOC, 2), out);
    assert(out.events.back().type == OutType::Trade && out.events.back().qty == 2);
    engine->on_new(order(8, 8, 0, 2, Side::Sell, TIF::Market, 2), out);
    assert(out.events.back().type == OutType::Expired && out.events.back().qty == 1);
    engine->on_new(order(9, 9, 102, 3, Side::Sell), out);
    engine->on_new(order(10, 10, 102, 2, Side::Buy, TIF::IOC, 2), out);
    assert(out.events.back().type == OutType::Trade && out.events.back().qty == 2);
    engine->on_cancel({9, 1, 0, 0}, 11, out);
    assert(out.events.back().type == OutType::CancelAck && out.events.back().qty == 1);
    engine->on_cancel({9, 1, 0, 0}, 12, out);
    assert(out.events.back().type == OutType::Reject && out.events.back().reason == RejectReason::UnknownOrder);
    engine->on_new(order(13, 13, 100, 3, Side::Sell), out);
    engine->on_new(order(14, 14, 100, 2, Side::Buy, TIF::IOC), out);
    assert(out.events[out.events.size() - 2].type == OutType::CancelAck);
    assert(out.events.back().type == OutType::Expired && out.events.back().qty == 2);
    engine->snapshot(book);
    assert(book.resting_qty_total == 0 && book.resting_order_count == 0 && book.n_bids == 0 && book.n_asks == 0);

    engine.reset(create_engine());
    out.events.clear();
    engine->on_new(order(1, 1, 100, 2, Side::Sell, TIF::Limit, 2), out);
    engine->on_new(order(2, 2, 100, 2, Side::Sell, TIF::Limit, 3), out);
    engine->on_new(order(3, 3, 100, 3, Side::Buy), out);
    assert(out.events[3].maker.client_order_id == 1 && out.events[3].qty == 2);
    assert(out.events[4].maker.client_order_id == 2 && out.events[4].qty == 1);
    engine->snapshot(book);
    assert(book.resting_qty_total == 1 && book.resting_order_count == 1);
}
