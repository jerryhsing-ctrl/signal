"""Merge OTC and TSE replay streams deterministically by matchTimeStr."""

from __future__ import annotations

from collections.abc import Iterator

from tw_signal_engine.market_data.market_data_records import NumTracker
from tw_signal_engine.records.market_event_records import MarketTick
from tw_signal_engine.replay.iterate_market_file import iterate_market_file


def merge_market_streams(
    market_a: str,
    date_a: str,
    market_b: str,
    date_b: str,
    data_dir: str = "./data/",
    tick_filter: set[str] | None = None,
    prev_day_limit_up: dict[str, bool] | None = None,
    num_tracker: NumTracker | None = None,
) -> Iterator[MarketTick]:
    """Merge two market replay files in matchTimeStr order.

    Sets prev_limit_up and volatility_pause on each yielded tick.
    """
    iter_a = iterate_market_file(market_a, date_a, data_dir, tick_filter)
    iter_b = iterate_market_file(market_b, date_b, data_dir, tick_filter)

    pdlu = prev_day_limit_up or {}
    nt = num_tracker or NumTracker()

    tick_a: MarketTick | None = next(iter_a, None)
    tick_b: MarketTick | None = next(iter_b, None)

    last_price: dict[str, int] = {}

    while tick_a is not None or tick_b is not None:
        winner: MarketTick
        if tick_a is not None and tick_b is not None:
            if tick_a.match_time_str <= tick_b.match_time_str:
                winner = tick_a
                tick_a = next(iter_a, None)
            else:
                winner = tick_b
                tick_b = next(iter_b, None)
        elif tick_a is not None:
            winner = tick_a
            tick_a = next(iter_a, None)
        else:
            assert tick_b is not None
            winner = tick_b
            tick_b = next(iter_b, None)

        winner.prev_limit_up = pdlu.get(winner.symbol, False)
        trade_count = nt.on_tick(winner.symbol, winner.match_time_us)
        winner.volatility_pause = trade_count <= 3
        last_price[winner.symbol] = winner.match.price
        yield winner
