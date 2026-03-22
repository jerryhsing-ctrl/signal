"""Helpers for comparing Python replay output against archived C++ baselines."""

from __future__ import annotations

import csv
import sys
from pathlib import Path

REQUIRED_TEXT_FIELDS = ("Symbol", "SignalType", "EnterCause", "LeaveCause", "EntryTime", "ExitTime")
REQUIRED_INT_FIELDS = ("GroupRank", "MemberRank", "RawMemberRank")


def _load_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def _parse_int(value: str) -> int:
    return int(value.strip() or "0")


def compare_trade_reports(date: str, base_dir: Path | None = None) -> list[str]:
    """Compare report_trades.csv for one date and return human-readable diffs."""
    base = Path("artifacts/baseline") if base_dir is None else base_dir
    cpp_path = base / "cpp" / date / "report_trades.csv"
    py_path = base / "python" / date / "report_trades.csv"

    if not cpp_path.exists():
        return [f"Missing C++ baseline: {cpp_path}"]
    if not py_path.exists():
        return [f"Missing Python baseline: {py_path}"]

    cpp_rows = _load_rows(cpp_path)
    py_rows = _load_rows(py_path)
    diffs: list[str] = []

    if len(cpp_rows) != len(py_rows):
        diffs.append(f"Trade count mismatch: C++={len(cpp_rows)} Python={len(py_rows)}")
        return diffs

    for idx, (cpp_row, py_row) in enumerate(zip(cpp_rows, py_rows, strict=True), start=1):
        for key in REQUIRED_TEXT_FIELDS:
            if cpp_row.get(key, "").strip() != py_row.get(key, "").strip():
                diffs.append(
                    f"Trade {idx}: {key} mismatch: C++={cpp_row.get(key, '')!r} Python={py_row.get(key, '')!r}"
                )

        try:
            cpp_pnl = _parse_int(cpp_row["PnL"])
            py_pnl = _parse_int(py_row["PnL"])
        except KeyError as exc:
            diffs.append(f"Trade {idx}: missing PnL column: {exc}")
        else:
            if abs(cpp_pnl - py_pnl) > 1:
                diffs.append(f"Trade {idx}: PnL mismatch: C++={cpp_pnl} Python={py_pnl}")

        for key in REQUIRED_INT_FIELDS:
            cpp_value = cpp_row.get(key, "").strip()
            py_value = py_row.get(key, "").strip()
            if not cpp_value and not py_value:
                continue
            if _parse_int(cpp_value) != _parse_int(py_value):
                diffs.append(
                    f"Trade {idx}: {key} mismatch: C++={cpp_value or '0'} Python={py_value or '0'}"
                )

    return diffs


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    if len(args) != 1:
        print("Usage: python tests/golden/compare_baseline.py YYYYMMDD")
        return 2

    date = args[0]
    diffs = compare_trade_reports(date)
    if diffs:
        print(f"DIFFS for {date}:")
        for diff in diffs:
            print(f"  {diff}")
        return 1

    print(f"MATCH for {date}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
