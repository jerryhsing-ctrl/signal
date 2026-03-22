"""Trade ledger: process on_tick for exit logic."""

from __future__ import annotations

from tw_signal_engine.config.strategy_config import ExecutionConfig
from tw_signal_engine.execution.apply_bailout_exit import check_bailout
from tw_signal_engine.execution.apply_stop_loss_exit import check_stop_loss
from tw_signal_engine.execution.apply_take_profit_plan import check_take_profit
from tw_signal_engine.execution.apply_time_exit import check_time_exit
from tw_signal_engine.records.market_event_records import TradeRecord
from tw_signal_engine.state.position_state import PositionState
from tw_signal_engine.state.symbol_state import IndexData


def on_tick_exit(
    config: ExecutionConfig,
    symbol: str,
    price: int,
    bid_price: int,
    match_time_str: int,
    signal_type: str,
    entry_idx: IndexData,
    pos: PositionState,
    completed_trades: list[TradeRecord],
) -> str | None:
    """Process exit logic for one tick. Returns leave cause or None."""
    if pos.stocks.get(symbol, 0) == 0:
        return None

    def record_close(cause: str) -> None:
        ot = pos.open_trades.get(symbol)
        if ot is None:
            return
        tr = TradeRecord(
            symbol=ot.symbol,
            signal_type=ot.signal_type,
            enter_cause=ot.enter_cause,
            entry_time_raw=ot.entry_time_raw,
            exit_time_raw=match_time_str,
            pnl=pos.symbol_cash.get(symbol, 0) - ot.baseline,
            return_pct=(pos.symbol_cash.get(symbol, 0) - ot.baseline) / config.position_cash * 100.0,
            final_leave_cause=cause,
            had_take_profit=ot.had_take_profit,
            group_name=ot.group_name,
            group_rank=ot.group_rank,
            member_rank=ot.member_rank,
            raw_member_rank=ot.raw_member_rank,
            m1_symbol=ot.m1_symbol,
            entry_price=ot.entry_price,
            entry_vwap=ot.entry_vwap,
            day_high_at_entry=ot.day_high_at_entry,
            prev_close=ot.prev_close,
            vol_ratio=ot.vol_ratio,
            month_trading_val=ot.month_trading_val,
            is_prev_day_lu=ot.is_prev_day_lu,
            is_disposition=ot.is_disposition,
            had_circuit_breaker=ot.had_circuit_breaker,
            group_limit_up_count=ot.group_limit_up_count,
            market_entry_chg_pct=ot.market_entry_chg_pct,
        )
        completed_trades.append(tr)
        del pos.open_trades[symbol]

    # Stop loss
    if check_stop_loss(config, symbol, price, bid_price, signal_type, entry_idx, pos):
        record_close("stopLoss")
        return "stopLoss"

    # Time exit
    exited, cause = check_time_exit(config, symbol, price, bid_price, match_time_str, pos)
    if exited:
        record_close(cause)
        return cause

    # Take profit
    if check_take_profit(symbol, price, pos):
        if symbol in pos.open_trades:
            pos.open_trades[symbol].had_take_profit = True
        reserve = pos.reserve_stocks.get(symbol, 0)
        if pos.stocks.get(symbol, 0) <= 0.001 and reserve <= 0.001:
            pos.stocks[symbol] = 0
            record_close("takeProfit")
            return "takeProfit"

    # Bailout
    if check_bailout(config, symbol, price, bid_price, entry_idx, pos):
        record_close("bailout")
        return "bailout"

    return None
