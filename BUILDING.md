# Building Saadet

Use the JieLi pi32v2 Linux toolchain and AC79 V1.2.1 SDK used by Melodee.
On macOS, Docker or OrbStack must be running for the Linux compiler.
Python requires Pillow with Raqm; the build also needs Node.js for editor tests.

```sh
python3 -m venv build/venv
build/venv/bin/pip install Pillow
PYTHON="$PWD/build/venv/bin/python" ./build.sh
PYTHON="$PWD/build/venv/bin/python" ./build.sh --legacy
npm ci --prefix web
PATH="$PWD/build/venv/bin:$PATH" tests/run_tests.sh
```

`./build.sh` builds the fm1-base port with Bluetooth MIDI (below):
`platform/build/saadet.fwsc`, development identity `FM-1_900`. `--no-ble`
builds the USB-only `platform/build/saadet-usb.fwsc` instead, `--host` the
simulator, and `--release X.Y[.Z]` a release identity.

`./build.sh --legacy` builds the original platform: `build/saadet.fwsc`, also
`FM-1_900`. Its internal compiler artifacts retain the inherited
`build/melodee.*` names. `./build.sh --legacy --nightly` produces
`build/saadet-nightly.fwsc` and includes the source revision in the version.
The legacy defaults retain USB audio, TRS MIDI, dual-core rendering and the
upstream worker-failure fallback. The host suite checks this package's update
layout, so build it before `tests/run_tests.sh`.

The legacy build checks generated tables, MMIO access, audio RAM placement,
resource capacity, boot/update layout and package structure. Host test logs
are under `build/host/log`.

Serve the editor without publishing:

```sh
python3 -m http.server 8846 --bind 127.0.0.1
```

Open `http://127.0.0.1:8846/web/editor.html`. Web MIDI requires a browser that
supports it and user-granted SysEx permission. The local editor contains no
multi-track simulator or Melodee release installer.

Installation is a separate, explicit operation. `tools/flash.sh` builds the
default package and installs it with the base's installer; `--legacy` builds
and installs the original platform's package with the retained
`tools/fm1_install.py`. Recovery is `tools/fm1_recover.py`. Use the actual
Saadet package and detected FM-1 port. Read the storage note in
README before switching between Saadet and Melodee. Host and target checks are
not physical audio/control/deadline validation.

The shipping audio path is Q23 (24-bit signed samples in 32-bit buffers).
The inherited DSP fixtures run with `AUDIO_FRAC_BITS=0` to preserve their Q15
numeric contracts. The suite also runs native Q23 integration, numerical,
concurrent-worker and precision/overflow checks. `tests/audio_precision_test.c`
checks signed PCM24, low-level synthesis, dithered PCM16 export, and all four
engines in a single 16 KiB resource bank without cache or auxiliary SRAM tails.
The linker enforces that engine-state budget, with matching compile-time size
checks. These checks do not measure physical noise floor or on-device render
deadlines. See `docs/BLE_MIGRATION.md` for the migration's current limits.

## fm1-base / BLE port (the default build)

The `platform/` target uses the pinned fm1-base and felucca-bsp
revisions in `tools/build_base.py` (fm1-base `6c0b47e4bfc5`, BSP `0f08fcaa6610`).
`./build.sh` runs that wrapper; it can also run directly. The wrapper fetches
those revisions if absent, verifies a clean dependency checkout, regenerates
Saadet's assets and applies the checked overlay to a disposable build copy. It
leaves the original GPL platform and its package in `build/` available for
comparison (`./build.sh --legacy`).

```sh
PYTHON="$PWD/build/venv/bin/python" ./build.sh --no-ble
PYTHON="$PWD/build/venv/bin/python" ./build.sh
PYTHON="$PWD/build/venv/bin/python" ./build.sh --host
tests/run_base_tests.sh
```

Device packages are `platform/build/saadet-usb.fwsc` and
`platform/build/saadet.fwsc` (BLE). Both have development identity `FM-1_900`.
The BLE target needs `AC79_SDK` (default `~/fw-AC79_AIoT_SDK`) and the same
JieLi toolchain (`JIELI_TOOLCHAIN`, default `~/.jieli/toolchain`). Simulator
builds require neither the SDK nor the toolchain. Builds must run sequentially:
the two variants share a disposable overlay and compiler outputs.

The wrapper also checks CPU1's private stacks and resident polling loop,
resident audio placement, packed image bytes, engine arena and new boot entry.
Logs for this migration are `build/base-{usb,ble,host}-build.log`,
`build/base-port-tests.log` and `build/base-legacy-regressions.log`.
The base's installer is in `build/fm1-base-platform/tools/`; the legacy
CRSTUB package validator is not the installer for this new package layout.
Neither these build commands nor the tests install firmware.

### Recovery bootloader and installation order

This pin supports **m-bootloader 1.2**. Every base build prepares its unmodified
FM-1 installer, plain boot image, MIT licence and source/checksum manifest in
`platform/build/bootloader/`. The checksums match Julius Vitkauskas's v1.2
release. The Saadet app package remains separate: a normal app update preserves
the device's bootloader, including m-bootloader once installed.

For the one-time migration, install the newly built Saadet app first. Its newer
BSP updater accepts the bootloader-only package; the previous Saadet build
rejects it. Then explicitly install the checked bootloader:

```sh
build/venv/bin/python tools/base_bootloader.py --check
build/venv/bin/python build/fm1-base-platform/tools/fm1_install.py platform/build/saadet.fwsc --port FM-1 --yes
build/venv/bin/python tools/base_bootloader.py --install --port FM-1
```

The last command writes the boot area and asks for confirmation (`--yes` skips
the prompt for an already authorized install). Keep power and USB connected.
It serves only the pinned FM-1 package, requires a running `FM-1_900` app and
checks that the same app returns. It does not use the normal two-stage app
installer, which would wait for an update loader that m-bootloader's installer
does not provide. A return to Saadet alone does not prove bootloader contents:
switch off, hold **PLAY**, and switch on to verify the recovery menu. Hold
**REC** while switching on for USB download mode. OCT− + OCT+ held in the app
no longer enters update mode in this upstream revision.

The recovery menu and REC download path run before Saadet, so a broken app can
be recovered without relying on its MIDI updater. A power loss during the
bootloader replacement itself can still require the RP2040 USB_KEY recovery
route. Use the app updater for subsequent Saadet updates; do not raw-flash the
package's synthetic flash head over the device's preserved boot area.

The port is experimental and private. BLE is a MIDI peripheral advertised as
**Saadet** in its GAP name and startup diagnostics, with
keyboard output and channel-message input. The default also enables BLE central
mode for one external MIDI device alongside one incoming phone/computer link.
GLOBAL > DEV provides discovery/connect/disconnect; GLOBAL > BLE manages the
phone link and can forget all BLE bonds. The existing editor and native bank imports continue over USB.
The distribution/provenance audit and physical checks are tracked in
`docs/BLE_MIGRATION.md`.
