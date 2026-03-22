# Research Audit

This document records the migration audit outcome that led to the current Python-first repository shape.

## What The Audit Found

- old root-level markdown files mixed active behavior with aspirational or unrelated architectures
- historical notes described C#, FastAPI, Redis, live screening, and other systems that are not present in the Python repo
- legacy C++ files mixed replay orchestration, screening, and reporting responsibilities in large modules
- documentation paths such as `current/`, `legacy/`, and `migration/` were too easy to misread as equally authoritative

## What Changed

- Python became the primary interface under `src/tw_signal_engine/`
- C++ moved under `legacy/cpp/` as an archive
- active documentation moved to indexed `docs/design-docs/`, `docs/product-specs/`, and `docs/references/`
- completed plans moved under `docs/exec-plans/completed/`

## What This Audit Is Good For

- understanding why the repo favors explicit domain namespaces
- understanding why older design notes are now marked archival
- tracing the rationale for keeping a parity archive while moving active work to Python

For current behavior, do not use this file as the primary spec. Use [docs/product-specs/current-strategy-spec.md](../product-specs/current-strategy-spec.md) and [docs/design-docs/runtime-architecture.md](../design-docs/runtime-architecture.md).
