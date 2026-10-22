# The Saadet panel

Saadet is one instrument with four engines: P5 (Prophet-5), FM6 (DX7), CZ-1 and SID. The screen takes on the colour of
the engine that is playing. Every button opens a place, and each place stays where you left it.

## Controls

| Control | What it does |
| --- | --- |
| VOLUME | Master level. |
| SELECT | Turns the page of the place on the screen. |
| PRESETS | Browses the engine's sounds: its factory sounds, then your sounds on it. The sound loads when the knob rests. |
| ALGORITHM | Browses the next or previous engine's sounds. |
| K1–K4 | Edit the four values at the foot of the screen. On HOME they are the sound's four macros. |
| OCT- / OCT+ | Shift the keyboard by an octave. Press both to return to 0. Hold both for 5 s to enter update mode. |

Tap a place's button again to turn its page. Hold a button to list what is inside it: the list also lights up on the
keys, and pressing a key jumps to that entry. While a button is held, the keys do not play notes.

## Places

| Button | Place | Inside |
| --- | --- | --- |
| HOME | HOME | The sound's name, engine and slot, plus what is shaping the notes (arp, scale, chord, latch). Turn K1–K4 to change the macros. Hold HOME to list your favourites on the keys; pressing a key plays that favourite. |
| FX | LIBRARY | Lists: FAVOURITES, USER, P5, FM6, CZ-1, SID. K1 picks a sound (it loads at rest), K2 stars it as a favourite. |
| SEL | SCALE | Scale, root, key mode, shift; the MPC degree. |
| ENV | ENV | The engine's own envelopes (P5 filter and amp; FM6 pitch EG; the CZ-1's six 8-stage envelopes), then the common MOD ENV. On SID, the common envelope is the amp. |
| LFO | LFO | The engine's own LFO (P5 LFO, FM LFO, CZ-1 vibrato), the common LFO, and the four-route matrix. |
| EDIT | EDIT | The engine's own panel. P5: oscillators, mixer, filter, poly mod, wheel mod, play. FM6: voice and algorithm, OP1–OP6, macros, function settings. CZ-1: tone, detune, line 1 and line 2. SID: oscillator and filter. A value that has no effect on the sound as it stands is shown dim, but you can still edit it. |
| GLO | GLOBAL | Tempo, output, tuning, MIDI channels, voice budget, display. |
| SAVE | SAVE | Choose a slot, set the name letter by letter (CURSOR, LETTER), and star it if you like. Press SAVE again to store. |
| ARP | ARP | Pattern and feel. Its LED flashes with the arp's notes. |
| SEQ | CHORD | Chord, voicing, strum. |
| PLAY | PLAY | Voice mode, glide, priority, unison. |
| REC | (latch) | Holds the notes you let go of until you play the next chord, or until you press REC again. With the arp on, it holds the arp's notes instead. |

## Importing SysEx

Send a dump to Saadet over USB-MIDI: a DX7 voice or a 32-voice bank, a CZ-1 tone, a Prophet-5 program, or a Saadet
user preset. Saadet opens SAVE and shows the sounds and the slots they will go into. Turn K1 to pick the first slot,
then press SAVE to import. If you import a single sound, it starts playing right away. Opening another place discards
the import. Nothing is written until you press SAVE.

## Removed

The insert effects are gone. Sounds that were saved with effect settings still load, but those settings are ignored.
