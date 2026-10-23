/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#include <stdint.h>
#define MELODEE_USB_AUDIO 1
static uint8_t ua_off, ua_off_want;
#define main settings_non_usb_main
#include "settings_test.c"
#undef main
int main(void)
{
    for (uint32_t flags = 0; flags < 4; ++flags)
        for (uint32_t budget = 8; budget <= 32; ++budget) {
            persist_t record = {0};
            record.magic = PERSIST_MAGIC; record.panel = PANEL_DEFAULT;
            record.ext.usb_off = WORK_BUDGET_TAG | (budget << 8) | flags;
            ua_off = ua_off_want = 255;
            assert(settings_import(&record, sizeof record));
            assert(ua_off == flags && ua_off_want == flags && settings_budget == budget);
            ua_off = (flags + 1) % 4;
            settings_export(&record);
            assert((record.ext.usb_off & 3) == ua_off);
            assert(record.ext.usb_off == (ua_off | (budget == VOICE_BUDGET_DEFAULT ? 0 : WORK_BUDGET_TAG | budget << 8)));
        }
    puts("USB device flags and workload budget import/export passed");
}
