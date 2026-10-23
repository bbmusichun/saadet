# Clean-room record: MIDI performance input

## Why

Saadet's MIDI performance input (pitch bend and its RPN 0 sensitivity, the sustain pedal, All Notes Off, All
Sound Off, Reset All Controllers and per-channel note ownership) was adapted from code contributed by
ChanceTheMaker (GitHub user 471816) in Felucca pull request 8, commit `14b7e16` (`670193c` in this history). That
code reached Saadet through Melodee and through Felucca 1.0 (`727f272`). Its author has not granted it under
MPL-2.0. To make the enclosing firmware MPL-2.0, it is replaced by an independent implementation.

The rest of pull request 8 needs no replacement:

- `019e2ad` (transport sync) and `57f2aa0` (TRS ring) are keremimo's, granted under MPL-2.0 (felucca-bsp
  `LICENSING.md`, felucca-bsp pull request 3).
- 70 of the 77 substantive lines of `firmware/src/midi_clock.c` match `019e2ad`, and none match ChanceTheMaker's
  commit, so its credit to ChanceTheMaker is a misattribution.
- ChanceTheMaker's other commits in that pull request (`3c1e670`, `362dff6`, `6889637`) are not in `main`'s
  history.

## Roles

**Specification and verification (the "dirty" side): the main Claude Code session, at Kerem Kilic's request.**
It read ChanceTheMaker's commit, the derived `midi_control.c`, and the bend code in `voice.c`. It then:

- wrote [midi-input-spec.md](midi-input-spec.md) from the MIDI 1.0 standard and Saadet's own interfaces. The
  spec describes behaviour only, without code.
- wrote `tests/midi_input_test.c` from that specification, and rewrote the oracle in
  `tests/control_prepare_test.c`, which had copied the derived bend formula, to check the specification's
  properties instead.
- checked both tests against the old implementation, to confirm they describe the behaviour Saadet has now.
- removed the derived code from the tree given to the implementer (below).

**Implementation (the "clean" side): a separate Claude subagent started with a fresh context.** It had none of
the above conversation, and was given only the specification, the test commands and the scrubbed working tree.

## What was removed before the implementation

- `firmware/src/midi_control.c`, all of it. Its controller-to-parameter mapping (`midi_parameter`, 36 lines,
  all by Kerem Kilic in `git blame`) was moved unchanged into `firmware/src/midi_param.c`.
- In `firmware/src/voice.c`: the bend state (`midi_bend_target`, `midi_bend_q8`), `p5_update_bend`, the per-block
  smoothing, and the bend's whole and fine terms in the voice pitch, `m.fine` and `m.plog`. Each place is marked
  `CLEANROOM`.
- `seq.c` and `platform/instrument.c` now call `midi_input_reset()` and `midi_held_count()` instead of clearing
  or reading the removed module's arrays. `tests/synth_harness.c` calls `midi_input_reset()` at power-on.

## Rules given to the implementer

- Work only in the given working tree.
- Do not use git history in any form: no `log`, `show`, `blame`, `diff` against other revisions, `checkout` or
  `restore` of other revisions, `stash`, `reflog`, `cat-file`, or reading `.git`.
- Do not read anything outside the working tree. This includes the main checkout, its `build/` and `platform/build/`
  directories, `.claude/`, the session's scratch files, and other worktrees.
- Do not look up Felucca, Melodee or Saadet sources, their pull requests or forks online.
- Use only the specification, the MIDI 1.0 standard, general knowledge, and the files in the working tree.

## Tests the implementation must pass

From the working tree, after `build/gen` has been generated:

    cc -O2 -DAUDIO_FRAC_BITS=0 -w -Ibuild/gen -Ifirmware/src tests/NAME.c -lm -pthread -o build/NAME && build/NAME

for NAME in `midi_input_test`, `synth_test`, `control_prepare_test`, `prophet_knob_test`, `dual_core_test`,
`mod_route_test`, `j106_test` and `fm6_test` (the last one takes a directory argument: `build/fm6_demo`). The
main session then runs the complete suites (`tests/run_tests.sh`, `tests/run_base_tests.sh`) and both firmware
builds.

## Result (23 October 2026)

**The implementation.** The implementer wrote `firmware/src/midi_input.c` and the pitch-bend parts of
`firmware/src/voice.c`, changing nothing else. It confirmed it followed the rules: it read and wrote only inside
the working tree, ran no git commands and looked nothing up. Its only outside input was the session context
attached automatically, which was the main checkout's `git status` (file names) and `CLAUDE.md`, with no code.

**The design is its own:**
- Per-channel state is bend, sensitivity, the RPN selection and the pedal.
- Each channel's notes on each track are kept as per-note bitmaps, with a separate bitmap for the notes only the
  pedal keeps.
- A channel's targets are computed when needed.
- The module uses 648 bytes of static RAM at one track.
- The bend smoothing moves a quarter of the remaining distance plus one unit each block. That's a time constant
  of about 2.5 ms, and it never overshoots.

**Independence check, by the verifier.** I compared the new file with the old one after removing `midi_parameter`
from the old file. 26 of the new file's 150 substantive lines also appear in the old file, and every one is
dictated by the spec:
- function signatures the spec requires;
- calls into the chord and strum interface of `chord.c` and `seq.c`;
- plain loops over 16 channels, 27 keys, 128 notes or the voices;
- the dispatch to `mod_midi`.

The whole-file similarity ratio (Python `difflib`) is 0.22. In `voice.c` the bend's fine part uses the same unit
conversion as before, 2367/2560. Those numbers are fixed by the units the spec gives: 2.367 per cent times 100/256
cents per Q8 unit.

**Verification, by the verifier.** Every build and suite below passed. The logs are under `build/cleanroom/`.

| Step | Result |
| --- | --- |
| `./build.sh --legacy` | Passed, `build/saadet.fwsc`, identity FM-1_900 |
| `tests/run_tests.sh` | ALL HOST TESTS PASSED, including `midi_input_test` and the rewritten `control_prepare_test` |
| `./build.sh` (fm1-base, BLE) | Passed; low RAM 92,988 B, pool 222,464 B |
| `./build.sh --no-ble` | Passed; low RAM 45,160 B |
| `./build.sh --host` | Passed |
| `tests/run_base_tests.sh` | ALL BASE PORT TESTS PASSED |

`midi_input_test` measures the bend in the voices' pitch by counting zero crossings. The new code gives the same
counts as the old one: 76 at centre, 153 an octave up, 39 an octave down.

These are host and build checks only. The new input has not yet been played on an FM-1.
