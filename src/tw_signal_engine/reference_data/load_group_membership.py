"""Load group membership from group.csv."""

from __future__ import annotations

import csv
from pathlib import Path

from tw_signal_engine.records.reference_records import GroupMembership


def load_group_membership(
    path: str | Path = "./files/group.csv",
) -> tuple[list[GroupMembership], dict[str, list[str]], dict[str, set[str]]]:
    """Parse group.csv.

    Returns:
        memberships: flat list of GroupMembership records
        symbol_to_groups: symbol -> [group_name, ...]
        group_members: group_name -> {symbol, ...}
    """
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"Group file not found: {path}")

    memberships: list[GroupMembership] = []
    symbol_to_groups: dict[str, list[str]] = {}
    group_members: dict[str, set[str]] = {}

    with open(path, encoding="utf-8-sig") as f:
        reader = csv.reader(f)
        for row in reader:
            if len(row) < 3:
                continue
            group_name = row[0].strip()
            symbol = row[1].strip()
            name = row[2].strip()
            if not group_name or not symbol:
                continue

            gm = GroupMembership(group_name=group_name, symbol=symbol, name=name)
            memberships.append(gm)

            if symbol not in symbol_to_groups:
                symbol_to_groups[symbol] = []
            symbol_to_groups[symbol].append(group_name)

            if group_name not in group_members:
                group_members[group_name] = set()
            group_members[group_name].add(symbol)

    return memberships, symbol_to_groups, group_members
