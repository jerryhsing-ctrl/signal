"""Rolling window data structures: RollingLow and RollingSum."""

from __future__ import annotations

from collections import deque


class RollingLow:
    """Monotonic deque-based sliding window minimum."""

    __slots__ = ("_window", "_duration")

    def __init__(self, duration: float = 0.0) -> None:
        self._window: deque[tuple[int, int]] = deque()  # (time, value)
        self._duration = duration

    def set_duration(self, d: float) -> None:
        self._duration = d

    def update(self, current_time: int, value: int) -> None:
        # Remove expired
        while self._window and (current_time - self._window[0][0] > self._duration):
            self._window.popleft()
        # Maintain monotonicity
        while self._window and self._window[-1][1] >= value:
            self._window.pop()
        self._window.append((current_time, value))

    def get_low(self) -> int:
        if not self._window:
            return 0
        return self._window[0][1]

    def empty(self) -> bool:
        return len(self._window) == 0


class RollingSum:
    """Sliding window sum tracker."""

    __slots__ = ("_history", "_duration", "_current_sum")

    def __init__(self, duration: float = 0.0) -> None:
        self._history: deque[tuple[int, int]] = deque()
        self._duration = duration
        self._current_sum: int = 0

    def set_duration(self, d: float) -> None:
        self._duration = d

    def update(self, current_time: int, value: int) -> None:
        self._history.append((current_time, value))
        self._current_sum += value
        while self._history and (current_time - self._history[0][0] > self._duration):
            self._current_sum -= self._history[0][1]
            self._history.popleft()
        if not self._history:
            self._current_sum = 0

    def get_sum(self) -> int:
        return self._current_sum
