#pragma once

#include <limits>

#include "domain_types.hpp"

namespace osbornex {

struct Constants
{
    inline static const Price InvalidPrice = std::numeric_limits<Price>::quiet_NaN();
};

} // namespace osbornex