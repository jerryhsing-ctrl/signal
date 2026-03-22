"""Position sizing: convert cash amount to share quantity."""

from __future__ import annotations

from tw_signal_engine.config.strategy_config import ExecutionConfig

PRICE_SCALE = 10000.0


def compute_entry_quantity(
    config: ExecutionConfig,
    ask_price: int,
    match_price: int,
    trades_entered_today: int,
) -> tuple[float, float]:
    """Compute quantity and effective position cash for an entry.

    Returns (quantity, effective_position_cash).
    """
    current_price = ask_price if ask_price > 0 else match_price
    current_price_actual = current_price / PRICE_SCALE

    effective_position = config.position_cash
    if trades_entered_today >= 1 and config.position_scale_nth != 1.0:
        effective_position = config.position_cash * config.position_scale_nth

    qty = effective_position / current_price_actual
    return qty, effective_position
