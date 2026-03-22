"""Tests for entry filters (should_enter)."""

from tw_signal_engine.config.strategy_config import ExecutionConfig
from tw_signal_engine.execution.create_entry_trade import should_enter
from tw_signal_engine.records.market_event_records import MarketTick, QuotePair
from tw_signal_engine.state.position_state import PositionState


def _make_tick(symbol: str = "2330", price: int = 500000, time_str: int = 100000000000) -> MarketTick:
    tick = MarketTick(symbol=symbol, match_time_str=time_str)
    tick.match = QuotePair(price=price, qty=100)
    tick.ask[0] = QuotePair(price=price, qty=10)
    return tick


class TestShouldEnter:
    def test_basic_entry_allowed(self):
        config = ExecutionConfig()
        tick = _make_tick()
        pos = PositionState()
        result = should_enter(config, tick, "StrongGroup", "SignalA", pos, False, 0, 0, 0.0)
        assert result is True

    def test_entry_time_limit(self):
        config = ExecutionConfig(entry_time_limit=100000000000)
        tick = _make_tick(time_str=130000000000)
        pos = PositionState()
        result = should_enter(config, tick, "StrongGroup", "SignalA", pos, False, 0, 0, 0.0)
        assert result is False

    def test_no_entry_friday(self):
        config = ExecutionConfig(no_entry_friday=True)
        tick = _make_tick()
        pos = PositionState()
        result = should_enter(config, tick, "StrongGroup", "SignalA", pos, True, 0, 0, 0.0)
        assert result is False

    def test_already_holding(self):
        config = ExecutionConfig()
        tick = _make_tick(symbol="2330")
        pos = PositionState()
        pos.stocks["2330"] = 1000
        result = should_enter(config, tick, "StrongGroup", "SignalA", pos, False, 0, 0, 0.0)
        assert result is False

    def test_max_entry_price(self):
        config = ExecutionConfig(max_entry_price=40.0)
        # Price 50.0 > max_entry_price 40.0
        tick = _make_tick(price=500000)
        pos = PositionState()
        result = should_enter(config, tick, "StrongGroup", "SignalA", pos, False, 0, 0, 0.0)
        assert result is False

    def test_prev_day_limit_up_filter(self):
        config = ExecutionConfig(filter_prev_day_limit_up=True)
        tick = _make_tick()
        tick.prev_limit_up = True
        pos = PositionState()
        result = should_enter(config, tick, "StrongGroup", "SignalA", pos, False, 0, 0, 0.0)
        assert result is False

    def test_max_0050_entry_chg(self):
        config = ExecutionConfig(max_0050_entry_chg=1.0)
        tick = _make_tick()
        pos = PositionState()
        # 0050 at +2% (prev=1000000, latest=1020000)
        result = should_enter(
            config, tick, "StrongGroup", "SignalA", pos, False,
            1000000, 1020000, 0.0,
        )
        assert result is False
