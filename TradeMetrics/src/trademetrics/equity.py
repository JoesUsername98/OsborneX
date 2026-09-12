"""Equity curve construction from a sequence of fills."""

from __future__ import annotations

from collections.abc import Callable, Iterable, Mapping
from dataclasses import dataclass

from .fills import Fill
from .position import PositionTracker


@dataclass(frozen=True)
class EquityPoint:
    timestamp: float
    equity: float


def build_equity_curve(
    fills: Iterable[Fill],
    mark_prices_at: Callable[[float], Mapping[str, float]] | None = None,
) -> list[EquityPoint]:
    """Replays fills in order, producing one EquityPoint per fill.

    By default, equity is cumulative realized PnL only -- open positions
    contribute nothing until closed. Pass mark_prices_at (a function from a
    fill's timestamp to a {symbol: price} mapping) to also mark any
    still-open position to market at each point, for a fuller "what is this
    strategy actually worth right now" curve.
    """
    tracker = PositionTracker()
    points: list[EquityPoint] = []
    for fill in fills:
        tracker.apply(fill)
        equity = tracker.realized_pnl()
        if mark_prices_at is not None:
            equity += tracker.unrealized_pnl(dict(mark_prices_at(fill.timestamp)))
        points.append(EquityPoint(fill.timestamp, equity))
    return points
