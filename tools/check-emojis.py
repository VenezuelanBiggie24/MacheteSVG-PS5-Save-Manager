#!/usr/bin/env python3
# MacheteSVG - Zero Emoji Verification Script
# Copyright (C) 2026 MacheteSVG Team
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Validates that no code or text file contains emoji Unicode characters.

import os
import subprocess
import sys

BINARY_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".ico", ".bin", ".ogg", ".wav",
    ".elf", ".a", ".so", ".dylib", ".o", ".zip", ".tar", ".gz",
    ".bmp", ".webp", ".ttf", ".otf", ".dds", ".at9", ".sfo",
    ".keystone", ".sealedkey"
}

IGNORED_DIRS = {".git", ".deps", "build", "dist"}

def is_emoji_codepoint(cp: int) -> bool:
    return (
        (0x1F600 <= cp <= 0x1F64F) or  # Emoticons
        (0x1F300 <= cp <= 0x1F5FF) or  # Misc Symbols and Pictographs
        (0x1F680 <= cp <= 0x1F6FF) or  # Transport and Map
        (0x1F1E0 <= cp <= 0x1F1FF) or  # Regional Indicators (Flags)
        (0x1F900 <= cp <= 0x1F9FF) or  # Supplemental Symbols and Pictographs
        (0x1FA70 <= cp <= 0x1FAFF) or  # Symbols and Pictographs Extended-A
        (0x2600 <= cp <= 0x26FF) or    # Misc symbols
        (0x2700 <= cp <= 0x27BF)       # Dingbats
    )

def collect_files():
    try:
        tracked = subprocess.check_output(["git", "ls-files"], text=True).splitlines()
        return [f for f in tracked if os.path.isfile(f)]
    except Exception:
        files = []
        for root, dirs, fnames in os.walk("."):
            dirs[:] = [d for d in dirs if d not in IGNORED_DIRS]
            for fn in fnames:
                files.append(os.path.join(root, fn))
        return files

def main():
    files = collect_files()
    violations = []
    scanned_count = 0

    for file_path in files:
        _, ext = os.path.splitext(file_path)
        if ext.lower() in BINARY_EXTENSIONS:
            continue
        if any(part in IGNORED_DIRS for part in file_path.split(os.sep)):
            continue

        scanned_count += 1
        try:
            with open(file_path, "r", encoding="utf-8", errors="replace") as fh:
                for line_idx, line in enumerate(fh, 1):
                    for char in line:
                        cp = ord(char)
                        if is_emoji_codepoint(cp):
                            violations.append({
                                "file": file_path,
                                "line": line_idx,
                                "char": char,
                                "codepoint": f"U+{cp:04X}",
                                "text": line.strip()[:80]
                            })
                            break
        except Exception as e:
            print(f"[WARN] Error reading {file_path}: {e}", file=sys.stderr)

    print(f"==================================================")
    print(f" MacheteSVG - Zero Emoji Verification Report")
    print(f"==================================================")
    print(f" Total text files scanned: {scanned_count}")
    print(f" Total emoji violations:   {len(violations)}")
    print(f"--------------------------------------------------")

    if violations:
        print("[FAIL] The following emoji violations were detected:")
        for v in violations:
            print(f"  {v['file']}:{v['line']} - Char '{v['char']}' ({v['codepoint']}): {v['text']}")
        print(f"==================================================")
        return 1

    print("[PASS] 100% Zero-Emoji certification passed cleanly.")
    print(f"==================================================")
    return 0

if __name__ == "__main__":
    sys.exit(main())
