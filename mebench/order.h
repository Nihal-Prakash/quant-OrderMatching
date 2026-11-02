#pragma once
#include <cstdint>

namespace mebench {
enum class Side : uint8_t { Buy, Sell };
enum class TIF : uint8_t { Limit, IOC, FOK, Market };

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
}
