# Golden Parity And Cutover

## Outcome

Completed. Archived parity artifacts and tests exist for eight replay dates, and the legacy C++ implementation has been moved under `legacy/cpp/`.

## Delivered

- archived C++ and Python baselines under `artifacts/baseline/`
- golden comparison helpers in `tests/golden/`
- parity test coverage for:
  - `20260127`
  - `20260128`
  - `20260129`
  - `20260130`
  - `20260211`
  - `20260223`
  - `20260224`
  - `20260225`
- refreshed runtime documentation tied to the Python codebase

## Notes

- the parity suite validates archived replay behavior, not future strategy intent
- several parity-sensitive quirks remain intentionally preserved; see [docs/references/parity-status.md](../../references/parity-status.md)
