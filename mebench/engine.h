#pragma once
#include "order.h"
#include "out.h"
#include <utility>
#include <vector>

namespace mebench {
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
