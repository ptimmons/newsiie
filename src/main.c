/*
 * newsiie -- an RSS and Atom reader for the Enhanced Apple //e, with the
 * network done by a CoGS card. Milestone 0: splash, the empty Feeds view,
 * the status bar and the help box.
 */
#include <apple2.h>
#include <stdint.h>
#include "screen.h"
#include "kbd.h"
#include "ui.h"

/* Before the 80-column screen is known to exist: a message on the
 * 40-column screen, written straight to page 1. */
static void fail40(const char *msg)
{
    uint8_t *p = (uint8_t *)0x0400;

    while (*msg) {
        *p++ = (uint8_t)(*msg++ | 0x80);
    }
    key_get();
}

int main(void)
{
    uint8_t os = get_ostype();
    uint8_t k;

    if (os < APPLE_IIE) {
        fail40("NEWSIIE NEEDS AN APPLE //E");
        return 1;
    }
    has_mousetext = (uint8_t)(os != APPLE_IIE);

    scr_init();
    splash_show();
    key_get();                  /* consumed: it must not act on the view */

    feeds_draw();
    for (;;) {
        k = key_get();
        if (!key_oa) {
            continue;
        }
        if (k == '?' || k == '/' || k == 'h' || k == 'H') {
            help_show();
            key_get();
            feeds_draw();
        } else if (k == 'q' || k == 'Q') {
            break;
        }
    }

    scr_done();
    return 0;
}
