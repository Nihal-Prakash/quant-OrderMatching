#pragma once
#include "order.h"

namespace mebench {
enum class OutType : uint8_t { Trade, Ack, Reject, CancelAck, Expired };
enum class RejectReason : uint8_t { None, UnknownOrder, FokUnfillable };

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
}
