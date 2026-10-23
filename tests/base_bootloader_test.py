#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Boot-only staging uses real updater framing; never opens hardware ports."""
from pathlib import Path
import shutil
import sys
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import base_bootloader as B

I = B.load_updater()
I.DELAY.update(start=0, reboot=0, reply=0, idle_check=.01, poll=.001)
IMAGE = I.logical_image(B.checked_files(B.ARTIFACTS)[B.PACKAGE])


class Link:
    def __init__(self, mode="accept"):
        self.mode, self.lost, self.closed = mode, False, False
        self.pending, self.reads, self.sent = [], 0, []

    def send(self, frame):
        self.sent.append(frame)
        if frame == I.UPGRADE:
            if self.mode == "refuse":
                return True
            self.pending = [(0, 512), (512, 512), (1024, 512), (I.FINISH_CHECK, 8)]
            if self.mode == "bounds":
                self.pending[0] = (len(IMAGE), 512)
            return True
        u = I.unpack7(frame[1:-1])
        addr, n = self.pending.pop(0)
        want = b"success\0" if addr == I.FINISH_CHECK else IMAGE[addr:addr+n]
        assert u[14:-1] == want, "Incorrect data served to bootloader installer"
        self.reads += 1
        if self.mode == "disconnect":
            self.pending.clear()
            self.lost = True
        return True

    def read(self, timeout):
        if not self.pending:
            return None
        addr, n = self.pending[0]
        data = bytearray([0, 0x59, 0x30, 8, 0, 0, 0])
        data += addr.to_bytes(4, "little") + n.to_bytes(2, "little") + b"\0"
        data.append(~sum(data[6:14]) & 255)
        return b"\xF0" + I.pack7(data) + b"\xF7"

    def close(self):
        self.closed = True


def device(link=None, product="FM-1_900", loader=False):
    return SimpleNamespace(link=link or Link(), id=SimpleNamespace(text=product, loader=loader, open=False))


class Updater(I.Updater):
    def __init__(self, back):
        self.back = back

    def wait_for(self, want, secs):
        return self.back


class BootloaderTest(unittest.TestCase):
    def run_install(self, mode="accept", back=None, dev=None):
        dev = dev or device(Link(mode))
        return B.install_bootloader(I, Updater(back or device()), dev, IMAGE, lambda *_: None)

    def test_boot_only_staging_and_original_app_return(self):
        dev = device()
        self.assertEqual(self.run_install(dev=dev), "FM-1_900")
        self.assertEqual(dev.link.reads, 4)
        self.assertTrue(dev.link.closed)
        self.assertEqual(dev.link.sent.count(I.UPGRADE), 1)

    def test_old_firmware_refusal(self):
        with self.assertRaisesRegex(ValueError, "staging did not complete"):
            self.run_install("refuse")

    def test_disconnection(self):
        with self.assertRaisesRegex(ValueError, "disconnected=True"):
            self.run_install("disconnect")

    def test_out_of_bounds_read(self):
        with self.assertRaises(I.InstallError) as caught:
            self.run_install("bounds")
        self.assertEqual(caught.exception.code, "badreq")

    def test_wrong_app_after_reboot(self):
        with self.assertRaises(I.InstallError) as caught:
            self.run_install(back=device(product="FM-1_015"))
        self.assertEqual(caught.exception.code, "mismatch")

    def test_app_never_returns(self):
        up = Updater(None)
        with self.assertRaises(I.InstallError) as caught:
            B.install_bootloader(I, up, device(), IMAGE, lambda *_: None)
        self.assertEqual(caught.exception.code, "noreturn")

    def test_wrong_starting_firmware_or_loader_never_upgrades(self):
        for dev in (device(product="FM-1_015"), device(loader=True)):
            with self.assertRaisesRegex(ValueError, "requires running Saadet"):
                self.run_install(dev=dev)
            self.assertFalse(dev.link.sent)

    def test_modified_package_or_boot_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)
            for name in (*B.HASHES, "LICENSE"):
                shutil.copyfile(B.ARTIFACTS / name, target / name)
            for name in B.HASHES:
                original = (target / name).read_bytes()
                (target / name).write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
                with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                    B.checked_files(target)
                (target / name).write_bytes(original)


if __name__ == "__main__":
    unittest.main()
