"""Parse legacy INI config files."""

from __future__ import annotations

import configparser
from pathlib import Path


def load_legacy_ini(path: str | Path) -> dict[str, dict[str, str]]:
    """Read a legacy INI file and return section -> key -> value dict.

    The legacy format uses `key=value` (no spaces around `=`).
    configparser handles this natively.
    """
    parser = configparser.RawConfigParser()
    parser.optionxform = str  # type: ignore[assignment]  # preserve case
    parser.read(str(path))

    result: dict[str, dict[str, str]] = {}
    for section in parser.sections():
        result[section] = dict(parser.items(section))
    return result
