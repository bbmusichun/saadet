# Saadet validation — 22 October 2026

Source: `ed2be851dede1da3d8618b294ee64c9a9e569111` plus a selective rollback
of the native 24-bit audio upgrade. The single-synth fork starts at Melodee
`b5148ba528fcaa4bfda0dde8b6d96ced061ed459`. The original Felucca checkout was
not modified.

## Audio and effects

The common signal path is restored to its original Q15 scale. Prophet, FM6
and CZ-1 retain their fp32 cores and original output conversion; SID retains
its chip-scale oscillator/filter model and original amplitude path. Mixer gain,
pan, DC removal, limiter, integer master saturation and codec output scaling
are restored to the pre-upgrade implementation. PCM16 effect histories and
USB rings replace the wider buffers and packed PCM24 histories. Echoverb again
uses one contiguous allocation per slot.

USB still offers PCM16 and PCM24 at 44.1 kHz. PCM24 playback discards its low
byte, and capture pads that byte with zero. Selecting a 24-bit USB container
does not increase internal synthesis precision. The native-precision dither
and floating-point cubic master saturation added by the upgrade are removed.

One serial chain still contains three inserts. Punch DSP and send buses remain
removed. Old stored send parameter numbers remain reserved and inert so
existing state/archive layouts retain their identity. Engine scheduling,
voice admission, worker fallback, sound persistence and the later interface
changes are outside this rollback.

## Interface

The on-device interface remains removed ahead of a new design. The screen
stays blank; the keys play, MASTER sets the volume and the other controls do
nothing. Startup maintenance, the update countdown, UPDATE and the crash report
still draw, and the maintenance screen's OCT- / OCT+ answers work. SysEx dumps
sent to the device are refused until an interface can choose their slots.

## Package

The target build completed RAM, audio placement, MMIO, boot/update and package
checks. `build/saadet.fwsc` is 610,542 bytes, identity `FM-1_900`;
the app is 237,504 bytes and loader 7,386 bytes.
Package SHA-256: `e5d2b42582cdc33a70c92d29d0074d7335d79385484725a8760266e150625279`.

The main audio arena has 194,112 bytes; the low DSP tail has 8,724 bytes,
the data tail 57,352 bytes and the retained tail 15,440 bytes. Guaranteed
SRAM banks total 275,628 bytes; the optional cache bank adds 28,672 bytes after
its boot self-test. The original linker and audio RAM checks apply.

## Checks

`tests/run_tests.sh` completed with **ALL HOST TESTS PASSED**, including engines,
insert effects, worker parity/fallback, MIDI, persistence, USB formats/lifecycle,
blank-panel behavior, package/recovery, editor protocol/DOM and address checks.
All DSP fixtures now use the same default Q15 path as the shipping build.

Eight dry host renders (two factory programs each for Prophet, FM6, CZ-1 and
SID, a four-note chord over two seconds) are byte-identical to `456cd1f`.
Every render contains nonzero audio. The two host simulators used the current
generated synth tables, `PRESET=1`, `CHORD=4`, `SECS=2` and polyphonic mode.
This checks restoration of the original synthesis and master output while
leaving sends and punch removed.

Logs are local ignored build artifacts: `build/q15-revert-build.log`,
`build/q15-revert-tests.log` and `build/q15-render-comparison.json`.
Reproduce the build and host suite with BUILDING.md.

Host and package checks do not establish physical noise floor, audible quality
or sustained on-device audio deadlines. CPU reduction has not been measured on
the device. No repository or site was published.

An earlier native-precision package (SHA-256 `218634fc…557e`, before the later
interface changes) was installed on an FM-1 with the user's authorization.
The rollback package above has not been installed.
