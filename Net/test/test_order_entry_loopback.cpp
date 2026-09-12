#include <Net/order_entry_client.hpp>
#include <Net/order_entry_listener.hpp>

#include <mutex>
#include <vector>

#include <TestSupport/wait_for.hpp>
#include <gtest/gtest.h>

using namespace OsborneX;
using OsborneX::TestSupport::wait_for;

namespace {

class ReceivedMessages
{
public:
    void add(Simulation::OrderMessage message)
    {
        std::lock_guard lock{ mutex_ };
        messages_.push_back(message);
    }

    std::vector<Simulation::OrderMessage> snapshot() const
    {
        std::lock_guard lock{ mutex_ };
        return messages_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<Simulation::OrderMessage> messages_;
};

} // namespace

TEST(OrderEntryLoopbackTest, ClientSendDeliversDecodedMessageToListenerHandler)
{
    ReceivedMessages received;
    Net::OrderEntryListener listener(0, [&](Simulation::OrderMessage message) { received.add(message); });
    ASSERT_TRUE(listener.start());

    Net::OrderEntryClient client("127.0.0.1", listener.local_port());
    ASSERT_TRUE(client.connect());

    Simulation::OrderMessage message{
        .source_timestamp = 42,
        .source = 3,
        .symbol = 7,
        .order_id = 100,
        .side = Simulation::Side::Buy,
        .price = 55.25,
        .quantity = 12,
        .type = Simulation::OrderType::GoodTillCancel,
        .action = Simulation::OrderAction::Add,
    };
    ASSERT_TRUE(client.send(message));

    ASSERT_TRUE(wait_for([&] { return received.snapshot().size() == 1; }));

    const auto messages = received.snapshot();
    EXPECT_EQ(messages[0].symbol, message.symbol);
    EXPECT_EQ(messages[0].order_id, message.order_id);
    EXPECT_EQ(messages[0].side, message.side);
    EXPECT_DOUBLE_EQ(messages[0].price, message.price);
    EXPECT_EQ(messages[0].quantity, message.quantity);
    EXPECT_EQ(messages[0].type, message.type);
    EXPECT_EQ(messages[0].action, message.action);

    client.disconnect();
    listener.stop();
}

TEST(OrderEntryLoopbackTest, ListenerHandlesMultipleConnectionsConcurrently)
{
    ReceivedMessages received;
    Net::OrderEntryListener listener(0, [&](Simulation::OrderMessage message) { received.add(message); });
    ASSERT_TRUE(listener.start());

    constexpr int client_count = 5;
    std::vector<Net::OrderEntryClient> clients;
    for (int i = 0; i < client_count; ++i)
        clients.emplace_back("127.0.0.1", listener.local_port());

    for (auto& client : clients)
        ASSERT_TRUE(client.connect());

    for (int i = 0; i < client_count; ++i)
    {
        ASSERT_TRUE(clients[static_cast<std::size_t>(i)].send(Simulation::OrderMessage{
            .symbol = static_cast<Simulation::SymbolId>(i),
            .order_id = static_cast<Simulation::OrderId>(i),
            .side = Simulation::Side::Buy,
            .price = 10.0,
            .quantity = 1,
            .type = Simulation::OrderType::GoodTillCancel,
            .action = Simulation::OrderAction::Add,
        }));
    }

    ASSERT_TRUE(wait_for([&] { return received.snapshot().size() == client_count; }));

    listener.stop();
}

TEST(OrderEntryLoopbackTest, StopReturnsPromptlyWithAnIdleConnectionStillOpen)
{
    Net::OrderEntryListener listener(0, [](Simulation::OrderMessage) {});
    ASSERT_TRUE(listener.start());

    Net::OrderEntryClient client("127.0.0.1", listener.local_port());
    ASSERT_TRUE(client.connect());
    // Connection is established but the client never sends anything -- stop()
    // must not hang waiting on a recv() that will never complete.

    listener.stop();
    SUCCEED();
}
