"""Average-cost position and realized-PnL tracking, per symbol."""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass

from .fills import Fill, Side


@dataclass
class PositionState:
    quantity: float = 0.0
    average_cost: float = 0.0
    realized_pnl: float = 0.0


class PositionTracker:
    """Tracks position size, average cost, and realized PnL per symbol.

    Uses average-cost accounting: opening or adding to a position only moves
    the average cost, booking no PnL; PnL is realized only when a fill reduces,
    closes, or reverses an existing position, against that running average cost.
    """

    def __init__(self) -> None:
        self._states: dict[str, PositionState] = {}

    def apply(self, fill: Fill) -> float:
        """Applies one fill. Returns the realized PnL booked by it (0.0 if the
        fill only opened or added to a position)."""
        state = self._states.setdefault(fill.symbol, PositionState())
        signed_qty = fill.quantity if fill.side is Side.BUY else -fill.quantity

        if state.quantity == 0.0 or (state.quantity > 0) == (signed_qty > 0):
            return self._open_or_add(state, fill.price, signed_qty)
        return self._reduce_close_or_reverse(state, fill.price, signed_qty)

    @staticmethod
    def _open_or_add(state: PositionState, price: float, signed_qty: float) -> float:
        new_quantity = state.quantity + signed_qty
        if new_quantity != 0.0:
            state.average_cost = (state.average_cost * state.quantity + price * signed_qty) / new_quantity
        state.quantity = new_quantity
        return 0.0

    @staticmethod
    def _reduce_close_or_reverse(state: PositionState, price: float, signed_qty: float) -> float:
        closing_qty = min(abs(signed_qty), abs(state.quantity))
        direction = 1.0 if state.quantity > 0 else -1.0
        realized = closing_qty * direction * (price - state.average_cost)
        state.realized_pnl += realized

        remaining = signed_qty + state.quantity
        if abs(signed_qty) > abs(state.quantity):
            # Fully closed and reversed -- the remainder opens a new position at this fill's price.
            state.quantity = remaining
            state.average_cost = price
        else:
            state.quantity = remaining
            if state.quantity == 0.0:
                state.average_cost = 0.0
            # else: the still-open remainder keeps its existing average cost.
        return realized

    def position(self, symbol: str) -> float:
        return self._states.get(symbol, PositionState()).quantity

    def average_cost(self, symbol: str) -> float:
        return self._states.get(symbol, PositionState()).average_cost

    def realized_pnl(self, symbol: str | None = None) -> float:
        if symbol is not None:
            return self._states.get(symbol, PositionState()).realized_pnl
        return sum(state.realized_pnl for state in self._states.values())

    def unrealized_pnl(self, mark_prices: dict[str, float]) -> float:
        """Mark-to-market PnL on currently open positions, using caller-supplied
        {symbol: price} marks. Symbols with no open position or no mark price
        given are skipped."""
        total = 0.0
        for symbol, state in self._states.items():
            if state.quantity == 0.0 or symbol not in mark_prices:
                continue
            total += state.quantity * (mark_prices[symbol] - state.average_cost)
        return total


def realized_pnls_from_fills(fills: Iterable[Fill]) -> list[float]:
    """The realized PnL booked by each fill that closed/reduced a position, in
    order, excluding fills that only opened or added to one (which book 0.0).
    This is the "per-trade" PnL series that win-rate/profit-factor-style
    trade-level statistics operate on."""
    tracker = PositionTracker()
    return [pnl for pnl in (tracker.apply(fill) for fill in fills) if pnl != 0.0]
