#pragma once

namespace OsborneX {

enum class OrderType
{
    GoodTillCancel,
    FillAndKill,
    FillOrKill,
    GoodForDay,
    Market
};

} // namespace OsborneX