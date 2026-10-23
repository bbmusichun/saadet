# Saadet platform and BLE migration

Work started on 23 October 2026. The separate `platform/` target now builds
Saadet on pinned fm1-base/felucca-bsp with the Bluetooth MIDI SDK linked.
It is an experimental local build. The first authorized flash wrote completely
but failed to return over MIDI. A startup-order fix was installed through
RP2040 recovery with complete flash readback verification. The device now
boots Saadet, answers USB MIDI/editor requests and reports BLE advertising;
pairing and physical audio/control validation remain pending.
See `docs/VALIDATION.md`. No combined firmware has
been cleared for distribution or relicensed.

## Completed engine preparation

- SID is removed, including reSID waveform inputs, generators and its two
  dedicated fixtures. Stored engine ID 3 is reserved and retired; it is never
  reassigned. Existing SID library entries remain unsupported. Last-sound and
  backup validation reject SID synth state rather than playing a different engine.
- The three selectable engines are Prophet (19), FM6 (12) and CZ-1 (15).
- PHASE/CrispyZebra is removed. Native CZ-1 has its own runtime state and note
  reset and keeps the BSD-3-Clause uPD933 renderer. Shared sine data and maths
  have their own provenance below. Native 144-byte tones remain unchanged. History checks attribute the extracted
  state/init/accessor definitions to Kerem (`5f71930a`, `fa64ef07`, `6da744c7`);
  the new native reset does not extract the PHASE oscillator.
- FM6 renders through the MSFA-style fp32 sine operators. MARK I/OPL operator
  code, their log-sine/exponent tables, and the multi-operator feedback special
  cases have been removed. Only MSFA appears in the renderer control.
- FM6 uses the instrument's common allocation and mono priority, with newly
  written retrigger/legato integration. The Dexed-style 16-entry shadow mono
  bank and its allocation/transfer logic are removed. Released voices may still
  advance their envelope/phase control state without evaluating sample kernels.
- FM6's existing patch formats, engine ID and function-byte layout remain.
  Historical renderer IDs 1/2 are accepted on state import and normalized to 0
  (MSFA). The patch bytes and unrelated controller settings are preserved.
  This does not promise the old MARK I/OPL sound: the rendering and mono/stealing
  policy have intentionally changed. There is no Dexed sample-parity claim.
- The unused undo handle and preserving-history resize code are removed.
- The old effects-related linker minima are replaced by a 16 KiB engine-state
  bank, checked against each supported engine type at compile time. Native
  precision tests use that bank without cache or auxiliary SRAM tails. This
  budget covers engine state only, not UI, USB, code, stacks or a Bluetooth stack.

## Source records

These upstream revisions were checked directly. MSFA and Dexed checkouts are
provenance references. The pinned fm1-base checkout and its nested BSP are
inputs to the separate platform build; the wrapper copies them into a disposable
overlay and leaves their original sources unchanged.

