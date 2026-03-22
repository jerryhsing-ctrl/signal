"""Take-profit order execution."""

from __future__ import annotations

from tw_signal_engine.state.position_state import PositionState

PRICE_SCALE = 10000.0


def check_take_profit(
    symbol: str,
    price: int,
    pos: PositionState,
) -> bool:
    """Check and fill take-profit orders. Returns True if any filled."""
    orders = pos.orders.get(symbol, [])
    if not orders:
        return False

    ever_taken = False
    remaining: list[tuple[int, float]] = []
    for order_price, qty in orders:
        if price >= order_price:
            income = qty * order_price / PRICE_SCALE
            pos.cash += income
            pos.symbol_cash[symbol] = pos.symbol_cash.get(symbol, 0.0) + income
            pos.stocks[symbol] = pos.stocks.get(symbol, 0) - qty
            ever_taken = True
        else:
            remaining.append((order_price, qty))
    pos.orders[symbol] = remaining

    if pos.stocks.get(symbol, 0) < 0.001:
        pos.stocks[symbol] = 0

    if ever_taken:
        pos.profit_taken[symbol] = True

    return ever_taken
