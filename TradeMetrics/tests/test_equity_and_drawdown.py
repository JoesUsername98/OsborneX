from trademetrics import (
    EquityPoint,
    Fill,
    Side,
    build_equity_curve,
    drawdown_series,
    max_drawdown,
    max_drawdown_duration,
)


def test_equity_curve_is_cumulative_realized_pnl_by_default():
    fills = [
        Fill(0, "X", Side.BUY, 100.0, 10),
        Fill(1, "X", Side.SELL, 110.0, 10),  # +100
        Fill(2, "X", Side.BUY, 105.0, 5),
        Fill(3, "X", Side.SELL, 100.0, 5),  # -25
    ]
    curve = build_equity_curve(fills)
    assert curve == [
        EquityPoint(0, 0.0),
        EquityPoint(1, 100.0),
        EquityPoint(2, 100.0),
        EquityPoint(3, 75.0),
    ]


def test_equity_curve_includes_unrealized_pnl_when_mark_prices_given():
    fills = [Fill(0, "X", Side.BUY, 100.0, 10)]
    curve = build_equity_curve(fills, mark_prices_at=lambda t: {"X": 105.0})
    assert curve == [EquityPoint(0, 50.0)]  # 10 * (105-100), still open


def test_drawdown_series_tracks_decline_from_running_peak():
    curve = [EquityPoint(t, v) for t, v in enumerate([100, 90, 80, 95, 120, 110])]
    assert drawdown_series(curve) == [0, -10, -20, -5, 0, -10]


def test_max_drawdown_is_the_largest_decline():
    curve = [EquityPoint(t, v) for t, v in enumerate([100, 90, 80, 95, 120, 110])]
    assert max_drawdown(curve) == -20


def test_max_drawdown_is_zero_for_a_monotonically_rising_curve():
    curve = [EquityPoint(t, v) for t, v in enumerate([0, 10, 20, 30])]
    assert max_drawdown(curve) == 0.0


def test_max_drawdown_is_zero_for_an_empty_curve():
    assert max_drawdown([]) == 0.0


def test_max_drawdown_duration_measures_longest_underwater_stretch():
    # peak of 100 at t=0, underwater through t=3, recovers to a new peak at t=4
    curve = [EquityPoint(t, v) for t, v in enumerate([100, 90, 80, 95, 100, 70])]
    assert max_drawdown_duration(curve) == 3  # underwater from t=0 through t=3

    # never recovers -- duration runs through the last point
    curve = [EquityPoint(t, v) for t, v in enumerate([100, 90, 80])]
    assert max_drawdown_duration(curve) == 2
