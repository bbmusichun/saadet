# Saadet validation — 22 October 2026

Base: committed Melodee nightly `b5148ba528fcaa4bfda0dde8b6d96ced061ed459`,
with the single-synth reduction, local native-precision/insert-only changes and
the single-instrument interface. The original Felucca checkout was not modified.
A fresh fetch confirmed `upstream/nightly` is still `b5148ba`.

## Audio and effects

The default signal path is Q23: signed 24-bit audio in 32-bit containers, with
wide gain/filter products and DC histories. Prophet, FM6 and CZ-1 retain their
fp32 cores and quantize their output at the higher precision. SID retains its
chip-scale oscillator/filter model and gains the higher-precision amplitude,
insert and output paths. The master saturation curve uses fp32 knots and cubic
interpolation; the existing curve, clipping ceiling and analogue level remain.

USB PCM24 preserves the low byte in both directions. PCM16 capture applies
TPDF dither at its final reduction; exact silence stays silent. USB format
selection does not change internal synthesis precision or the 44.1 kHz rate.

One serial chain contains three inserts. Punch DSP, punch keyboard capture,
send buses, send/return workspaces, shared/send pages and the editor's send
controls are removed. FX-held notes remain musical notes and double-tapping FX
does not lock a keyboard layer. Old stored send parameter numbers remain
reserved and inert so existing state/archive layouts retain their identity.

Delay histories use packed PCM24 with unchanged lengths. Echoverb's diffusion
state and history have separate fixed arena handles. The native SRAM fixture
checks two full-length 500 ms Echoverbs, chorus and FM6 together without the
optional cache bank, and checks that disabling a slot releases both handles.
The working arena still bounds the combinations of memory-using effects.

### Review against nightly

Engine cores (`fm6_core.c`, `eng_cz.c`, Prophet patch code), the batch
scheduler (`audio_batch.h`: kernel cost table, CPU0 service reserve, worker
split), the worker (`audio_worker.h`), the voice contract and the production
performance options are byte-identical to nightly. The differences are the
drum engine's removal from dispatch and admission, one part (`NPART` 1), the
Q23 output scaling of the four engines and UB-safe SID shifts. The voice budget
(16 weighted units, 18 with CPU1 at 360 MHz) and admission weights (Prophet 3,
CZ-1 3, FM6 and SID 2) match nightly's single-part case.

Three precision defects were corrected:

- The PCM helpers (`audio_mul`, `audio_clamp`, DC decay, packed history) were
  emitted as calls from the per-sample mixer, pan, DC and master loops. They are
  now always inlined; no calls remain in the target disassembly. ROOM's kernel
  moved to POOL SRAM beside chorus and Echoverb so the low DSP bank keeps the
  fixture's 8 KiB tail.
- The limiter's release tail decremented one Q23 unit instead of one Q15 unit,
  making the last 2 dB of release about three times slower than nightly. It now
  steps in whole Q15 units; `audio_precision_test` checks the envelope sample
  for sample against the Q15 schedule.
- The optional hardware-EQ path's sanity bound was not scaled to Q23 (it is off
  by default).

## Interface

The on-device interface has been removed ahead of a new design. The screen
stays blank; the keys play, MASTER sets the volume and the other controls do
nothing. Startup maintenance, the update countdown, UPDATE and the crash report
still draw, and the maintenance screen's OCT- / OCT+ answers work again (the
single-synth fork had dropped their input). SysEx dumps sent to the device are
refused until an interface can choose their slots.

## Package

The target build completed RAM, audio placement, MMIO, boot/update and package
checks. `build/saadet.fwsc` is 610,542 bytes, identity `FM-1_900`;
the app is 239,244 bytes and loader 7,386 bytes.
Package SHA-256: `10338c4d34f416c1ae705b98f589e788d9588fc21ecdd17ba32616a72059145d`.

The main audio arena has 184,064 bytes against its enforced 172,032-byte fixture
minimum; the low DSP tail has 8,884 bytes against 8,192. Guaranteed SRAM banks total 257,996 bytes; the optional cache bank adds
28,672 bytes after its boot self-test. Low, data and retained bank minima are
also enforced by the linker and modeled conservatively by the native fixture.

## Checks

`tests/run_tests.sh` completed with **ALL HOST TESTS PASSED**. The inherited
fixtures run at their original Q15 numeric scale, and the shipping Q23 path has
separate integration, numerical, concurrent-worker and precision checks.
Native checks were repeated after the final master-curve update. Coverage:

- Four engines, native controls, polyphony, stereo spread and release tails;
  FM6 sub-PCM16 output survives conversion, and all four finished engine paths
  produce fractional PCM16 samples.
- Signed PCM24 edges, 100,000 random round trips, packed history precision,
  sub-PCM16 stereo duplex and unbiased dithered PCM16 reduction.
- Master curve compared with double-precision libm: maximum error 2.297 native
  Q23 LSBs over the sampled input sweep, including the clipping plateau.
- Every active audio insert with sub-PCM16 and hot input; independent slot
  controls, tone filters, histories, bypass release and failed allocation.
- 1,536 FM6 and 348 Prophet serial/concurrent comparisons, 311,604 concurrent
  batch groups, three-insert chain parity and worker corruption/fault handling.
- Native precision under address and undefined-behavior sanitizers. Bounds and
  arithmetic overflow checks are enabled; signed-shift and alignment checks
  are excluded because the inherited host fixture strips target alignment
  attributes and the upstream phase code uses signed shifts.
- MIDI ownership, repeat timing, sustain, channel filtering, strum and arp HOLD;
  no held button claims the keyboard, and the blank panel changes nothing.
- State recall, slot 384, flash write failures, previous-copy recovery,
  shared-library/archive boundaries, editor DOM and USB transport/lifecycle.

Logs and generated images are local ignored build artifacts. Reproduce with
BUILDING.md. Host and package checks do not establish physical noise floor,
audible improvement or sustained on-device audio deadlines. No repository or site was published.

An earlier package (SHA-256 `218634fc…557e`, before this review and the new
interface) was installed on an FM-1 with the user's authorization. The package
above has not been installed. Physical audition, the new interface on hardware
and sustained on-device deadline checks remain unverified.
