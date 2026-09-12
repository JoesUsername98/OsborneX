"""A mean-reversion strategy: starts a server, starts bot participants, runs
this strategy against them, then prints a performance report.

Run from the Python/ directory after building a CMake preset, e.g.:
    cmake --build --preset windows-msvc-debug
    uv sync
    uv run python strategies/mean_reversion_strategy.py

Press Enter (or Ctrl+C) at any time to stop the simulation and print the report.
See also Python/examples/basic_strategy.py, the library's own (unmodified)
reference sample.
"""

import math
import statistics
import threading
import time
from collections import deque
from dataclasses import dataclass

import osbornex
import risk
import trademetrics

# Bot::RandomStrategy (Bot/inc/Bot/random_strategy.hpp) recenters every order on the
# live top-of-book mid and only perturbs it by +/-25bps, ~100ms combined cadence
# across the two bots (submit_interval=200ms each) -- price is a bounded random walk
# with no persistent "fair value" to hardcode, so the signal below tracks a rolling
# mean/stdev of the mid instead and trades the deviation (z-score) from it.


@dataclass(frozen=True)
class MeanReversionConfig:
    window: int = 20  # rolling ticks of mid-price for mean/stdev (~2s at the bots' cadence)
    entry_z: float = 1.5  # |z| >= this opens a position
    exit_z: float = 0.5  # |z| <= this (once open) closes it -- the gap vs entry_z is the hysteresis band
    trade_qty: int = 10  # base clip size, within the bots' own 1-20 order-size range
    min_std: float = 1e-6  # floor so z-score never divides by ~0


class MeanReversionStrategy(osbornex.Strategy):
    def __init__(
        self,
        symbols: list[int],
        risk_limits: risk.RiskLimits = risk.RiskLimits(),
        friction: risk.FrictionModel = risk.FrictionModel(),
        signal: MeanReversionConfig = MeanReversionConfig(),
    ) -> None:
        super().__init__()
        self.symbols = list(symbols)
        self.signal = signal
        self.risk = risk.RiskTracker(risk_limits, friction)

        self.mid_history: dict[int, deque[float]] = {}
        # A top-of-book update can fire again at an unchanged price (e.g. a partial
        # fill against a resting order changes its quantity but not its price), which
        # would otherwise re-trigger the same entry/exit signal before the order we
        # already placed has come back as a fill -- tracking one outstanding qty per
        # symbol gates that: no new order for a symbol while one is still in flight.
        self.pending_qty: dict[int, float] = {}

    # -- market data -------------------------------------------------------

    def on_top_of_book(self, book: osbornex.TopOfBook) -> None:
        if self.risk.halted:
            return

        mid = self.risk.update_book(book)
        if mid is None:
            return

        history = self.mid_history.setdefault(book.symbol, deque(maxlen=self.signal.window))
        history.append(mid)

        if self.risk.check_drawdown_and_maybe_halt(self._flatten_positions):
            return  # just flattened + halted this tick

        if len(history) < self.signal.window:
            return  # not enough data yet for a stable mean/stdev

        mean = statistics.fmean(history)
        stdev = max(statistics.pstdev(history, mu=mean), self.signal.min_std)
        z = (mid - mean) / stdev

        if self.pending_qty.get(book.symbol, 0.0) > 0:
            return  # already have an order working for this symbol -- wait for it to resolve

        position = self.risk.position_tracker.position(str(book.symbol))
        if position == 0:
            self._maybe_enter(book, z)
        elif position > 0:
            self._maybe_exit_long(book, z, position)
        else:
            self._maybe_exit_short(book, z, position)

    # -- signal: entry/exit with hysteresis ---------------------------------

    def _place(self, symbol: int, side: osbornex.Side, price: float, qty: int, label: str, z: float | None = None) -> None:
        suffix = f" (z={z:.2f})" if z is not None else ""
        print(f"symbol {symbol}: {label} qty={qty} @ {price:.4f}{suffix}")
        if side is osbornex.Side.BUY:
            self.buy(symbol, price=price, qty=qty)
        else:
            self.sell(symbol, price=price, qty=qty)
        self.pending_qty[symbol] = self.pending_qty.get(symbol, 0.0) + qty

    def _maybe_enter(self, book: osbornex.TopOfBook, z: float) -> None:
        if z <= -self.signal.entry_z and not math.isnan(book.ask_price):
            qty = self.risk.size_order(book.ask_price, self.signal.trade_qty)
            if qty > 0:
                self._place(book.symbol, osbornex.Side.BUY, book.ask_price, qty, "entering LONG", z)
        elif z >= self.signal.entry_z and not math.isnan(book.bid_price):
            qty = self.risk.size_order(book.bid_price, self.signal.trade_qty)
            if qty > 0:
                self._place(book.symbol, osbornex.Side.SELL, book.bid_price, qty, "entering SHORT", z)

    def _maybe_exit_long(self, book: osbornex.TopOfBook, z: float, position: float) -> None:
        if z >= -self.signal.exit_z and not math.isnan(book.bid_price):
            self._place(book.symbol, osbornex.Side.SELL, book.bid_price, int(position), "exiting LONG", z)

    def _maybe_exit_short(self, book: osbornex.TopOfBook, z: float, position: float) -> None:
        if z <= self.signal.exit_z and not math.isnan(book.ask_price):
            self._place(book.symbol, osbornex.Side.BUY, book.ask_price, int(abs(position)), "exiting SHORT", z)

    # -- risk: flatten on drawdown breach ------------------------------------

    def _flatten_positions(self, equity: float, drawdown: float, limit: float) -> None:
        for symbol in self.symbols:
            if self.pending_qty.get(symbol, 0.0) > 0:
                continue  # an order is already working -- its own fill will settle the position
            position = self.risk.position_tracker.position(str(symbol))
            if position == 0:
                continue
            book = self.risk.last_book.get(symbol)
            if position > 0:
                price = book.bid_price if book and not math.isnan(book.bid_price) else self.risk.last_mid.get(symbol)
                if price is None or math.isnan(price):
                    print(f"symbol {symbol}: cannot flatten yet -- no bid/mark available")
                    continue
                self._place(symbol, osbornex.Side.SELL, price, int(position), "flattening LONG")
            else:
                price = book.ask_price if book and not math.isnan(book.ask_price) else self.risk.last_mid.get(symbol)
                if price is None or math.isnan(price):
                    print(f"symbol {symbol}: cannot flatten yet -- no ask/mark available")
                    continue
                self._place(symbol, osbornex.Side.BUY, price, int(abs(position)), "flattening SHORT")

    # -- fills: frictional cost, live PnL/cash tracking ---------------------

    def on_trade(self, trade: osbornex.TradeExecution) -> None:
        own = self.my_fill(trade)
        if own is None:
            return  # a trade between other participants -- not ours

        fill, realized = self.risk.record_fill(trade.symbol, own)
        fee = abs(fill.price - own.price) * own.quantity

        remaining = self.pending_qty.get(trade.symbol, 0.0) - own.quantity
        self.pending_qty[trade.symbol] = max(0.0, remaining)

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
    strategy = MeanReversionStrategy(symbols=[1, 2])
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
