# Python Rewrite And Repo Reorganization

## Outcome

Completed. The active replay/backtest path now lives in the Python package `src/tw_signal_engine/`, and the old C++ implementation is archived under `legacy/cpp/`.

## Delivered

- explicit domain-based Python package namespaces
- daily and batch replay CLIs
- historical cache loading, screening, signal, execution, and reporting modules
- unit tests plus archived golden parity coverage
- indexed documentation that treats Python as the current source of truth

## Not Carried Forward

- live UDP ingestion as an active concern
- thread pinning and scheduler behavior from the old C++ runtime
- unrelated live-system or web-serving designs from earlier notes

## Follow-Ups

Open design and implementation debt now lives in [docs/exec-plans/tech-debt-tracker.md](../tech-debt-tracker.md).
