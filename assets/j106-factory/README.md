# Roland Juno-106 factory patches

`j106-factory.syx` holds Roland's 128 Juno-106 factory patches (A11 BRASS to B88 OWGAN) as the
instrument's patch messages: 128 messages `F0 41 30 00 pp`, sixteen slider bytes, SW1, SW2, `F7`
(24 bytes; `pp` is the program, 0–127 for A11–B88), the format the web editor imports and exports.
`tools/gen_j106_factory.py` checks it and turns it into `build/gen/melodee_j106.h` at build time:
JUNO-106's presets 1–128, after INIT TONE.

SHA-256: `635c40b1003b7ea7f32266fb201623d86b199785b4aa17117a90c3a5672e93f3`.

The patches are Roland's (Juno-106, Roland Corporation, 1984). They are included as sound data for
compatibility with the instrument; no licence is granted for them by this project. Factory patch data
remains attributed to Roland; the software licences (GPL-3.0-only, and MPL-2.0 for the JUNO-106 engine)
do not relicense this data.

## Where they come from

The Juno-106 keeps its patches in battery-backed memory; Roland supplied the factory bank on a data
cassette. `tools/make_j106_factory.py` decodes two recordings of that cassette and checks them against
each other and against a third, independent copy (all downloaded 23 October 2026):

1. Sunshine Jones, "Roland Factory Data Cassettes" (<https://sunshine-jones.com/roland-factory-data-cassettes/>):
   `JUNO-106.zip` (<https://sunshine-jones.com/wp-content/uploads/2020/05/JUNO-106.zip>, SHA-256
   `386261d8614f13f78cca9effa9e4f303fe00ab45b8961d1b6396a5203026a17f`), recordings of bank A and bank B
   (8-bit, 22.05 kHz, files dated 1998).
2. Edy Hinzen, JUNO-106 CONNECTION (<http://www.hinzen.de/midi/juno-106/>): `junot020.wav` (SHA-256
   `0c5d2e93dc98a88ebc66920aa8b1ff805aefeb17771219b9e7b4b06f6b8b8bc3`), a recording of bank A
   (8-bit, 11.025 kHz), and `Factory_Patches.pat` (SHA-256
   `d9579f1cbb682022de3b7dd9582eeede14b38db64766808f89ecc7578959ecd1`), the 128 patches as
   patch-message bytes, named by ear by the page's author ("Trumpet 1", "Xylophone", "Melodic
   Helicopter"; unrecognised slots "Fact. A27", "Fact. A48" and so on).

The cassette format is not published by Roland; the script reads it as follows. Every half cycle is a
bit, about 600 µs a 1 and about 180 µs a 0, after a leader of 1s. Bytes are framed as a start bit 0,
eight bits LSB first and a stop bit 1. A bank is `4A 55`, 64 records of 16 bytes and the 8-bit sum of
everything before it. A record's low seven bits are the sixteen sliders; bit 7 of record bytes 0–7 and
8–15 are SW1 and SW2, bit 0 first. 23 records set SW1 bit 7, which a patch message's seven-bit data
bytes cannot carry; it is not kept, and its meaning is not known here.

Every recording passes its checksum. The two bank A recordings are identical in 62 of 64 records. At A74
and A88 the Sunshine Jones bank differs: its A74 is a byte-for-byte copy of its A82 (RESONANCE FUNK), so
that bank had been edited, and `junot020.wav` is used for both slots. All 128 patches then equal
`Factory_Patches.pat`. Bank B therefore rests on the Sunshine Jones recording and the hinzen.de file,
and A74 and A88 on hinzen.de's recording and file. `junot000.wav` from the same page holds one tone 64
times and is not used. The jarvik7 Juno-106 Librarian's factory bank agrees, but names A11 "Brass Set 1"
as `Factory_Patches.pat` does, so it is not counted as a separate source. Roland's own "JUNO-106: The
Original 128" (Roland Cloud) was not available for comparison.

    tools/make_j106_factory.py JUNO-106.zip junot020.wav Factory_Patches.pat

## Names

The instrument stores no names. These are the factory list as given by synthmania.com
(<http://www.synthmania.com/juno-106.htm>) and mirrored by hinzen.de, with "Pizzicato" and "Tremolo"
spelt so, uppercased and, where longer than 12 characters, shortened (`SHORT` in
`tools/gen_j106_factory.py`). The list's playing notes are not part of the names: CELESTE and CLOCK
CHIMES "1 oct. up", DRUM BOOMS "1 oct. down", GONG "play low chords", and SYN BASS I, SNARE DRUM,
TOM TOMS and TIMPANI "(unison)" (a key mode of the instrument, not stored in a patch).

| | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1 | BRASS | BRASS SWELL | TRUMPET | FLUTES | MOVING STRGS | BRASS&STRGS | CHOIR | PIANO I |
| A2 | ORGAN I | ORGAN II | COMBO ORGAN | CALLIOPE | DONALD PLUCK | CELESTE | E.PIANO I | E.PIANO II |
| A3 | CLOCK CHIMES | STEEL DRUMS | XYLOPHONE | BRASS III | FANFARE | STRINGS III | PIZZICATO | HIGH STRINGS |
| A4 | B.CLARINET | ENGLISH HORN | BRASS ENSMBL | GUITAR | KOTO | DARK PLUCK | FUNKY I | SYN BASS I |
| A5 | LEAD I | LEAD II | LEAD III | FUNKY II | SYN BASS II | FUNKY III | THUD WAH | GOING UP |
| A6 | PIANO II | CLAV. | FRONTIER ORG | SNARE DRUM | TOM TOMS | TIMPANI | SHAKER | SYNTH PAD |
| A7 | SWEEP I | PLUCK SWEEP | REPEATER | SWEEP II | PLUCK BELL | DK SYN PIANO | SUSTAINER | WAH RELEASE |
| A8 | GONG | RESO FUNK | DRUM BOOMS | DUST STORM | ROCKET MEN | HAND CLAPS | F X SWEEP | CAVERNS |
| B1 | STRINGS | VIOLIN | CHORUS VIBES | ORGAN 1 | HARPSICHRD 1 | RECORDER | PERC. PLUCK | NOISE SWEEP |
| B2 | SPACE CHIMES | NYLON GUITAR | ORCH PAD | BRIGHT PLUCK | ORGAN BELL | ACCORDION | FX RISE 1 | FX RISE 2 |
| B3 | BRASS | HELICOPTER | LUTE | CHORUS FUNK | TOMITA | FX SWEEP 1 | SHARP REED | BASS PLUCK |
| B4 | RESO RISE | HARPSICHRD 2 | DARK ENSMBL | CONTACT WAH | NOISE SWEEP2 | GLASSY WAH | PHASE ENSMBL | CHORUSD BELL |
| B5 | CLAV | ORGAN 2 | BASSOON | AUTO REL SWP | BRASS ENSMBL | ETHEREAL | CHORUS BELL2 | BLIZZARD |
| B6 | E.PIANO TREM | CLARINET | THUNDER | REEDY ORGAN | FLUTE / HORN | TOY RHODES | SURF'S UP | OW BASS |
| B7 | PICCOLO | MELODIC TAPS | MEOW BRASS | VIOLIN HIGH | HIGH BELLS | ROLLING WAH | PING BELL | BRASSY ORGAN |
| B8 | LOW DK STRGS | PICCOLO TPT | CELLO | HIGH STRINGS | ROCKET MEN | FORBIDDEN PL | FROGGY | OWGAN |
