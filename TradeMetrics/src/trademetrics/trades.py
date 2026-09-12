"""Trade-level statistics from a sequence of realized-PnL amounts (one per
closing fill -- see position.realized_pnls_from_fills)."""

from __future__ import annotations

import math
from collections.abc import Sequence


def win_rate(realized_pnls: Sequence[float]) -> float:
    """Fraction of closed trades with positive PnL (0.0 if there are none)."""
    if not realized_pnls:
        return 0.0
    wins = sum(1 for pnl in realized_pnls if pnl > 0.0)
    return wins / len(realized_pnls)


def profit_factor(realized_pnls: Sequence[float]) -> float:
    """Gross profit / gross loss. inf if there are wins and no losses, 0.0 if
    there are neither."""
    gains = sum(pnl for pnl in realized_pnls if pnl > 0.0)
    losses = -sum(pnl for pnl in realized_pnls if pnl < 0.0)
    if losses == 0.0:
        return math.inf if gains > 0.0 else 0.0
    return gains / losses


def average_win(realized_pnls: Sequence[float]) -> float:
    wins = [pnl for pnl in realized_pnls if pnl > 0.0]
    return sum(wins) / len(wins) if wins else 0.0


def average_loss(realized_pnls: Sequence[float]) -> float:
    """Mean of losing trades' PnL (<= 0; 0.0 if there are none)."""
    losses = [pnl for pnl in realized_pnls if pnl < 0.0]
    return sum(losses) / len(losses) if losses else 0.0
