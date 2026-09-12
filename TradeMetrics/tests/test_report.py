from trademetrics import Fill, Side, format_report, summarize


def _sample_fills():
    return [
        Fill(0, "X", Side.BUY, 100.0, 10),
        Fill(1, "X", Side.SELL, 110.0, 10),  # +100
        Fill(2, "X", Side.BUY, 105.0, 5),
        Fill(3, "X", Side.SELL, 100.0, 5),  # -25
    ]


def test_summarize_combines_pnl_drawdown_and_trade_stats():
    report = summarize(_sample_fills())
    assert report.total_realized_pnl == 75.0
    assert report.total_trades == 2
    assert report.win_rate == 0.5
    assert report.max_drawdown == -25.0  # peak 100 at t=1, drops to 75 at t=3
    assert len(report.equity_curve) == 4


def test_summarize_of_no_fills_is_all_zeros():
    report = summarize([])
    assert report.total_realized_pnl == 0.0
    assert report.total_trades == 0
    assert report.win_rate == 0.0
    assert report.max_drawdown == 0.0
    assert report.equity_curve == []


def test_format_report_is_human_readable_and_omits_the_raw_curve():
    text = format_report(summarize(_sample_fills()))
    assert "Total realized PnL:" in text
    assert "Sharpe ratio:" in text
    assert "EquityPoint" not in text
