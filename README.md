# Saadet

A local, single-part synthesizer fork of Melodee for the Felucca FM-1.

Saadet starts at Melodee `nightly` commit
`b5148ba` (the committed upstream tip fetched on 22 October 2026). The original
checkout and its uncommitted work are separate. `upstream` is read-only locally;
this repository has no publishing remote.

- One polyphonic synth part: Prophet, FM6, CZ-1 or SID.
- Three insert FX, three send FX, serial or separate send routing, and 27 keyboard punch effects.
- Arpeggiator, HOLD, chords, strum, scales, microtonal tuning, modulation and MIDI performance.
- Native sound editing, factory libraries, SysEx import/export and 384 shared user sound slots.
- Stereo USB playback and stereo post-FX capture at 44.1 kHz, with 16/24-bit USB formats.
  The internal/output precision is inherited from nightly; selecting PCM24 does not add synthesis precision.
- Automatic last-sound recall, device settings, complete Saadet backups, USB update and recovery.

There are no additional tracks, track selection, solo/multitrack mixer, drum engine,
sequencer, patterns, songs, recording, motion automation, projects, project templates,
MPC bank-to-track routing or USB stems. PLAY toggles arp HOLD; REC, SEQ and the
former track-selection encoder have no musical action.

HOME retains the four synth knobs, waveform/chord overlay and sound lane.
Tap HOME again for the chord wheel. FX opens the insert/send pages; hold or
lock FX for keyboard punch effects. GLO provides level, pan, spread and tempo.
The MIDI SYSTEM page has one receive channel (OMNI or 1–16) and one output channel.

Build and test instructions are in [BUILDING.md](BUILDING.md). Serve the repository
locally and open [the editor](web/editor.html) for Sound, FM6, Library and Settings.
The [manual](web/manual/index.html) describes the reduced controls.

## Storage and compatibility

Saadet stores its last complete synth state in an A/B pair at `0xA0000` and
`0xA1000`, formerly part of Melodee project A. Installing and saving Saadet can
therefore replace that project's data. Preset and settings identities remain in
place for the inherited 384-slot migration. Old library phrase bytes and unsupported legacy entries are inert. Reserved protocol/parameter numbers
remain reserved; they do not enable the removed features.

Saadet backups use `saadet-backup` version 1 and include settings, all 22 shared
sound banks and synth state object 32. Object 28 remains a preset bank. Project,
song, pattern and sample objects are rejected. Melodee project/backup files are
not Saadet backups. Settings retain an opaque legacy tail for compatibility,
without exposing project-template functions.

## Validation and provenance

The host suite exercises native engines, controls, MIDI ownership and repeats,
FX, power-loss recovery, archive round trips, USB transport and UI layout in
both fonts. The target build checks RAM placement and firmware packaging.
These checks do not establish physical audio quality, control behavior or
sustained buffer deadlines on an FM-1. No hardware installation is part of this fork's creation.

New Saadet/Melodee work is by Kerem Kilic (Ellic Studio). Retained Felucca work
is by Leo Kuroshita (@kurogedelic), Hügelton Instruments, with its notices kept.
The initial reduction was adapted from earlier Melodee synth-only work, then
updated against this nightly's native engines, FX, shared library and drawing code.
See [LICENSING.md](LICENSING.md), [LICENSE](LICENSE), and [LICENSES](LICENSES/)
for applicable upstream and third-party terms.
