"""Derive previous-day limit-up map."""

from __future__ import annotations

from pathlib import Path

from tw_signal_engine.reference_data.load_symbol_reference import load_symbol_reference


def derive_prev_day_limit_up(
    date: str,
    files_dir: str = "./files/",
) -> dict[str, bool]:
    """Check which symbols closed at limit-up on the previous trading day.

    Compares today's previous_close with prev day's limit_up_price.
    """
    files_path = Path(files_dir)

    # Find the most recent date before `date` that has a Symbols file
    prev_date = ""
    for p in files_path.glob("Symbols_*.csv"):
        file_date = p.stem.replace("Symbols_", "")
        if len(file_date) == 8 and file_date < date:
            if not prev_date or file_date > prev_date:
                prev_date = file_date

    if not prev_date:
        return {}

    today_syms = load_symbol_reference(date, files_dir)
    prev_syms = load_symbol_reference(prev_date, files_dir)

    result: dict[str, bool] = {}
    for symbol, today_data in today_syms.items():
        prev_data = prev_syms.get(symbol)
        if prev_data is not None:
            result[symbol] = today_data.previous_close == prev_data.limit_up_price
    return result
