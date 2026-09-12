"""One-call summary combining PnL, drawdown, and return-based metrics."""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass, field

from .drawdown import max_drawdown, max_drawdown_duration
from .equity import EquityPoint, build_equity_curve
from .fills import Fill
from .position import realized_pnls_from_fills
from .returns import returns_from_equity_curve, sharpe_ratio, sortino_ratio, volatility
from .trades import average_loss, average_win, profit_factor, win_rate


@dataclass(frozen=True)
class PerformanceReport:
    total_realized_pnl: float
    total_trades: int
    win_rate: float
    profit_factor: float
    average_win: float
    average_loss: float
    max_drawdown: float
    max_drawdown_duration: float
    volatility: float
    sharpe_ratio: float
    sortino_ratio: float
    equity_curve: list[EquityPoint] = field(repr=False)


def summarize(
    fills: Iterable[Fill],
    *,
    risk_free_rate: float = 0.0,
    periods_per_year: float = 252.0,
) -> PerformanceReport:
    """Builds a full PerformanceReport from a sequence of fills. See
    sharpe_ratio's docstring for how to pick periods_per_year for your data."""
    fills = list(fills)
    equity_curve = build_equity_curve(fills)
    realized_pnls = realized_pnls_from_fills(fills)
    returns = returns_from_equity_curve(equity_curve)

    return PerformanceReport(
        total_realized_pnl=equity_curve[-1].equity if equity_curve else 0.0,
        total_trades=len(realized_pnls),
        win_rate=win_rate(realized_pnls),
        profit_factor=profit_factor(realized_pnls),
        average_win=average_win(realized_pnls),
        average_loss=average_loss(realized_pnls),
        max_drawdown=max_drawdown(equity_curve),
        max_drawdown_duration=max_drawdown_duration(equity_curve),
        volatility=volatility(returns),
        sharpe_ratio=sharpe_ratio(returns, risk_free_rate, periods_per_year),
        sortino_ratio=sortino_ratio(returns, risk_free_rate, periods_per_year),
        equity_curve=equity_curve,
    )


def format_report(report: PerformanceReport) -> str:
    """A human-readable, print-ready summary (omits the raw equity_curve)."""
    return "\n".join(
        [
            f"Total realized PnL:      {report.total_realized_pnl:.4f}",
            f"Total trades:            {report.total_trades}",
            f"Win rate:                {report.win_rate:.1%}",
            f"Profit factor:           {report.profit_factor:.4f}",
            f"Average win:             {report.average_win:.4f}",
            f"Average loss:            {report.average_loss:.4f}",
            f"Max drawdown:            {report.max_drawdown:.4f}",
            f"Max drawdown duration:   {report.max_drawdown_duration:.4f}",
            f"Volatility (per period): {report.volatility:.6f}",
            f"Sharpe ratio:            {report.sharpe_ratio:.4f}",
            f"Sortino ratio:           {report.sortino_ratio:.4f}",
        ]
    )
