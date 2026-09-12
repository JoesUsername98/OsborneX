"""Your own strategy: starts a server, starts bot participants, runs this
strategy against them, then prints a performance report -- edit MyStrategy
below with your real logic.

Run from the Python/ directory after building a CMake preset, e.g.:
    cmake --build --preset windows-msvc-debug
    uv sync
    uv run python strategies/my_strategy.py

Press Enter (or Ctrl+C) at any time to stop the simulation and print the report.
See also Python/examples/basic_strategy.py, the library's own (unmodified)
reference sample.
"""

import math
import threading
import time

import osbornex
import trademetrics

# Bot::RandomStrategy (Bot/inc/Bot/random_strategy.hpp) centers every order on the
# current top-of-book midpoint and only perturbs it by +/-25bps, so waiting for the
# ask to wander a full 1% below par (the previous `ask_price < 99.0` check) could take
# a very long time in real-time. Crossing the spread whenever it's on the favorable
# side of "par" (buy a cheap ask, sell an expensive bid) fires on virtually every
# top-of-book update instead -- trades happen almost immediately, and both legs get
# exercised so the report below has realized (not just open) PnL to show.
_PAR_PRICE = 100.0


class MyStrategy(osbornex.Strategy):
    def __init__(self) -> None:
        super().__init__()
        self.fills: list[trademetrics.Fill] = []

    def on_top_of_book(self, book: osbornex.TopOfBook) -> None:
        if not math.isnan(book.ask_price) and book.ask_price <= _PAR_PRICE:
            print(f"symbol {book.symbol}: buying at {book.ask_price}")
            self.buy(book.symbol, price=book.ask_price, qty=10)
        elif not math.isnan(book.bid_price) and book.bid_price >= _PAR_PRICE:
            print(f"symbol {book.symbol}: selling at {book.bid_price}")
            self.sell(book.symbol, price=book.bid_price, qty=10)

    def on_trade(self, trade: osbornex.TradeExecution) -> None:
        own = self.my_fill(trade)
        if own is None:
            return  # a trade between other participants -- not ours

        print(f"symbol {trade.symbol}: filled {own.side.name} {own.quantity} @ {own.price}")
        self.fills.append(
            trademetrics.Fill(
                # TradeExecution.timestamp is the server's internal ingress clock,
                # not a wall-clock epoch, so it isn't meaningful to plot against --
                # stamp with wall-clock time at the moment we observe the fill instead.
                timestamp=time.time(),
                symbol=str(trade.symbol),
                side=trademetrics.Side.BUY if own.side is osbornex.Side.BUY else trademetrics.Side.SELL,
                price=own.price,
                quantity=own.quantity,
            )
        )


def _wait_for_enter(stop_event: threading.Event) -> None:
    try:
        input()
    except EOFError:
        pass  # stdin closed out from under us -- fall back to Ctrl+C
    stop_event.set()


def main() -> None:
    strategy = MyStrategy()
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
    print(trademetrics.format_report(trademetrics.summarize(strategy.fills)))


if __name__ == "__main__":
    main()
