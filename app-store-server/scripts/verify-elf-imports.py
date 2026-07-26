#!/usr/bin/env python3
"""Reject portable modules that import privileged or unstable firmware APIs."""

from __future__ import annotations

import re
import subprocess
import sys


ALLOWED_IMPORTS = {
    "abort",
    "memcmp",
    "memcpy",
    "memmove",
    "memset",
    "strlen",
    "strcmp",
    "strncmp",
    "strstr",
    "vsnprintf",
}


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify-elf-imports.py <module.so>", file=sys.stderr)
        return 2
    result = subprocess.run(
        ["xtensa-esp32s3-elf-nm", "-u", sys.argv[1]],
        check=True,
        capture_output=True,
        text=True,
    )
    imports: set[str] = set()
    for line in result.stdout.splitlines():
        match = re.search(r"\bU\s+([^\s@]+)", line)
        if match:
            imports.add(match.group(1))
    rejected = sorted(imports - ALLOWED_IMPORTS)
    if rejected:
        print("Rejected ELF imports:", file=sys.stderr)
        for symbol in rejected:
            print(f"  {symbol}", file=sys.stderr)
        return 1
    print(f"Verified {len(imports)} allowed ELF imports")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
