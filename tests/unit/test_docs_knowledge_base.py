"""Documentation structure and local link checks."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

REQUIRED_DOCS = (
    "AGENTS.md",
    "ARCHITECTURE.md",
    "README.md",
    "docs/index.md",
    "docs/design-docs/index.md",
    "docs/product-specs/index.md",
    "docs/references/index.md",
    "docs/references/legacy/index.md",
    "docs/exec-plans/active/index.md",
    "docs/exec-plans/completed/index.md",
    "docs/exec-plans/tech-debt-tracker.md",
    "docs/generated/index.md",
)

MARKDOWN_LINK_RE = re.compile(r"\[[^\]]+\]\(([^)]+)\)")


def _iter_markdown_files() -> list[Path]:
    files = [
        ROOT / "AGENTS.md",
        ROOT / "ARCHITECTURE.md",
        ROOT / "README.md",
        ROOT / "CLAUDE.md",
    ]
    files.extend(sorted((ROOT / "docs").rglob("*.md")))
    return files


def _iter_local_link_targets(text: str) -> list[str]:
    targets: list[str] = []
    for target in MARKDOWN_LINK_RE.findall(text):
        if target.startswith(("http://", "https://", "mailto:", "#")):
            continue
        local_target = target.split("#", 1)[0].strip()
        if not local_target:
            continue
        targets.append(local_target)
    return targets


def test_required_docs_exist() -> None:
    missing = [path for path in REQUIRED_DOCS if not (ROOT / path).exists()]
    assert not missing, f"Missing required docs: {missing}"


def test_legacy_doc_roots_are_removed() -> None:
    for rel_path in ("docs/current", "docs/legacy", "docs/migration"):
        assert not (ROOT / rel_path).exists(), f"Legacy doc root should not exist anymore: {rel_path}"


def test_exec_plan_docs_are_partitioned() -> None:
    exec_plan_root = ROOT / "docs/exec-plans"
    allowed_root_files = {"tech-debt-tracker.md"}
    unexpected = sorted(
        path.relative_to(ROOT).as_posix()
        for path in exec_plan_root.glob("*.md")
        if path.name not in allowed_root_files
    )
    assert not unexpected, f"Plan docs must live in active/ or completed/: {unexpected}"


def test_local_markdown_links_resolve() -> None:
    broken: list[str] = []

    for path in _iter_markdown_files():
        text = path.read_text(encoding="utf-8")
        for target in _iter_local_link_targets(text):
            resolved = (path.parent / target).resolve()
            if not resolved.exists():
                broken.append(f"{path.relative_to(ROOT)} -> {target}")

    assert not broken, "Broken local markdown links:\n" + "\n".join(sorted(broken))
