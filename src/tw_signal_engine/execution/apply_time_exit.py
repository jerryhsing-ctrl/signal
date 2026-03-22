"""Time-based exit and locked-limit-up handling."""

from __future__ import annotations

from tw_signal_engine.config.strategy_config import ExecutionConfig
from tw_signal_engine.state.position_state import PositionState

PRICE_SCALE = 10000.0


def check_time_exit(
    config: ExecutionConfig,
    symbol: str,
    price: int,
    bid_price: int,
    match_time_str: int,
    pos: PositionState,
) -> tuple[bool, str]:
    """Check time-based exit. Returns (exited, cause)."""
    if match_time_str < config.exit_time_limit:
        return False, ""

    reserve = pos.reserve_stocks.get(symbol, 0)
    limit_up = pos.limit_up_prices.get(symbol, 0)

    if reserve > 0.001 and limit_up > 0 and price >= limit_up:
        # Locked limit-up: sell reserve at limit-up price
        pos.orders[symbol] = []
        non_reserve = max(0.0, pos.stocks.get(symbol, 0) - reserve)
        if non_reserve > 0.001:
            market_price = (bid_price if bid_price > 0 else price) / PRICE_SCALE
            income = non_reserve * market_price
            pos.cash += income
            pos.symbol_cash[symbol] = pos.symbol_cash.get(symbol, 0.0) + income
        # Reserve at limit-up
        income = reserve * limit_up / PRICE_SCALE
        pos.cash += income
        pos.symbol_cash[symbol] = pos.symbol_cash.get(symbol, 0.0) + income
        pos.stocks[symbol] = 0
        pos.reserve_stocks[symbol] = 0
        pos.profit_taken[symbol] = False
        return True, "lockedLimitUp"
    else:
        # Normal time exit: close all at market
        pos.orders[symbol] = []
        market_price = (bid_price if bid_price > 0 else price) / PRICE_SCALE
        qty = pos.stocks.get(symbol, 0)
        income = qty * market_price
        pos.cash += income
        pos.symbol_cash[symbol] = pos.symbol_cash.get(symbol, 0.0) + income
        pos.stocks[symbol] = 0
        pos.reserve_stocks[symbol] = 0
        pos.profit_taken[symbol] = False
        return True, "timeExit"
