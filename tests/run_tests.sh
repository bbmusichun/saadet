#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Saadet host tests. Run ./build.sh and npm ci --prefix web first.
set -eu
cd "$(dirname "$0")/.."
OUT=build/host
LOG=$OUT/log
mkdir -p "$LOG" build/fm6_demo
CC=${CC:-cc}
compile() { "$CC" -O2 -w -Ibuild/gen -Ifirmware/src "tests/$1.c" -lm -pthread -o "$OUT/$1" >"$LOG/$1.compile.log" 2>&1 || { cat "$LOG/$1.compile.log"; exit 1; }; }
run() { name=$1; shift; echo "Checking $name"; "$@" >"$LOG/$name.log" 2>&1 || { cat "$LOG/$name.log"; exit 1; }; }
for name in synth_test fx_test midi_insert_test prophet_knob_test echoverb_test prophet_test prophet_fast_test fm6_test fm6_ams_test fm6_prepare_test cz_window_test sid_test sid_filter_test synth_fpu_test mod_route_test mod_spread_test control_prepare_test dual_core_test midi_uart_test input_test boot_stub_test storage_test power_test audio_boot_test usb_audio_test usb_audio_driver_test theme_test text_ref_test settings_test core1_stop_test core1_fallback_test libc_word_test ui_test; do
 compile "$name"
 if [ "$name" = fm6_test ]; then run "$name" "$OUT/$name" build/fm6_demo; else run "$name" "$OUT/$name"; fi
done
compile ota_test
run ota "$OUT/ota_test" build/saadet.fwsc
run usb-descriptors python3 tests/usb_audio_desc_test.py
run audio-ram python3 tests/audio_ram_test.py
run installer python3 tests/install_test.py
run recovery python3 tests/recovery_test.py
run boot-package python3 tests/boot_stub_pkg_test.py
run editor-protocol node web/test_synth.mjs
run editor-dom node web/test_dom.mjs
# Bounds checks are separate from sample/cost tests so instrumentation cannot skew their timings.
"$CC" -O1 -g -w -fsanitize=address -fno-omit-frame-pointer -Ibuild/gen -Ifirmware/src tests/synth_test.c -lm -o "$OUT/synth_asan"
run synth-address "$OUT/synth_asan"
printf '%s\n' 'ALL HOST TESTS PASSED'
