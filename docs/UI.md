# The Saadet panel

Saadet is one instrument with four engines: P5 (Prophet-5), FM6 (DX7), CZ-1 and J106 (JUNO-106). The screen takes on
the colour of the engine that is playing. Every button opens a place, and each place stays where you left it.

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
| HOME | HOME | The sound's name, engine and slot. Under the name, the chord you're playing and its notes (a slash chord when the lowest note isn't the root); after you let go it stays, faded, until you play again. Before you play anything it shows the key. A keyboard from C2 to B5 marks the scale, with the sounding notes lit. The status line shows the arp, chord keys and latch. Turn K1–K4 to change the macros. Hold HOME to list your favourites on the keys; pressing a key plays that favourite. |
| FX | LIBRARY | Lists: FAVOURITES, USER, P5, FM6, CZ-1, J106. K1 picks a sound (it loads at rest), K2 stars it as a favourite. User sounds also have K4 DELETE. |
| SEL | SCALE | Scale, root, key mode, shift; the MPC degree. |
| ENV | ENV | The engine's own envelopes (P5 filter and amp; FM6 pitch EG; the CZ-1's six 8-stage envelopes; the J106's one ADSR with its VCA mode, VCF amount and polarity), then the common MOD ENV. |
| LFO | LFO | The engine's own LFO (P5 LFO, FM LFO, CZ-1 vibrato, J106 LFO rate, delay and its DCO and VCF depths), the common LFO, and the four-route matrix. |
| EDIT | EDIT | The engine's own panel. P5: oscillators, mixer, filter, poly mod, wheel mod, play. FM6: voice and algorithm, OP1–OP6, macros, function settings. CZ-1: tone, detune, line 1 and line 2. J106: DCO waves and modulation, VCF cutoff and modulation (with the HPF), VCA mode, level and chorus. A value that has no effect on the sound as it stands is shown dim, but you can still edit it. |
| GLO | GLOBAL | Tempo, output, tuning, MIDI channels and thru, voice budget, display, erase everything. |
| SAVE | SAVE | Choose a slot, set the name letter by letter (CURSOR, LETTER), and star it if you like. Press SAVE again to store. |
| ARP | ARP | Pattern and feel. Its LED flashes with the arp's notes. |
| SEQ | CHORD | Chord, voicing, strum. |
| PLAY | PLAY | Voice mode, glide, priority, unison. |
| REC | (latch) | Holds the notes you let go of until you play the next chord, or until you press REC again. With the arp on, it holds the arp's notes instead. |

## Bluetooth (experimental platform build)

Saadet can connect to one external Bluetooth MIDI device while also accepting
one phone/computer connection. Both links support MIDI input and output; keys
played on Saadet go to both connected destinations. USB remains available.

**GLOBAL > DEV** scans while that page is open. Put the external keyboard,
controller or synth in its Bluetooth MIDI pairing/advertising mode. K1 DEVICE
selects a discovered device; SAVE or a clockwise turn of K4 LINK connects it.
The page shows CONNECTING, FINDING MIDI, then CONNECTED. SAVE/K4 disconnects
an active or pending connection. Leaving the page stops scanning, but keeps an
established connection. Up to eight discoveries can be selected, with three
visible rows that scroll with the choice. This is BLE MIDI only, not Bluetooth
audio or arbitrary Bluetooth accessories.

**GLOBAL > BLE** manages the phone/computer link. K1 RADIO switches the whole
radio off/on for this session. K2 DROP disconnects only the phone/computer;
K4 RETRY drops a stalled phone link and resumes advertising when available.
On Mac, connect to Saadet in Audio MIDI Setup > Window > Show MIDI Studio >
Bluetooth. On iPhone/iPad, use a music app's Bluetooth MIDI settings.
WAITING FOR MIDI means the link is up but the host has not enabled MIDI:
on Mac, quit and reopen Audio MIDI Setup, turn RETRY, then Connect again.

To forget saved bonds, turn **K3 FORGET clockwise** on GLOBAL > BLE, then
press **SAVE** while FORGET ALL PAIRINGS? is displayed. This disconnects both
BLE links and clears **all** saved BLE pairings on Saadet, preserving radio
address/calibration and sounds. Turn K3 back, leave the page, or wait 15 seconds
to cancel before confirmation. FORGET FAILED means the operation did not
complete; PAIRINGS CLEARED means the SDK accepted the clear. Forget Saadet on
the other device as well if it still remembers the old bond, then reconnect.

Saadet accepts optional Just Works pairing. Like Charles's CTL firmware, its
outgoing MIDI client does not force a PIN/pairing request on devices that do
not require one. It does not automatically reconnect an external device at
boot; select and connect it from DEV.

## Importing SysEx

Send a dump to Saadet over USB-MIDI: a DX7 voice or a 32-voice bank, a CZ-1 tone, a Prophet-5 program, or a Saadet
user preset. Saadet opens SAVE and shows the sounds and the slots they will go into. Turn K1 to pick the first slot,
then press SAVE to import. If you import a single sound, it starts playing right away. Opening another place discards
the import. Nothing is written until you press SAVE.

## Removed

The insert effects are gone. Sounds that were saved with effect settings still load, but those settings are ignored.

## Deleting a sound

In any LIBRARY list, select a user sound and turn K4 DELETE up. A white header asks to delete its slot, and the FX
button breathes. Tap FX to delete. Turn K4 back, move SOUND, change page, press another button, or wait three seconds
to cancel. Factory sounds have no DELETE control.

Deleting clears the saved slot and its favourite. The sound playing keeps playing with its current edits, without
a saved slot; the next sound in the list does not load automatically. A failed write shows DELETE ERROR and keeps
the record. There is no undo.

## Erasing everything

Go to the last GLOBAL page, ERASE. Turn K1 ERASE ALL up, then hold GLO for three seconds. The countdown replaces
the page list. Letting go cancels without turning the page; turn K1 back or leave the page to cancel the armed
state. A GLO press that began before arming never counts toward confirmation.

The device shows progress while erasing all user sounds, favourites, stored engine banks, settings, panel
calibration and the sound restored at power-on. Keep power connected until it finishes. It then restarts with
defaults. Built-in factory sounds and the firmware stay. If this FM-1 required hardware calibration, calibrate
it again by holding OCT- and OCT+ at power-on. Erasing has no undo.

## Battery reference

`tools/battery_calibrate.py --capture-full` reads 31 battery ADC samples over USB-MIDI and saves their median as
this device's full-charge reference. Run it with the battery fully charged. The reference is stored with device
settings and included in backups; erase everything clears it. It does not produce a percentage until the empty
reference has also been measured. The command requires firmware with battery calibration support.


## Experimental base target

On the separate fm1-base target, GLOBAL > TEMPO's CLOCK choices are INT, USB,
TRS and BLE. BLE is a MIDI peripheral advertised as **Saadet** and starts
through the base's boot/crash guard. Physical keyboard notes are also sent to
connected USB/BLE hosts.

**GLOBAL > MIDI K3 THRU** (device setting, default OFF) also sends incoming
MIDI out again: USB to BLE, TRS to USB and BLE, and either BLE link to USB.
BLE input does not go back out over BLE, because the base sends BLE output to
the phone and the external device together and would echo it to its source.
When a BLE link drops or THRU is turned off, notes and sustain it passed on
are released at their destination. When an out queue is nearly full,
pressure, pitch bend and continuous controllers are dropped first. SysEx is
not passed through. The native editor and DX7/CZ bank imports use USB.
See `BLE_MIGRATION.md` for current limitations and unmeasured hardware checks.
