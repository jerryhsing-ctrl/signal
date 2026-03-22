"""Tests for IndexCalc and IndexData."""

from tw_signal_engine.state.symbol_state import IndexCalc


class TestIndexCalc:
    def test_initial_calc(self):
        calc = IndexCalc()
        idx = calc.calc(500000, 100)  # price=50.0, qty=100
        assert idx.day_high == 500000
        assert idx.day_low == 500000
        assert idx.vwap == 500000  # Only one tick

    def test_vwap_calculation(self):
        calc = IndexCalc()
        # Two ticks at different prices
        calc.calc(500000, 100)   # 50.0 * 100 = 5000
        idx = calc.calc(600000, 100)  # 60.0 * 100 = 6000
        # VWAP = (5000*10000 + 6000*10000) / 200 = 550000
        expected_vwap = (500000 * 100 + 600000 * 100) // 200
        assert idx.vwap == expected_vwap

    def test_day_high_low(self):
        calc = IndexCalc()
        calc.calc(500000, 100)
        calc.calc(600000, 100)
        idx = calc.calc(400000, 100)
        assert idx.day_high == 600000
        assert idx.day_low == 400000
