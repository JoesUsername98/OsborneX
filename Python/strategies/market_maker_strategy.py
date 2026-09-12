"""A market-making strategy: continuously quotes both a bid and ask around a
fair-value estimate, profiting from the spread rather than taking a
directional view. Starts a server, starts bot participants, runs this
strategy against them, then prints a performance report.

Run from the Python/ directory after building a CMake preset, e.g.:
    cmake --build --preset windows-msvc-debug
    uv sync
    uv run python strategies/market_maker_strategy.py

Press Enter (or Ctrl+C) at any time to stop the simulation and print the report.
See also Python/examples/basic_strategy.py, the library's own (unmodified)
reference sample, and strategies/mean_reversion_strategy.py for a directional
strategy sharing the same risk/friction plumbing (strategies/risk.py).
"""

import math
import statistics
import threading
import time
from collections import deque
from dataclasses import dataclass, field

import osbornex
import risk
import trademetrics

# Bot::RandomStrategy (Bot/inc/Bot/random_strategy.hpp) recenters every order on the
# live top-of-book mid, perturbed by +/-25bps, ~100ms combined cadence across the two
# bots. Once this strategy's own quote becomes best-of-book, the bots' next orders
# perturb around *our* price too -- a real feedback effect real market makers also
# face on real markets. A longer fair-value window than the mean-reversion strategy's
# dilutes any single tick's influence (including a self-referential one) to keep the
# fair-value estimate reasonably stable.


@dataclass(frozen=True)
class MarketMakerConfig:
    fair_value_window: int = 60  # rolling ticks for the fair-value mean (3x mean reversion's 20)
    # Bots continuously post BOTH sides at random, so most of their orders cross each
    # other directly rather than resting -- the resulting natural best-bid/best-ask
    # spread (measured empirically over a 20s bots-only run) has median ~5.2bps, mean
    # ~6.7bps (occasionally as wide as ~29bps). A half-spread has to be narrow enough
    # to often be competitive with that natural spread (or we just never reach the
    # front of the book and never get filled), while still clearing the ~4bps
    # round-trip friction cost below -- 4bps half-spread (8bps total) does both.
    half_spread_bps: float = 4.0
    skew_gamma: float = 0.001  # price units the reservation price shifts per unit of net inventory
    quote_qty: int = 5  # per-side clip size -- half of the mean-reversion strategy's trade_qty=10,
    # since this strategy holds two sides of exposure live at once
    min_requote_interval_s: float = 0.5  # don't cancel/replace a given side more than ~2x/sec
    min_requote_move: float = 0.015  # ...and only if the desired price moved at least this much


@dataclass
class QuoteSlot:
    order_id: int | None = None
    price: float | None = None
    qty: int = 0
    last_requote_at: float = 0.0


@dataclass
class SymbolQuotes:
    bid: QuoteSlot = field(default_factory=QuoteSlot)
    ask: QuoteSlot = field(default_factory=QuoteSlot)


