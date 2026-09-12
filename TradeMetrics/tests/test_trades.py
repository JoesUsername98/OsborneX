import math

from trademetrics import average_loss, average_win, profit_factor, win_rate


def test_win_rate_of_no_trades_is_zero():
    assert win_rate([]) == 0.0


def test_win_rate_counts_strictly_positive_pnl_as_a_win():
    assert win_rate([100.0, -50.0, 20.0, -10.0]) == 0.5


def test_profit_factor_is_gross_profit_over_gross_loss():
    assert profit_factor([100.0, -50.0, 20.0, -10.0]) == 2.0  # 120 / 60


def test_profit_factor_is_infinite_with_no_losses():
    assert profit_factor([100.0, 20.0]) == math.inf


def test_profit_factor_is_zero_with_no_trades_at_all():
    assert profit_factor([]) == 0.0


def test_average_win_and_average_loss():
    pnls = [100.0, -50.0, 20.0, -10.0]
    assert average_win(pnls) == 60.0  # (100+20)/2
    assert average_loss(pnls) == -30.0  # (-50-10)/2


def test_average_win_and_loss_are_zero_when_absent():
    assert average_win([-5.0, -10.0]) == 0.0
    assert average_loss([5.0, 10.0]) == 0.0
