import math

from osbornex import wire


def test_struct_sizes_match_cpp_sizeof():
    assert wire.FRAME_HEADER.size == 8
    assert wire.ORDER_ENTRY_WIRE.size == 37
    assert wire.TOP_OF_BOOK_WIRE.size == 36
    assert wire.TRADE_EXECUTION_WIRE.size == 56


def test_frame_header_round_trips():
    data = wire.encode_frame_header(wire.MessageType.TOP_OF_BOOK, 36)
    header = wire.decode_frame_header(data)
    assert header.magic == wire.FRAME_MAGIC
    assert header.version == wire.FRAME_VERSION
    assert header.message_type == wire.MessageType.TOP_OF_BOOK
    assert header.payload_length == 36


def test_frame_header_rejects_bad_magic():
    data = wire.encode_frame_header(wire.MessageType.TOP_OF_BOOK, 36)
    corrupted = (0).to_bytes(4, "little") + data[4:]
    try:
        wire.decode_frame_header(corrupted)
        assert False, "expected ProtocolError"
    except wire.ProtocolError:
        pass


def test_order_entry_round_trips_via_raw_struct():
    payload = wire.encode_order_entry(
        source_timestamp=123456789,
        source=7,
        symbol=42,
        order_id=(2**64) - 1,
        side=wire.Side.SELL,
        order_type=wire.OrderType.GOOD_TILL_CANCEL,
        action=wire.OrderAction.ADD,
        price=101.5,
        quantity=10,
    )
    assert len(payload) == wire.ORDER_ENTRY_WIRE.size
    decoded = wire.ORDER_ENTRY_WIRE.unpack(payload)
    source_timestamp, source, symbol, order_id, side, order_type, action, price, quantity = decoded
    assert source_timestamp == 123456789
    assert source == 7
    assert symbol == 42
    assert order_id == (2**64) - 1
    assert side == wire.Side.SELL
    assert order_type == wire.OrderType.GOOD_TILL_CANCEL
    assert action == wire.OrderAction.ADD
    assert price == 101.5
    assert quantity == 10


def test_top_of_book_round_trips_including_nan_prices():
    # TopOfBookUpdate defaults bid_price/ask_price to NaN when there's no resting
    # order on that side (Messages/inc/Messages/types.hpp) -- confirm the NaN bit
    # pattern survives struct packing.
    payload = wire.TOP_OF_BOOK_WIRE.pack(5, float("nan"), 0, 102.25, 20, 99)
    book = wire.decode_top_of_book(payload)
    assert book.symbol == 5
    assert math.isnan(book.bid_price)
    assert book.bid_quantity == 0
    assert book.ask_price == 102.25
    assert book.ask_quantity == 20
    assert book.sequence == 99


def test_trade_execution_round_trips():
    payload = wire.TRADE_EXECUTION_WIRE.pack(9, 1, 2, 1, 50.0, 2, 50.0, 4)
    trade = wire.decode_trade_execution(payload)
    assert trade.symbol == 9
    assert trade.sequence == 1
    assert trade.timestamp == 2
    assert trade.bid_order_id == 1
    assert trade.bid_price == 50.0
    assert trade.ask_order_id == 2
    assert trade.ask_price == 50.0
    assert trade.quantity == 4
