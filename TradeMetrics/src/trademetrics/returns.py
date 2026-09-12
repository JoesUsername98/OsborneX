"""Return-based risk metrics computed from an equity curve's period-over-period changes."""

from __future__ import annotations

import math
from collections.abc import Sequence

from .equity import EquityPoint


def returns_from_equity_curve(equity_curve: Sequence[EquityPoint]) -> list[float]:
    """Point-to-point returns, as a fraction of the equity level at the start
    of each step. A step starting from zero or negative equity contributes no
    return (dividing by it would be meaningless), so it's skipped."""
    returns: list[float] = []
    for previous, current in zip(equity_curve, equity_curve[1:]):
        if previous.equity == 0.0:
            continue
        returns.append((current.equity - previous.equity) / abs(previous.equity))
    return returns


def volatility(returns: Sequence[float]) -> float:
    """Population standard deviation of a return series (0.0 for fewer than 2 samples)."""
    if len(returns) < 2:
        return 0.0
    mean = sum(returns) / len(returns)
    variance = sum((r - mean) ** 2 for r in returns) / len(returns)
    return math.sqrt(variance)


def sharpe_ratio(returns: Sequence[float], risk_free_rate: float = 0.0, periods_per_year: float = 252.0) -> float:
    """Annualized Sharpe ratio from a per-period return series.

    periods_per_year scales the result to your sampling frequency -- 252 for
    daily returns, 252 * 6.5 * 60 for one-minute returns during a 6.5-hour
    trading day, etc. Pick whatever matches how equity_curve was sampled.
    """
    if len(returns) < 2:
        return 0.0
    period_risk_free = risk_free_rate / periods_per_year
    excess = [r - period_risk_free for r in returns]
    mean_excess = sum(excess) / len(excess)
    vol = volatility(excess)
    if vol == 0.0:
        return 0.0
    return (mean_excess / vol) * math.sqrt(periods_per_year)


def sortino_ratio(returns: Sequence[float], risk_free_rate: float = 0.0, periods_per_year: float = 252.0) -> float:
    """Like sharpe_ratio, but only penalizes downside volatility (excess
    returns below zero) rather than volatility in either direction."""
    if len(returns) < 2:
        return 0.0
    period_risk_free = risk_free_rate / periods_per_year
    excess = [r - period_risk_free for r in returns]
    mean_excess = sum(excess) / len(excess)
    downside = [min(0.0, r) for r in excess]
    downside_deviation = math.sqrt(sum(d * d for d in downside) / len(downside))
    if downside_deviation == 0.0:
        return 0.0
    return (mean_excess / downside_deviation) * math.sqrt(periods_per_year)
