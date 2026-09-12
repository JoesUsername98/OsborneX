from trademetrics import Fill, PositionTracker, Side, realized_pnls_from_fills


def test_opening_a_position_books_no_pnl_and_sets_average_cost():
    tracker = PositionTracker()
    realized = tracker.apply(Fill(0, "X", Side.BUY, 100.0, 10))
    assert realized == 0.0
    assert tracker.position("X") == 10
    assert tracker.realized_pnl("X") == 0.0


def test_adding_to_a_position_updates_weighted_average_cost():
    tracker = PositionTracker()
    tracker.apply(Fill(0, "X", Side.BUY, 100.0, 10))
    tracker.apply(Fill(1, "X", Side.BUY, 106.0, 5))
    assert tracker.position("X") == 15
    # (100*10 + 106*5) / 15 = 102.0
    assert tracker.average_cost("X") == 102.0


def test_partial_close_books_pnl_on_closed_portion_only():
    tracker = PositionTracker()
    tracker.apply(Fill(0, "X", Side.BUY, 100.0, 10))
    realized = tracker.apply(Fill(1, "X", Side.SELL, 110.0, 4))
    assert realized == 40.0  # 4 * (110 - 100)
    assert tracker.position("X") == 6
    assert tracker.average_cost("X") == 100.0  # unchanged for the remainder


def test_exact_close_zeroes_position_and_average_cost():
    tracker = PositionTracker()
    tracker.apply(Fill(0, "X", Side.BUY, 100.0, 10))
    realized = tracker.apply(Fill(1, "X", Side.SELL, 105.0, 10))
    assert realized == 50.0
    assert tracker.position("X") == 0.0
    assert tracker.average_cost("X") == 0.0


def test_reversal_closes_old_position_and_opens_new_one_at_fill_price():
    tracker = PositionTracker()
    tracker.apply(Fill(0, "X", Side.BUY, 100.0, 10))
    realized = tracker.apply(Fill(1, "X", Side.SELL, 110.0, 15))
    assert realized == 100.0  # closed 10 @ (110-100)
    assert tracker.position("X") == -5
    assert tracker.average_cost("X") == 110.0


def test_short_position_realizes_pnl_symmetrically():
    tracker = PositionTracker()
    tracker.apply(Fill(0, "X", Side.SELL, 100.0, 10))
    realized = tracker.apply(Fill(1, "X", Side.BUY, 90.0, 10))
    assert realized == 100.0  # short covered at a profit: 10 * (90-100) * -1
    assert tracker.position("X") == 0.0


def test_symbols_are_tracked_independently():
    tracker = PositionTracker()
    tracker.apply(Fill(0, "X", Side.BUY, 100.0, 10))
    tracker.apply(Fill(1, "Y", Side.SELL, 50.0, 5))
    assert tracker.position("X") == 10
    assert tracker.position("Y") == -5
    assert tracker.realized_pnl() == 0.0  # neither has closed yet


def test_unrealized_pnl_marks_open_positions_to_supplied_prices():
    tracker = PositionTracker()
    tracker.apply(Fill(0, "X", Side.BUY, 100.0, 10))
    assert tracker.unrealized_pnl({"X": 105.0}) == 50.0
    assert tracker.unrealized_pnl({}) == 0.0  # no mark price given for X


def test_realized_pnls_from_fills_only_includes_closing_fills():
    fills = [
        Fill(0, "X", Side.BUY, 100.0, 10),  # opens -- no pnl
        Fill(1, "X", Side.SELL, 110.0, 10),  # closes -- +100
        Fill(2, "X", Side.BUY, 105.0, 5),  # opens again -- no pnl
        Fill(3, "X", Side.SELL, 100.0, 5),  # closes -- -25
    ]
    assert realized_pnls_from_fills(fills) == [100.0, -25.0]
