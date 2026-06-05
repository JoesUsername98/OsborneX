#pragma once
#include <vector>

#include "domain_types.hpp"

namespace osbornex {

struct LevelInfo 
{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

} // namespace osbornex