| Source | Revision | Material and terms |
| --- | --- | --- |
| [Google MSFA](https://github.com/google/music-synthesizer-for-android) | `f67d41d313b7dc85f6fb99e79e515cc9d208cfff` | Apache-2.0 DX7 algorithms/operator model, pitch and scaling, sine/frequency generation, envelopes and LFO. |
| [Dexed](https://github.com/asb2m10/dexed) | `fea2f0f0f2c397799f59b27eb11c038cd34b22c5` | Only explicitly Apache-2.0 MSFA files provide retained synthesis/data references: `source/msfa/{dx7note.cc,env.cc,lfo.cc,pitchenv.cc,controllers.h,porta.cpp}` and MSFA kernel/table files. `source/engine/EngineMkI.cpp` and `EngineOpl.cpp` are GPL and are not retained implementations. |
| [fm1-base](https://github.com/charlesvestal/fm1-base) | `93bee3fe9def08127dd5dff638df5bd57cebcc80` | `include/fastmath.h`, MPL-2.0, Charles Vestal. Imported at `firmware/src/x0x/fastmath.h` with Kerem's unsigned exponent-bit adjustment. The original Charles attribution is restored; no Leo authorship is inferred from shared licensing. |
| fm1-base platform dependency | `6c0b47e4bfc56fd3cd5b34463210d0e7e0ce0431` | MPL-2.0. Updated for optional m-bootloader installation; the earlier fastmath import above retains its original source record. |
| felucca-bsp nested in that base | `0f08fcaa6610b7cabe8cfdcc5c50d6facdae0e7f` | The BSP linked by the experimental `platform/` target, including bootloader-only OTA staging and the loader progress screen; the legacy build is separate. |
| [m-bootloader](https://github.com/jvitkauskas/m-bootloader/releases/tag/v1.2) | v1.2, source `22e2559906f9fc4e077f2d6aee20bf332300439e` | MIT, Julius Vitkauskas. Unmodified FM-1 `.fwsc` and `.boot` from the pinned base; SHA-256 checked against the author's release. Its full MIT notice accompanies the prepared artifacts. |
| [MAME uPD933](https://github.com/mamedev/mame/blob/master/src/devices/sound/upd933.cpp) | Existing port; see `LICENSING.md` and `docs/CZ1_SYSEX.md` | BSD-3-Clause, Devin Acker. Existing native phase/envelope/amplitude implementation remains, with its notices. No new MAME import was made in this step. |

Source files were inspected, so the new integration is not represented as a
formally separated clean-room implementation. This work uses permissively
licensed source with attribution and independently written instrument policy.

## Implemented platform port

`tools/build_base.py` builds the tracked `platform/` integration against the
pinned dependency revisions. `--no-ble` produces a USB-only comparison target;
the default includes BLE MIDI. The old platform remains available through
`build.sh`; its boot stub, loader and OTA transaction are not linked into the
new target. Only its asset generator is reused as a build-time tool.

The port includes:

- The three native engines, Q23 mixing, the SRAM audio graph, bounded CPU1
  worker launch/join, fault retirement and retained overload-victim policy.
  CPU1 uses private 8 KiB stacks. Clock-dependent voice admission reads the
  existing PLL settings without changing them.
- The existing UI, raw physical panel calibration, LED duty planes, display
  sleep, saved-object formats and the 384-slot library. The base has separate
  A/B objects at `0xA2000..0xA5000` for panel/radio data. Tests enumerate all
  48 live legacy sectors and verify no platform or OTA overlap.
- USB and TRS MIDI plus BLE MIDI peripheral input/output. BLE advertises
  **Saadet**; central mode is not built. BLE is started by the base's existing
  boot/crash guard. GLOBAL > TEMPO adds BLE to the clock-source choices.
- Per-port physical note/sustain holds before the shared channel controller
  path. Disconnecting BLE releases its notes, while USB/TRS holds on the same
  pitch survive. Existing per-channel expression remains shared.
- Stereo 44.1 kHz USB PCM16/PCM24 in both directions, using native sample
  rings in POOL SRAM. PCM24 retains low bits; PCM16 capture keeps TPDF dither.
  Synth capture precedes computer playback. The physical output keeps its
  inherited -6 dB ceiling after capture/mixing. Standard USB playback master
  mute/volume is retained. Endpoint/interface numbering and the playback
  clock servo now follow the base, so physical host interoperability must be
  measured again.
- The existing USB editor/custom SysEx and raw USB DX7/CZ bank parsers. Raw
  byte hooks let full bank dumps reach their bounded parsers even when larger
  than the base's small command-frame collector. Native bank imports and the
  editor are USB facilities; BLE SysEx/editor transport is not implemented.

The legacy `ED_AUDIO_STATS` command is currently not advertised on this port;
the base has its own render/xrun/radio diagnostics. Existing editor preset,
settings and backup commands still use Saadet's protocol. No physical backup
round trip or BLE connection has yet been measured. The initial migration
required RP2040 recovery; its measured write/readback and subsequent boot are
recorded in `docs/VALIDATION.md`.

The updated dependency also supports m-bootloader as a separate, explicit
installation after the new app. `tools/base_bootloader.py` checks the exact
v1.2 FM-1 hashes before serving the boot-only staging transaction, then requires
the original app identity to return. Build preparation and tests do not write
the device. The new app and bootloader were subsequently installed on this
unit, and the user confirmed the PLAY recovery menu. USB download was briefly
detected, but macOS denied kernel-driver detach and the device returned to
Saadet. The user deferred further terminal checks; end-to-end direct USB
recovery on this Mac remains unverified.
The device installation receipts are recorded in `docs/VALIDATION.md`.
Installation order and keys are in `BUILDING.md`.

## Computer and phone connection repair (23 October 2026)

Reference: [Charles Vestal's fm1-ctl](https://github.com/charlesvestal/fm1-ctl)
`e181b4a1ac1510ce18ebf6e271a143c2fe364ccd`, especially its Bluetooth UI and
`docs/MANUAL.md` connection/troubleshooting sections. That app pins fm1-base
`b16c177a9086be955cd3e418383f2fd92c6d658b`; Saadet's newer base already
contains the same MIDI service, optional Just Works pairing and bond storage.
No separate authentication protocol or central-device scanner was imported.
Existing Charles Vestal and JieLi notices remain in the dependency overlay;
Saadet's integration additions carry Kerem's credit.

The connected unit's pre-change log reports `post: no task sys_timer`
repeatedly after advertising starts. The BLE-only shim has no such task, so
SDK timers explicitly assigned to that service never reach their callbacks.
The checked overlay routes those timers to the existing main-loop timer pump,
while preserving other named task owners and the WiFi build's task semantics.
This repairs an observed callback failure; it does not yet establish the
cause of the user's failed physical connection.

Additional connection adaptations:

- GLOBAL > BLE shows advertising, startup/stopped/off, a link waiting for
  MIDI subscription, or MIDI connected. K1 turns the radio off/on; turning
  K4 RETRY clockwise drops a held link and re-enables advertising. These radio
  controls are session-only; BLE still starts at the next normal boot, subject
  to the inherited crash guard.
- A fresh connection clears its notification subscription, restores the
  default ATT payload size, and restarts connection-parameter negotiation.
  A link counts as MIDI ready only after notifications are enabled.
- GAP name reads accept a host buffer larger than the name, support partial
  reads, and return zero beyond the name. Startup diagnostics now say Saadet.
- Turning the radio off during initialization remains effective when the
  asynchronous Bluetooth-ready event arrives.

For a Mac, open Audio MIDI Setup > Window > Show MIDI Studio > Bluetooth and
connect to Saadet. If the page says WAITING FOR MIDI, quit and reopen Audio
MIDI Setup, then use RETRY and Connect again. On iPhone/iPad, connect from a
music app's Bluetooth MIDI settings. The retained Just Works handler accepts
pairing if the host asks; the peripheral does not force a PIN exchange.

Tests execute the patched timer source, radio-ready handler, ATT read and
connection/disconnect helpers with controlled IO under address/undefined
behavior sanitizers. The UI fixture checks all displayed states and text
layout. The host scenario exercises the actual platform bridge, link-only
recovery, disabling/re-enabling, reconnecting and release of BLE-held notes.
These are software checks; this repair has not been installed or verified over
a physical BLE link. Receipts and package hash are in `VALIDATION.md`.

## Licensing boundary still outstanding

The enclosing firmware remains GPL-3.0-only. New MPL/Apache files do not
relicense the rest of it. The new experimental BLE target links JieLi's
Bluetooth binaries through the base. That local build is not evidence that
all linked source is licensed for a combined distribution.

Leo's [recorded permission](https://github.com/hugelton/felucca-bsp/issues/4#issuecomment-6216224984)
says anyone may use Felucca code for which he personally owns the copyright
under MPL-2.0; it also identifies his PHASE waveform code as MIT in
Felucca-Engines. It expressly excludes other contributors' code. This record is
broader than an assumption based only on which files a downstream base copied,
but is not blanket permission for the entire Felucca/Melodee tree.

Known work remaining before a distributable BLE firmware:

1. Audit every linked app file, tool and asset against history and original
   sources. In particular `midi_control.c` and `midi_clock.c` retain
   ChanceTheMaker contributions under their existing GPL notices; Leo's grant
   does not cover those contributions. Factory preset data and all other
   contributor boundaries still need explicit decisions. Do not relabel
   enclosing headers in bulk.
2. Measure the move from the legacy CRSTUB/update layout to the base's entry
   at `0x02000120`, including interrupted updates, reboot/recovery and saved
   library/state retention. The old ChoralRoot-derived loader and Andre
   Rezemini's retained `firmware/src/ota.c` remain only in the legacy build.
3. Measure actual FM-1 radio calibration/startup, connection loss, simultaneous
   USB/TRS/BLE traffic, physical keys/LEDs/display, native SysEx/editor/backup,
   USB host-volume and format changes, sustained polyphony and DMA deadlines.
   Simulator output and linked footprint cannot establish these properties.
4. Finish the editor audio-diagnostics integration and any issues found during
   those hardware checks. The original platform remains the comparison build.

The subsequent user-authorized flash attempt and startup repair are recorded
in `docs/VALIDATION.md`. No publication or repository commit has been performed.

## Validation

The engine preparation uses the existing target build and full host suite,
including native Q23, concurrent worker parity and sanitizer checks. Additional
integration coverage exercises historical FM6 renderer-state imports and SID
state rejection. The MSFA routing fixture compares the new operator plan with
the retained pre-migration MODERN plan over all algorithms, feedback settings
and changing gain states; legacy renderer arguments map to MSFA.

Current run results and footprint belong in `docs/VALIDATION.md`. Logs are
`build/migration-build.log`, `build/migration-tests.log`, and `build/host/log/`.
The header/notice inventory is `build/migration-provenance.json`; it is an
inventory aid, not a substitute for contributor and source-history checks.

The platform-specific checks live in `tests/run_base_tests.sh` and
`tests/base_port_test.py`. They exercise the patched USB stream/descriptor and
volume-control routines under address/undefined-behavior sanitizers, validate
storage ranges, and drive the base host through all three engines, BLE note and
sustain disconnects, USB/TRS overlap, keyboard MIDI output and BLE clock-source
selection. `tools/check_base.py` verifies the actual target symbol map and
packed resident-code bytes. See `docs/VALIDATION.md` for run results.
