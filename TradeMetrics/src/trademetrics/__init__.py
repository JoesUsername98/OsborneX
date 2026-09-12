"""Generic PnL, drawdown, and trading performance metrics.

No dependency on any specific trading system -- feed it plain Fill objects
(timestamp, symbol, side, price, quantity) from wherever your fills come from.
"""

from .drawdown import drawdown_series, max_drawdown, max_drawdown_duration
from .equity import EquityPoint, build_equity_curve
from .fills import Fill, Side
from .position import PositionState, PositionTracker, realized_pnls_from_fills
from .report import PerformanceReport, format_report, summarize
from .returns import returns_from_equity_curve, sharpe_ratio, sortino_ratio, volatility
from .trades import average_loss, average_win, profit_factor, win_rate

__all__ = [
    "EquityPoint",
    "Fill",
    "PerformanceReport",
    "PositionState",
    "PositionTracker",
    "Side",
    "average_loss",
    "average_win",
    "build_equity_curve",
    "drawdown_series",
    "format_report",
    "max_drawdown",
    "max_drawdown_duration",
    "profit_factor",
    "realized_pnls_from_fills",
    "returns_from_equity_curve",
    "sharpe_ratio",
    "sortino_ratio",
    "summarize",
    "volatility",
    "win_rate",
]
