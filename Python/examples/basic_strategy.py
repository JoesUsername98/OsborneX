"""Runnable end-to-end example: start a server, start two bot participants, then run
a strategy against them from Python.

Run from the Python/ directory after building a CMake preset, e.g.:
    cmake --build --preset windows-msvc-debug
    uv sync
    uv run python examples/basic_strategy.py

Ctrl+C stops the strategy and tears down the bots/server.
"""

import osbornex


class MyStrategy(osbornex.Strategy):
    def on_top_of_book(self, book: osbornex.TopOfBook) -> None:
        if book.ask_price < 99.0:
            print(f"symbol {book.symbol}: buying at {book.ask_price}")
            self.buy(book.symbol, price=book.ask_price, qty=10)

    def on_trade(self, trade: osbornex.TradeExecution) -> None:
        print(f"symbol {trade.symbol}: traded {trade.quantity} @ {trade.bid_price}")


def main() -> None:
    with osbornex.Server.start() as server:
        bots = [osbornex.Bot.start(symbols=[1, 2], source=i, server=server) for i in (1, 2)]
        try:
            # source=999 must be distinct from every Bot's source (1, 2 above) --
            # order ids are namespaced by source, so reusing one causes collisions.
            client = osbornex.connect(server, source=999)
            client.run(MyStrategy(), symbols=[1, 2])
        finally:
            for bot in bots:
                bot.stop()


if __name__ == "__main__":
    main()
