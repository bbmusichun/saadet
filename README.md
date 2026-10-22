# Saadet

A local, single-part synthesizer fork of Melodee for the Felucca FM-1.

Saadet starts at Melodee `nightly` commit
`b5148ba` (the committed upstream tip fetched on 22 October 2026). The original
checkout and its uncommitted work are separate. `upstream` is read-only locally;
this repository has no publishing remote.

- One polyphonic synth part: Prophet, FM6, CZ-1 or SID.
- Three serial insert FX with independent wet/dry mix and tone controls.
- Arpeggiator, HOLD, chords, strum, scales, microtonal tuning, modulation and MIDI performance.
- Native sound editing, factory libraries, SysEx import/export and 384 shared user sound slots.
- Stereo USB playback and stereo post-FX capture at 44.1 kHz, with 16/24-bit USB formats.
  Native rendering and the common audio path use 24-bit precision; USB format selection is independent.
- Automatic last-sound recall, device settings, complete Saadet backups, USB update and recovery.

There are no additional tracks, track selection, solo/multitrack mixer, drum engine,
sequencer, patterns, songs, recording, motion automation, send buses, punch effects, projects, project templates,
MPC bank-to-track routing or USB stems.

The on-device interface has been removed ahead of a new design. Until it lands
the screen stays blank: the keys play the current sound, MASTER sets the volume
and the other buttons and knobs do nothing. Startup maintenance, the update
countdown (OCT- and OCT+ held for five seconds), UPDATE and the crash report still
draw. Sound editing, the library and settings are in the editor.

Build and test instructions are in [BUILDING.md](BUILDING.md). Serve the repository
locally and open [the editor](web/editor.html) for Sound, FM6, Library and Settings.
The [manual](web/manual/index.html) describes the instrument.

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
insert FX, native 24-bit transport, power-loss recovery, archive round trips, USB transport and the blank
panel. The target build checks RAM placement and firmware packaging.
These checks do not establish physical audio quality, control behavior or
sustained buffer deadlines on an FM-1. No hardware installation is part of this fork's creation.

New Saadet/Melodee work is by Kerem Kilic (Ellic Studio). Retained Felucca work
is by Leo Kuroshita (@kurogedelic), Hügelton Instruments, with its notices kept.
The initial reduction was adapted from earlier Melodee synth-only work, then
updated against this nightly's native engines, FX, shared library and drawing code.
See [LICENSING.md](LICENSING.md), [LICENSE](LICENSE), and [LICENSES](LICENSES/)
for applicable upstream and third-party terms.
