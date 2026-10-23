# Saadet / Melodee licensing

Saadet is a single-synth fork of Melodee. New work is by Kerem Kilic (Ellic Studio); retained upstream and third-party notices below continue to apply.

Melodee is a modified version of Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments
(<https://github.com/hugelton/Felucca>), and keeps Felucca's licensing.

Melodee is free software, licensed under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). That covers the code and its own assets. A few
bundled or ported parts keep their own licences; they are listed below, and their licence
texts are in `LICENSES/`.

Melodee code and modifications: Copyright (C) 2026 Kerem Kilic (Ellic Studio)\
Retained Felucca portions: Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments

Copyright notices follow the code's actual origin. Independently written Melodee files
credit Kerem Kilic; mixed files also identify the retained Felucca portions. Unchanged
upstream files and third-party ports retain their original notices. Sharing Felucca's
GPL licence does not make new Melodee code Hügelton Instruments' work.

## What is GPL-3.0-only

Every file in this tree that carries an `SPDX-License-Identifier: GPL-3.0-only` header:

- the firmware: `firmware/` (app, HAL, update loader)
- the build script and tools: `build.sh`, `tools/`
- the web pages (installer, editor) and their tests: `web/` (not the Fukiai font, below)
- the host tests: `tests/`

Three source files are ports and keep the licence of their originals: `firmware/src/phys_dsp.c`
(DaisySP, MIT), `firmware/src/phys_symp.c` (Rings, MIT), and `firmware/src/cz_native.c`
(MAME uPD933, BSD-3-Clause). `firmware/src/fm6_core.c` and
`firmware/src/eng_fm6.c` retain MSFA (Apache-2.0) synthesis and data in C with native fp32 signal arithmetic (below).
The new operator and voice-policy files, the JUNO-106 engine files (below) and the imported fm1-base maths keep their per-file MPL/Apache notices.
This preparation does not relicense the enclosing application; the full provenance and platform migration are still in progress (see `docs/BLE_MIGRATION.md`).
The firmware built with them is GPL-3.0-only as a whole.

You may use, study, change and share Melodee under the GPL. If you distribute Melodee, or
firmware derived from it, you must also give your recipients its complete corresponding
source under the same licence. That includes devices that ship with modified Melodee inside.

## JUNO-106 engine (MPL-2.0)

The JUNO-106 engine is new work by Kerem Kilic (Ellic Studio), licensed under the
Mozilla Public License 2.0 (`LICENSES/MPL-2.0.txt`) file by file:

| File | What |
| --- | --- |
| `firmware/src/j106_patch.h` | The native tone: the instrument's patch-message layout, switch bits, decoding and encoding |
| `firmware/src/eng_j106.c` | The voice, the part's LFO, high-pass and chorus, and the presets (Roland's factory patches themselves are vendor data, below) |
| `firmware/src/j106_edit.h` | Panel field descriptors and edits |
| `firmware/src/editor_j106.c` | Editor command 96 and the instrument's SysEx messages |
| `tests/j106_test.c` | Host tests |

It was written from the Juno-106's published architecture and from its MIDI
implementation as publicly documented (the owner's-manual layout reproduced in
ST-Log issue 33, Peter Cassidy's 1994 analogue-list summary in the Hyperreal
archive, and the hinzen.de Juno-106 SysEx page) and, for the VCA LEVEL
slider's -5..+5 scale, Analogue Renaissance's JUNO-106 test procedure. Those
were read for facts only: message layouts and parameter meanings. No code, tables or data from KR-106,
fm-106, TAL-U-NO, the Felucca forks' Juno engines or any other emulation were read
or used; those are GPL-licensed and incompatible with this file-level licence.
The DSP is generic (polynomial band-limited steps, a trapezoidal four-pole
low-pass, one-pole filters, a modulated delay line) and uses the MPL-2.0 fm1-base
maths already in the tree (`firmware/src/x0x/fastmath.h`, Charles Vestal). Its
times, ranges, curves and chorus rates are engineering estimates, not measurements
of an instrument.

