#include <gtest/gtest.h>
#include <chrono>

#include <Orderbook/orderbook.hpp>

namespace osbornex {
namespace {

using namespace std::chrono;

constexpr hours kMarketCloseHour{16};

std::chrono::system_clock::time_point LocalTime(
    const local_days day,
    const hours hour,
    const minutes minute = minutes{0},
    const seconds second = seconds{0})
{
    const local_seconds localTime = local_seconds{day} + hour + minute + second;
    return zoned_time{current_zone(), localTime}.get_sys_time();
}

std::chrono::system_clock::time_point ExpectedClose(const local_days day)
{
    return LocalTime(day, kMarketCloseHour);
}

TEST(OrderbookTestMarketClose, IfBeforeCutoffOnWeekday_ThenNextCloseIsSameDay) {
    Orderbook orderbook{kMarketCloseHour};
    const local_days wednesday{2024y / June / 5d};
    const auto asof = LocalTime(wednesday, hours{15}, minutes{59}, seconds{59});

    const auto nextClose = orderbook.GetNextMarketClose(asof);

    EXPECT_EQ(nextClose, ExpectedClose(wednesday));
}

TEST(OrderbookTestMarketClose, IfAfterCutoffOnWeekday_ThenNextCloseIsNextDay) {
    Orderbook orderbook{kMarketCloseHour};
    const local_days wednesday{2024y / June / 5d};
    const local_days thursday{2024y / June / 6d};
    const auto asof = LocalTime(wednesday, kMarketCloseHour, minutes{0}, seconds{1});

    const auto nextClose = orderbook.GetNextMarketClose(asof);

    EXPECT_EQ(nextClose, ExpectedClose(thursday));
}

TEST(OrderbookTestMarketClose, IfAfterCutoffOnFriday_ThenNextCloseIsSaturday) {
    Orderbook orderbook{kMarketCloseHour};
    const local_days friday{2024y / June / 7d};
    const local_days saturday{2024y / June / 8d};
    const auto asof = LocalTime(friday, kMarketCloseHour, minutes{0}, seconds{1});

    const auto nextClose = orderbook.GetNextMarketClose(asof);

    EXPECT_EQ(nextClose, ExpectedClose(saturday));
}

TEST(OrderbookTestMarketClose, IfAfterCutoffOnSunday_ThenNextCloseIsMonday) {
    Orderbook orderbook{kMarketCloseHour};
    const local_days sunday{2024y / June / 9d};
    const local_days monday{2024y / June / 10d};
    const auto asof = LocalTime(sunday, kMarketCloseHour, minutes{0}, seconds{1});

    const auto nextClose = orderbook.GetNextMarketClose(asof);

    EXPECT_EQ(nextClose, ExpectedClose(monday));
}

} // namespace
} // namespace osbornex
