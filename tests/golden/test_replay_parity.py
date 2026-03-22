"""Golden parity tests against archived C++ replay outputs."""

from __future__ import annotations

import pytest

from tests.golden.compare_baseline import compare_trade_reports

PARITY_DATES_PRIMARY = ("20260129", "20260130", "20260224", "20260225")
PARITY_DATES_SECONDARY = ("20260127", "20260128", "20260211", "20260223")
PARITY_DATES = PARITY_DATES_PRIMARY + PARITY_DATES_SECONDARY


@pytest.mark.golden
@pytest.mark.parametrize("date", PARITY_DATES)
def test_replay_matches_baseline(date: str) -> None:
    diffs = compare_trade_reports(date)
    assert not diffs, "\n".join(diffs)
