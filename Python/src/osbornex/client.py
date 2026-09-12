"""TCP order-entry client + UDP multicast market-data receiver.

Mirrors Net::OrderEntryClient (Net/src/order_entry_client.cpp) and
Net::MarketDataListener (Net/src/market_data_multicast.cpp) in pure Python.
"""

from __future__ import annotations

import socket
import struct
import threading
import time
from collections.abc import Callable, Iterable
from typing import TYPE_CHECKING

from . import wire

if TYPE_CHECKING:
    from .orchestration import Server
    from .strategy import Strategy

# Orderbook::AddOrder silently drops a duplicate order_id with no diagnostic
# (Orderbook/src/orderbook.cpp), and nothing in the stack enforces uniqueness across
# sources. Reserving the top 16 bits of the 64-bit order_id for `source` and the low
# 48 bits for a local counter (exactly the scheme Bot::RandomStrategy uses --
# Bot/src/random_strategy.cpp) guarantees this Client's order ids never collide with
# a bot's, as long as every participant is given a distinct `source`.
_LOCAL_ID_BITS = 48
_LOCAL_ID_MASK = (1 << _LOCAL_ID_BITS) - 1
DEFAULT_CLIENT_SOURCE = 65535  # top of the uint16_t SourceId range -- unlikely to
# collide with small bot sources (1, 2, ...), but every Bot/Client sharing a run
# should still be given an explicit, distinct `source`.


def make_order_id(source: int, local_id: int) -> int:
    return (source << _LOCAL_ID_BITS) | (local_id & _LOCAL_ID_MASK)


class MarketDataReceiver:
    """Joins a UDP multicast group and dispatches decoded frames to callbacks.

    Polls with a socket timeout in a loop checking a stop flag, rather than a
    blocking recvfrom with no way to interrupt it -- same reasoning as
    Net::detail::wait_readable in the C++ listener.
    """

    def __init__(self, group: str, port: int, poll_timeout: float = 0.1) -> None:
        self._group = group
        self._port = port
        self._poll_timeout = poll_timeout
        self._socket: socket.socket | None = None
        self._thread: threading.Thread | None = None
        self._stop_event = threading.Event()
        self._handlers_lock = threading.Lock()
        self._top_of_book_handlers: list[Callable[[wire.TopOfBook], None]] = []
        self._trade_handlers: list[Callable[[wire.TradeExecution], None]] = []

    def add_top_of_book_handler(self, callback: Callable[[wire.TopOfBook], None]) -> None:
        with self._handlers_lock:
            self._top_of_book_handlers.append(callback)

    def add_trade_handler(self, callback: Callable[[wire.TradeExecution], None]) -> None:
        with self._handlers_lock:
            self._trade_handlers.append(callback)

    def start(self) -> None:
        if self._thread is not None:
            return
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("", self._port))
        membership = struct.pack("4sl", socket.inet_aton(self._group), socket.INADDR_ANY)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, membership)
        sock.settimeout(self._poll_timeout)
        self._socket = sock
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        if self._thread is None:
            return
        self._stop_event.set()
        self._thread.join(timeout=2.0)
        self._thread = None
        if self._socket is not None:
            self._socket.close()
            self._socket = None

    def _run(self) -> None:
        assert self._socket is not None
        header_size = wire.FRAME_HEADER.size
        while not self._stop_event.is_set():
            try:
                data, _ = self._socket.recvfrom(512)
            except socket.timeout:
                continue
            except OSError:
                return

            if len(data) < header_size:
                continue
            try:
                header = wire.decode_frame_header(data[:header_size])
            except wire.ProtocolError:
                continue  # stray/foreign datagram on the group -- ignore

            payload = data[header_size:]
            if header.message_type == wire.MessageType.TOP_OF_BOOK and len(payload) >= wire.TOP_OF_BOOK_WIRE.size:
                book = wire.decode_top_of_book(payload[: wire.TOP_OF_BOOK_WIRE.size])
                with self._handlers_lock:
                    handlers = list(self._top_of_book_handlers)
                for handler in handlers:
                    handler(book)
            elif (
                header.message_type == wire.MessageType.TRADE_EXECUTION
                and len(payload) >= wire.TRADE_EXECUTION_WIRE.size
            ):
                trade = wire.decode_trade_execution(payload[: wire.TRADE_EXECUTION_WIRE.size])
                with self._handlers_lock:
                    handlers = list(self._trade_handlers)
                for handler in handlers:
                    handler(trade)


