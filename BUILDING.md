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
`build/host/log`; UI reports and frames are under `build/ui_new` and
`build/ui_pixel`. `UI=renders tests/run_tests.sh` runs only layout rendering.

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
