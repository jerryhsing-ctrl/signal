# Tech Debt Tracker

## Open Items

| Priority | Area | Issue | Follow-up |
| --- | --- | --- | --- |
| High | Replay orchestration | `src/tw_signal_engine/replay/replay_session.py` still mixes setup, loop control, closeout, and report generation. | Split into loader/setup, tick loop, and session finalization modules. |
| High | Replay universe | `build_replay_universe()` currently keys off strong-group validity plus `0050`, which can under-include future strong-single or broader strategy candidates. | Redesign universe construction before enabling more signal paths. |
| Medium | Parity artifact | `state/GroupRank` preserves score-collision overwrite behavior for C++ parity. | Make parity mode explicit, then remove the collision bug from default ranking. |
| Medium | Strategy artifact | `StrongSingleEvaluator._eval_price_cond()` keeps a dead parity-style branch where the amplitude condition is effectively unused. | Refactor when strong-single is re-enabled. |
| Medium | Input validation | Low-level replay iterators silently skip missing market files. | Decide whether daily replay should fail fast when TSE or OTC files are missing. |
| Low | Docs governance | The new docs test validates structure and links, not semantic drift against code/config. | Add targeted assertions for committed config values if the docs become more config-heavy. |
