#pragma once

namespace osbornex {

enum class OrderType
{
    GoodTillCancel,
    FillAndKill,
    FillOrKill,
    GoodForDay
};

} // namespace osbornex