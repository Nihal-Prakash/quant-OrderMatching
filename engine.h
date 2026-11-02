#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace mebench {
enum class Side : uint8_t { Buy, Sell };
enum class TIF : uint8_t { Limit, IOC, FOK, Market };
enum class OutType : uint8_t { Trade, Ack, Reject, CancelAck, Expired };
enum class RejectReason : uint8_t { None, UnknownOrder, FokUnfillable };

struct OrderRef {
    uint64_t client_order_id;
    uint16_t session_id;
    uint16_t participant_id;
    uint32_t _pad;
};

struct Order {
    uint64_t seq;
    uint64_t client_order_id;
    int32_t price;
    uint32_t qty;
    uint16_t session_id;
    uint16_t participant_id;
    Side side;
    TIF tif;
    uint16_t _pad;
    OrderRef ref() const { return {client_order_id, session_id, participant_id, 0}; }
};

struct OutEvent {
    uint64_t in_seq;
    OrderRef maker;
    OrderRef taker;
    int32_t price;
    uint32_t qty;
    OutType type;
    Side aggressor_side;
    RejectReason reason;
    uint8_t _pad[5];
};

class OutSink {
public:
    virtual void emit(const OutEvent& event) noexcept = 0;
    virtual ~OutSink() = default;
};

using LevelVec = std::vector<std::pair<int32_t, uint64_t>>;
struct BookSnapshot {
    uint64_t at_seq;
    uint32_t n_bids, n_asks;
    LevelVec bids, asks;
    uint64_t resting_qty_total;
    uint32_t resting_order_count;
};

class IMatchingEngine {
public:
    virtual void on_new(const Order& order, OutSink& out) noexcept = 0;
    virtual void on_cancel(OrderRef ref, uint64_t seq, OutSink& out) noexcept = 0;
    virtual void snapshot(BookSnapshot& out) const = 0;
    virtual void bid_levels(LevelVec& out) const = 0;
    virtual void ask_levels(LevelVec& out) const = 0;
    virtual ~IMatchingEngine() = default;
};
}

extern "C" mebench::IMatchingEngine* create_engine();
