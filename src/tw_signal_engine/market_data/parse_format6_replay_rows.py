"""Parse Format6 replay rows (text-based Trade/Depth lines)."""

from __future__ import annotations

from tw_signal_engine.records.market_event_records import MarketTick, QuotePair


def _convert_raw_time_to_us(raw_time: int) -> int:
    """Convert HMMSS000000 format to microseconds since midnight."""
    micros = raw_time % 1_000_000
    remaining = raw_time // 1_000_000
    seconds = remaining % 100
    remaining //= 100
    minutes = remaining % 100
    hours = remaining // 100
    return (hours * 3600 + minutes * 60 + seconds) * 1_000_000 + micros


def _get_best_prices(depth_line: str) -> tuple[int, int]:
    """Extract best bid/ask prices from depth line."""
    bid_price = 0
    ask_price = 0

    bid_pos = depth_line.find("BID:")
    if bid_pos != -1:
        after_bid = depth_line[bid_pos + 4 :]
        try:
            count = int(after_bid.split(",")[0])
            if count > 0:
                comma_pos = depth_line.find(",", bid_pos)
                if comma_pos != -1:
                    price_str = depth_line[comma_pos + 1 :]
                    # Read until non-digit
                    digits = ""
                    for c in price_str:
                        if c.isdigit():
                            digits += c
                        else:
                            break
                    if digits:
                        bid_price = int(digits)
        except (ValueError, IndexError):
            pass

    ask_pos = depth_line.find("ASK:")
    if ask_pos != -1:
        after_ask = depth_line[ask_pos + 4 :]
        try:
            count = int(after_ask.split(",")[0])
            if count > 0:
                comma_pos = depth_line.find(",", ask_pos)
                if comma_pos != -1:
                    price_str = depth_line[comma_pos + 1 :]
                    digits = ""
                    for c in price_str:
                        if c.isdigit():
                            digits += c
                        else:
                            break
                    if digits:
                        ask_price = int(digits)
        except (ValueError, IndexError):
            pass

    return bid_price, ask_price


def parse_trade_line(trade_line: str, depth_line: str, market: str) -> MarketTick | None:
    """Parse a Trade line + optional Depth line into a MarketTick.

    Trade format: Trade,SYMBOL,MATCHTIME,STATUSCODE,PRICE,QTY,...
    """
    if len(trade_line) < 2 or trade_line[0] != "T" or trade_line[1] != "r":
        return None

    parts = trade_line.split(",")
    if len(parts) < 6:
        return None

    symbol = parts[1].strip()
    if not symbol:
        return None

    try:
        match_time_str = int(parts[2])
        status_code = int(parts[3])
        price = int(parts[4])
        qty = int(parts[5])
    except (ValueError, IndexError):
        return None

    tick = MarketTick(
        symbol=symbol,
        market=market,
        match_time_str=match_time_str,
        match_time_us=_convert_raw_time_to_us(match_time_str),
        status_code=status_code,
        trade_code=1,
        match=QuotePair(price=price, qty=qty),
    )

    # Parse depth if available
    if depth_line:
        bid_price, ask_price = _get_best_prices(depth_line)
        tick.bid[0].price = bid_price
        tick.ask[0].price = ask_price
        if price == bid_price:
            tick.trade_at = 1
        else:
            tick.trade_at = 2

    return tick
