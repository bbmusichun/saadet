# Saadet sound designer

Open SOUND DESIGNER from the device menu. Start from the current sound or an
initialized sound and choose a goal such as bass, pad, lead, keys, pluck or FX.
Features selects the engine's audible building blocks; SELECT follows the
applicable expert pages and PRESETS jumps between stages. The keyboard remains
playable while editing. Native Prophet, FM6 and CZ-1 data stay in their original
formats; SID uses its normal sound controls.

SAVE compares the draft and original. Leaving offers Keep the sound, Save as
user, or Discard. Save as user opens the shared 384-slot library and its naming
screen. Discard restores the exact original sound, including its full user-slot
identity. Drafts do not enter automatic last-sound recall before acceptance.

`tests/designer_test.c` drives recipes, feature dependencies, comparison,
accept/discard, naming and page coverage through the actual panel/UI code and
renders their output. These are host tests, not physical control/audio validation.
