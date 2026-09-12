import socket
import threading

import pytest
from conftest import wait_until

from osbornex import wire
from osbornex.client import Client


class RecordingOrderEntryServer:
    """A plain TCP listener that captures decoded OrderEntryWire frames -- enough to
    verify what Client sends without needing a real OsborneX ServerMain."""

    def __init__(self) -> None:
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.bind(("127.0.0.1", 0))
        self._socket.listen(1)
        self.port = self._socket.getsockname()[1]
        self.received: list[tuple] = []
        self._thread = threading.Thread(target=self._accept, daemon=True)
        self._thread.start()

    def _accept(self) -> None:
        try:
            conn, _ = self._socket.accept()
        except OSError:
            return
        with conn:
            while True:
                header_bytes = self._recv_exact(conn, wire.FRAME_HEADER.size)
                if header_bytes is None:
                    return
                header = wire.decode_frame_header(header_bytes)
                payload = self._recv_exact(conn, header.payload_length)
                if payload is None:
                    return
                self.received.append(wire.ORDER_ENTRY_WIRE.unpack(payload))

    @staticmethod
    def _recv_exact(conn: socket.socket, size: int) -> bytes | None:
        buf = b""
        while len(buf) < size:
            chunk = conn.recv(size - len(buf))
            if not chunk:
                return None
            buf += chunk
        return buf

    def close(self) -> None:
        self._socket.close()


@pytest.fixture
def recording_server():
    server = RecordingOrderEntryServer()
    yield server
    server.close()


def _connected_client(server: RecordingOrderEntryServer) -> Client:
    client = Client("127.0.0.1", server.port, "239.255.99.99", 0)
    client.connect(timeout=2.0, retry_interval=0.05)
    return client


def test_buy_sends_add_with_correct_fields(recording_server):
    client = _connected_client(recording_server)
    order_id = client.buy(symbol=7, price=100.5, qty=10)
    assert wait_until(lambda: len(recording_server.received) >= 1)

    _, _source, symbol, wire_order_id, side, order_type, action, price, quantity = recording_server.received[0]
    assert symbol == 7
    assert wire_order_id == order_id
    assert side == wire.Side.BUY
    assert order_type == wire.OrderType.GOOD_TILL_CANCEL
    assert action == wire.OrderAction.ADD
    assert price == 100.5
    assert quantity == 10
    client.disconnect()


def test_sell_sends_add_with_sell_side(recording_server):
    client = _connected_client(recording_server)
    client.sell(symbol=8, price=101.0, qty=3)
    assert wait_until(lambda: len(recording_server.received) >= 1)

    _, _, symbol, _, side, _, action, price, quantity = recording_server.received[0]
    assert symbol == 8
    assert side == wire.Side.SELL
    assert action == wire.OrderAction.ADD
    assert price == 101.0
    assert quantity == 3
    client.disconnect()


def test_cancel_sends_correct_symbol_for_a_previously_bought_order(recording_server):
    client = _connected_client(recording_server)
    order_id = client.buy(symbol=3, price=10.0, qty=5)
    client.cancel(order_id)
    assert wait_until(lambda: len(recording_server.received) >= 2)

    _, _, symbol, wire_order_id, _, _, action, _, _ = recording_server.received[1]
    assert symbol == 3
    assert wire_order_id == order_id
    assert action == wire.OrderAction.CANCEL
    client.disconnect()


def test_cancel_on_unknown_order_id_raises(recording_server):
    client = _connected_client(recording_server)
    with pytest.raises(KeyError):
        client.cancel(999999)
    client.disconnect()
