"""Build and load volume caches (placeholder for binary cache optimization)."""

from __future__ import annotations

# The C++ version has a binary VOLCACHE format for speed.
# For correctness-first Python port, we always parse from text files.
# Cache optimization can be added later if needed.
