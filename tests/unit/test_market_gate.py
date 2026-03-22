"""Tests for MarketGate."""

from tw_signal_engine.config.strategy_config import StrategyGlobalConfig
from tw_signal_engine.records.market_event_records import MarketTick, QuotePair
from tw_signal_engine.replay.apply_market_gate import MarketGate


def _make_tick(price: int, time_str: int = 90000000000) -> MarketTick:
    tick = MarketTick(symbol="0050", trade_code=1, match_time_str=time_str)
    tick.match = QuotePair(price=price, qty=100)
    return tick


class TestMarketGate:
    def test_initial_state(self):
        cfg = StrategyGlobalConfig()
        gate = MarketGate(cfg, p0050_prev=1000000)
        assert gate.p0050_latest == 0
        assert not gate.market_disabled

    def test_first_tick_sets_open(self):
        cfg = StrategyGlobalConfig(
            market_open_min_chg=-99.0,
            market_rally_disable_threshold=99.0,
        )
        gate = MarketGate(cfg, p0050_prev=1000000)
        tick = _make_tick(1010000, 90000000000)
        gate.on_tick(tick)
        assert gate.p0050_latest == 1010000
        assert gate.p0050_open == 1010000

    def test_rally_disables_market(self):
        cfg = StrategyGlobalConfig(
            market_open_min_chg=-99.0,
            market_rally_disable_threshold=0.01,
        )
        gate = MarketGate(cfg, p0050_prev=1000000)
        # First tick at open
        gate.on_tick(_make_tick(1000000, 90000000000))
        # Tick at 09:15 with 2% gain
        gate.on_tick(_make_tick(1020000, 91500000000))
        assert gate.market_disabled

    def test_open_min_chg_disables(self):
        cfg = StrategyGlobalConfig(
            market_open_min_chg=0.01,  # Need at least 1% gain at open
            market_rally_disable_threshold=99.0,
        )
        gate = MarketGate(cfg, p0050_prev=1000000)
        # Open with -0.5% loss
        gate.on_tick(_make_tick(995000, 90000000000))
        assert gate.market_disabled

    def test_no_disable_when_normal(self):
        cfg = StrategyGlobalConfig(
            market_open_min_chg=-99.0,
            market_rally_disable_threshold=0.05,
        )
        gate = MarketGate(cfg, p0050_prev=1000000)
        gate.on_tick(_make_tick(1005000, 90000000000))  # +0.5%
        gate.on_tick(_make_tick(1010000, 91500000000))  # +1% at 09:15
        assert not gate.market_disabled
