#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Retired firmware commands must reject requests before building or installing anything."""
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BuildTargetsTest(unittest.TestCase):
    def test_legacy_build_is_unavailable(self):
        self.assertFalse((ROOT / "tools/build.py").exists())
        for command in (["sh", "build.sh", "--legacy"],
                        [sys.executable, "tools/build_base.py", "--legacy"]):
            with self.subTest(command=command):
                result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("retired", result.stdout + result.stderr)

    def test_legacy_flash_option_is_unavailable(self):
        result = subprocess.run(["sh", "tools/flash.sh", "--legacy"], cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("unknown option", result.stdout + result.stderr)

    def test_old_package_and_release_commands_cannot_write(self):
        for tool in ("tools/fm1pkg_make.py", "tools/prepare_release.py"):
            with self.subTest(tool=tool):
                result = subprocess.run([sys.executable, tool], cwd=ROOT, text=True, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("retired", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
