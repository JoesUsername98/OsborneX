"""Callback-based strategy base class, mirroring Bot::RandomStrategy's pattern."""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

from . import wire

if TYPE_CHECKING:
    from .client import Client


@dataclass(frozen=True)
class OwnFill:
    """One of this Strategy's own orders getting filled, extracted from a
    TradeExecution by my_fill()."""

    side: wire.Side
    price: float
    quantity: int


class Strategy:
    """Subclass and override on_top_of_book/on_trade; call self.buy/self.sell/self.cancel
    from inside them. Bound to a Client by Client.run()."""

    def __init__(self) -> None:
        self._client: "Client | None" = None
        self._own_order_ids: set[int] = set()

    def _bind(self, client: "Client") -> None:
        self._client = client

    def on_top_of_book(self, book: wire.TopOfBook) -> None:
        """Called on every top-of-book update for a subscribed symbol. No-op by default."""

    def on_trade(self, trade: wire.TradeExecution) -> None:
        """Called on every trade print for a subscribed symbol -- including
        trades this Strategy had no part in. No-op by default. Use my_fill()
        inside an override to check whether a given trade was one of this
        Strategy's own orders getting filled."""

    def buy(self, symbol: int, price: float, qty: int) -> int:
        order_id = self._require_client().buy(symbol, price, qty)
        self._own_order_ids.add(order_id)
        return order_id

    def sell(self, symbol: int, price: float, qty: int) -> int:
        order_id = self._require_client().sell(symbol, price, qty)
        self._own_order_ids.add(order_id)
        return order_id

    def cancel(self, order_id: int) -> None:
        self._require_client().cancel(order_id)

    def my_fill(self, trade: wire.TradeExecution) -> OwnFill | None:
        """Returns an OwnFill if trade involved an order this Strategy submitted
        via buy()/sell(), else None. A trade always has one bid-side and one
        ask-side order id; this checks both against every order id this
        Strategy has ever submitted."""
        if trade.bid_order_id in self._own_order_ids:
            return OwnFill(wire.Side.BUY, trade.bid_price, trade.quantity)
        if trade.ask_order_id in self._own_order_ids:
            return OwnFill(wire.Side.SELL, trade.ask_price, trade.quantity)
        return None

    def _require_client(self) -> "Client":
        if self._client is None:
            raise RuntimeError("Strategy is not bound to a Client -- call Client.run(strategy, ...) first")
        return self._client
