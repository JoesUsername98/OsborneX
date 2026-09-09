#include <gtest/gtest.h>

#include <Ingress/ingress.hpp>
#include <Messages/order_sink.hpp>
#include <Messages/types.hpp>

#include <thread>
#include <vector>

namespace OsborneX::Simulation {
namespace {

class FakeOrderSink : public OrderSink
{
public:
    void route(OrderMessage message) override
    {
        received.push_back(std::move(message));
    }

    std::vector<OrderMessage> received;
};

OrderMessage MakeAdd(OrderId orderId)
{
    return OrderMessage{
        .source = 1,
        .symbol = 1,
        .order_id = orderId,
        .side = Side::Buy,
        .price = 100.0,
        .quantity = 1,
        .type = OrderType::GoodTillCancel,
        .action = OrderAction::Add,
    };
}

TEST(IngressTest, StampsIngressTimestampAndForwardsToSink)
{
    FakeOrderSink sink;
    Ingress ingress{ sink };

    ingress.receive(MakeAdd(1));

    ASSERT_EQ(sink.received.size(), 1u);
    EXPECT_GT(sink.received.front().ingress_timestamp, 0u);
    EXPECT_EQ(sink.received.front().order_id, 1u);
}

TEST(IngressTest, AssignsStrictlyMonotonicSequencesAcrossCalls)
{
    FakeOrderSink sink;
    Ingress ingress{ sink };

    for (OrderId id = 1; id <= 5; ++id)
        ingress.receive(MakeAdd(id));

    ASSERT_EQ(sink.received.size(), 5u);
    for (std::size_t i = 0; i < sink.received.size(); ++i)
        EXPECT_EQ(sink.received[i].ingress_sequence, i);

    EXPECT_EQ(ingress.next_sequence(), 5u);
}

TEST(IngressTest, NextSequenceStartsAtZero)
{
    FakeOrderSink sink;
    Ingress ingress{ sink };
    EXPECT_EQ(ingress.next_sequence(), 0u);
}

#ifndef NDEBUG
TEST(IngressDeathTest, ReceiveFromASecondThreadAsserts)
{
    FakeOrderSink sink;
    Ingress ingress{ sink };
    ingress.receive(MakeAdd(1)); // establishes this thread as the caller

    EXPECT_DEATH(
        {
            std::thread other([&] { ingress.receive(MakeAdd(2)); });
            other.join();
        },
        "");
}
#endif

} // namespace
} // namespace OsborneX::Simulation