The presets are INIT and Roland's 128 factory patches. Those patches are Roland's
data, kept in `assets/j106-factory/` and generated into a build header; they are
not part of the MPL-2.0 files and no licence is granted for them (see Third-party
material below). Patches a user imports remain the user's material. The hooks
that connect the engine to the existing instrument (engine table, library, state, mixer, panel,
editor and web editor) are edits to GPL-3.0-only files and stay GPL-3.0-only; the
legacy firmware built with them is GPL-3.0-only as a whole. MPL-2.0 permits this
combination (its section 3.3); the JUNO-106 files themselves remain available
under MPL-2.0, and anyone distributing them must keep that licence and offer
their source as MPL-2.0 requires.

## Retained Hügelton Instruments material

The following original material is by Hügelton Instruments (Leo Kuroshita).
Melodee's additions and modifications are credited separately in the source files:

| What | Licence | Where |
| --- | --- | --- |
| Original Felucca portions retained in the firmware, tools, web pages and tests | GPL-3.0-only | Source notices; `LICENSE` |
| The Fukiai icon font (<https://github.com/hugelton/Fukiai>): the web editor's icons | MIT | `web/fukiai.ttf`, `LICENSES/MIT-Fukiai.txt`, `web/FUKIAI-LICENSE.txt` |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| ChoralRoot FM-1 contributors (<https://github.com/Quixotic7/ChoralRootFM1>, v0.16, commit `9c7445cdd7b767c779741d28654fb59cca3acd97`): boot stub, interrupted-update write ordering, commit-marker/CRC compensation, and recovery tests, adapted for Melodee. The shared `CRSTUB` / `CRBM` format is retained. | GPL-3.0-only | `firmware/stub/`, `firmware/loader/ldr_core.c`, `tools/build.py`, `tools/fm1pkg_make.py`, `tests/boot_stub*`, `tests/loader_powercut_test.c` |
| Andre Rezemini (SLOOP RAM branch): retained update-path contributions, not covered by Leo's own-code permission. This path remains in the existing GPL build and is excluded from the experimental base target. | GPL-3.0-only | `firmware/src/ota.c` |
| Archivo font by The Archivo Project Authors (<https://github.com/Omnibus-Type/Archivo>): the UI text, rasterised into the firmware at build time (`tools/gen_aa_font.py --preset stage`; the generated tables are not offered as a font, and the font declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/Archivo[wdth,wght].ttf`, `LICENSES/OFL-Archivo.txt` |
| Rubik font by The Rubik Project Authors (<https://github.com/googlefonts/rubik>): the `rubik` preset of `tools/gen_aa_font.py` (the firmware's text up to the Saadet redesign; the font declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/Rubik[wght].ttf`, `LICENSES/OFL-Rubik.txt` |
| Inter Tight font by The Inter Project Authors: the `inter-tight` preset of `tools/gen_aa_font.py` (the firmware's text was Inter Tight up to Melodee 0.13) | SIL OFL 1.1 | `assets/fonts/InterTight[wght].ttf`, `LICENSES/OFL-InterTight.txt` (also `assets/fonts/OFL.txt`) |
| DaisySP by Electrosmith, Corp and Emilie Gillet (<https://github.com/electro-smith/DaisySP>): the PHYS engine's modal and string models and the resonator, ported to fixed point | MIT | `firmware/src/phys_dsp.c`, `LICENSES/MIT-DaisySP.txt` |
| Rings by Emilie Gillet (<https://github.com/pichenettes/eurorack>): the PHYS engine's sympathetic strings, ported to fixed point | MIT | `firmware/src/phys_symp.c`, `LICENSES/MIT-Rings.txt` |
| MSFA (Google Inc., Pascal Gauthier, Jean Pierre Cimalando): DX7 synthesis, scaling, envelopes, algorithms, LFO, frequency/sine data, controllers and portamento, from explicitly Apache-2.0 source files. Saadet uses native fp32 operators and its own voice lifecycle. MARK I/OPL renderers and tables and the Dexed-style shadow mono bank have been removed. Sources and exact revisions are in `docs/BLE_MIGRATION.md`; historical GPL notices remain until the enclosing-file audit is complete. | Apache-2.0 upstream portions; current enclosing app GPL-3.0-only; new operator file MPL-2.0 AND Apache-2.0 | `firmware/src/fm6_core.c`, `firmware/src/eng_fm6.c`, `firmware/src/fm6_msfa_ops.h`, `tools/gen_tables.py`, `LICENSES/Apache-2.0-msfa.txt` |
| fm1-base by Charles Vestal: explicitly MPL-licensed single-precision maths, imported at `93bee3fe9def08127dd5dff638df5bd57cebcc80`; Kerem's unsigned exponent-bit adjustment avoids a signed-shift issue. This replaces the earlier GPL copy of the same helper. | MPL-2.0 | `firmware/src/x0x/fastmath.h`, `LICENSES/MPL-2.0.txt` |
| MAME uPD933 device model by Devin Acker: native CZ phase functions, envelope rate law and logarithmic amplitude adapted to C with native fp32 playback | BSD-3-Clause | `firmware/src/cz_native.c`, `LICENSES/BSD-3-Clause-uPD933.txt` |
| Casio CZ-1 preset tones A-1..H-8 (Casio Computer Co., Ltd., 1986): the 64 factory sounds as native tone data, combined from the "64 original CZ-1 patches" SysEx (GeoCities CZ-101 page, archived 2009) and Oli Larkin's VirtualCZ conversions of the same presets (LCD names, line levels, velocity). Included as sound data for CZ-1 compatibility; the rights stay with Casio. See `assets/cz1-factory/README.md` | Casio's (no licence granted) | `assets/cz1-factory/cz1-factory.syx`, `tools/make_cz1_factory.py`, `tools/gen_cz1_factory.py` |
| Sequential Prophet-5/10 factory programs v1.03 (Sequential, 2025; <https://sequential.com/support/download/prophet-5-10-sounds/>): the 200 programs of `P5_Factory_Programs_USER_v1.03.syx`, unchanged native data, as PROPHET's factory presets and its default native user collection. Included as sound data for Prophet-5 compatibility; the rights stay with Sequential. See `assets/prophet5-factory/README.md` | Sequential's (no licence granted) | `assets/prophet5-factory/prophet5-v1.03.syx`, `tools/gen_prophet_factory.py` |
| Roland Juno-106 factory patches A11..B88 (Roland Corporation, 1984): the 128 factory patches as patch messages, decoded from two recordings of Roland's factory data cassette (sunshine-jones.com, hinzen.de) and checked against hinzen.de's patch file, as JUNO-106's presets; names from the factory list (synthmania.com). Included as sound data for Juno-106 compatibility; the rights stay with Roland. See `assets/j106-factory/README.md` | Roland's (no licence granted) | `assets/j106-factory/j106-factory.syx`, `tools/make_j106_factory.py`, `tools/gen_j106_factory.py` |
| klattsch by Tony Gies (<https://github.com/tgies/klattsch>): design reference for the VOICE engine; no code copied. Formant data from Klatt (1980) / Hillenbrand et al. (1995) | MIT (klattsch) | credit only |
| JieLi AC79 SDK by JieLi Technology: three external files go into every `.fwsc` package; the optional RAM recovery helper is in `assets/recovery/` | Apache-2.0 | `LICENSES/Apache-2.0.txt` |

On the device, HOME held > ABOUT opens the information screen; turning PRESETS scrolls on into
CREDITS, a short list of these authors, licences and source URLs.

## JieLi SDK files in the packages (Apache-2.0)

A `.fwsc` package made by `tools/build.py` (with `tools/fm1pkg_make.py`; this is the package the
web installer installs) holds three unmodified files from the JieLi AC79 SDK
(<https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK>, `cpu/wl82/tools/`). They are read from your SDK
checkout at build time (see BUILDING.md); those three SDK files are not in this tree.

| File in the package | What it is |
| --- | --- |
| `uboot.boot` | the first-stage boot loader (SPL) |
| `cfg_tool.bin` | the chip configuration block |
| `eq_cfg_hw.bin` | the default hardware EQ table |

They are licensed under the Apache License, Version 2.0 (`LICENSES/Apache-2.0.txt`), not under
the GPL, and their copyright stays with JieLi Technology. The SDK has no NOTICE file. Apache-2.0
files may be distributed together with GPL-3.0 code; Melodee itself stays GPL-3.0-only.

Every distribution of a package carries the licence texts with it: the release folder
(`tools/build.py --release`) and the site (`web/make_site.py`, next to the package in
`firmware/` and linked from the installer page).

## Standalone ROM recovery tool

The optional `fm1-recover` executables include `assets/recovery/wl82loader.bin`,
an unmodified RAM recovery helper from JieLi's AC79 SDK under Apache-2.0. Its
source version and checksum are recorded in `assets/recovery/README.md`; it is
loaded into RAM, not installed in flash. Copyright remains with JieLi Technology.
This SDK file is included in the source tree and the recovery distributions;
the three SDK files in firmware packages are still read from an external checkout.

Recovery archives carry Melodee's GPL-3.0-only licence, the SDK's Apache-2.0 licence,
and the bundled Python/PyUSB/PyInstaller/libusb notices. libusb is dynamically
loaded under LGPL-2.1-or-later; its corresponding source archive is included,
and `--libusb PATH` allows using a replacement library. PyInstaller's bootloader
exception permits distributing the generated executable under Melodee's licence.

## No vendor firmware material

No M-VAVE material is part of Melodee. The firmware links no vendor code and contains no vendor
data, and the packages hold only Melodee and the three SDK files above. The installer's
"Return to official V15" holds only the official file's size and SHA-256: you select the
official firmware file you downloaded yourself, and it is checked and installed in your browser,
never uploaded or redistributed.

## Contributions

Contributions are welcome under GPL-3.0-only.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.

"M-VAVE" and "FM-1" are trademarks of their respective owners. "Juno-106" and
"Roland" are trademarks of Roland Corporation; JUNO-106 names the instrument this
engine's tones are compatible with, and Saadet is not affiliated with Roland. Melodee is independent
firmware that runs on FM-1 hardware. It is not affiliated with, endorsed by or supported
by those owners, nor by Hügelton Instruments.

## Removed material

SID/reSID code, its measured waveform assets and generators, and PHASE/CrispyZebra code have been removed from the current tree. Their notices still apply to historical revisions that contain them. CZ-1 now has independent runtime state and reset handling around the BSD-3-Clause native renderer.

## Radio

Melodee never enables the Bluetooth / Wi-Fi radio of the hardware.


## Experimental base target (23 October 2026)

`platform/`, `tools/build_base.py` and `tools/base_overlay.py` create a separate
local fm1-base/felucca-bsp build. Dependency revisions and retained notices are
recorded in `docs/BLE_MIGRATION.md`. New files keep their stated per-file
licences and Kerem Kilic (Ellic Studio) attribution; included legacy modules
keep their actual licences and contributor notices. CPU1 and overload policy
adaptations identify their retained Saadet/Felucca provenance.

This target excludes the legacy ChoralRoot boot stub/update transaction and
`firmware/src/ota.c`. It still includes GPL app modules, including MIDI control
and clock code with ChanceTheMaker contributions. No complete source/assets
clearance or permission from those other contributors has been established.
The linked JieLi BLE SDK does not make that enclosing application MPL. The
experimental package is a private validation artifact, with distribution and
relicensing unresolved.

The optional recovery bootloader is **m-bootloader 1.2** by Julius Vitkauskas
(<https://github.com/jvitkauskas/m-bootloader>), under MIT. The pinned fm1-base
dependency provides its unmodified FM-1 installer and boot image. Base builds
copy these into `platform/build/bootloader/` with their full original MIT
licence and a source/checksum manifest. It is installed separately from Saadet;
its copyright and licence remain Julius Vitkauskas's. The Saadet preparation
and installation wrapper is new MPL-2.0 code by Kerem Kilic (Ellic Studio).
