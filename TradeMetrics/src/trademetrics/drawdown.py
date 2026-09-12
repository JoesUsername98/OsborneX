"""Drawdown metrics computed purely from an equity curve."""

from __future__ import annotations

from collections.abc import Sequence

from .equity import EquityPoint


def drawdown_series(equity_curve: Sequence[EquityPoint]) -> list[float]:
    """Running drawdown (<= 0) from the running peak-so-far, at each point."""
    series: list[float] = []
    peak = float("-inf")
    for point in equity_curve:
        peak = max(peak, point.equity)
        series.append(point.equity - peak)
    return series


def max_drawdown(equity_curve: Sequence[EquityPoint]) -> float:
    """The largest peak-to-trough decline (<= 0; 0.0 for an empty curve or one
    that never fell below a prior peak)."""
    return min(drawdown_series(equity_curve), default=0.0)


def max_drawdown_duration(equity_curve: Sequence[EquityPoint]) -> float:
    """The longest stretch (in the equity curve's own timestamp units) spent
    at or below a prior peak before that peak is exceeded again. Measured up
    to the last point still underwater before a recovery (or through the end
    of the curve, if it never recovers). 0.0 if never underwater."""
    if not equity_curve:
        return 0.0
    longest = 0.0
    peak = equity_curve[0].equity
    peak_time = equity_curve[0].timestamp
    for point in equity_curve:
        if point.equity >= peak:
            peak = point.equity
            peak_time = point.timestamp
        else:
            longest = max(longest, point.timestamp - peak_time)
    return longest
