# Legacy Python Signal Design Notes

This file preserves the fact that earlier Python strategy notes existed, but the detailed content no longer matches the committed implementation.

## Status

- historical reference only
- not a reliable source for current thresholds or state-machine details
- superseded by code-derived docs written against `src/tw_signal_engine/`

## Why It Was Archived

- it mixed prototype intent with implementation assumptions that no longer hold
- it described shapes that diverged from the committed `parameter.cfg`
- it was easy to confuse with the active Python runtime spec

## Use Instead

- architecture: [docs/design-docs/runtime-architecture.md](../../design-docs/runtime-architecture.md)
- current behavior: [docs/product-specs/current-strategy-spec.md](../../product-specs/current-strategy-spec.md)
