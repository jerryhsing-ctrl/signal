"""Per-symbol mutable intraday state."""

from __future__ import annotations

import sys
from dataclasses import dataclass


@dataclass(slots=True)
class IndexData:
    """Calculated per-tick metrics for a symbol."""

    vwap: float = 0.0
    rolling_low: int = 0
    day_high: int = 0
    day_low: int = sys.maxsize


class IndexCalc:
    """Maintains running VWAP, day high, day low for a symbol."""

    __slots__ = ("symbol", "_price_vol_sum", "_vol_sum", "_day_high", "_day_low")

    def __init__(self) -> None:
        self.symbol = ""
        self._price_vol_sum: int = 0
        self._vol_sum: int = 0
        self._day_high: int = 0
        self._day_low: int = sys.maxsize

    def calc(self, price: int, qty: int) -> IndexData:
        self._price_vol_sum += price * qty
        self._vol_sum += qty
        vwap = self._price_vol_sum / self._vol_sum if self._vol_sum > 0 else 0.0

        if price > self._day_high:
            self._day_high = price
        if price < self._day_low:
            self._day_low = price

        return IndexData(
            vwap=vwap,
            day_high=self._day_high,
            day_low=self._day_low,
        )
