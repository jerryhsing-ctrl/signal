# Legacy Stop-Loss Notes

This file preserves earlier exit-design intent but is not the current execution spec.

## Status

- archival only
- superseded by the actual Python execution modules under `src/tw_signal_engine/execution/`

## Main Drift From Current Code

- current exit ordering is `stop-loss -> time exit -> take-profit -> bailout`
- current time exit is `13:20`, not the older `13:25`
- current take-profit logic uses two `+3%` slices plus three reserved slices, not the old five-step linear grid
- current bailout threshold is `day_high_at_entry * 0.8`, which is far looser than the earlier notes

## Use Instead

- current strategy behavior: [docs/product-specs/current-strategy-spec.md](../../product-specs/current-strategy-spec.md)
- runtime architecture: [docs/design-docs/runtime-architecture.md](../../design-docs/runtime-architecture.md)
