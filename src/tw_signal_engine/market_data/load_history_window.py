"""Load 21-session history window of cumulative volume/value data."""

from __future__ import annotations

from pathlib import Path

from tw_signal_engine.market_data.market_data_records import LinearVolumeTracker
from tw_signal_engine.market_data.parse_format6_replay_rows import parse_trade_line

DAY_PER_MONTH = 20


def _find_history_files(market_type: str, date: str, data_dir: str = "./data/") -> list[tuple[str, int]]:
    """Find up to 21 replay files for the given market, ending at date.

    Returns list of (filepath, index) tuples where index 0 = target date.
    """
    data_path = Path(data_dir)
    prefix = f"{market_type}Quote"
    date_file_map: dict[str, str] = {}

    if not data_path.exists():
        return []

    for entry in data_path.iterdir():
        name = entry.name
        if name.startswith(prefix) and "." in name:
            file_date = name.split(".")[-1]
            if len(file_date) == 8:
                date_file_map[file_date] = str(entry)

    if not date_file_map:
        return []

    sorted_dates = sorted(date_file_map.keys())
    # Find dates <= target date
    valid_dates = [d for d in sorted_dates if d <= date]
    if not valid_dates:
        return []

    # Take up to 21 most recent
    selected = valid_dates[-21:]
    selected.reverse()  # newest first
    return [(date_file_map[d], i) for i, d in enumerate(selected)]


def _parse_vol_cum_from_file(
    filename: str,
    vol_tracker: LinearVolumeTracker,
    val_tracker: LinearVolumeTracker,
    trading_val: dict[str, int],
) -> None:
    """Read a replay file and populate cumulative volume/value trackers."""
    try:
        with open(filename, encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line or len(line) < 2 or line[0] != "T" or line[1] != "r":
                    continue
                tick = parse_trade_line(line, "", "")
                if tick is None or tick.status_code != 0:
                    continue
                vol_tracker.on_tick(tick.symbol, tick.match_time_us, tick.match.qty)
                tv = tick.match.price * tick.match.qty // 10
                val_tracker.on_tick(tick.symbol, tick.match_time_us, tv)
                trading_val[tick.symbol] = trading_val.get(tick.symbol, 0) + tick.match.qty * tick.match.price // 10
    except FileNotFoundError:
        pass


def load_history_window(
    market_type: str,
    date: str,
    data_dir: str = "./data/",
) -> tuple[list[LinearVolumeTracker], list[LinearVolumeTracker], list[dict[str, int]]]:
    """Load 21-day history window.

    Returns:
        vol_cum: list of 21 LinearVolumeTracker (cumulative volume)
        val_cum: list of 21 LinearVolumeTracker (cumulative trading value)
        trading_val: list of 21 dicts (total trading value per symbol)
    """
    files = _find_history_files(market_type, date, data_dir)

    vol_cum = [LinearVolumeTracker() for _ in range(DAY_PER_MONTH + 1)]
    val_cum = [LinearVolumeTracker() for _ in range(DAY_PER_MONTH + 1)]
    trading_val: list[dict[str, int]] = [{} for _ in range(DAY_PER_MONTH + 1)]

    for filepath, idx in files:
        if idx > DAY_PER_MONTH:
            continue
        _parse_vol_cum_from_file(filepath, vol_cum[idx], val_cum[idx], trading_val[idx])

    return vol_cum, val_cum, trading_val
