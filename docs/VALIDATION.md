# Saadet validation — 23 October 2026

## JUNO-106 engine and the BLE default build

Source: `ui-reset` with the uncommitted JUNO-106 engine (MPL-2.0, see
`LICENSING.md`) and `./build.sh` now building the fm1-base BLE package. No
device was flashed and no repository commit or publication was made.

Host checks (`tests/j106_test.c`, in the Q15 fixtures, the shipping Q23 build
and under address/undefined-behaviour sanitizers):

- Patch messages: 500 random frames round-trip every byte; short, foreign,
  out-of-channel and 8-bit frames are refused atomically; `41 30 .. pp` is named
  by its A11–B88 label and `41 31` MANUAL. The switch bits follow the published
  layout (SW1 82 = chorus I, saw, no pulse, 8'; inverted HPF bits), and every
  switch field edits only its own bits.
- Voice: A4 at 8' measures 440.0 Hz, 16' 220.0 Hz, 4' 880.0 Hz, the sub alone
  220.0 Hz. Attack 0 completes in 2.2 ms and 127 in 3.01 s; release to 1 %
  takes 2.2 ms at 0 and 12.05 s at 127. GATE closes within a few blocks. Full
  resonance self-oscillates with the mixer shut; resonance 80 stays silent.
  Every starter tone sounds, stays bounded and ends.
- Part: chorus I and II produce a decorrelated stereo pair through the mixer;
  off is mono. For a low note the HPF orders 0 > 1 > 2 > 3.
- Library, state and editor: store/load/import in shared slots, 8-bit records
  refused, favourites in the tagged row, the last state with the tone, and a
  pre-JUNO-106 state and backup upgrading with the INIT tone. Command 96 GET,
  PUT, field, range refusal and name; INFO tag `4A 01`; favourites namespaces.
  The sanitizer run found and led to the fix of a one-byte over-read in the GET
  reply's name.
- Existing suites with JUNO-106 included where they iterate every engine
  (`synth_test`, `ui_test` layout lint of every page, `audio_precision_test`);
  `web/test_synth.mjs` covers the web tone format, INFO and library kind 4.
- The base simulator steps PROPHET, FM6, CZ-1 and JUNO-106 with ALGORITHM;
  JUNO-106 peaks at -13.4 dBFS (the others -11.4 to -15.0).

`tests/run_tests.sh` (with the new sanitizer step) ended **ALL HOST TESTS
PASSED**; `tests/run_base_tests.sh` ended **ALL BASE PORT TESTS PASSED**. Every
target build passed its layout checks; the audio RAM graph includes the
JUNO-106 voice, block, post and stereo stages.

| Build | Package | App image | Low RAM | Resident audio | Engine arena | SHA-256 |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `./build.sh` (BLE) | `platform/build/saadet.fwsc` | 550,580 B | 93,452 B | 84,144 B | 120,896 B | `21fe1a3f…629b` |
| `./build.sh --no-ble` | `platform/build/saadet-usb.fwsc` | 414,392 B | 45,720 B | 83,664 B | 145,952 B | `31fb963c…71f6` |
| `./build.sh --legacy` | `build/saadet.fwsc` | 284,140 B | 37,272 B | — | — | `4ab985b1…0bb3` |

Unmeasured: the engine's sound against a Juno-106, its CPU cost on the FM-1
(the 2-unit voice charge and the batch cost reuse estimates), chorus/HPF
calibration, and SysEx from a real instrument. TRS MIDI input drops SysEx, so
patch and control messages reach Saadet over USB only.

## Direct BLE MIDI devices and forgetting

The default BLE target now enables the base's simultaneous central/peripheral
roles: one external MIDI peripheral, plus one phone/computer connected to
Saadet. GLOBAL > DEV uses Charles Vestal's unchanged btdev helper to scan,
select and connect/disconnect. GLOBAL > BLE adds phone-only DROP and confirmed
FORGET ALL PAIRINGS through the SDK's shared bond-table clear API. The radio
address/calibration store is not erased. See UI.md for controls and
BLE_MIGRATION.md for source attribution and implementation boundaries.

Validation on 23 October 2026:

- BLE target: app 533,364 bytes, low RAM 93,404 bytes, resident audio 75,788
  bytes, POOL 214,752 bytes, engine arena 129,312 bytes. CPU1, resident-code,
  packed image, engine-arena and boot-layout checks pass.
- BLE package `platform/build/saadet.fwsc`, identity `FM-1_900`, SHA-256
  `5ad1ee6589f49c2a0491bf9316a114ee2011aba4fb49b4be0d852c2626ee5c06`.
- USB-only target also passes: app 397,212 bytes, low RAM 45,672 bytes,
  resident audio 75,328 bytes, POOL 189,696 bytes. The BLE target is built last
  so the shared sidecars describe the BLE package.
- `tests/run_base_tests.sh`: ALL BASE PORT TESTS PASSED. New scenarios exercise
  page-driven discovery, SAVE/K4 connection actions, outgoing MIDI with only
  the external device connected, simultaneous phone and external holds,
  independent disconnects including external sustain, shared BLE clocks,
  phone-only DROP and confirmed/cancelled/expired forget requests.
- Sanitized helper tests execute the real MIDI parser with separate source
  tags, the SDK bond-clear guard in both role configurations, and the actual
  target adapter's wait-for-links, success, SDK-failure and timeout paths.
  They also check RETRY preserves a pending client search's advertising pause.
- The new UI states and scrolling list pass screen layout checks. The existing
  legacy UI and erase regressions report UI test passed and UI ERASE TESTS PASSED.

Logs: `build/base-ble-central-{host-build,build,usb-build,tests,unit-tests,devices-test,ui-regression,erase-regression}.log`.
Screens are in `build/base-ui-ble/` and `build/base-ble-devices/`.
The source update was committed as `472b60c` on `ui-reset`, then the exact
BLE package above was installed on USB MIDI port `FM-1` without rebuilding.
The installer completed and reported `done: the FM-1 runs FM-1_900`; an
independent request confirmed `FM-1_900 [running]`. The new boot's log reports
`le_config:2 1 1 1`, advertising as Saadet, heap peak 11,280 of 24,576 bytes,
and no recorded crash. Remote panel commands opened GLOBAL > DEV and waited
before another log read; no crash was reported. These diagnostics confirm
startup, not real peer interoperability. No physical forget operation was
invoked and no saved-state readback was performed.

Receipts: `build/base-ble-central-flash.log`,
`build/base-ble-central-after-device-info.log`,
`build/base-ble-central-after-device.log`, and
`build/base-ble-central-scan-device.log`.
Real peer connection, simultaneous RF/MIDI operation and persistent bond
removal across a reboot remain pending hardware validation.

## Computer/phone BLE connection repair

Reviewed Charles Vestal's fm1-ctl at `e181b4a1ac1510ce18ebf6e271a143c2fe364ccd`
and the shared base's connection/pairing history. The live unit's pre-change
log (`build/base-ble-connect-before-device.log`) contains repeated
`post: no task sys_timer`, with advertising reported and no recorded crash.
The patch repairs named default-service timer dispatch in a BLE-only build,
adds GLOBAL > BLE connection state and RADIO/RETRY controls, corrects GAP name
reads, resets negotiation/subscription per link, and respects OFF during
asynchronous startup. See `BLE_MIGRATION.md` for provenance and boundaries.

Software validation:

- The BLE target passed CPU1, resident-code/packed-image, engine-arena and
  boot-layout checks. The app is 522,148 bytes, low RAM 91,792 bytes, resident
  audio 75,684 bytes and POOL use 214,624 bytes.
- `tests/run_base_tests.sh`: **ALL BASE PORT TESTS PASSED**. This includes the
  actual patched timer source (BLE and WiFi ownership behavior), startup-ready
  handler, ATT name reads, fresh-link state, disconnect/re-advertising, UI
  layout and controls, and host connection/reconnection with BLE-note release.
  Existing native USB, storage, MIDI overlap and bootloader/updater checks pass.
- The separate legacy UI regression reports **UI test passed**.
- The USB-only comparison target also passes; app 394,468 bytes, low RAM
  45,368 bytes, resident audio 75,336 bytes and POOL use 189,696 bytes.
- The BLE package is `platform/build/saadet.fwsc`, identity `FM-1_900`, SHA-256
  `7af16e7bdbef2bf761cbdab937aa7bcc866fee84a67af56c0595884d4ef8a828`.

Logs: `build/base-ble-connect-{host-build,build,tests,unit-tests,ui-regression,usb-build}.log`.
Screen fixtures are in `build/base-ui-ble/` and the platform host screenshots
in `build/base-ble-connect/`. The target build runs last so its shared sidecar
files describe the BLE package rather than the USB comparison.

All outstanding source changes were committed as `dd9d0f8` on `ui-reset`,
then the exact BLE package above was installed through the base's normal app
installer on USB MIDI port `FM-1`. The installer completed its write and
reported `done: the FM-1 runs FM-1_900`; a separate identity request confirmed
`FM-1_900 [running]`. The package was reused without rebuilding.

Post-install diagnostics report no recorded crash and `BT ready: advertising
as Saadet`. The retained log also contains earlier boot history; after the
latest `BT: starting` marker, no `post: no task sys_timer` warning appears,
including the subsequent delayed log read. This establishes startup and
advertising diagnostics, not a physical BLE connection. Receipts:
`build/base-ble-connect-flash.log`,
`build/base-ble-connect-after-device-info.log`, and
`build/base-ble-connect-after-device.log`.

A Mac/iPhone/iPad connection, host pairing/bond persistence, actual BLE MIDI
traffic, timing and physical audio remain unverified. No saved-state readback
was performed. The causal role of the missing callback in the user's failed
physical connection is not yet established. Installation receipts in the next
sections refer to earlier firmware.

## m-bootloader integration

The platform now pins fm1-base `6c0b47e4bfc56fd3cd5b34463210d0e7e0ce0431`
and felucca-bsp `0f08fcaa6610b7cabe8cfdcc5c50d6facdae0e7f`.
The new BLE package is 611,354 bytes, SHA-256
`49bb09219c1a92aafe871442b5f876fb09d01d435df5473ae3fa8b86cc90a3d2`.
Its app is 520,468 bytes, low RAM 91,792 bytes, resident audio 75,684 bytes
and engine arena 129,440 bytes. Target boot-order, packed-code, CPU1 and RAM
checks passed. After the user's explicit update request, this new app was
installed over USB MIDI. The updater completed and verified `FM-1_900`, and
native editor INFO returned `SAADET v0.1.0-base-dev`.

Builds prepare the unmodified m-bootloader 1.2 FM-1 artifacts, their MIT notice
and a source/checksum manifest. Their hashes match the author's v1.2 release:

- Installer `.fwsc`: `0e89141f52d1f2d62198d4f69147a2c15c8360806e64d48002990a6fdab6d6dd`.
- Plain `.boot`: `9e5ec0e39dc55c167f8e289cfdac612d7d340b5f46100e1be2540ef4d6ec2f13`.

`ALL BASE PORT TESTS PASSED` includes the updated host simulator, native USB
sanitizer fixtures, eight bootloader-wrapper tests, the real BSP OTA staging
tests under ASan, upstream MIDI app-installer tests and the upstream browser
bootloader-installer simulation. Coverage includes refusal by an older app,
disconnects, out-of-range reads, a missing/wrong app after restart, altered
bootloader files, corrupt writer bodies and wrong-board/damaged boot areas.

A separate offline check compiled m-bootloader v1.2's actual `core.c` under
ASan/UBSan against the new Saadet app combined with this unit's saved boot
head. Its boot replacement preserves the app, mounts the app at `0x4000`
with entry `0x02000120`, accepts all file CRCs and rejects an intentionally
corrupted app. The synthetic boot head inside Saadet's app-only `.fwsc` does
not have the stock allocation required for boot replacement; it must not be
raw-flashed. The normal app updater preserves the unit's existing boot head.

Logs: `build/base-mboot-build.log`, `build/base-mboot-host-build.log`,
`build/base-mboot-port-tests.log`, `build/base-mboot-app-check.log`.
The pinned m-bootloader installer was then staged with
`tools/base_bootloader.py --install --port FM-1 --yes`. Staging completed and
Saadet returned as `FM-1_900`. Receipts are
`build/base-mboot-device-update.log`, `build/base-mboot-device-app-info.log`
and `build/base-mboot-device-bootloader.log`. The user confirmed the PLAY
recovery menu appeared at power-on, establishing that m-bootloader is active.
REC at power-on and the menu's DOWNLOAD MODE both led to a black screen.
The direct USB tool briefly found the expected `4c4a:8057` ROM device but
failed while detaching its kernel driver (`USBError: Access denied`);
subsequent enumeration found no ROM device. The user reported that Saadet
returned without a requested power-cycle. No flash, RAM-loader upload or
reboot command was sent during these download checks. The reason for the
return is not established. Trace: `build/base-mboot-menu-download.log`.

Final checks again reported `FM-1_900 [running]`, native editor INFO
`SAADET v0.1.0-base-dev`, no recorded crash and BLE advertising startup
(`build/base-mboot-final-device.log`). No boot-area or saved-state readback
was performed during this installation. The user deferred further terminal
download-mode checks until later; end-to-end USB recovery on this Mac remains
unverified. See `BUILDING.md` for the app-first installation order and explicit
bootloader command.

## First device flash and startup repair

The user authorized installation after the host/target checks. The base
installer wrote the BLE package with SHA-256
`c2cdbbd37fe1ab7f07c8841d7d7e4a89061041c39a4953ac09bcdb869a71e45d`
through 100%, then timed out waiting for MIDI to return. The device was absent
from both MIDI and ROM USB at the subsequent checks. This is a failed startup,
not a verified installation. The receipt is `build/base-flash.log`.

Review identified an early-boot dependency: resident P33 helpers and their
far-call pointers could run before SRAM/data initialization. The corrected
base startup stops CPU1 first, clears buffers, copies initialized data and
both RAM code sections, then enters hardware/time/watchdog setup. The actual
target disassembly was inspected to confirm those loops precede the first
P33 call; `tools/check_base.py` now enforces this boot-order contract.

The corrected BLE package is `520,340` app bytes, SHA-256
`d90034c3217393cf3cde6f78b8a1f32fa19e8205a485bc6955cf5033a25d5ec7`. Its target checks, offline recovery-package / chip-key /
loader checks, and USB/MIDI/storage sanitizer fixtures pass.

The FM-1 was recovered through a Seeed XIAO RP2040 running the verified
Transporter firmware. Recovery reported `key=980F type=3 id=856014`.
Two full flash reads were identical (SHA-256
`ed5130c30520587b98fecc5b36c67944a6f3d951314fc8e438290cc9e580adca`).
After a successful dry run, 128 application sectors were written from the
corrected BLE package. The final full 1 MiB read exactly matched the expected
image (SHA-256
`5054e3353d2769dbaae02fb651aad0948fbfdb39e6e417cb232c7fc798de3abc`),
including unchanged bootloader `0x0000..0x3FFF` and device-data
`0x93000..0xFFFFF` regions. Receipts are `build/transporter-dump.log`,
`build/transporter-restore-check.log` and `build/transporter-restore.log`.
After removal of the recovery wiring and a normal power-cycle, the user
confirmed that Saadet booted. USB MIDI returned as `FM-1`; the base updater
reported `FM-1_900 [running]`, and native editor INFO returned
`SAADET v0.1.0-base-dev`. The diagnostics reported no recorded crash, MIDI BLE
profile initialization, and `BT ready: advertising as FM-1`, with radio heap
peak 6,472 of 24,576 bytes. These are device-reported startup observations,
not an over-the-air pairing or musical test. The startup log also contains
`post: no task sys_timer` messages and nonzero timing-late counters; sustained
audio/deadline behavior is not yet qualified. Receipts are
`build/base-post-recovery.log` and `build/base-post-recovery-diagnostics.log`.

The original pre-installation measurements below describe the earlier
package; they are retained as the record of the failed artifact's validation.

## Experimental fm1-base / BLE port

Source: `ui-reset` with the existing local UI/audio work, completed engine
preparation and the uncommitted `platform/` port. No device was flashed and no
repository commit or publication was made.

Both target variants passed upstream build checks and Saadet's additional
CPU1, SRAM-copy, packed-code, engine-arena and boot-entry checks. Package
identity is `FM-1_900`; each package is 610,136 bytes.

| Target | App image | Low RAM used / 98,304 B | Resident audio | Engine arena |
| --- | ---: | ---: | ---: | ---: |
| BLE MIDI + USB | 520,356 B | 92,304 B | 75,796 B | 129,312 B |
| USB-only comparison | 392,872 B | 45,880 B | 75,336 B | 154,368 B |

The BLE target leaves 6,000 bytes of ordinary low RAM.
Its linked radio buffers/tasks are already included in those figures; physical
radio startup and runtime heap/deadline behavior still need measurement.
CPU1 has private 8 KiB stacks; its polling loop and native synth graph are
copied into SRAM before worker launch. The guaranteed engine arena exceeds the
16 KiB engine-state requirement. Optional cache RAM is disabled in this port.

Packages:

- `platform/build/saadet.fwsc` (BLE), SHA-256
  `c2cdbbd37fe1ab7f07c8841d7d7e4a89061041c39a4953ac09bcdb869a71e45d`.
- `platform/build/saadet-usb.fwsc`, SHA-256
  `7337c23c1e37aff22c4d10bc471be16124f454ec31a70d1b5bf74d7d6f41548e`.

### Completed checks

- All 43 named legacy host checks passed, ending with
  `ALL HOST TESTS PASSED`. These include native-Q23 integration, concurrent
  engine worker parity, UI/persistence/editor/recovery and sanitizer checks.
- `tests/run_base_tests.sh` passed, ending with `ALL BASE PORT TESTS PASSED`.
  The base simulator produces audio from Prophet, FM6 and CZ-1; it verifies
  BLE note/sustain disconnects, overlapping USB/TRS holds, keyboard output to
  both USB and BLE, and BLE clock transport while ignoring USB transport.
- The actual patched USB routines passed native PCM24 edge/low-bit packet
  round trips, PCM16/dither bounds and silence, both format descriptors,
  playback decode/mix and standard mute/volume controls under sanitizers.
- Input overflow clears pre-panic port ownership; the next note-on/off pair
  remains releasable. Non-SysEx status reaches the native import-abort hook;
  real-time clock does not abort the frame.
- All 48 live legacy flash sectors were enumerated, checked for unique
  addresses and verified separate from the base's panel/pairing objects and
  OTA area. This verifies the address plan, not saved data on a physical unit.
- The simulator's engine-library screenshot was inspected for display output.

Logs: `build/base-ble-build.log`, `build/base-usb-build.log`,
`build/base-host-build.log`, `build/base-port-tests.log`,
`build/base-legacy-regressions.log`. Target footprint/hash records are
`platform/build/saadet-validation.json` and
`platform/build/saadet-usb-validation.json`.

### Still unmeasured or incomplete

Actual BLE advertising/pairing, disconnect behavior, physical controls/audio,
USB 16/24-bit host interoperability and volume, sustained render deadlines,
radio calibration, native import/backup round trips, and migration update /
recovery have not been tested on the FM-1. The base simulator runs the
instrument serially; the separate legacy tests exercise concurrent engine
workers, while the new target's actual dual-core lifecycle remains unmeasured.

The port currently omits the legacy `ED_AUDIO_STATS` capability. BLE central
and BLE SysEx/editor transport are not implemented. The linked-source and
preset-asset distribution audit remains open, including retained
ChanceTheMaker MIDI contributions; see `docs/BLE_MIGRATION.md` and
`LICENSING.md`. These results do not establish release readiness.

---

# Historical Q23 restoration — 22 October 2026

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

## UI row F — 23 October 2026

Source: `9402c54` plus local delete/erase changes. Earlier sections describe the
22 October audio restoration; effects were subsequently removed by `42ac8f5`.

Library user sounds now expose K4 DELETE, with an expiring FX confirmation,
cancellation gestures, a warning header and a breathing FX LED. Deletion keeps
the current sound playing and removes its slot and favourite. GLOBAL ends with
ERASE: K1 arms, a fresh three-second GLO hold confirms, and release cancels
without paging. Successful erasure clears settings/calibration retention and
reboots; failures keep the error screen and do not report success or reboot.

The target build passed all RAM, audio placement, MMIO, boot/update and package
checks. The app is 294,592 bytes, `.data + .bss` is 44,920 of 98,304 bytes,
and the loader is 7,386 bytes. `build/saadet.fwsc` is 610,542 bytes,
identity `FM-1_900`, SHA-256
`3307b17e390d9a2411a855bb2775a9f20033e7bc2be304c448eb2f671f459fa6`.

`tests/run_tests.sh` finished with `ALL HOST TESTS PASSED`. The additional
`ui_erase_test` passed address and undefined-behavior checks, with the same
inherited shift/alignment exclusions used by the audio fixture. It exercises
all four engine lists, favourites, slot 384, cancelled/expired/stale delete
confirmations, failed writes, delete persistence, the erase countdown,
release suppression, unavailable flash, and reported/silent/partial erase
failures. Both A/B copies of all 24 live objects are erased and read back.
An independent expected sector map verifies every other byte of the 1 MiB
flash model remains unchanged, including firmware and staged-loader storage.
The new screens passed the layout lint and were visually checked at 240×240.

Logs: `build/ui-row-f-build.log`, `build/ui-row-f-tests.log`,
`build/host/log/ui_erase_asan.log`. Screen captures: `build/ui-row-f/`.
This package has not been installed. Physical controls, audio continuity and
device erase/reboot behavior remain unverified.

## Full-charge reference support — 23 October 2026

Before installation, the connected device identified as `SAADET v0.1.0-dev`,
`FM-1_900`, MIDI port `FM-1`, and did not answer the new battery-read command.

The prepared update adds command 81 for a fresh ADC sample and a saved full
reference. It uses tagged reserved settings bytes within the existing 604-byte
PER5 record; sounds, favourites and panel mapping retain their layouts. The
capture tool takes 31 samples and saves the median. No empty point or battery
percentage is inferred. Settings and integration fixtures verify round trips,
write idempotence, invalid values, transport blocking and failed flash writes.

The target package is `build/saadet.fwsc`, identity `FM-1_900`, 610,542 bytes;
the app is 294,880 bytes. SHA-256:
`73747ede0c2c8f20711e6b8cf93c6200d317ffae1617d1b8b6a8b047edb094ba`.
Build log: `build/battery-reference-build.log`. The complete suite finished
with `ALL HOST TESTS PASSED` (`build/battery-reference-tests.log`), including
Q23 integration and address checks.

Installed that package on 23 October 2026 using `tools/fm1_install.py`;
the installer completed and confirmed running identity `FM-1_900`. With the
user reporting a full battery, 31 fresh ADC samples ranged from 627 to 630,
median 628. Command 81 saved 628 as the full-charge reference and confirmed
it by readback. Receipt: `build/battery-full-reference.json`. The empty point
and a percentage curve remain uncalibrated. This installation and calibration
do not establish physical audio quality or performance deadlines.

## 23 October 2026: BLE migration engine preparation

This stage retains the existing GPL platform and does not enable Bluetooth.
SID/reSID and PHASE/CrispyZebra implementations and SID waveform inputs are
removed. CZ-1 has standalone state/reset handling. FM6 uses native MSFA-style
operators, accepts historical renderer IDs on import through MSFA, and uses the
common instrument's allocation/mono priority with new retrigger/legato hooks.
MARK I/OPL sound parity is intentionally not claimed. Sources, licensing limits
and remaining platform work are recorded in `BLE_MIGRATION.md`.

The target build passed the image, RAM, resident-code, dual-core, register-access,
boot/update and package checks. The app is **271,180 bytes**; `.data + .bss`
is **37,224 of 98,304 bytes**. The contiguous main resource arena is 199,264
bytes; the enforced engine-state minimum is now 16,384 bytes, replacing obsolete
effect budgets. Native tests render all three engines with that 16 KiB bank
alone and no optional cache or auxiliary SRAM tails. These figures are for the
current non-radio build; they do not establish the final BLE memory layout.

`tests/run_tests.sh` completed all 43 named checks with **ALL HOST TESTS PASSED**,
including native Q23, dual-core serial/concurrent parity, bounds and overflow
sanitizers, UI/layout, storage/update/recovery, and editor tests. New coverage
checks FM6 state migration (native patch bytes and unrelated controller settings
preserved) and retired SID state rejection. The MSFA routing fixture passes
91,392 edge cases plus 200,000 changing blocks against the historical MODERN
reference; the envelope wrapper passes 816,000 state transitions. The final
three-engine delete fixture was rerun with slot 384 on the last engine and passed.

The rebuilt package is `build/saadet.fwsc`, 610,542 bytes,
identity `FM-1_900`, SHA-256 `365af9e3363826dd8095d4681764dc93c364721b96980f1d62f9471140aee7d7`. It has not been flashed.
Physical audio, control behavior and sustained hardware deadlines remain
unverified. Build/test logs: `build/migration-build.log`,
`build/migration-tests.log`, `build/host/log/`.
