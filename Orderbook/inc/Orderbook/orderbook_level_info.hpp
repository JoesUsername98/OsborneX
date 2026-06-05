#pragma once

#include "level_info.hpp"

namespace osbornex {

struct OrderbookLevelInfos 
{
    LevelInfos bids_;
    LevelInfos asks_;
};

} // namespace osbornex