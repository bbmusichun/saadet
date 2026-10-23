#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Rebuild assets/j106-factory/j106-factory.syx: Roland's 128 Juno-106 factory patches as patch messages.

  tools/make_j106_factory.py JUNO-106.zip junot020.wav Factory_Patches.pat [OUT.syx]

Three sources (see assets/j106-factory/README.md), checked against each other:
  JUNO-106.zip         sunshine-jones.com "Roland Factory Data Cassettes": the factory cassette's bank A and
                       bank B as recorded audio (JUNO106 Bank A.wav, JUNO106 Bank B.wav).
  junot020.wav         hinzen.de JUNO-106 CONNECTION: a second recording of the cassette's bank A.
  Factory_Patches.pat  hinzen.de: the 128 patches as patch-message bytes (18 hex bytes and a name a line).
The recordings are decoded here: every half cycle is a bit (long 1, short 0) after a leader of 1s; bytes go
start bit 0, eight bits LSB first, stop bit 1. A bank is 4A 55, 64 records of 16 bytes and the 8-bit sum of
everything before it. A record's low seven bits are the sixteen sliders; bit 7 of record bytes 0..7 and
8..15 are SW1 and SW2, bit 0 first. A patch message has no room for SW1 bit 7, which some records set.
Both bank A recordings agree record for record but at A74 and A88, where the Sunshine Jones bank holds a
copy of its A82 and another tone: there the hinzen.de recording is used. Every patch must then equal
Factory_Patches.pat.
"""
import hashlib
import io
import sys
import wave
import zipfile
from pathlib import Path

SHA256 = {"JUNO-106.zip": "386261d8614f13f78cca9effa9e4f303fe00ab45b8961d1b6396a5203026a17f",
          "junot020.wav": "0c5d2e93dc98a88ebc66920aa8b1ff805aefeb17771219b9e7b4b06f6b8b8bc3",
          "Factory_Patches.pat": "d9579f1cbb682022de3b7dd9582eeede14b38db64766808f89ecc7578959ecd1"}
LONG = 380e-6                      # a half cycle at least this long is a 1 (the tones' halves: ~180 and ~600 us)
EDITED = (51, 63)                  # A74, A88: the Sunshine Jones bank A's own tones (A74 a copy of its A82)


def source(path):
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA256.get(Path(path).name):
        raise SystemExit(f"{path}: not the documented {Path(path).name} (SHA-256 differs)")
    return data


def bits(data):
    """an 8-bit mono recording -> its half cycles as bits"""
    w = wave.open(io.BytesIO(data))
    if w.getsampwidth() != 1 or w.getnchannels() != 1:
        raise SystemExit("cassette audio: want 8-bit mono")
    rate, s = w.getframerate(), w.readframes(w.getnframes())
    mean = sum(s) / len(s)
    s = [x - mean for x in s]
    out, state, last = [], 1 if s[0] >= 0 else -1, None
    for i in range(1, len(s)):
        if state > 0 and s[i] < -6 or state < 0 and s[i] > 6:      # a zero crossing past a small hysteresis
            j = i
            while j > 0 and (s[j - 1] < 0) == (s[i] < 0):
                j -= 1
            a, b = s[j - 1], s[j]
            t = (j - 1 + (a / (a - b) if a != b else 0.5)) / rate
            if last is not None:
                out.append(1 if t - last >= LONG else 0)
            last, state = t, -state
    return out


def bank(data, what):
    """a recorded bank -> its 64 records of 16 bytes"""
    b = bits(data)
    i = 0
    while i < len(b) and b[i]:
        i += 1
    if i < 200:
        raise SystemExit(f"{what}: no leader")
    out = []
    while len(out) < 2 + 64 * 16 + 1:
        f = b[i:i + 10]
        if len(f) < 10 or f[0] or not f[9]:
            raise SystemExit(f"{what}: framing error at byte {len(out)}")
        out.append(sum(f[1 + k] << k for k in range(8)))
        i += 10
    if out[:2] != [0x4A, 0x55] or sum(out[:-1]) & 0xFF != out[-1]:
        raise SystemExit(f"{what}: bad header or checksum")
    return [bytes(out[2 + 16 * k:18 + 16 * k]) for k in range(64)]


def patch(r):
    """a cassette record -> the patch message's sixteen sliders, SW1 and SW2"""
    sw1 = sum((r[i] >> 7) << i for i in range(8)) & 0x7F
    sw2 = sum((r[8 + i] >> 7) << i for i in range(8))
    if sw2 > 0x7F:
        raise SystemExit("cassette record: SW2 bit 7 set")
    return bytes(x & 0x7F for x in r) + bytes([sw1, sw2])


def label(k):
    return f"{'AB'[k // 64]}{k // 8 % 8 + 1}{k % 8 + 1}"


def main(sj, hz, pat, out):
    with zipfile.ZipFile(io.BytesIO(source(sj))) as z:
        a, b = (bank(z.read(f"JUNO106 Bank {x}.wav"), f"JUNO106 Bank {x}.wav") for x in "AB")
    a2 = bank(source(hz), "junot020.wav")
    differ = tuple(k for k in range(64) if a[k] != a2[k])
    if differ != EDITED or a[EDITED[0]] != a[57]:
        raise SystemExit(f"bank A recordings differ at {[label(k) for k in differ]}, not as documented")
    records = [a2[k] for k in range(64)] + b
    rows = [line.split(",", 1)[0].split() for line in source(pat).decode("latin-1").splitlines() if line.strip()]
    if len(rows) != 128:
        raise SystemExit(f"{pat}: {len(rows)} patches, want 128")
    patches = [patch(r) for r in records]
    for k, (p, row) in enumerate(zip(patches, rows)):
        if p != bytes(int(x, 16) for x in row):
            raise SystemExit(f"{label(k)}: the cassette and {Path(pat).name} differ")
    hidden = sum(r[7] >> 7 for r in records)
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    Path(out).write_bytes(b"".join(bytes([0xF0, 0x41, 0x30, 0, k]) + p + b"\xf7" for k, p in enumerate(patches)))
    print(f"j106 factory: 128 patches ({hidden} with SW1 bit 7 on the cassette, not in a patch message) -> {out}")


if __name__ == "__main__":
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    main(*sys.argv[1:4], sys.argv[4] if len(sys.argv) == 5 else
         Path(__file__).resolve().parent.parent / "assets/j106-factory/j106-factory.syx")
