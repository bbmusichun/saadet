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

The interface is built for one instrument. Every screen shares a shell: the header
shows the tempo, octave shift, arp HOLD and low battery at the left and the sound
(HOME: its engine) at the right; a rail at the foot names the page and the pages
SELECT turns to. HOME shows the four tone knobs, the sound's name and source, a
scope with the held chord, and chips for scale, chord keys, arp and inserts; tap
HOME again for the chord wheel. SELECT sets the tempo on HOME. EDIT opens the
engine's own panel in sections, ENV and LFO its envelopes and modulation, SEQ its
voice settings (polyphony, unison, glide, bend). FX opens the three-slot insert
rack; the keys keep playing while FX is held. GLO holds output, tuning, MIDI and
the voice budget. PLAY toggles arp HOLD. REC and the former track encoder have no action.
The MIDI page has one receive channel (OMNI or 1–16) and one output channel.

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
insert FX, native 24-bit transport, power-loss recovery, archive round trips, USB transport and UI layout in
both fonts. The target build checks RAM placement and firmware packaging.
These checks do not establish physical audio quality, control behavior or
sustained buffer deadlines on an FM-1. No hardware installation is part of this fork's creation.

New Saadet/Melodee work is by Kerem Kilic (Ellic Studio). Retained Felucca work
is by Leo Kuroshita (@kurogedelic), Hügelton Instruments, with its notices kept.
The initial reduction was adapted from earlier Melodee synth-only work, then
updated against this nightly's native engines, FX, shared library and drawing code.
See [LICENSING.md](LICENSING.md), [LICENSE](LICENSE), and [LICENSES](LICENSES/)
for applicable upstream and third-party terms.
