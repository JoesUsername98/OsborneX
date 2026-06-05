#pragma once

namespace osbornex {

enum class OrderType
{
    GoodTillCancel,
    FillAndKill,
    FillOrKill,
    GoodForDay,
    Market
};

} // namespace osbornex