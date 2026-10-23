#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Additional invariants specific to Saadet's base port, after upstream checks."""
from pathlib import Path
import hashlib
import json
import re
import sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'platform/build'
syms=(OUT/'saadet.symbols').read_text()
def sym(name):
    m=re.search(r'^([0-9a-f]+) .*\s'+re.escape(name)+r'$',syms,re.M)
    assert m,f'missing required target symbol {name}'
    return int(m[1],16)
assert sym('_start')==0x02000120
# SRAM placement alone is insufficient: boot itself calls some resident P33
# helpers. Initialize code and pointer data before the first platform call.
boot=(ROOT/'build/fm1-base-platform/device/main_fm1.c').read_text().split('void fm1_cstart(void)',1)[1]
first=boot.index('    fm1_time_init();')
for marker in ('d = _bss_start','d = _pool_start','s = _data_load','s = _rt_load','s=_sd_audio_load'):
    assert boot.index(marker)<first,f'boot uses resident helpers before initialization: {marker}'
assert boot.index('d = _pool_start')<boot.index('s=_sd_audio_load'),'POOL clear wipes resident code'
assert sym('sd_worker_stop')>=0x02000120,'early CPU1 stop must execute from flash before SRAM initialization'
lo,hi=sym('_sd_audio_start'),sym('_sd_audio_end')
assert 0x1c20000<=lo<hi<=sym('_pool_end')<=sym('_resource_end')==0x1c74000
assert sym('_resource_end')-sym('_resource_start')>=16384
assert sym('_cpu1_stacks_hi')-sym('_cpu1_stacks_lo')==8192
for name in ('saadet_render','audio_worker_loop','fm1_core1_main'):
    assert lo<=sym(name)<hi,f'{name} must be resident before the worker starts'
# CPU1 must poll shared SRAM using inlined helpers; only a checked job may call.
dis=(OUT/'saadet.dis').read_text()
body=dis.split('\naudio_worker_loop:',1)[1].split('\n\n',1)[0]
assert not re.search(r'\bcall -?\d+',body),'CPU1 idle loop contains an outlined helper'
# The resident section must actually be part of the install image.
image=(OUT/'saadet.bin').read_bytes()
packed=(OUT/'audio.bin').read_bytes()
at=sym('_sd_audio_load')-sym('_start')
assert len(packed)==hi-lo and image[at:at+len(packed)]==packed
assert sym('_bss_end')<=0x1c20000
for retired in ('fm1_stub_start','crstub_main'):
    assert not re.search(r'\s'+retired+r'$',syms,re.M),f'legacy boot/update linked: {retired}'
package=Path(sys.argv[1]) if len(sys.argv)>1 else OUT/'saadet.fwsc'
assert b'CRSTUB' not in image,'legacy boot-stub signature present'
report={'app_bytes':len(image),'low_ram_bytes':sym('_bss_end')-0x1c08000,
        'resident_audio_bytes':hi-lo,'pool_bytes':sym('_pool_end')-0x1c20000,
        'engine_arena_bytes':sym('_resource_end')-sym('_resource_start'),
        'package_sha256':hashlib.sha256(package.read_bytes()).hexdigest()}
(OUT/(package.stem+'-validation.json')).write_text(json.dumps(report,indent=2)+'\n')
print('Saadet target checks passed: CPU1, SRAM code, packed image, arena, boot layout')
print(json.dumps(report))
