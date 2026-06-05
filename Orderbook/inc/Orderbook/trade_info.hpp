#pragma once

#include "domain_types.hpp"

namespace osbornex {

struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

} // namespace osbornex