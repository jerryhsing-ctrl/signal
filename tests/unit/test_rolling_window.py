"""Tests for RollingLow and RollingSum."""

from tw_signal_engine.state.rolling_window import RollingLow, RollingSum


class TestRollingLow:
    def test_empty(self):
        rl = RollingLow(duration=1000)
        assert rl.empty()
        assert rl.get_low() == 0

    def test_single_value(self):
        rl = RollingLow(duration=1000)
        rl.update(100, 50)
        assert rl.get_low() == 50
        assert not rl.empty()

    def test_decreasing_values(self):
        rl = RollingLow(duration=10000)
        rl.update(100, 50)
        rl.update(200, 40)
        rl.update(300, 30)
        assert rl.get_low() == 30

    def test_increasing_values(self):
        rl = RollingLow(duration=10000)
        rl.update(100, 30)
        rl.update(200, 40)
        rl.update(300, 50)
        assert rl.get_low() == 30

    def test_expiration(self):
        rl = RollingLow(duration=100)
        rl.update(100, 30)
        rl.update(150, 50)
        # At time 250, the value at time 100 has expired (250-100=150 > 100)
        rl.update(250, 60)
        assert rl.get_low() == 50

    def test_monotonic_deque_property(self):
        """After inserting a lower value, all higher values are removed."""
        rl = RollingLow(duration=10000)
        rl.update(100, 50)
        rl.update(200, 60)
        rl.update(300, 40)
        # 50 and 60 are gone because 40 <= both
        assert rl.get_low() == 40


class TestRollingSum:
    def test_empty(self):
        rs = RollingSum(duration=1000)
        assert rs.get_sum() == 0

    def test_single_value(self):
        rs = RollingSum(duration=1000)
        rs.update(100, 42)
        assert rs.get_sum() == 42

    def test_accumulation(self):
        rs = RollingSum(duration=10000)
        rs.update(100, 10)
        rs.update(200, 20)
        rs.update(300, 30)
        assert rs.get_sum() == 60

    def test_expiration(self):
        rs = RollingSum(duration=100)
        rs.update(100, 10)
        rs.update(150, 20)
        # At time 250, value at time 100 expires (250-100=150 > 100)
        rs.update(250, 5)
        assert rs.get_sum() == 25  # 20 + 5

    def test_set_duration(self):
        rs = RollingSum()
        rs.set_duration(50)
        rs.update(100, 10)
        rs.update(200, 20)
        # Time 100 expired (200-100=100 > 50)
        assert rs.get_sum() == 20
