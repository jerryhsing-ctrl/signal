"""Tests for symbol reference loading."""

from __future__ import annotations

from tw_signal_engine.reference_data.load_symbol_reference import load_symbol_reference


def test_load_symbol_reference_supports_utf8_sig(tmp_path) -> None:
    path = tmp_path / "Symbols_20260129.csv"
    path.write_text(
        "0050,元大台灣50,T,74.20,81.60,66.80,00,,0\n",
        encoding="utf-8-sig",
    )

    result = load_symbol_reference("20260129", str(tmp_path))

    assert result["0050"].name == "元大台灣50"
    assert result["0050"].previous_close == 74.20


def test_load_symbol_reference_supports_cp950(tmp_path) -> None:
    path = tmp_path / "Symbols_20260129.csv"
    path.write_bytes("0050,元大台灣50,T,74.20,81.60,66.80,00,,0\n".encode("cp950"))

    result = load_symbol_reference("20260129", str(tmp_path))

    assert result["0050"].name == "元大台灣50"
    assert result["0050"].limit_up_price == 81.60
