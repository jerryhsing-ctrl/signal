"""Immutable reference data records."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(slots=True)
class ReferenceSymbol:
    """Static symbol reference data from Symbols_YYYYMMDD.csv."""

    symbol: str
    name: str
    market: str
    previous_close: float  # actual price (e.g. 50.00)
    limit_up_price: float
    limit_down_price: float
    industry: str
    security: str
    error_code: str


@dataclass(slots=True)
class GroupMembership:
    """One row of group.csv: group_name, symbol, name."""

    group_name: str
    symbol: str
    name: str
