# Independent replacement of the final base-build GPL exceptions

23 October 2026. This record supplements the earlier MIDI replacement in [PROCESS.md](PROCESS.md).

## Scope and provenance

Claude's “Firmware release preparation” session (`1a2b12a9-5c80-4c81-ac42-772fea27f3d8`) audited the 96
repository files compiled into the fm1-base build. It left four GPL exceptions while changing the cleared
files to MPL-2.0. Those changes were still uncommitted when this work started, on `main` after `a6efb4d`.

| File replaced in full | Earlier source of the exception |
| --- | --- |
| `firmware/src/editor_preferences.c` | ChanceTheMaker's preferences/favourites protocol contributions, Felucca PRs 10–12 |
| `firmware/src/favorites.c` | ChanceTheMaker's favourites storage, the same contributions |
| `firmware/src/settings_persist.c` | ChanceTheMaker's favourites persistence, the same contributions |
| `firmware/src/scratch.h` | Andre Rezemini's SLOOP PR 111 shared main-loop workspace |

The verifier checked the current files and their source history, including the Felucca 1.0 import `727f272`
and the scratch-sharing commit `71bf054`. A history author's name alone was not treated as proof of original
authorship; the earlier attribution audit and retained contribution notices identify the upstream work.
All four current files were replaced, including their later Melodee additions, to keep the implementer's
input free of the earlier implementations. The replacements are new work credited to
Kerem Kilic (Ellic Studio), under MPL-2.0. The superseded source in history has not been relicensed.
Other files' upstream and third-party notices remain intact.

## Separation

**Specification and verification:** the parent Codex session. An initial search exposed part of an existing
implementation. Before any replacement was written, the user explicitly authorized a fresh implementer;
the parent then read the four originals, inspected callers and tests, and wrote
[preferences-spec.md](preferences-spec.md). It describes interfaces, observed behaviour, wire layouts and
storage facts without providing implementation bodies.

**Implementation:** a subagent started with `fork_turns="none"`, without the parent conversation. It received
only the specification, restrictions and a separate tree. The tree was an ordinary copied directory, with
the four original files absent, no `.git`, no existing build artifacts, and only generated `build/gen` inputs.
The parent's baseline binaries, AST inspection output, original-file copies and other scratch files stayed
outside that directory and were prohibited inputs.

The implementer was restricted to that tree and forbidden to read outside it, use Git/history in any form,
or fetch Saadet, Melodee, Felucca or SLOOP source/PRs online. It could use the specification, remaining tree
interfaces/tests and general knowledge. It wrote only the four replacement files and generated test outputs.
It reported following those restrictions. Follow-up clarifications concerned interface facts only: monitor
event invalidation, retention of the runtime zoom byte and custom budget encoding, and when ADC sampling
occurs relative to a flash write. No old implementation text was sent back.

## Independent design and review

- Favourites use common bit-access and tagged-row helpers for both the JUNO and scale collections, plus a
  shared-user namespace dispatcher. The old implementation had separate per-collection implementations.
- Persistence uses explicit field codecs and separate USB-word shape, budget and encoding helpers. It keeps
  the exact 604-byte PER5 record, tagged fields, reserved bytes and feature-specific ownership rules.
- Editor commands dispatch into separate state, validation, capacity, favourites and battery handlers.
- The workspace gains natural four-byte alignment through a uint32 member; its members still alias the same
  bounded storage. Application and loader capacities remain 1024 and 640 bytes.

The verifier read the replacements and compared them with the originals after implementation. Whole-file
`difflib.SequenceMatcher` ratios were 0.283 (editor), 0.275 (favourites), 0.275 (persistence), and 0.333 (scratch).
Shared material consists of required types, field layouts, API signatures, protocol values, ordinary C idioms
and existing interface calls. Similarity statistics are a review aid, not proof of independence; the separated
inputs and implementer restrictions are the process evidence.

## Compatibility oracle

Before replacing any file, the verifier wrote `tests/preferences_compat_test.c` and ran it against the original
implementations. Its 12,109 cases cover:

- 1,024 deterministic PER5 inputs, including invalid lengths/magic, valid and invalid tags, palettes, HOLD,
  workload budgets, USB bits, A4, recording preferences, battery calibration, lights, voice costs, old-record
  validation and ownership; normalized records, runtime settings and re-exported records are captured.
