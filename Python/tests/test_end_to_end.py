from conftest import wait_until

from osbornex import wire
from osbornex.client import connect
from osbornex.orchestration import Server


def test_crossing_orders_produce_a_trade_over_multicast(server_binary):
    server = Server.start(
        binary=server_binary,
        order_entry_port=19001,
        market_data_group="239.5.5.5",
        market_data_port=19002,
    )
    try:
        client = connect(server, source=1)
        trades: list[wire.TradeExecution] = []
        client.subscribe([1], on_trade=trades.append)

        client.sell(symbol=1, price=100.0, qty=10)
        client.buy(symbol=1, price=100.0, qty=10)

        assert wait_until(lambda: len(trades) >= 1, timeout=5.0)
        trade = trades[0]
        assert trade.symbol == 1
        assert trade.quantity == 10
        assert trade.bid_price == 100.0
        assert trade.ask_price == 100.0

        client.disconnect()
    finally:
        server.stop()


def test_cancel_before_fill_prevents_a_trade(server_binary):
    server = Server.start(
        binary=server_binary,
        order_entry_port=19011,
        market_data_group="239.5.5.6",
        market_data_port=19012,
    )
    try:
        client = connect(server, source=1)
        trades: list[wire.TradeExecution] = []
        client.subscribe([2], on_trade=trades.append)

        order_id = client.sell(symbol=2, price=50.0, qty=5)
        client.cancel(order_id)
        client.buy(symbol=2, price=50.0, qty=5)  # would cross the sell if it were still resting

        produced = wait_until(lambda: len(trades) >= 1, timeout=1.0)
        assert not produced

        client.disconnect()
    finally:
        server.stop()
