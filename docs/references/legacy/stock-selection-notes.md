# Legacy Stock Selection Notes

This file stands in for older handwritten stock-selection notes.

## Status

- archival only
- not synchronized with the current Python strong-group implementation
- several thresholds and filter combinations differ from the committed config

## Main Drift From Current Code

- current strong-group member conditions `cond1`, `cond2`, and `cond4` are disabled in the committed config
- the current engine requires raw member rank `1` and selects only one member per group
- the replay universe is driven by strong-group prevalidation, not by a separate handwritten selection notebook

## Use Instead

- current strategy behavior: [docs/product-specs/current-strategy-spec.md](../../product-specs/current-strategy-spec.md)
