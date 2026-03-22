"""Iterate one replay file and yield MarketTick events."""

from __future__ import annotations

from collections.abc import Iterator
from pathlib import Path

from tw_signal_engine.market_data.parse_format6_replay_rows import parse_trade_line
from tw_signal_engine.records.market_event_records import MarketTick


def iterate_market_file(
    market: str,
    date: str,
    data_dir: str = "./data/",
    tick_filter: set[str] | None = None,
) -> Iterator[MarketTick]:
    """Read a replay file line by line and yield valid MarketTick events.

    Handles the Trade/Depth line pairing logic from the C++ readFile.
    """
    filename = Path(data_dir) / f"{market}Quote.{date}"
    if not filename.exists():
        return

    with open(filename, encoding="utf-8", errors="replace") as f:
        stored_line: str | None = None

        while True:
            if stored_line is not None:
                trade_line = stored_line
                stored_line = None
            else:
                trade_line = f.readline()
                if not trade_line:
                    break
                trade_line = trade_line.rstrip("\n")

            if not trade_line or len(trade_line) < 2 or trade_line[0] != "T" or trade_line[1] != "r":
                continue

            # Quick symbol filter before full parse
            if tick_filter:
                parts = trade_line.split(",", 3)
                if len(parts) >= 2:
                    sym = parts[1].strip()
                    if sym not in tick_filter:
                        # Still need to read potential depth line
                        depth_line = f.readline()
                        if depth_line:
                            depth_line = depth_line.rstrip("\n")
                            if len(depth_line) >= 2 and depth_line[0] == "T" and depth_line[1] == "r":
                                stored_line = depth_line
                        continue

            # Try to read depth line
            depth_line = f.readline()
            if depth_line:
                depth_line = depth_line.rstrip("\n")
            else:
                depth_line = ""

            # If "depth" line is actually next trade, store it
            if len(depth_line) >= 2 and depth_line[0] == "T" and depth_line[1] == "r":
                stored_line = depth_line
                depth_line = ""

            # Verify trade/depth match (check same symbol+time region)
            if depth_line and len(trade_line) > 25 and len(depth_line) > 25:
                if trade_line[5:25] != depth_line[5:25]:
                    depth_line = ""

            tick = parse_trade_line(trade_line, depth_line, market)
            if tick is not None and tick.status_code == 0:
                yield tick
