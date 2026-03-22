"""Taiwan stock exchange tick size rules."""

from __future__ import annotations

PRICE_SCALE = 10000

# Category 1: Stocks
RULE1 = [
    (1000.0, 5.0),
    (500.0, 1.0),
    (100.0, 0.5),
    (50.0, 0.1),
    (10.0, 0.05),
    (0.0, 0.01),
]

# Category 2: Warrants
RULE2 = [
    (500.0, 5.0),
    (150.0, 1.0),
    (50.0, 0.5),
    (10.0, 0.1),
    (5.0, 0.05),
    (0.0, 0.01),
]

# Category 3: Convertible bonds
RULE3 = [
    (1000.0, 5.0),
    (150.0, 1.0),
    (0.0, 0.05),
]

# Category 4: ETF
RULE4 = [
    (50.0, 0.05),
    (0.0, 0.01),
]

# Category 5: Bonds
RULE5 = [
    (0.0, 0.05),
]


def get_category(symbol: str) -> str:
    """Classify a symbol into category 1-7."""
    if not symbol or len(symbol) < 4:
        return "1"

    if symbol[0] == "F":
        return "6"

    try:
        code = int(symbol[:4])
    except ValueError:
        return "1"

    c5 = symbol[5] if len(symbol) > 5 else ""
    c6 = symbol[6] if len(symbol) > 6 else ""

    if code < 100:
        return "7"
    if 300 <= code <= 899 and c5.isdigit() and (c6.isdigit() or c6 in "PFQCBXY"):
        return "2"
    if 1000 <= code <= 9999 and c5.isdigit() and c5 != "0":
        return "3"
    if c5 == "G" and "D" <= c6 <= "L":
        return "3"
    if c5 == "F" and c6.isdigit() and c6 != "0":
        return "3"
    if c5 and "L" <= c5 <= "Z":
        return "3"
    if c5 == "0" and c6.isdigit() and c6 != "0":
        return "3"
    if 9100 <= code <= 9199:
        return "5"
    if code <= 199:
        return "4"
    if 300 <= code <= 899:
        return "2"

    return "1"


def _tick_lookup(rules: list[tuple[float, float]], price: float) -> float:
    for threshold, tick in rules:
        if price >= threshold:
            return tick
    return rules[-1][1]


def get_tick(symbol: str, price: float, is_up: bool) -> float:
    """Get tick size for a given symbol and price level."""
    cat = get_category(symbol)
    if not is_up:
        price -= 0.0001

    rules = {
        "1": RULE1,
        "2": RULE2,
        "3": RULE3,
        "4": RULE4,
        "5": RULE5,
    }.get(cat, RULE1)

    return _tick_lookup(rules, price)


def get_price_cond(symbol: str, price_raw: int, ticks: int) -> int:
    """Move price by N ticks. price_raw is int * 10000."""
    if not symbol or ticks == 0:
        return price_raw

    price = price_raw / PRICE_SCALE
    if ticks > 0:
        for _ in range(ticks):
            price += get_tick(symbol, price, True)
    else:
        for _ in range(abs(ticks)):
            price -= get_tick(symbol, price, False)

    return int(price * PRICE_SCALE + 0.01)
