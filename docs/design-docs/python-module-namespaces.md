# Python Module Namespaces

## Review Result

The Python package namespace is already in good shape for agentic navigation. Domain intent is explicit, directories are shallow, and there are no major `utils` or `helpers` dumping grounds in the active Python code. No code-path rename was required during this documentation refresh.

## Current Namespace Tree

```text
src/tw_signal_engine/
├── cli/
├── config/
├── execution/
├── market_data/
├── records/
├── reference_data/
├── replay/
├── reporting/
├── screening/
├── signals/
└── state/
```

### What each namespace owns

- `cli`: command-line entrypoints only
- `config`: legacy parameter parsing and normalized config objects
- `reference_data`: static symbol and group inputs
- `market_data`: replay-line parsing plus historical cache building
- `replay`: session orchestration and market-stream control
- `screening`: strong-group and strong-single qualification
- `signals`: signal-generation logic
- `execution`: entries, staged exits, and position accounting
- `reporting`: persisted CSV outputs
- `records`: immutable data contracts
- `state`: mutable intraday state

## Files That Still Carry Multiple Responsibilities

- `src/tw_signal_engine/replay/replay_session.py`
  - orchestrates config loading, history loading, evaluator setup, the event loop, forced closeout, and report generation
  - still acceptable for the current repo size, but it is the clearest future split target

No other Python file currently crosses a boundary badly enough to justify a rename or move during this pass.

## Documentation Namespace Refactor

The active documentation layout was the bigger problem. The old tree mixed current behavior, migration notes, legacy notes, and completed plans under flat or ambiguous paths.

### New authoritative docs tree

```text
docs/
├── design-docs/
│   ├── index.md
│   ├── runtime-architecture.md
│   └── python-module-namespaces.md
├── exec-plans/
│   ├── active/
│   │   └── index.md
│   ├── completed/
│   │   ├── index.md
│   │   ├── golden-parity-and-cutover.md
│   │   └── python-rewrite-reorganization.md
│   └── tech-debt-tracker.md
├── generated/
│   └── index.md
├── product-specs/
│   ├── index.md
│   └── current-strategy-spec.md
└── references/
    ├── index.md
    ├── parity-status.md
    ├── research-audit.md
    ├── runtime-conventions.md
    └── legacy/
        ├── index.md
        ├── logic-csharp-design.md
        ├── signal-python-design.md
        ├── stock-selection-notes.md
        └── stop-loss-notes.md
```

## Move And Rename Plan Executed

| Old path | New path | Reason |
| --- | --- | --- |
| `docs/current/strategy-runtime.md` | `docs/design-docs/runtime-architecture.md` | current runtime behavior belongs under design docs |
| `docs/current/parity-deltas.md` | `docs/references/parity-status.md` | parity is reference material, not a design doc |
| `docs/exec-plans/python-rewrite-reorganization-plan.md` | `docs/exec-plans/completed/python-rewrite-reorganization.md` | completed plan summary |
| `docs/exec-plans/golden-parity-and-cutover-plan.md` | `docs/exec-plans/completed/golden-parity-and-cutover.md` | completed plan summary |
| `docs/migration/research-audit.md` | `docs/references/research-audit.md` | historical migration audit |
| `docs/legacy/*` | `docs/references/legacy/*` | archival references should not look current |

## Naming Rules Going Forward

- Keep Python package paths domain-based.
- Keep `AGENTS.md` short and pointer-driven.
- Put active system behavior under `docs/design-docs/` or `docs/product-specs/`.
- Put history and parity material under `docs/references/`.
- Put plans only under `docs/exec-plans/active/` or `docs/exec-plans/completed/`.

## Validation

The repository now includes a docs structure test that checks:

- required indexes exist
- execution plans are partitioned into `active/` or `completed/`
- legacy `docs/current/`, `docs/legacy/`, and `docs/migration/` paths do not reappear
- local Markdown links resolve
