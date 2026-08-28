#!/usr/bin/env python3
"""Reject portable modules that import privileged or unstable firmware APIs."""

from __future__ import annotations

import re
import subprocess
import sys


ALLOWED_IMPORTS = {
    "__errno",
    "__getreent",
    "_ctype_",
    "abort",
    "atoi",
    "esp_log",
    "esp_log_timestamp",
    "exit",
    "fclose",
    "fflush",
    "fopen",
    "fprintf",
    "fread",
    "fseek",
    "ftell",
    "fwrite",
    "heap_caps_get_free_size",
    "heap_caps_get_largest_free_block",
    "memcmp",
    "memcpy",
    "memmove",
    "memset",
    "mkdir",
    "printf",
    "putchar",
    "puts",
    "remove",
    "rename",
    "snprintf",
    "sscanf",
    "strcasecmp",
    "strlen",
    "strcmp",
    "strncasecmp",
    "strncmp",
    "strncpy",
    "strrchr",
    "strstr",
    "vTaskDelay",
    "vfprintf",
    "vsnprintf",
}

# Keep this list identical to the public symbols exported by the firmware's
# ELF loader. A package that passes CI must not fail later at dlopen().
RUNTIME_EXPORTS = {
    *ALLOWED_IMPORTS,
    "strnlen",
}


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify-elf-imports.py <module.so>", file=sys.stderr)
        return 2
    result = subprocess.run(
        ["xtensa-esp32s3-elf-nm", "-D", "-u", sys.argv[1]],
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
    unavailable = sorted(imports - RUNTIME_EXPORTS)
    if unavailable:
        print("Imports missing from firmware runtime:", file=sys.stderr)
        for symbol in unavailable:
            print(f"  {symbol}", file=sys.stderr)
        return 1

    exports = subprocess.run(
        [
            "xtensa-esp32s3-elf-nm",
            "-D",
            "-g",
            "--defined-only",
            sys.argv[1],
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    if not re.search(r"\bidreeswatch_module$", exports.stdout, re.MULTILINE):
        print("Missing required exported descriptor: idreeswatch_module", file=sys.stderr)
        return 1

    print(
        f"Verified module descriptor and {len(imports)} allowed ELF imports"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
