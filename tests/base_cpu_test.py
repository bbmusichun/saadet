#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Exercise the generated CPU0 exception setup against register mocks."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'build/fm1-base-platform/bsp/chip/fm1_irq.h').read_text()
start = source.index('static void fm1_irq_init(void)')
opening = source.index('{', start)
end, depth = opening + 1, 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
body = source[start:end].replace(
    '*(volatile uint32_t *)(0x1EEF100u + 4u * i)', 'host_icfg[i]')
app = (ROOT / 'platform/app.c').read_text()
gate = next(line for line in app.splitlines() if line.startswith('int sd_worker_allowed(void)'))
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t host_icfg[32], vectors[128], ilat, emu_msg, unlock, dbg_clear, emu, etm;
static char fm1_fatal_stubs[128*6];
#define FM1_ICFG(n) host_icfg[(n)>>3]
#define FM1_VEC vectors
#define FM1_ILAT_CLR ilat
#define FM1_EMU_MSG emu_msg
#define FM1_DBG_WR_EN unlock
#define FM1_DBG_MSG_CLR dbg_clear
#define FM1_EMU_CON emu
#define FM1_ETM_CON etm
static uint8_t fm1_power_ready;
static struct { uint32_t failed; } bootguard;
''' + body + '\n' + gate + r'''
int main(void) {
    const uint32_t states[] = {0, 0xFFFFFFFFu, 0x001F000Cu, 0xA5A5A5A5u, 8u};
    for (unsigned i = 0; i < sizeof states / sizeof *states; i++) {
        emu = states[i]; etm = 0x24u; unlock = 1u;
        fm1_irq_init();
        /* The five FPU exception enables and div0 are cleared on every boot;
         * stack protection and all unrelated bits retain their input state. */
        assert(emu == (states[i] & ~0x001F0004u));
        assert((emu & 8u) == (states[i] & 8u));
        assert(etm == 0x25u && ilat == 0xFFu);
        assert(emu_msg == 0xFFFFFFFFu && dbg_clear == 0xFFFFFFFFu);
        assert((host_icfg[0] & 0xF0u) == 0xF0u);
    }
    for (unsigned ready = 0; ready < 2; ready++)
        for (unsigned failed = 0; failed < 4; failed++) {
            fm1_power_ready = ready; bootguard.failed = failed;
            assert(sd_worker_allowed() == (ready && !failed));
        }
    puts("base CPU: Melodee fp32 trap policy, retained stack guard, exception vector and trace setup passed");
    puts("base CPU: unverified supplies and early failed boots keep CPU1 held");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp)
    (path / 'test.c').write_text(code)
    subprocess.run([os.getenv('CC', 'cc'), '-O1', '-g', '-fsanitize=address,undefined',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
