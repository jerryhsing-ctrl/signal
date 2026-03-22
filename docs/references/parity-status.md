# Parity Status

## Current Status

Archived golden parity coverage exists for eight replay dates:

- `20260127`
- `20260128`
- `20260129`
- `20260130`
- `20260211`
- `20260223`
- `20260224`
- `20260225`

The parity fixtures live under:

- `artifacts/baseline/cpp/YYYYMMDD/`
- `artifacts/baseline/python/YYYYMMDD/`

The automated comparison path is:

- `tests/golden/compare_baseline.py`
- `tests/golden/test_replay_parity.py`

## What The Golden Test Compares

The current comparator validates:

- `Symbol`
- `SignalType`
- `EnterCause`
- `LeaveCause`
- `EntryTime`
- `ExitTime`
- `PnL` within a tolerance of `1`
- `GroupRank`
- `MemberRank`
- `RawMemberRank`

## Confirmed Replay Delta Fixes

- `Symbols_YYYYMMDD.csv` decoding now supports real dataset encodings such as `cp950`, avoiding startup failures on production-style symbol files.

## Parity-Sensitive Behaviors Still Preserved

Some implementation details are kept because the archived baselines depend on them:

- `state/GroupRank` preserves the old score-collision overwrite behavior
- `StrongSingleEvaluator._eval_price_cond()` keeps the effective C++ behavior where only the day-high increase path matters
- the merged replay flow uses the exact exit ordering `stop-loss -> time exit -> take-profit -> bailout`

These are descriptions of the current parity lock, not endorsements of the design.

## Scope Note

The golden tests verify archived replay parity, not future strategy intent. If the strategy config or runtime logic changes intentionally, this document and the golden baselines should be updated together.
