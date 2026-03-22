"""Tests for position sizing."""

from tw_signal_engine.config.strategy_config import ExecutionConfig
from tw_signal_engine.execution.position_sizing import compute_entry_quantity


class TestComputeEntryQuantity:
    def test_basic_quantity(self):
        config = ExecutionConfig(position_cash=10_000_000.0)
        # ask_price = 50.0 * 10000 = 500000
        qty, eff = compute_entry_quantity(config, 500000, 500000, 0)
        assert eff == 10_000_000.0
        assert qty == 10_000_000.0 / 50.0

    def test_uses_ask_price_when_positive(self):
        config = ExecutionConfig(position_cash=1_000_000.0)
        qty, _ = compute_entry_quantity(config, 200000, 100000, 0)
        # Should use ask_price = 20.0
        assert qty == 1_000_000.0 / 20.0

    def test_falls_back_to_match_price_when_ask_zero(self):
        config = ExecutionConfig(position_cash=1_000_000.0)
        qty, _ = compute_entry_quantity(config, 0, 200000, 0)
        # Should use match_price = 20.0
        assert qty == 1_000_000.0 / 20.0

    def test_scale_nth(self):
        config = ExecutionConfig(position_cash=10_000_000.0, position_scale_nth=0.5)
        # First trade: no scaling
        _, eff1 = compute_entry_quantity(config, 500000, 500000, 0)
        assert eff1 == 10_000_000.0
        # Second trade: scaled
        _, eff2 = compute_entry_quantity(config, 500000, 500000, 1)
        assert eff2 == 5_000_000.0