class Client:
    """TCP order-entry connection to an OsborneX server, plus market-data subscription."""

    def __init__(
        self,
        host: str,
        order_entry_port: int,
        market_data_group: str,
        market_data_port: int,
        source: int = DEFAULT_CLIENT_SOURCE,
    ) -> None:
        self._host = host
        self._order_entry_port = order_entry_port
        self._market_data_group = market_data_group
        self._market_data_port = market_data_port
        self._source = source

        self._socket: socket.socket | None = None
        self._send_lock = threading.Lock()
        self._next_local_id = 1
        self._order_symbols: dict[int, int] = {}
        self._market_data: MarketDataReceiver | None = None

    def connect(self, timeout: float = 10.0, retry_interval: float = 0.5) -> None:
        """Poll-connects until the order-entry port accepts, mirroring BotMain's own
        retry loop (Bot/apps/bot_main.cpp) for the case a server was just started."""
        deadline = time.monotonic() + timeout
        last_error: Exception | None = None
        while time.monotonic() < deadline:
            try:
                sock = socket.create_connection((self._host, self._order_entry_port), timeout=retry_interval)
            except OSError as exc:
                last_error = exc
                time.sleep(retry_interval)
                continue
            sock.settimeout(None)
            self._socket = sock
            return
        raise ConnectionError(
            f"could not connect to order-entry server at {self._host}:{self._order_entry_port} within {timeout}s"
        ) from last_error

    def disconnect(self) -> None:
        if self._market_data is not None:
            self._market_data.stop()
            self._market_data = None
        if self._socket is not None:
            self._socket.close()
            self._socket = None

    def __enter__(self) -> "Client":
        self.connect()
        return self

    def __exit__(self, *exc_info: object) -> None:
        self.disconnect()

    def buy(self, symbol: int, price: float, qty: int) -> int:
        return self._add(symbol, wire.Side.BUY, price, qty)

    def sell(self, symbol: int, price: float, qty: int) -> int:
        return self._add(symbol, wire.Side.SELL, price, qty)

    def cancel(self, order_id: int) -> None:
        try:
            symbol = self._order_symbols[order_id]
        except KeyError as exc:
            raise KeyError(f"unknown order_id {order_id} -- was it submitted by this Client?") from exc
        # Side/price/quantity are ignored for a Cancel action (Sharding/src/shard.cpp
        # only reads order_id), so these are inert placeholders. symbol must be
        # correct though -- it's what routes the message to the right shard/book.
        self._send(
            symbol=symbol,
            side=wire.Side.BUY,
            order_type=wire.OrderType.GOOD_TILL_CANCEL,
            action=wire.OrderAction.CANCEL,
            price=0.0,
            quantity=0,
            order_id=order_id,
        )

    def _add(self, symbol: int, side: wire.Side, price: float, qty: int) -> int:
        with self._send_lock:
            local_id = self._next_local_id
            self._next_local_id += 1
        order_id = make_order_id(self._source, local_id)
        self._send(
            symbol=symbol,
            side=side,
            order_type=wire.OrderType.GOOD_TILL_CANCEL,
            action=wire.OrderAction.ADD,
            price=price,
            quantity=qty,
            order_id=order_id,
        )
        self._order_symbols[order_id] = symbol
        return order_id

    def _send(
        self,
        *,
        symbol: int,
        side: wire.Side,
        order_type: wire.OrderType,
        action: wire.OrderAction,
        price: float,
        quantity: int,
        order_id: int,
    ) -> None:
        if self._socket is None:
            raise ConnectionError("Client is not connected -- call connect() first")
        payload = wire.encode_order_entry(
            source_timestamp=time.time_ns(),
            source=self._source,
            symbol=symbol,
            order_id=order_id,
            side=side,
            order_type=order_type,
            action=action,
            price=price,
            quantity=quantity,
        )
        header = wire.encode_frame_header(wire.MessageType.ORDER_ENTRY, len(payload))
        with self._send_lock:
            self._socket.sendall(header + payload)

    def subscribe(
        self,
        symbols: Iterable[int],
        on_top_of_book: Callable[[wire.TopOfBook], None] | None = None,
        on_trade: Callable[[wire.TradeExecution], None] | None = None,
    ) -> None:
        wanted = frozenset(symbols)
        if self._market_data is None:
            self._market_data = MarketDataReceiver(self._market_data_group, self._market_data_port)

        if on_top_of_book is not None:

            def dispatch_top(book: wire.TopOfBook, _wanted: frozenset[int] = wanted, _cb=on_top_of_book) -> None:
                if book.symbol in _wanted:
                    _cb(book)

            self._market_data.add_top_of_book_handler(dispatch_top)

        if on_trade is not None:

            def dispatch_trade(
                trade: wire.TradeExecution, _wanted: frozenset[int] = wanted, _cb=on_trade
            ) -> None:
                if trade.symbol in _wanted:
                    _cb(trade)

            self._market_data.add_trade_handler(dispatch_trade)

        self._market_data.start()

    def run(
        self,
        strategy: "Strategy",
        symbols: Iterable[int],
        stop_event: threading.Event | None = None,
    ) -> None:
        """Binds strategy to this client, subscribes its callbacks, and blocks the
        calling thread until interrupted (Ctrl+C) or, if given, until stop_event is
        set by another thread (e.g. a driver script waiting on terminal input)."""
        strategy._bind(self)
        self.subscribe(symbols, on_top_of_book=strategy.on_top_of_book, on_trade=strategy.on_trade)
        event = stop_event if stop_event is not None else threading.Event()
        try:
            while not event.is_set():
                time.sleep(0.2)
        except KeyboardInterrupt:
            pass
        finally:
            self.disconnect()


def connect(server: "Server", *, source: int = DEFAULT_CLIENT_SOURCE, timeout: float = 10.0) -> Client:
    client = Client(
        server.host,
        server.order_entry_port,
        server.market_data_group,
        server.market_data_port,
        source=source,
    )
    client.connect(timeout=timeout)
    return client
