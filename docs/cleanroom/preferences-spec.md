# Settings, favourites and editor compatibility specification

23 October 2026. Behaviour and binary interfaces only. This specification is for a fresh implementer replacing
four files, without access to their earlier implementations or repository history. All other tree files may be
used for the interfaces they expose. Preserve existing tests and audio/control behaviour.

## Scope and rules

Write `firmware/src/favorites.c`, `settings_persist.c`, `editor_preferences.c` and `scratch.h` independently.
Each new file is MPL-2.0, Copyright (C) 2026 Kerem Kilic (Ellic Studio). Do not retain implementation text,
comments or notices from an implementation you have not been given. Ordinary interface declarations and binary
format facts below are requirements, not implementation suggestions. Use your own structure and naming for
private helpers. You may improve factoring within these files. Do not change the rest of the firmware to evade
compatibility tests. Do not read outside the supplied tree, use git, or fetch other Saadet/Melodee/Felucca/SLOOP
source code. Do not read the parent agent's conversation or scratch files.

This file records the original handoff. After the implementation and initial verification, the user retired
the legacy firmware target. Its package-dependent OTA fixture below is no longer part of current validation;
the shipping updater is checked in `tests/run_base_tests.sh`. See preferences-process.md for the final workflow.

## Favourites interface and storage

Expose `MELODEE_FAVORITES` as 1, `favorites_t`, static `favorites`, and static uint32 `favorites_user_hi`.
`favorites_t` has `factory` (16 rows of 32 uint8 bytes), `user` and `filter` (uint32), in that order.
Existing callers require `favorite_has(uint32 engine, uint32 preset)` and
`favorite_set(uint32 engine, uint32 preset, int on)` returning int. Queries return boolean. Setters normalize
`on` to boolean and return whether a change was made. Out-of-range requests do not change anything.

- Prophet factory namespace ENGI_PROPHET delegates to `p5_favorite_has(preset,1)` and
  `p5_favorite_set(preset,on,1)` (declared here, implemented later in the translation unit).
- JUNO-106 factory namespace ENGI_J106 uses settings row 3: first 28 bytes are 224 little-bit-order flags,
  bytes 28..31 identify the format with ASCII J, 6, F and version 2. Expose `J106_STAR_CAP` as 224.
  Unrecognized tags make all queries false. A write that changes a flag first initializes the flag area
  and current tag. Turning off an already false flag does not initialize anything. Old version 1 is ignored.
- USER_GENERAL and USER_NATIVE_P5/FM/CZ/J106 all address the shared user collection via `up_favorite`
  and `up_favorite_set`. Its capacity is UP_SLOTS. Retain the public legacy user words for persistence and
  bank migration; current user stars themselves belong to the shared preset module.
- Other namespaces are raw factory rows when engine is below both NENGINES and 16 and preset below 256.
  A flag's byte is its index divided by eight, least-significant bit first. No unbounded indexing.
- `scale_favorite(uint32)` and `scale_favorite_set(uint32,int)` use 96 flags (`SCALE_FAV_CAP`) in row 14,
  bytes 16..27. Bytes 28..31 are ASCII S,C,L, version 1. Untagged flags are false; first effective write
  initializes only bytes 16..31 and preserves bytes 0..15. Redundant writes return zero without modifying tags.

## Persisted settings ABI

Include `recording_preferences.h`. Expose `persist_t` with these fields in order:

| Field | Type |
| --- | --- |
| magic, palette, lowcut, zoom | uint32 each |
| panel | panel_t |
| bold | uint32 |
| favorites | structure: uint8 factory[16][32], uint32 user, uint32 filter |
| ext | structure: uint32 usb_off, uint32 boot, int16 glo[4], uint32 drumch, uint32 lights, uint32 spare[2] |

The whole record is 604 bytes; ext is 32 bytes. Preserve this layout and unknown/reserved bytes on export.
Expose static `settings_glo` (four int16), `settings_import(persist_t*,int)` (int),
`settings_export(persist_t*)` (void), `battery_full_of(const persist_t*)` (uint16),
`settings_owned(const persist_t*)` and `settings_legacy_valid(const persist_t*)` (int).
Import rejects any length other than 604 or magic other than PERSIST_MAGIC (0x50455235), without changing
runtime state. It normalizes some fields in its argument as described below. Export updates an existing record;
it must not zero the entire record or overwrite another feature's fields.

Wire constants required by callers/tests: A4_TAG=0x41340000, AUTOSAVE_OFF_TAG=0xA1,
WORK_BUDGET_TAG=0x57420000, FX_PREF_ROW=1 and FX_PREF_BYTE=24. Other tags in this section are binary facts;
their identifiers are an implementation choice unless another file references them.

