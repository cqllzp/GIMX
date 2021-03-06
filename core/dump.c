/*
 Copyright (c) 2011 Mathieu Laurendeau <mat.lau@laposte.net>
 License: GPLv3
 */

#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include "dump.h"
#include "sixaxis.h"
#include "controllers/ds3.h"

void sixaxis_dump_state(struct sixaxis_state *state, int id)
{
    int i;
    int ledmap = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct sixaxis_state_sys *sys = &state->sys;

    printf("%d %ld.%06ld %c %02x ", id, tv.tv_sec, tv.tv_usec,
           sys->reporting_enabled ? 'R' : 'N',
           sys->feature_ef_byte_6);

    /* led state */
    for (i = 0; i < 5; i++) {
        switch (sys->led[i]) {
        case LED_ON:
            putchar('o');
            ledmap |= (1 << i);
            break;
        case LED_FLASH:
            putchar('x');
            ledmap |= (1 << i);
            break;
        case LED_OFF:
        default:
            putchar('.');
            break;
        }
        if (i == 0)
            putchar(' ');
    }

    /* controller number, from led state */
    putchar(' ');
    switch (ledmap) {
    case 0x02: putchar('1'); break;
    case 0x04: putchar('2'); break;
    case 0x08: putchar('3'); break;
    case 0x10: putchar('4'); break;
    case 0x12: putchar('5'); break;
    case 0x14: putchar('6'); break;
    case 0x18: putchar('7'); break;
    default: putchar('?'); break;
    }

    /* rumble state */
    printf(" { %02x %02x }", sys->rumble[0], sys->rumble[1]);

    printf("\n");
}
