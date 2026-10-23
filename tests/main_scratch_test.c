/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/scratch.h"
#include "../firmware/src/scratch.h"

_Static_assert(__alignof__(main_scratch) >= 4, "workspace supports aligned accesses");
_Static_assert(sizeof main_scratch.st == 256, "storage page ABI");
_Static_assert(sizeof main_scratch.dec == 640, "OTA frame ABI");
#ifdef MELODEE_LOADER
_Static_assert(sizeof main_scratch == 640, "loader needs only a frame buffer");
#else
_Static_assert(sizeof main_scratch == 1024, "application workspace capacity");
_Static_assert(sizeof main_scratch.snap == 1024, "scope sample ABI");
_Static_assert(sizeof main_scratch.transfer == 256, "backup chunk ABI");
#endif
int main(void)
{
    memset(&main_scratch, 0, sizeof main_scratch);
    assert((void *)main_scratch.st == (void *)main_scratch.dec);
    main_scratch.dec[255] = 0xC7;
    assert(main_scratch.st[255] == 0xC7);
#ifndef MELODEE_LOADER
    assert((void *)main_scratch.snap == (void *)main_scratch.st);
    assert((void *)main_scratch.transfer == (void *)main_scratch.st);
    main_scratch.snap[0] = 12345;
    int16_t sample;
    memcpy(&sample, main_scratch.transfer, sizeof sample);
    assert(sample == 12345);
#endif
    puts("Main-loop scratch ABI, alignment, aliasing and include safety passed");
}
