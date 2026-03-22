"""Tests for Taiwan tick size rules."""

from tw_signal_engine.execution.taiwan_tick_size import (
    get_category,
    get_price_cond,
    get_tick,
)

PRICE_SCALE = 10000


class TestGetCategory:
    def test_regular_stock(self):
        assert get_category("2330") == "1"
        assert get_category("2317") == "1"

    def test_small_etf_code(self):
        # Codes < 100 are category 7 per implementation
        assert get_category("0050") == "7"
        assert get_category("0056") == "7"

    def test_etf_range(self):
        # Codes 100-199 are category 4
        assert get_category("0100") == "4"

    def test_empty(self):
        assert get_category("") == "1"


class TestGetTick:
    def test_stock_high_price(self):
        # Price >= 1000 => tick = 5.0
        assert get_tick("2330", 1000.0, True) == 5.0

    def test_stock_mid_price(self):
        # 100 <= price < 500 => tick = 0.5
        assert get_tick("2330", 200.0, True) == 0.5

    def test_stock_low_price(self):
        # 10 <= price < 50 => tick = 0.05
        assert get_tick("2330", 25.0, True) == 0.05

    def test_category7_uses_rule1(self):
        # Category 7 (code < 100) uses RULE1 (default)
        # 100 <= price < 500 => tick = 0.5
        assert get_tick("0050", 100.0, True) == 0.5


class TestGetPriceCond:
    def test_zero_ticks(self):
        price = 500000  # 50.0 * 10000
        assert get_price_cond("2330", price, 0) == price

    def test_one_tick_up(self):
        # 50.0 => tick is 0.1, so 50.0 + 0.1 = 50.1
        price = int(50.0 * PRICE_SCALE)
        result = get_price_cond("2330", price, 1)
        assert abs(result - int(50.1 * PRICE_SCALE)) <= 1

    def test_one_tick_down(self):
        # 50.0 => tick for down is at 49.9999 => 10 <= p < 50 => tick = 0.05
        price = int(50.0 * PRICE_SCALE)
        result = get_price_cond("2330", price, -1)
        assert abs(result - int(49.95 * PRICE_SCALE)) <= 1

    def test_multiple_ticks_up(self):
        price = int(50.0 * PRICE_SCALE)
        result = get_price_cond("2330", price, 3)
        # 50.0 + 0.1 + 0.1 + 0.1 = 50.3
        assert abs(result - int(50.3 * PRICE_SCALE)) <= 1

    def test_empty_symbol(self):
        assert get_price_cond("", 500000, 1) == 500000
