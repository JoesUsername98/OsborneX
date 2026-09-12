"""The one input type this whole library needs: a fill, i.e. one executed trade."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class Side(Enum):
    BUY = "buy"
    SELL = "sell"


@dataclass(frozen=True)
class Fill:
    """One executed trade from any trading system.

    timestamp is an arbitrary numeric clock -- seconds since the epoch, a
    sequence number, whatever your source provides -- as long as it's
    consistently increasing and comparable across all fills you pass in
    together, since it's what drawdown/return metrics measure time against.
    """

    timestamp: float
    symbol: str
    side: Side
    price: float
    quantity: float
