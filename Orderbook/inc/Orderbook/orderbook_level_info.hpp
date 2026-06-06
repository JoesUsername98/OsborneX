#pragma once

#include "level_info.hpp"

namespace OsborneX {

struct OrderbookLevelInfos 
{
    LevelInfos bids_;
    LevelInfos asks_;
};

} // namespace OsborneX