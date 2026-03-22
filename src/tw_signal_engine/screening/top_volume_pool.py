"""Top-K volume tracker for strong single screening."""

from __future__ import annotations

import heapq


class TopKVolumeTracker:
    """Maintain a pool of top-K symbols by cumulative trading value."""

    def __init__(self, k: int = 200) -> None:
        self._k = k
        self._all_volumes: dict[str, int] = {}
        self._in_top: set[str] = set()
        # Min-heap of (volume, symbol) for the top-K
        self._heap: list[tuple[int, str]] = []
        self._min_threshold = 0

    def in_pool(self, symbol: str) -> bool:
        return symbol in self._in_top

    def on_tick(self, symbol: str, volume_delta: int) -> None:
        old_vol = self._all_volumes.get(symbol, 0)
        new_vol = old_vol + volume_delta
        self._all_volumes[symbol] = new_vol

        if symbol in self._in_top:
            # Already in pool, update (rebuild if needed)
            self._rebuild_if_needed()
            return

        if len(self._in_top) < self._k:
            self._in_top.add(symbol)
            heapq.heappush(self._heap, (new_vol, symbol))
            return

        # Check if can displace minimum
        if new_vol > self._min_threshold:
            self._rebuild_if_needed()

    def _rebuild_if_needed(self) -> None:
        """Rebuild top-K set from all volumes."""
        if not self._all_volumes:
            return
        # Sort all by volume descending, take top K
        sorted_items = sorted(self._all_volumes.items(), key=lambda x: x[1], reverse=True)
        top_k = sorted_items[: self._k]
        self._in_top = {s for s, _ in top_k}
        self._min_threshold = top_k[-1][1] if top_k else 0
        self._heap = [(v, s) for s, v in top_k]
        heapq.heapify(self._heap)
