# Building Saadet

Use the JieLi pi32v2 Linux toolchain and AC79 V1.2.1 SDK used by Melodee.
On macOS, Docker or OrbStack must be running for the Linux compiler.
Python requires Pillow with Raqm; the build also needs Node.js for editor tests.

```sh
python3 -m venv build/venv
build/venv/bin/pip install Pillow
PYTHON="$PWD/build/venv/bin/python" ./build.sh
npm ci --prefix web
PATH="$PWD/build/venv/bin:$PATH" tests/run_tests.sh
```

The normal package is `build/saadet.fwsc`, development identity `FM-1_900`.
Internal compiler artifacts retain the inherited `build/melodee.*` names.
`./build.sh --nightly` produces `build/saadet-nightly.fwsc` and includes the
source revision in the version. The defaults retain USB audio, TRS MIDI,
dual-core rendering and the upstream worker-failure fallback.

The build checks generated tables, MMIO access, audio RAM placement, resource
capacity, boot/update layout and package structure. Host test logs are under
`build/host/log`.

Serve the editor without publishing:

```sh
python3 -m http.server 8846 --bind 127.0.0.1
```

Open `http://127.0.0.1:8846/web/editor.html`. Web MIDI requires a browser that
supports it and user-granted SysEx permission. The local editor contains no
multi-track simulator or Melodee release installer.

Installation is a separate, explicit operation. The retained command-line
installer is `tools/fm1_install.py`; recovery is `tools/fm1_recover.py`.
Use the actual Saadet package and detected FM-1 port. Read the storage note in
README before switching between Saadet and Melodee. Host and target checks are
not physical audio/control/deadline validation.

The shipping audio path is Q23 (24-bit signed samples in 32-bit buffers).
The inherited DSP fixtures run with `AUDIO_FRAC_BITS=0` to preserve their Q15
numeric contracts. The suite also runs native Q23 integration, numerical,
concurrent-worker and precision/overflow checks. `tests/audio_precision_test.c`
checks signed PCM24, low-level synthesis/effects, dithered PCM16 export, insert
controls and two full-length Echoverbs plus chorus in conservative target SRAM
banks. The linker enforces those bank minima. These checks do not measure
physical noise floor or on-device render deadlines.
