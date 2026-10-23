#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Recovery archive platform names; the legacy firmware release builder is retired."""

TARGETS = ("windows-x64", "macos-arm64", "macos-x64", "linux-x64")

if __name__ == "__main__":
    raise SystemExit("Legacy firmware releases are retired. Use ./build.sh --release and tools/make_site.py.")
