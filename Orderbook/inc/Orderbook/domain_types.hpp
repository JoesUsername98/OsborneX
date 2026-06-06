#pragma once

#include <cstdint>
#include <vector>

namespace OsborneX {

using Price = double;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
using OrderIds = std::vector<OrderId>;
} // namespace OsborneX