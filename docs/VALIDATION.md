# Saadet validation — 22 October 2026

Source: `9f6e8ad` plus the restored native 24-bit audio path and existing local
interface work. The precision rollback `e51e199` is reversed selectively;
newer committed and in-progress interface changes are retained.
The original Felucca checkout was not modified.

## Audio and effects

The default signal path is Q23: signed 24-bit audio in 32-bit containers, with
wide gain/filter products and DC histories. Prophet, FM6 and CZ-1 retain their
fp32 cores and quantize their output at higher precision. SID retains its
chip-scale model and gains the higher-precision amplitude and output paths.
The master saturation curve uses fp32 knots and cubic interpolation.

USB PCM24 preserves the low byte in both directions. PCM16 capture applies
TPDF dither at final reduction; exact silence stays silent. USB format
selection does not change synthesis precision or the 44.1 kHz rate.
Delay histories use packed PCM24 with unchanged lengths. Echoverb's diffusion
state and history use separate fixed arena handles. Precision helpers remain
always inlined, limiter release retains the original Q15 time scale and ROOM
runs in POOL SRAM to preserve the low DSP arena tail.

One serial chain still contains three inserts. Punch DSP and send buses remain
removed. Reserved send parameter numbers remain inert for state compatibility.
Engine scheduling, voice admission, worker fallback and sound persistence are
unchanged by the precision restoration.

## Interface

The newer Stage theme and existing local interface work are retained. This
audio restoration does not edit the interface sources or UI tests.

## Package

The target build completed RAM, audio placement, MMIO, boot/update and package
checks. `build/saadet.fwsc` is 610,542 bytes, identity `FM-1_900`;
the app is 267,872 bytes and loader 7,386 bytes.
Package SHA-256: `ee325dd83dfc17b5cee4fe00b8dd2547f13603d4c51021ac94e31255db60c092`.

The main audio arena has 184,128 bytes against its enforced 172,032-byte
minimum. Low, data and retained SRAM tails have 8,948, 55,784 and 15,440 bytes
respectively. Guaranteed SRAM banks total 264,300 bytes; the optional cache bank
adds 28,672 bytes after its boot self-test.

## Checks

The host suite passed all checks through native-Q23 concurrent-worker parity,
including engines, effects, MIDI, persistence, USB, UI, recovery and editor
checks. Historical DSP fixtures use their original Q15 scale; separate
shipping-Q23 integration and numerical fixtures exercise the native path.

The restored precision fixture initially failed its obsolete assertion that
all held buttons leave the keys musical. The newer UI intentionally uses held
buttons to select pages, as its existing UI test verifies. Only that precision
fixture was adapted: it checks musical keys on the released FX page and the
three-insert contract. Both remaining checks then passed when rerun:
`build/host/native-audio_precision_test` and `build/host/precision_asan`.
The latter enables address and undefined-behavior checks, excluding inherited
signed-shift and host alignment assumptions.

PCM24 edge/random round trips, sub-PCM16 synthesis from all four engines,
active audio effects, dither, the master curve, limiter release and concurrent
Echoverb/chorus allocation passed. Native precision reports a maximum master
curve error of 2.297 Q23 LSBs. All pre-existing local UI and flash-script edits
were verified unchanged byte for byte.

Logs are local ignored build artifacts: `build/q23-restore-build.log` and
`build/q23-restore-tests.log` (including the initial stale-test failure).
The completed reruns are in `build/host/log/native-audio_precision_test.log`
and `build/host/log/precision-address-overflow.log`. Reproduce the complete
suite with BUILDING.md.
Host and package checks do not establish physical noise floor, audible quality
or sustained on-device deadlines. Device CPU use has not been measured here.
No repository or site was published, and this package has not been installed.