class MarketMakerStrategy(osbornex.Strategy):
    def __init__(
        self,
        symbols: list[int],
        risk_limits: risk.RiskLimits = risk.RiskLimits(),
        friction: risk.FrictionModel = risk.FrictionModel(),
        config: MarketMakerConfig = MarketMakerConfig(),
    ) -> None:
        super().__init__()
        self.symbols = list(symbols)
        self.config = config
        self.risk = risk.RiskTracker(risk_limits, friction)

        self.fair_value_history: dict[int, deque[float]] = {}
        self.quotes: dict[int, SymbolQuotes] = {}

    # -- market data / quoting -----------------------------------------------

    def on_top_of_book(self, book: osbornex.TopOfBook) -> None:
        if self.risk.halted:
            return

        mid = self.risk.update_book(book)
        if mid is None:
            return

        history = self.fair_value_history.setdefault(book.symbol, deque(maxlen=self.config.fair_value_window))
        history.append(mid)

        if self.risk.check_drawdown_and_maybe_halt(self._cancel_quotes_and_flatten):
            return  # just canceled quotes + flattened + halted this tick

        if len(history) < self.config.fair_value_window:
            return  # not enough data yet for a stable fair-value estimate

        fair_value = statistics.fmean(history)
        self._maybe_requote(book.symbol, fair_value)

    def _maybe_requote(self, symbol: int, fair_value: float) -> None:
        position = self.risk.position_tracker.position(str(symbol))
        reservation_price = fair_value - position * self.config.skew_gamma
        half_spread = fair_value * self.config.half_spread_bps / 10_000
        desired_bid = reservation_price - half_spread
        desired_ask = reservation_price + half_spread

        state = self.quotes.setdefault(symbol, SymbolQuotes())
        self._maybe_requote_side(
            symbol, osbornex.Side.BUY, desired_bid, state.bid, position, fair_value, reservation_price
        )
        self._maybe_requote_side(
            symbol, osbornex.Side.SELL, desired_ask, state.ask, position, fair_value, reservation_price
        )

    def _maybe_requote_side(
        self,
        symbol: int,
        side: osbornex.Side,
        desired_price: float,
        slot: QuoteSlot,
        position: float,
        fair_value: float,
        reservation_price: float,
    ) -> None:
        now = time.monotonic()
        if slot.price is not None:
            if now - slot.last_requote_at < self.config.min_requote_interval_s:
                return
            if abs(desired_price - slot.price) < self.config.min_requote_move:
                return
            self.cancel(slot.order_id)  # fire-and-forget: silent no-op server-side if already filled
            slot.order_id, slot.price, slot.qty = None, None, 0

        qty = self.risk.size_order(desired_price, self.config.quote_qty, existing_exposure_qty=abs(position))
        if qty <= 0:
            return

        order_id = (
            self.buy(symbol, price=desired_price, qty=qty)
            if side is osbornex.Side.BUY
            else self.sell(symbol, price=desired_price, qty=qty)
        )
        slot.order_id, slot.price, slot.qty, slot.last_requote_at = order_id, desired_price, qty, now
        print(
            f"symbol {symbol}: quoting {side.name} qty={qty} @ {desired_price:.4f} "
            f"(fair={fair_value:.4f} reservation={reservation_price:.4f} skew={reservation_price - fair_value:+.4f})"
        )

    # -- risk: cancel quotes + flatten on drawdown breach --------------------

    def _cancel_quotes_and_flatten(self, equity: float, drawdown: float, limit: float) -> None:
        for state in self.quotes.values():
            for slot in (state.bid, state.ask):
                if slot.order_id is not None:
                    self.cancel(slot.order_id)
                    slot.order_id, slot.price, slot.qty = None, None, 0

        for symbol in self.symbols:
            position = self.risk.position_tracker.position(str(symbol))
            if position == 0:
                continue
            book = self.risk.last_book.get(symbol)
            if position > 0:
                price = book.bid_price if book and not math.isnan(book.bid_price) else self.risk.last_mid.get(symbol)
                if price is None or math.isnan(price):
                    print(f"symbol {symbol}: cannot flatten yet -- no bid/mark available")
                    continue
                print(f"symbol {symbol}: flattening LONG {int(position)} @ {price:.4f} (crossing to bid)")
                self.sell(symbol, price=price, qty=int(position))
            else:
                price = book.ask_price if book and not math.isnan(book.ask_price) else self.risk.last_mid.get(symbol)
                if price is None or math.isnan(price):
                    print(f"symbol {symbol}: cannot flatten yet -- no ask/mark available")
                    continue
                print(f"symbol {symbol}: flattening SHORT {int(abs(position))} @ {price:.4f} (crossing to ask)")
                self.buy(symbol, price=price, qty=int(abs(position)))

    # -- fills: frictional cost, live PnL/cash tracking, requote bookkeeping -

    def on_trade(self, trade: osbornex.TradeExecution) -> None:
        own = self.my_fill(trade)
        if own is None:
            return  # a trade between other participants -- not ours

        fill, realized = self.risk.record_fill(trade.symbol, own)
        fee = abs(fill.price - own.price) * own.quantity

        # my_fill's own logic branches exactly this way -- BUY iff bid_order_id
        # matched, SELL iff ask_order_id matched -- so this recovers which order id
        # actually filled without needing a library change.
        matched_order_id = trade.bid_order_id if own.side is osbornex.Side.BUY else trade.ask_order_id
        state = self.quotes.setdefault(trade.symbol, SymbolQuotes())
        slot = state.bid if own.side is osbornex.Side.BUY else state.ask

        if matched_order_id == slot.order_id:
            # the currently-resting quote on this side filled -- free it up so the
            # next on_top_of_book tick (which always follows a trade) requotes it
            slot.order_id, slot.price, slot.qty = None, None, 0
        # else: a stale, already-superseded order on this side filled after we'd
        # already replaced it (the cancel/replace race) -- the fill above already
        # booked PnL, inventory, and cash correctly; the *current* resting slot is
        # untouched and still accurate, so there's nothing else to reconcile.

        print(
            f"symbol {trade.symbol}: filled {own.side.name} {own.quantity} @ {own.price:.4f} "
            f"(fee {fee:.4f}, effective {fill.price:.4f}); cash={self.risk.cash:.2f} realized={realized:+.4f}"
        )


def _wait_for_enter(stop_event: threading.Event) -> None:
    try:
        input()
    except EOFError:
        pass  # stdin closed out from under us -- fall back to Ctrl+C
    stop_event.set()


def main() -> None:
    strategy = MarketMakerStrategy(symbols=[1, 2])
    with osbornex.Server.start() as server:
        bots = [osbornex.Bot.start(symbols=[1, 2], source=i, server=server) for i in (1, 2)]
        try:
            # source=999 must be distinct from every Bot's source (1, 2 above) --
            # order ids are namespaced by source, so reusing one causes collisions.
            client = osbornex.connect(server, source=999)
            stop_event = threading.Event()
            threading.Thread(target=_wait_for_enter, args=(stop_event,), daemon=True).start()
            print("Simulation running -- press Enter to stop and see the performance report...")
            client.run(strategy, symbols=[1, 2], stop_event=stop_event)
        finally:
            for bot in bots:
                bot.stop()

    print("\n=== Performance report ===")
    print(trademetrics.format_report(trademetrics.summarize(strategy.risk.fills)))

    print()
    print(f"Starting capital:       {strategy.risk.risk_limits.starting_capital:.2f}")
    print(f"Ending cash:            {strategy.risk.cash:.2f}")
    print(f"Total fees paid:        {strategy.risk.total_fees_paid:.2f}")
    final_marks = {str(symbol): mid for symbol, mid in strategy.risk.last_mid.items()}
    print(f"Ending equity (mtm):    {strategy.risk.equity(final_marks):.2f}")
    print(f"Peak equity:            {strategy.risk.peak_equity:.2f}")
    if strategy.risk.halted:
        print(f"RISK LIMIT BREACHED at t={strategy.risk.halted_at:.3f} -- {strategy.risk.halt_reason}")
    else:
        print("Risk limits: not breached during this run.")


if __name__ == "__main__":
    main()
