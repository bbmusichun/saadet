# Saadet validation — 22 October 2026

Base: committed Melodee nightly `b5148ba528fcaa4bfda0dde8b6d96ced061ed459`.
The original Felucca checkout's pending FX changes were not included or modified.

The target build completed its RAM, audio placement, MMIO, boot/update and package
checks. `build/saadet.fwsc` is 610,542 bytes, identity `FM-1_900`;
the app is 368,824 bytes and loader 7,386 bytes.
Package SHA-256: `295be4da942012ddaaa11b1648978b764e273a023d9e024edbbc836235ce8e61`.

`tests/run_tests.sh` completed with **ALL HOST TESTS PASSED**. This includes:

- Four audible engines; all 57 native Prophet controls and sound-designer recipes.
- Serial/concurrent engine parity, worker corruption/fault retirement, allocation
  and lifecycle checks; 311,604 concurrent batch groups in the final worker probe.
- Insert/send FX, Echoverb, all 27 punch effects, MIDI repeat timing/ownership,
  sustain, channel filtering, strum and arp HOLD.
- State recall for all four engines, slot 384, write failures, previous-copy
  recovery and boot-loop protection; shared-library/archive object boundaries.
- USB PCM16/24 stereo routing, independent channel tones, drift, packet bounds,
  endpoint ownership, descriptors, volume/mute and direction/reconnect lifecycle.
- Power, boot, update installer and recovery checks; physical stock-V15 fixture
  checks were skipped where that vendor package was not present.
- 6,367 screen states in each font, zero layout findings and zero invalid MONO
  pixels; address-sanitized integration and screen rendering.
- Editor startup/tab actions in a DOM model, real firmware INFO parsing,
  384-slot addressing, archive round trips, preflight, corrupt/foreign rejection.
  Editor-only cleanup was rechecked after the full suite completed.

Logs and generated images are local ignored build artifacts. Reproduce with the
commands in BUILDING.md. Physical audio/control behavior, USB electrical timing,
flash installation and sustained device buffer deadlines remain unverified.
Browser visual testing was unavailable because browser access was disabled.
No firmware was installed and no repository or site was published.
