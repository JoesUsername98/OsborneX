#pragma once

#include "domain_types.hpp"

namespace OsborneX {

struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

} // namespace OsborneX