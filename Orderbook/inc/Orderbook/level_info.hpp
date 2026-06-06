#pragma once
#include <vector>

#include "domain_types.hpp"

namespace OsborneX {

struct LevelInfo 
{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

} // namespace OsborneX