"""Build the replay universe as the union of group/single candidates + proxies."""

from __future__ import annotations


def build_replay_universe(
    group_valid_symbols: set[str],
    single_valid_symbols: set[str] | None = None,
    required_proxies: set[str] | None = None,
) -> set[str]:
    """Build tick filter for replay.

    Union of:
    - strong-group candidate symbols
    - strong-single candidate symbols
    - required market proxies (e.g. 0050)
    """
    universe = set(group_valid_symbols)
    if single_valid_symbols:
        universe |= single_valid_symbols
    if required_proxies:
        universe |= required_proxies
    universe.add("0050")  # always needed for market gating
    return universe
