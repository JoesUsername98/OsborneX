from osbornex import wire
from osbornex.strategy import OwnFill, Strategy


class _FakeClient:
    """A minimal stand-in for Client -- just enough for Strategy.buy/sell to
    allocate ids without needing a real socket."""

    def __init__(self) -> None:
        self._next_id = 1

    def buy(self, symbol: int, price: float, qty: int) -> int:
        order_id = self._next_id
        self._next_id += 1
        return order_id

    def sell(self, symbol: int, price: float, qty: int) -> int:
        return self.buy(symbol, price, qty)


def _bound_strategy() -> Strategy:
    strategy = Strategy()
    strategy._bind(_FakeClient())
    return strategy


def _trade(bid_order_id: int, ask_order_id: int) -> wire.TradeExecution:
    return wire.TradeExecution(
        symbol=1,
        sequence=1,
        timestamp=0,
        bid_order_id=bid_order_id,
        bid_price=100.0,
        ask_order_id=ask_order_id,
        ask_price=100.0,
        quantity=10,
    )


def test_buy_and_sell_return_and_track_order_ids():
    strategy = _bound_strategy()
    order_id = strategy.buy(symbol=1, price=100.0, qty=10)
    assert order_id in strategy._own_order_ids


def test_my_fill_recognizes_own_bid_side_order():
    strategy = _bound_strategy()
    order_id = strategy.buy(symbol=1, price=100.0, qty=10)
    trade = _trade(bid_order_id=order_id, ask_order_id=999)
    assert strategy.my_fill(trade) == OwnFill(wire.Side.BUY, 100.0, 10)


def test_my_fill_recognizes_own_ask_side_order():
    strategy = _bound_strategy()
    order_id = strategy.sell(symbol=1, price=100.0, qty=10)
    trade = _trade(bid_order_id=999, ask_order_id=order_id)
    assert strategy.my_fill(trade) == OwnFill(wire.Side.SELL, 100.0, 10)


def test_my_fill_returns_none_for_a_trade_between_other_participants():
    strategy = _bound_strategy()
    strategy.buy(symbol=1, price=100.0, qty=10)
    trade = _trade(bid_order_id=111, ask_order_id=222)
    assert strategy.my_fill(trade) is None
