#!/usr/bin/env python3
"""Scan the repository for unresolved Git merge conflict markers.

Run this utility after merging or rebasing to make sure no ``<<<<<<<`` markers
remain in the working tree. The script ignores the ``.git`` directory and exits
with a non-zero status code if markers are found so it can be used in CI jobs or
local hooks.
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Iterable, Iterator, Set, Tuple

CONFLICT_MARKERS = ("<<<<<<<", "=======", ">>>>>>>")
SKIP_DIRS = {".git", "__pycache__"}
SKIP_SUFFIXES = {".pyc", ".pyo", ".swp", ".swo"}


def iter_candidate_files(root: Path, extensions: Set[str]) -> Iterator[Path]:
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if name not in SKIP_DIRS]
        for filename in filenames:
            path = Path(dirpath) / filename
            if path.suffix.lower() in SKIP_SUFFIXES:
                continue
            if extensions and path.suffix.lower() not in extensions:
                continue
            yield path


def scan_file(path: Path) -> Iterator[Tuple[int, str]]:
    try:
        with path.open("r", encoding="utf-8", errors="ignore") as handle:
            for lineno, line in enumerate(handle, start=1):
                for marker in CONFLICT_MARKERS:
                    if line.lstrip().startswith(marker):
                        yield lineno, line.rstrip()
                        break
    except OSError as exc:
        print(f"warning: skipped {path}: {exc}", file=sys.stderr)


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path.cwd(),
        help="Repository root to scan (defaults to current working directory)",
    )
    parser.add_argument(
        "--extensions",
        nargs="*",
        default=[],
        metavar=".EXT",
        help="Optional whitelist of file extensions to scan (e.g. .cpp .h)",
    )
    return parser.parse_args(list(argv))


def main(argv: Iterable[str]) -> int:
    args = parse_args(argv)
    root = args.root.resolve()
    if not root.exists():
        print(f"error: {root} does not exist", file=sys.stderr)
        return 2

    markers_found = False
    extensions = {ext.lower() for ext in args.extensions}

    for file_path in iter_candidate_files(root, extensions):
        for lineno, line in scan_file(file_path):
            if not markers_found:
                print("Detected potential merge conflicts:\n")
            markers_found = True
            print(f"{file_path.relative_to(root)}:{lineno}: {line}")

    if markers_found:
        print("\nResolve the conflicts above before continuing.", file=sys.stderr)
        return 1

    print("No merge conflict markers detected.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
