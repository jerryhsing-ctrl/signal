"""0050-based market disable logic."""

from __future__ import annotations

from tw_signal_engine.config.strategy_config import StrategyGlobalConfig
from tw_signal_engine.records.market_event_records import MarketTick


class MarketGate:
    """Tracks 0050 price to decide if market should be disabled."""

    def __init__(self, config: StrategyGlobalConfig, p0050_prev: int) -> None:
        self.config = config
        self.p0050_prev = p0050_prev
        self.p0050_latest = 0
        self.p0050_open = 0
        self.market_open_chg_pct = 0.0
        self.market_disabled = False
        self._got_open = False
        self._got_915 = False

    def on_tick(self, tick: MarketTick) -> bool:
        """Process a 0050 tick. Returns True if market just got disabled."""
        if tick.symbol != "0050" or tick.trade_code != 1 or tick.match.price <= 0:
            return False

        self.p0050_latest = tick.match.price

        if not self._got_open and tick.match_time_str >= 90_000_000_000:
            self.p0050_open = tick.match.price
            self._got_open = True
            if self.p0050_prev > 0:
                open_chg = (self.p0050_open - self.p0050_prev) / self.p0050_prev
                self.market_open_chg_pct = open_chg * 100.0
                if open_chg < self.config.market_open_min_chg:
                    self.market_disabled = True
                    return True

        if not self._got_915 and tick.match_time_str >= 91_500_000_000:
            self._got_915 = True
            if self.p0050_prev > 0:
                at915_chg = (tick.match.price - self.p0050_prev) / self.p0050_prev
                if at915_chg >= self.config.market_rally_disable_threshold:
                    self.market_disabled = True
                    return True

        return False
