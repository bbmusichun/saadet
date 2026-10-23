#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Read the FM-1 battery ADC; --capture-full saves the median as its full reference."""
import argparse
import json
import statistics
import time
from datetime import datetime, timezone
from pathlib import Path

from prophet_device import Device

COMMAND = 81


def reading(device, arguments=()):
    reply = device.request(COMMAND, arguments)
    if len(reply) != 6 or reply[0] != 1:
        raise RuntimeError("Firmware does not support battery calibration schema 1")
    if reply[1]:
        raise RuntimeError(f"Battery request failed (result {reply[1]}); full reference not confirmed saved")
    raw = reply[2] | reply[3] << 7
    full = reply[4] | reply[5] << 7
    if not 1 <= raw <= 1023 or not 0 <= full <= 1023:
        raise RuntimeError("Invalid battery ADC reading")
    return raw, full


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="FM-1")
    parser.add_argument("--capture-full", action="store_true", help="battery is fully charged: save its measured reference")
    parser.add_argument("--output", type=Path, help="write the measurement receipt as JSON")
    args = parser.parse_args()
    device = Device(args.port)
    try:
        samples = []
        for _ in range(31):
            raw, full = reading(device)
            samples.append(raw)
            time.sleep(.1)
        median = int(statistics.median(samples))
        if args.capture_full:
            _, full = reading(device, [1, median & 127, median >> 7])
            _, verified = reading(device)
            if full != median or verified != median:
                raise RuntimeError("Full-charge reference did not read back correctly")
        result = dict(port=args.port, measured_at=datetime.now(timezone.utc).isoformat(),
                      samples=samples, minimum=min(samples), maximum=max(samples), median=median,
                      full_reference=full, saved=args.capture_full)
        text = json.dumps(result, indent=2) + "\n"
        if args.output:
            args.output.write_text(text)
        print(text, end="")
    finally:
        device.close()


if __name__ == "__main__":
    main()
