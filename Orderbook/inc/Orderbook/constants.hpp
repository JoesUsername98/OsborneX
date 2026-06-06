#pragma once

#include <limits>

#include "domain_types.hpp"

namespace OsborneX {

struct constants
{
    inline static const Price InvalidPrice = std::numeric_limits<Price>::quiet_NaN();
};

} // namespace OsborneX