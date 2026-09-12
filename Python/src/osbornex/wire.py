"""Byte-exact mirror of OsborneX's C++ wire protocol (Net/inc/Net/wire/*.hpp).

Both OsborneX build targets are little-endian x86-64 and use `#pragma pack(push, 1)`
with no network-byte-order conversion anywhere in the wire layer -- this module makes
the same assumption (all struct formats below use "<", i.e. little-endian/no padding).
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from enum import IntEnum

FRAME_MAGIC = 0x4F584E31  # matches Net::wire::kFrameMagic ("OXN1")
FRAME_VERSION = 1  # matches Net::wire::kFrameVersion

# magic, version, message_type, payload_length -- Net/inc/Net/wire/frame.hpp
FRAME_HEADER = struct.Struct("<IBBH")

# source_timestamp, source, symbol, order_id, side, order_type, action, price, quantity
# -- Net/inc/Net/wire/order_entry_wire.hpp
ORDER_ENTRY_WIRE = struct.Struct("<QHIQBBBdI")

# symbol, bid_price, bid_quantity, ask_price, ask_quantity, sequence
# -- Net/inc/Net/wire/market_data_wire.hpp
TOP_OF_BOOK_WIRE = struct.Struct("<IdIdIQ")

# symbol, sequence, timestamp, bid_order_id, bid_price, ask_order_id, ask_price, quantity
# -- Net/inc/Net/wire/market_data_wire.hpp
TRADE_EXECUTION_WIRE = struct.Struct("<IQQQdQdI")


class MessageType(IntEnum):
    """Net::wire::MessageType."""

    ORDER_ENTRY = 1
    TOP_OF_BOOK = 2
    TRADE_EXECUTION = 3


class Side(IntEnum):
    """OsborneX::Side (Orderbook/inc/Orderbook/side.hpp)."""

    BUY = 0
    SELL = 1


class OrderType(IntEnum):
    """OsborneX::OrderType (Orderbook/inc/Orderbook/order_type.hpp)."""

    GOOD_TILL_CANCEL = 0
    FILL_AND_KILL = 1
    FILL_OR_KILL = 2
    MARKET = 3


class OrderAction(IntEnum):
    """OsborneX::Simulation::OrderAction (Messages/inc/Messages/types.hpp).

    MODIFY is defined for wire completeness but never sent by this client --
    order scope here is add + cancel only.
    """

    ADD = 0
    CANCEL = 1
    MODIFY = 2


class ProtocolError(Exception):
    """Raised when a received frame's magic/version doesn't match this protocol."""


@dataclass(frozen=True)
class FrameHeader:
    magic: int
    version: int
    message_type: int
    payload_length: int


@dataclass(frozen=True)
class TopOfBook:
    symbol: int
    bid_price: float
    bid_quantity: int
    ask_price: float
    ask_quantity: int
    sequence: int


@dataclass(frozen=True)
class TradeExecution:
    symbol: int
    sequence: int
    timestamp: int
    bid_order_id: int
    bid_price: float
    ask_order_id: int
    ask_price: float
    quantity: int


def encode_frame_header(message_type: MessageType, payload_length: int) -> bytes:
    return FRAME_HEADER.pack(FRAME_MAGIC, FRAME_VERSION, int(message_type), payload_length)


def decode_frame_header(data: bytes) -> FrameHeader:
    magic, version, message_type, payload_length = FRAME_HEADER.unpack(data)
    if magic != FRAME_MAGIC or version != FRAME_VERSION:
        raise ProtocolError(
            f"unrecognized frame header (magic={magic:#x}, version={version}); "
            f"expected magic={FRAME_MAGIC:#x}, version={FRAME_VERSION}"
        )
    return FrameHeader(magic, version, message_type, payload_length)


def encode_order_entry(
    *,
    source_timestamp: int,
    source: int,
    symbol: int,
    order_id: int,
    side: Side,
    order_type: OrderType,
    action: OrderAction,
    price: float,
    quantity: int,
) -> bytes:
    return ORDER_ENTRY_WIRE.pack(
        source_timestamp,
        source,
        symbol,
        order_id,
        int(side),
        int(order_type),
        int(action),
        price,
        quantity,
    )


def decode_top_of_book(payload: bytes) -> TopOfBook:
    symbol, bid_price, bid_quantity, ask_price, ask_quantity, sequence = TOP_OF_BOOK_WIRE.unpack(payload)
    return TopOfBook(symbol, bid_price, bid_quantity, ask_price, ask_quantity, sequence)


def decode_trade_execution(payload: bytes) -> TradeExecution:
    symbol, sequence, timestamp, bid_order_id, bid_price, ask_order_id, ask_price, quantity = (
        TRADE_EXECUTION_WIRE.unpack(payload)
    )
    return TradeExecution(symbol, sequence, timestamp, bid_order_id, bid_price, ask_order_id, ask_price, quantity)