- palette: use `palette_from_stored`/`palette_to_stored`. Import canonicalizes the stored field. Set runtime
  settings magic to SETTINGS_MAGIC, palette to the decoded index, lowcut to zero. Export lowcut as zero.
- zoom low byte: runtime true only for value 1. High half tag 0x5250 denotes recording preferences in byte 1:
  bits 0..1 click (0..2, 3 becomes 0); 2..3 level (1 means quiet/0, 2 loud/2, otherwise medium/1);
  4..5 count-in (0..2, 3 becomes 0); bit 6 preview; bit 7 chord-add. Without tag use defaults encoded by zero.
  Export click and count-in modulo 3, level with the above mapping, and the boolean flags. Omit the tag if
  all preference bits are zero; retain runtime zoom in the low byte.
- panel: import the saved panel when its magic is PANEL_MAGIC; otherwise PANEL_DEFAULT. Detailed calibration
  validation belongs to existing `panel_init`. Export runtime panel.
- bold: use `hold_from_stored` and `hold_to_stored(old_bold,settings_hold)`, preserving legacy bold 0/1.
- when MELODEE_FAVORITES is defined, import/export the full favourites block and spare[0] as favorites_user_hi.
  Runtime filter is true only when stored filter equals 1. Preserve the saved filter field on import.
  When favourites are not compiled, export must preserve the existing block.
- every export writes owner tag ASCII M,D,S, version 1 at row 1 bytes 28..31. `settings_owned` tests this tag.
- battery reference: row 1 bytes 24..27 are ASCII B, version 1, uint16 little-endian full-charge ADC.
  Values 1..1023 are valid; any other tag/value means zero. Export clears these four bytes then writes
  the battery tag/value only for valid runtime settings_batt_full. This also clears the retired FQ format.
- usb_off: low two bits retain USB-device flags. Bits 16..31 equal 0x5742 for custom workload budget, stored
  in bits 8..15 (8..32). Import budget uses that tag/range; otherwise VOICE_BUDGET_DEFAULT. USB flags are
  accepted only when the entire word is 0..3, or all bits except the budget byte and low two flags exactly
  equal WORK_BUDGET_TAG. Normalize the argument to valid flags plus a tag/budget only when nondefault.
  With MELODEE_USB_AUDIO set, import sets ua_off and ua_off_want to flags, and export obtains flags from ua_off.
  Without USB audio, export preserves the existing low two bits. Export runtime custom budget above them.
- boot: 0..4 accepted; other values normalize stored/runtime to 0. Export settings_boot.
- glo: four saved int16 values copied verbatim both ways.
- drumch: saved 0 means runtime 10; saved 17 means runtime 0/OFF; saved 1..16 their channels. Saved >17
  normalizes to 0 (runtime 10). Export is the inverse mapping.
- lights: saved 1..LIGHTS_N maps to runtime value minus 1; all other values mean LIGHTS_MID. Export MID as
  zero and other runtime levels as level plus one.
- spare[1]: A4 tag in the high half, frequency in the low half. Import accepts A4_MIN..A4_MAX; otherwise
  A4_DEFAULT. Export default as zero, other runtime values as tag plus frequency.
- row 15 byte 27: settings_screen, accepted when below 8, else zero. Export current value.
- row 15 byte 28: only AUTOSAVE_OFF_TAG disables settings_autosave. Export zero for ON, that tag for OFF.
- row 15 byte 24 bit 6: settings_thru. Export changes only that bit, preserving all others.
- row 8 bytes 8..15: ASCII W, version 1, five voice-cost bytes, final flags byte. With valid tag, costs
  0..15 accepted (invalid individual values become zero); final bit 0 CLEAR means safety enabled. Untagged
  means all costs and safety zero. Export clears all eight bytes and writes tag, costs and inverse-safety
  bit 0 only if safety or any custom cost is nonzero.

`settings_legacy_valid` recognizes a complete old record without requiring an owner tag: correct record magic,
tagged-valid palette, PANEL_MAGIC, lowcut <=2, zoom low byte <=1 and either no upper data (word <=255) or
recording tag 0x5250; valid hold storage; filter <=1; valid USB flags with optional budget tag/range; boot <=4;
drumch <=17; lights <=LIGHTS_N. It validates only; does not normalize or mutate.

## Editor preferences protocol

Use existing `ed_b`, `ed_v`, `ed_str`, `ed_rv`, and framing in editor.c. Implement `ed_ui_caps()` and
`ed_ui_handle(uint32 cmd,const uint8_t *args,uint32 count)` returning int. Handle commands 34..38 and ED_BATTERY
(81); return zero for unrelated commands. Invalid recognized commands still return a reply as described below.
Capabilities: palette=1, performance=32, plus monitor=4 if MELODEE_MONITOR and favourites=8 if
MELODEE_FAVORITES; retired font=2 and recording=16 are not advertised. Keep capability enum names
ED_UI_PALETTE/FONT/MONITOR/FAVORITES/RECORDING/PERFORMANCE for callers.

