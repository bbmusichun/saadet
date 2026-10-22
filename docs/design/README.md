# Saadet interface

One instrument, one sound. Every screen uses the same shell on the 240 × 240 LCD:

- **Header**, rows 0–29: the instrument's state. Left: clock (stopped/running),
  tempo, then only what is on now — octave shift and arp HOLD as outlined pills,
  BAT! while the battery runs low. Right: what is played — HOME the engine, a page
  the sound, the section map its engine. Messages and held layers take the right
  side while they last.
- **Knob strip**, rows 32–79: four cells for KNOB 1–4, label over value; the knob
  just turned is drawn in its colour with a bar under it.
- **Panel**, rows 84–207: the page's picture — what the page shapes. Envelopes
  (track ADSR, Prophet filter/amp, CZ-1 eight-step, FM6 EG), LFO shapes, filter
  responses, FM6 algorithms, arp notes, FX descriptions; otherwise the live scope.
- **Rail**, rows 212–239: the page's name in a theme pill, the pages SELECT turns
  to at either side (sentence case, each only when it fits whole). Lists, slots,
  the MOD routes and the FX rack end above it. There is no track numeral anywhere.

HOME replaces the panel with the sound (name, source, favourite), a scope with the
held chord and notes (or the shape of the knob turning), and chips at the foot for
the scale, chord keys, arp and active inserts — lit while they act. A second HOME
tap opens the chord wheel and keys.

Buttons open families; ENV, LFO and SEQ go first to the engine's own pages.
EDIT is the engine's panel in sections (PRESETS jumps a section, EDIT again opens
the map); Store and Tools pages come last. SEQ is VOICE (polyphony, unison, glide,
bend). GLO is OUTPUT, TUNING, MIDI, VOICES and WEIGHTS. SELECT turns pages, and on
HOME the tempo. There are no track, pattern, song, drum, recording or motion views.

The layout suite (`tests/synth_ui_render.c`) checks every page of every engine
in both fonts and all palettes.

## Status indicators

Ongoing state belongs in the shared header, rows 0–29. Keep it visible for its
actual lifetime and remove it when the state ends. The left side shows clock
transport, BPM, octave and HOLD; the right side shows the sound or engine, or the
active quick layer.
There is no punch layer, send rack or send-routing status.

BAT! indicates the lowest battery range while external power is absent. It
appears beside tempo/octave and disappears immediately on recovery or power
connection. It participates in the header redraw signature.

Brief action feedback uses the header message area; a longer brief explanation
may use a toast. Decisions use dialogs. Values stay beside their controls.
Check both fonts, crowded headers and incremental clearing in
`tests/synth_ui_render.c` when changing indicators.
