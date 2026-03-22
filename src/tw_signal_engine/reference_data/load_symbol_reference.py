"""Load symbol reference data from Symbols_YYYYMMDD.csv files."""

from __future__ import annotations

import csv
from pathlib import Path

from tw_signal_engine.records.reference_records import ReferenceSymbol


def _open_reference_csv(path: Path) -> tuple[str, str]:
    """Read a symbol reference CSV using the first compatible legacy encoding."""
    raw = path.read_bytes()
    for encoding in ("utf-8-sig", "cp950", "big5hkscs"):
        try:
            return raw.decode(encoding), encoding
        except UnicodeDecodeError:
            continue
    raise UnicodeDecodeError("symbol_reference", raw, 0, 1, f"Unsupported encoding for {path}")


def load_symbol_reference(date: str, files_dir: str = "./files/") -> dict[str, ReferenceSymbol]:
    """Load Symbols_YYYYMMDD.csv and return symbol -> ReferenceSymbol map."""
    path = Path(files_dir) / f"Symbols_{date}.csv"
    if not path.exists():
        raise FileNotFoundError(f"Symbol reference file not found: {path}")

    result: dict[str, ReferenceSymbol] = {}
    content, _ = _open_reference_csv(path)
    reader = csv.reader(content.splitlines())
    for row in reader:
        if len(row) < 9:
            continue
        symbol = row[0].strip()
        if not symbol:
            continue
        try:
            ref = ReferenceSymbol(
                symbol=symbol,
                name=row[1].strip(),
                market=row[2].strip(),
                previous_close=float(row[3].strip()),
                limit_up_price=float(row[4].strip()),
                limit_down_price=float(row[5].strip()),
                industry=row[6].strip(),
                security=row[7].strip(),
                error_code=row[8].strip(),
            )
            result[symbol] = ref
        except (ValueError, IndexError):
            continue
    return result
