#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""tools/licence_check.py: the tree passes, and a stray GPL file, a missing SPDX line or a stale exception fail."""
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import licence_check as L  # noqa: E402


class LicenceCheckTest(unittest.TestCase):
    def test_tree_passes(self):
        files = L.compiled()
        self.assertIn("firmware/src/midi_input.c", files)
        self.assertIn("platform/instrument.c", files)
        bad, known = L.problems(files)
        self.assertEqual(bad, [])
        self.assertEqual(known, [])
        self.assertEqual(L.KNOWN_GPL, {})
        for name in ("editor_preferences.c", "favorites.c", "settings_persist.c", "scratch.h"):
            path = "firmware/src/" + name
            self.assertIn(path, files)
            self.assertEqual(L.spdx(ROOT / path), "MPL-2.0")

    def test_failures(self):
        lic = {"a.c": "MPL-2.0", "b.c": "GPL-3.0-only", "c.c": None, "d.c": "MIT"}
        look = lambda p: lic[str(Path(p).relative_to(ROOT))]  # noqa: E731
        bad, known = L.problems(set(lic), look)
        self.assertEqual(len(bad), 2)
        self.assertTrue(any(b.startswith("b.c: GPL-3.0-only") for b in bad))
        self.assertTrue(any(b.startswith("c.c: no SPDX") for b in bad))
        self.assertEqual(known, [])
        with patch.dict(L.KNOWN_GPL, {"former.c": "test exception"}):
            lic["former.c"] = "GPL-3.0-only"
            bad, _ = L.problems(set(lic) - {"former.c"}, look)
            self.assertTrue(any(b.startswith("former.c:") and "no longer compiled" in b for b in bad))
            lic["former.c"] = "MPL-2.0"
            bad, _ = L.problems(set(lic), look)
            self.assertTrue(any(b.startswith("former.c:") and "now MPL-2.0" in b for b in bad))

    def test_mixed_third_party_licence(self):
        self.assertEqual(L.spdx(ROOT / "firmware/src/fm6_core.c"), "MPL-2.0 AND Apache-2.0")

    def test_manifest_agrees_with_source_licences(self):
        self.assertEqual(L.manifest_problems(), [])
        with patch.object(L.tomllib, "load", return_value={"licence": "GPL-3.0-only"}):
            self.assertEqual(len(L.manifest_problems()), 1)


if __name__ == "__main__":
    unittest.main()
