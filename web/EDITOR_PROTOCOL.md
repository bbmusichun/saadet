# Saadet editor protocol

The running implementation is `firmware/src/editor.c` and its included native,
preference, library and backup handlers. The host fixtures in `tests/synth_test.c`
and `web/test_synth.mjs` exercise the actual capability and archive records.
Hardware protocol behavior has not been verified for Saadet.

Frames use `F0 7D 46 4C command arguments… F7`. All payload bytes are seven-bit
safe. Signed parameter values use two least-significant-first seven-bit bytes,
offset by 8192. Replies echo the command. WATCH enables parameter/reload pushes.

INFO identifies `SAADET`, one part, zero steps, synth marker `53 01` (hex), native
engine capabilities, and shared-library marker `55 01` with a two-byte total of
384 slots. Engine identities remain Prophet 19, FM6 12, CZ-1 15 and SID 3.
Reserved identities do not expose additional engines or parts.

| Commands | Purpose |
|---|---|
| 1–5, 8, 10 | INFO, parameter get/set/dump/descriptors, preset selection, factory names |
| 16–21 | Legacy seven-bit user-slot compatibility; use command 80 for the full library |
| 22–25 | WATCH, CHANGED, RELOAD and PING |
| 27–29, 31–32 | Single-part compatibility, restricted to part zero; no additional tracks |
| 34–38 | UI state/preferences/palettes and favorites |
| 65–67 | Saadet archive inventory, chunk read and staged restore |
| 68–71 | Native FM6 transfers |
| 72 | Audio diagnostics |
| 78 | Legacy native-slot compatibility |
| 80 | Shared 384-slot library |
| 81 | Raw battery ADC and full-charge reference |
| 75–77 | Native CZ transfers and legacy bank metadata where applicable |
| 95 | Native Prophet transfers |

Step, pattern, song, motion and project commands are rejected without a reply.
Sample uploading is unavailable. Clients must use the advertised capabilities.
The editor exposes only Sound, FM6, Library and Settings.

## Battery reference

Command 81 without arguments reads a fresh battery ADC sample. Arguments
`1, full_low7, full_high7` save a measured full-charge value (1–1023) in device
settings. Replies are `1, result, raw_low7, raw_high7, full_low7, full_high7`;
the leading 1 is the schema, and a zero stored full value means uncalibrated.
Result 0 confirms the request, 1 rejects arguments, 2 means no valid reading or
a write blocked by transport/old settings, 3 means applied in RAM but not saved,
and 4 means the settings write is queued. These values have no 8192 offset.
The full reference alone does not establish a battery percentage or an empty
reference. `tools/battery_calibrate.py --capture-full` samples the current
battery and saves the median, so use it only when the battery is fully charged.

## Shared sounds

Command 80 arguments begin with operation, slot low seven bits and slot high
bits. Replies echo operation and slot followed by a result code. Absolute slots
are 0–383; displayed slots are 1–384.

Operations are list (0, at most 16 entries), get (1), put (2), store current sound
(3), load (4), erase (5) and rename (6). Store/load accept only part zero.
Native payload kinds are FM6 (1, 128 bytes), CZ-1 (2, 144 bytes), and Prophet
(3, 138 bytes). Native data is grouped in seven-byte chunks preceded by the
chunk's high-bit mask. Legacy phrase fields in the numeric wire layout remain
reserved; no phrase is applied or recorded.

## Backups

Archives are `saadet-backup` version 1. The ordered object identities are
`1, 6, 7, 9–28, 32`: settings, 22 shared sound banks, and current synth state.
Object 28 is a sound bank; 32 is exclusively Saadet state. No project, sample,
pattern or song identities are accepted.

The inventory supplies object length and CRC32. Restore begins an object with
its expected length/CRC, transfers offset-checked chunks, then commits or cancels.
The firmware checks schema, native records, panel mapping and state checksum
before A/B flash publication. The editor validates the complete archive and
preflights all sizes before committing any object, restores sound banks first,
settings next, and current state last. Restore is atomic per object, rather than
across the whole archive. A USB reset, reused transfer scratch or timeout cancels
staged data. See `web/fm1backup.js` for the client-side framing and validation.

Saadet state uses `SDS1` and an internal size/FNV checksum. Its flash pair is
`0xA0000/0xA1000`, formerly part of Melodee project A; saving Saadet can replace
that project's data. Melodee backups and project files are not Saadet archives.
