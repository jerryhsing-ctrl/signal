"""Tests for TopKVolumeTracker."""

from tw_signal_engine.screening.top_volume_pool import TopKVolumeTracker


class TestTopKVolumeTracker:
    def test_empty(self):
        tracker = TopKVolumeTracker(k=3)
        assert not tracker.in_pool("A")

    def test_add_within_k(self):
        tracker = TopKVolumeTracker(k=3)
        tracker.on_tick("A", 100)
        tracker.on_tick("B", 200)
        assert tracker.in_pool("A")
        assert tracker.in_pool("B")

    def test_eviction_when_full(self):
        tracker = TopKVolumeTracker(k=2)
        tracker.on_tick("A", 100)
        tracker.on_tick("B", 200)
        tracker.on_tick("C", 300)
        # A has lowest volume, should be evicted
        assert tracker.in_pool("B")
        assert tracker.in_pool("C")
        assert not tracker.in_pool("A")

    def test_cumulative_volume(self):
        tracker = TopKVolumeTracker(k=2)
        tracker.on_tick("A", 50)
        tracker.on_tick("B", 200)
        tracker.on_tick("C", 150)
        # A=50, B=200, C=150 => top-2: B, C
        assert not tracker.in_pool("A")
        # Now A gets more volume
        tracker.on_tick("A", 300)
        # A=350, B=200, C=150 => top-2: A, B
        assert tracker.in_pool("A")
        assert tracker.in_pool("B")
