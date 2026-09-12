import math

from trademetrics import EquityPoint, returns_from_equity_curve, sharpe_ratio, sortino_ratio, volatility


def test_returns_from_equity_curve_are_pct_change_between_points():
    curve = [EquityPoint(0, 100.0), EquityPoint(1, 110.0), EquityPoint(2, 99.0)]
    returns = returns_from_equity_curve(curve)
    assert returns == [0.10, -0.10]


def test_returns_skip_a_step_starting_from_zero_equity():
    curve = [EquityPoint(0, 0.0), EquityPoint(1, 50.0), EquityPoint(2, 55.0)]
    returns = returns_from_equity_curve(curve)
    assert returns == [0.10]  # only the 50 -> 55 step is well-defined


def test_volatility_of_constant_returns_is_zero():
    assert volatility([0.01, 0.01, 0.01]) == 0.0


def test_volatility_needs_at_least_two_samples():
    assert volatility([]) == 0.0
    assert volatility([0.05]) == 0.0


def test_volatility_matches_hand_computed_population_stddev():
    returns = [0.1, -0.1, 0.1, -0.1]
    # mean = 0, variance = mean((r-0)^2) = 0.01, stddev = 0.1
    assert math.isclose(volatility(returns), 0.1)


def test_sharpe_ratio_is_zero_when_returns_have_no_volatility():
    assert sharpe_ratio([0.01, 0.01, 0.01]) == 0.0


def test_sharpe_ratio_is_positive_for_consistently_positive_excess_returns():
    returns = [0.02, 0.01, 0.03, 0.015]
    assert sharpe_ratio(returns, risk_free_rate=0.0) > 0.0


def test_sortino_ratio_ignores_upside_volatility():
    # All returns are >= the risk-free rate, so there's no downside deviation.
    returns = [0.01, 0.05, 0.02, 0.10]
    assert sortino_ratio(returns, risk_free_rate=0.0) == 0.0
