"""Tests for history-window loading."""

from __future__ import annotations

import pytest

from tw_signal_engine.market_data.load_history_window import load_history_window


def test_load_history_window_requires_target_day_file(tmp_path) -> None:
    (tmp_path / "OTCQuote.20260128").write_text("", encoding="utf-8")

    with pytest.raises(FileNotFoundError, match="OTCQuote\\.20260129"):
        load_history_window("OTC", "20260129", str(tmp_path))
