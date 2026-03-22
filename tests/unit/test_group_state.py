"""Tests for GroupRank."""

from tw_signal_engine.state.group_state import GroupRank


class TestGroupRank:
    def test_empty(self):
        gr = GroupRank()
        assert gr.get_rank("A") == -1
        assert not gr.is_top_n("A", 1)

    def test_single_entry(self):
        gr = GroupRank()
        gr.on_tick("A", 0.05)
        assert gr.get_rank("A") == 1
        assert gr.is_top_n("A", 1)

    def test_ranking_order(self):
        gr = GroupRank()
        gr.on_tick("A", 0.03)
        gr.on_tick("B", 0.05)
        gr.on_tick("C", 0.01)
        # Descending: B(0.05) > A(0.03) > C(0.01)
        assert gr.get_rank("B") == 1
        assert gr.get_rank("A") == 2
        assert gr.get_rank("C") == 3

    def test_update_rank(self):
        gr = GroupRank()
        gr.on_tick("A", 0.03)
        gr.on_tick("B", 0.05)
        # A jumps ahead
        gr.on_tick("A", 0.10)
        assert gr.get_rank("A") == 1
        assert gr.get_rank("B") == 2

    def test_erase(self):
        gr = GroupRank()
        gr.on_tick("A", 0.03)
        gr.on_tick("B", 0.05)
        gr.erase("B")
        assert gr.get_rank("B") == -1
        assert gr.get_rank("A") == 1

    def test_is_top_n(self):
        gr = GroupRank()
        gr.on_tick("A", 0.05)
        gr.on_tick("B", 0.04)
        gr.on_tick("C", 0.03)
        assert gr.is_top_n("A", 1)
        assert gr.is_top_n("B", 2)
        assert not gr.is_top_n("C", 2)
        assert gr.is_top_n("C", 3)

    def test_collision_bug_parity(self):
        """C++ map<double, string> overwrites when same gain is inserted."""
        gr = GroupRank()
        gr.on_tick("A", 0.05)
        gr.on_tick("B", 0.05)  # Same gain overwrites A
        assert gr.get_rank("B") == 1
        # A's name_to_gain still exists but gain_to_name points to B
        # So get_rank("A") iterates and won't find "A" in gain_to_name
        assert gr.get_rank("A") == -1

    def test_iter_ranked(self):
        gr = GroupRank()
        gr.on_tick("A", 0.03)
        gr.on_tick("B", 0.05)
        gr.on_tick("C", 0.01)
        ranked = gr.iter_ranked()
        assert ranked == [(0.05, "B"), (0.03, "A"), (0.01, "C")]