State payload, in order: caps, palette, unsupported font (127), monitor or 127, filter or 127,
28-bit favourite signature (four 7-bit bytes, least significant first), up_gen encoded the same way,
settings_click, settings_click_level, settings_countin, settings_preview, settings_chord_add,
retired preference 127, scr_get(). Signature is FNV-1a over the bytes of favourites then favorites_user_hi,
starting with 2166136261, followed by one FNV xor/multiply step using up_gen as a whole uint32; transmit low
28 bits. Without favourites begin signature at zero before the up_gen step.

- UI_STATE (34): state payload.
- UI_PALETTES (36): count NPALETTES, then every palette name as NUL-terminated string limited to 12 chars.
- UI_SET (35): require exactly two bytes (id,value), id <=10. Validate capability first, then value.
  IDs 0..3 require corresponding capability bit; 4..8 recording capability; 9..10 performance capability.
  Thus font and recording fields answer unsupported (2). ID 9 is retired and unsupported after range check.
  Ranges: palette <NPALETTES; monitor id 2 and ids 4..6 <3; id10 <5; other values <2.
  Invalid arguments/range give 1. Supported actions: palette assigns settings.palette and calls palette_set;
  filter assigns favourites.filter; id10 calls scr_put. Monitor, if compiled, updates settings.monitor,
  monitor_mode and invalidates monitor_event under IRQ disable/enable. After supported updates ui.force=1,
  use save status below. Reply status, id (127 if missing), value (127 if missing), then state payload.
- Save status: call settings_save. With no flash feature, status 3. With flash: unavailable flash or pending
  retry state 2 ->3; queued pending state 1 ->4. Otherwise export current record and confirm zoom, decoded
  palette, optional monitor, and whole favourites block match persist_saved: mismatch ->3, match ->0.
  Repeated identical writes must not erase again (existing settings path handles this). A protected old
  template or active transport can prevent saving, retaining existing queuing semantics.
- FAV_GET (37): favourites feature absent ->2. Otherwise require four bytes: namespace, signed-offset-8192
  14-bit start via ed_rv, count 1..32. Namespace <=USER_NATIVE_J106; range must fit native_limit(mapped
  engine), UP_SLOTS for USER_GENERAL, or ENGINES[namespace]->npresets. Error ->1 alone. Success ->0,
  namespace, ed_v(start), count, then one boolean per flag.
- FAV_SET (38): same four-byte format with boolean instead of count; validate nonnegative index, namespace
  and capacity. Starring empty USER_GENERAL or native slots is invalid (1); clearing an empty slot is allowed.
  Capture old state and call favorite_set. For Prophet/JUNO factory or native namespaces, if setter reports no
  change while old state differs from requested, report 4 when transport_busy else 3, with namespace/index and
  OLD boolean. Otherwise ui.force=1 and return save status with namespace/index/requested boolean; native
  Prophet/JUNO changes return status 0 because the shared-record setter owns their persistence. Other native
  namespaces follow normal settings save status. Invalid requests give 1 alone; feature absent ->2 alone.

Battery command (81) reply: version 1, status, raw ADC in two 7-bit bytes, full reference in two 7-bit bytes.
Raw ADC is fm1_adc_read(FM1_ADC_BATT) on target FM1_IRQ_TARGET, song.batt_raw on host. No arguments queries.
Write requires exactly three bytes: action 1, 14-bit full value 1..1023. Invalid ->1; busy transport or protected
old template ->2 without applying. Valid updates settings_batt_full and uses save status above, additionally
checking saved battery value when save says success. Invalid raw ADC (<=0 or >1023) is reported as zero,
and makes an otherwise successful status 2. Never invent a battery percentage.

## Main-loop workspace

`scratch.h` is safely includable multiple times and includes stdint types. Expose static `main_scratch` with
byte member st[256] and dec[640]; in non-MELODEE_LOADER builds also int16 snap[512] and byte transfer[256].
Members alias one allocation, aligned to at least four bytes. Size is 1024 bytes normally, 640 with the loader.
These are synchronous main-loop borrows only; neither ISR nor DMA nor CPU1 retains a reference across a call.
Preserve these constraints and the existing callers. There is no need to retain the original union's spelling,
member ordering, helper comments, or include guard name.

## Verification

Use existing generated build/gen and compile tests with cc -O2 -DAUDIO_FRAC_BITS=0 -w -Ibuild/gen -Ifirmware/src,
link -lm -pthread. Run settings_test, synth_test, j106_test, ui_test, ui_erase_test, storage_test, ota_test (last
needs a package; parent handles it if absent). Also compile/run synth_test at native default precision. Parent
runs all build variants and complete suites, independent compatibility oracles and licensing checks.
