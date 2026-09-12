"""Shared risk/friction/live-PnL plumbing for strategies/*.py.

Deliberately strategy-agnostic: this module never calls buy/sell/cancel --
placing or canceling orders (crossing to flatten, pulling resting quotes) is
always specific to the strategy composing this, and is done by a
flatten_callback the strategy supplies to check_drawdown_and_maybe_halt().
"""

import math
import time
from dataclasses import dataclass

import osbornex
import trademetrics


@dataclass(frozen=True)
class RiskLimits:
    starting_capital: float = 100_000.0
    max_position_notional_pct: float = 0.10  # per-symbol cap, fraction of starting_capital
    max_drawdown_pct: float = 0.05  # of starting_capital, peak-to-trough on live equity
    min_cash_buffer: float = 0.0  # never enter a trade that would push cash below this


@dataclass(frozen=True)
class FrictionModel:
    flat_fee_per_fill: float = 0.05
    fee_bps_of_notional: float = 1.0  # 1bp = 0.01% of notional


def mid_price(book: osbornex.TopOfBook) -> float | None:
    """(bid+ask)/2 if both sides are present, else whichever side is, else None."""
    have_bid, have_ask = not math.isnan(book.bid_price), not math.isnan(book.ask_price)
    if have_bid and have_ask:
        return (book.bid_price + book.ask_price) / 2.0
    if have_bid:
        return book.bid_price
    if have_ask:
        return book.ask_price
    return None


class RiskTracker:
    """Owns live cash/PnL/position/drawdown state and the friction-cost model.
    Composed into a Strategy (self.risk = RiskTracker(...)); never touches
    order placement itself."""

    def __init__(
        self,
        risk_limits: RiskLimits = RiskLimits(),
        friction: FrictionModel = FrictionModel(),
    ) -> None:
        self.risk_limits = risk_limits
        self.friction = friction

        self.fills: list[trademetrics.Fill] = []
        self.position_tracker = trademetrics.PositionTracker()
        self.cash = risk_limits.starting_capital
        self.total_fees_paid = 0.0
        self.peak_equity = risk_limits.starting_capital
        self.halted = False
        self.halt_reason: str | None = None
        self.halted_at: float | None = None

        self.last_mid: dict[int, float] = {}
        self.last_book: dict[int, osbornex.TopOfBook] = {}

    # -- market data ---------------------------------------------------

    def update_book(self, book: osbornex.TopOfBook) -> float | None:
        """Records last_book/last_mid for book.symbol. Returns the mid, or None
        if book has no usable bid/ask yet (caller should skip the tick)."""
        mid = mid_price(book)
        if mid is None:
            return None
        self.last_book[book.symbol] = book
        self.last_mid[book.symbol] = mid
        return mid

    # -- sizing ----------------------------------------------------------

    def size_order(self, price: float, max_qty: int, existing_exposure_qty: float = 0.0) -> int:
        """Clip size for a new order at `price`, capped by remaining per-symbol
        position-notional headroom (cap minus `existing_exposure_qty` units
        already at risk on this symbol -- 0.0 when called with no existing
        exposure, e.g. mean reversion entering flat) and by cash affordability.
        `max_qty` is the caller's own base clip size."""
        cap_notional = self.risk_limits.max_position_notional_pct * self.risk_limits.starting_capital
        remaining_notional = max(cap_notional - abs(existing_exposure_qty) * price, 0.0)
        qty_by_cap = int(remaining_notional // price) if price > 0 else 0

        affordable = max(self.cash - self.risk_limits.min_cash_buffer, 0.0)
        cost_per_unit = price * (1 + self.friction.fee_bps_of_notional / 10_000)
        qty_by_cash = int(affordable // cost_per_unit) if cost_per_unit > 0 else 0

        return max(min(max_qty, qty_by_cap, qty_by_cash), 0)

    # -- fills: frictional cost, live PnL/cash tracking -------------------

    def record_fill(self, symbol: int, own: osbornex.OwnFill) -> tuple[trademetrics.Fill, float]:
        """Folds friction into an effective fill price, applies it to the
        position tracker, updates cash/total_fees_paid, and returns
        (fill, realized_pnl_from_this_fill). Does not touch any
        pending-order/resting-quote bookkeeping -- that's strategy-specific."""
        notional = own.price * own.quantity
        fee = self.friction.flat_fee_per_fill + notional * self.friction.fee_bps_of_notional / 10_000
        self.total_fees_paid += fee
        fee_per_unit = fee / own.quantity
        effective_price = own.price + fee_per_unit if own.side is osbornex.Side.BUY else own.price - fee_per_unit

        fill = trademetrics.Fill(
            # TradeExecution.timestamp is the server's internal ingress clock,
            # not a wall-clock epoch, so it isn't meaningful to plot against --
            # stamp with wall-clock time at the moment we observe the fill instead.
            timestamp=time.time(),
            symbol=str(symbol),
            side=trademetrics.Side.BUY if own.side is osbornex.Side.BUY else trademetrics.Side.SELL,
            price=effective_price,
            quantity=own.quantity,
        )
        self.fills.append(fill)
        realized = self.position_tracker.apply(fill)
        self.cash += -effective_price * own.quantity if own.side is osbornex.Side.BUY else effective_price * own.quantity
        return fill, realized

    # -- risk: live equity, drawdown, halt --------------------------------

    def equity(self, marks: dict[str, float]) -> float:
        return (
            self.risk_limits.starting_capital
            + self.position_tracker.realized_pnl()
            + self.position_tracker.unrealized_pnl(marks)
        )

    def check_drawdown_and_maybe_halt(self, flatten_callback) -> bool:
        """flatten_callback(equity: float, drawdown: float, limit: float) -> None
        is called only once, only when this call breaches the limit, and is
        entirely responsible for unwinding the strategy's own working orders
        and open positions (crossing to flatten, canceling resting quotes,
        whatever is specific to that strategy). Returns True iff it just halted."""
        marks = {str(symbol): mid for symbol, mid in self.last_mid.items()}
        equity = self.equity(marks)
        self.peak_equity = max(self.peak_equity, equity)
        drawdown = self.peak_equity - equity
        limit = self.risk_limits.max_drawdown_pct * self.risk_limits.starting_capital
        if drawdown < limit:
            return False

        self.halted = True
        self.halted_at = time.time()
        self.halt_reason = f"drawdown {drawdown:.2f} >= limit {limit:.2f} (equity={equity:.2f}, peak={self.peak_equity:.2f})"
        print(f"\n!!! RISK LIMIT BREACHED -- {self.halt_reason}\nFlattening open positions and halting.\n")
        flatten_callback(equity, drawdown, limit)
        return True
