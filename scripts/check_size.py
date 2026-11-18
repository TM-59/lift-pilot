# Copyright 2026 TM-59
"""Local browser distribution budget; no third-party packages. Python 3.14."""

import sys
from pathlib import Path

LIMIT_BYTES = 10_000_000


class DistributionTooLargeError(SystemExit):
    """The files in dist/ add up to more than the budget."""

    def __init__(self) -> None:
        """Exit with the limit in the message."""
        super().__init__(f"Browser distribution exceeds {LIMIT_BYTES:,} B")


files = sorted(p for p in Path("dist").rglob("*") if p.is_file())
total = sum(p.stat().st_size for p in files)
for p in files:
    sys.stdout.write(f"{p}: {p.stat().st_size:,} B\n")
sys.stdout.write(f"Browser distribution: {total:,} B ({len(files)} files)\n")
if total > LIMIT_BYTES:
    raise DistributionTooLargeError
