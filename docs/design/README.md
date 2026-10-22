# Saadet interface

HOME uses the four permanent tone knobs, waveform/chord overlay and sound lane.
A second HOME tap opens the chord wheel. There are no track, pattern, song, drum,
recording or motion views. FX provides three inserts, three send slots and punch
controls. GLO has one output strip: level, pan, spread and tempo.

The shared header occupies rows 0–29. The tone controls occupy rows 32–79;
waveforms and native graphs use banded drawing to stay within the LCD canvas.
The layout suite checks both fonts and all palettes.

## Status indicators

Ongoing state belongs in the shared header, rows 0–29. Keep it visible for its
actual lifetime and remove it when the state ends. The left side shows clock
transport, BPM and octave; the right side shows the context or active punch FX.

Punch status takes priority over brief action feedback and the normal context.
Show the compact SLOOP name through its final wet fade; use FX HOLD / FX LOCK
when the layer is open, or FX MEMORY after buffered-effect allocation fails.
Clear on release, cancellation or layer close. These states are never toasts.

BAT! indicates the lowest battery range while external power is absent. It
appears beside tempo/octave and disappears immediately on recovery or power
connection. It participates in the header redraw signature.

Brief action feedback uses the header message area; a longer brief explanation
may use a toast. Decisions use dialogs. Values stay beside their controls.
Check both fonts, crowded headers and incremental clearing in
`tests/synth_ui_render.c` when changing indicators.
