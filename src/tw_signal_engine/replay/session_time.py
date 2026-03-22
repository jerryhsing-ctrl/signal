"""Time formatting utilities for replay session."""

from __future__ import annotations


def fmt_time(raw: int) -> str:
    """Format matchTimeStr to HH:MM:SS."""
    total = raw // 1_000_000
    hh = total // 10000
    mm = (total % 10000) // 100
    ss = total % 100
    return f"{hh:02d}:{mm:02d}:{ss:02d}"


def duration_sec(entry: int, exit_time: int) -> int:
    """Compute holding duration in seconds."""

    def to_sec(t: int) -> int:
        ts = t // 1_000_000
        return (ts // 10000) * 3600 + ((ts % 10000) // 100) * 60 + (ts % 100)

    return to_sec(exit_time) - to_sec(entry)


def fmt_duration(sec: int) -> str:
    """Format seconds to Xh00m00s."""
    h = sec // 3600
    m = (sec % 3600) // 60
    s = sec % 60
    return f"{h}h{m:02d}m{s:02d}s"