- All namespaces around factory/shared/native bounds; repeated favourite writes, malformed JUNO tags and
  old/new JUNO versions; scale tags with preservation of the retired row's first 16 bytes.
- Editor state/palettes/set/get, malformed request lengths, empty and populated shared slots, native namespace
  capacity checks, absent flash, failed erase, active transport, protected old settings and raw battery bounds.

The old and replacement implementations produced byte-identical snapshots. The baseline snapshot SHA-256 is
`0f7866d4761f4d5efa75add0ab0c2100d56886c52d2dffeb403267c1a4da65d8`.
The test keeps the fixed FNV-1a digest `7079ed26` and case count; neither was updated to accommodate
the replacement. Additional `settings_usb_test` and application/loader `main_scratch_test` fixtures passed
against the originals before replacement and are included in the host suite.

## Build and suite verification

The implementer passed settings, synth (Q15 and native precision), JUNO-106, UI, erase and storage host tests.
It compiled the OTA fixture but could not run it without a package in the isolated tree; the verifier runs that
fixture against the actual legacy package as part of the complete suite.

The initial legacy build, full host suite, BLE build, USB-only build, host build and base-port suite all passed.
After those checks, the user explicitly retired the legacy platform. Its compiler/release entry points, flash
option and local executable artifacts were removed. Shared asset generation was extracted into
`tools/generate_assets.py` after checking its history; it retains Kerem and Leo's applicable notices, with no
ChoralRoot notice because that contribution was in the removed packaging/stub implementation. The temporary
implementer tree was removed as well, since it contained a copy of the now-retired builder.

The base resident-audio pass also used the retired module's compiler-invocation helpers. Those helpers were
extracted into `tools/jieli_toolchain.py`, preserving their source and GPL/Leo notices (history `5684e187`).
Neither extracted module includes the old application, stub, loader, package or release builders. Legacy
package/release command-line tools reject requests; format readers and recovery helpers retain their licences.

The host suite no longer needs or exercises a legacy firmware package; current updater/package coverage is in
`tests/run_base_tests.sh`. Final verification uses only the BLE, USB-only and host targets and the revised suites.
Logs and the byte-for-byte original/replacement comparison are under `build/cleanroom-preferences/`
(local generated evidence, not distribution source).

Final results after retiring the legacy target:

| Check | Result |
| --- | --- |
| Revised `tests/run_tests.sh`, without a legacy package | ALL HOST TESTS PASSED |
| `./build.sh` (BLE) | Passed; low RAM 92,988 B, pool 222,464 B |
| `./build.sh --no-ble` | Passed; low RAM 45,160 B, pool 197,376 B |
| `./build.sh --host` | Passed |
| Revised `tests/run_base_tests.sh` | ALL BASE PORT TESTS PASSED |
| Preferences compatibility under AddressSanitizer + UndefinedBehaviorSanitizer | All 12,109 cases passed with the fixed baseline digest |
| Retired build/flash/package/release command rejection tests | Passed |

The BLE package SHA-256 is `1a667a97e552f476538e50da460fb13e3d4e1a6f24446c12e61dd50af8202efb`;
USB-only is `d4c9f8d9c642e682ee0dce8df2d42c83415706763b8c8a5f6e5cfa3d693e1f95`.
Both packages are unchanged by separating the build helpers from the retired compiler. The full host suite
also runs native Q23 integration, concurrent-worker parity and the existing sanitizer fixtures. Unrelated
pre-existing source changes were checked against an initial hash manifest and preserved byte for byte.

An inherited integration issue was found before the first full build could proceed: `gen_scales.py --check`
still generated a GPL header for `scales.h`, which the earlier audit had changed to MPL. The generator's emitted
header was updated to MPL-2.0 to match the independently authored catalogue; the generator itself stays GPL.
No scale contents changed.

`tools/licence_check.py` now reports all 96 compiled repository dependencies as MPL/permissive with an empty
GPL exception registry. Its tests reject stray GPL files, missing SPDX notices and stale synthetic exceptions.
This is a source-dependency/header check; other third-party terms remain documented in `LICENSING.md`.

No publication or firmware installation was performed during implementation or verification. Physical
audio/control and radio behaviour have not been requalified by the host tests.
