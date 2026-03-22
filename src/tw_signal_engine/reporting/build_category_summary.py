"""Generate report_by_category.csv."""

from __future__ import annotations

import csv
from collections import defaultdict
from pathlib import Path

from tw_signal_engine.records.market_event_records import TradeRecord


def write_category_report(completed_trades: list[TradeRecord], log_dir: str) -> None:
    """Write report_by_category.csv."""
    if not completed_trades:
        return

    by_signal: dict[str, list[float]] = defaultdict(list)
    by_cause: dict[str, list[float]] = defaultdict(list)
    by_leave: dict[str, list[float]] = defaultdict(list)

    for t in completed_trades:
        by_signal[t.signal_type].append(t.pnl)
        by_cause[t.enter_cause].append(t.pnl)
        by_leave[t.final_leave_cause].append(t.pnl)

    path = Path(log_dir) / "report_by_category.csv"
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Category", "Value", "Count", "WinRate", "TotalPnL", "AvgPnL"])

        def write_group(cat: str, m: dict[str, list[float]]) -> None:
            for key, pnls in m.items():
                cnt = len(pnls)
                total = sum(pnls)
                wins = sum(1 for p in pnls if p > 0)
                w.writerow([
                    cat, key, str(cnt),
                    f"{wins * 100.0 / cnt:.1f}%",
                    f"{total:.0f}",
                    f"{total / cnt:.0f}",
                ])

        write_group("SignalType", by_signal)
        write_group("EnterCause", by_cause)
        write_group("LeaveCause", by_leave)
    print(f"[Report] {path}")
