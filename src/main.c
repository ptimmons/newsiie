/*
 * newsiie -- an RSS and Atom reader for the Enhanced Apple //e, with the
 * network done by a CoGS card or a FujiNet (netdrv.h).
 */
#include <apple2.h>
#include <stdint.h>
#include "screen.h"
#include "kbd.h"
#include "store.h"
#include "feedlist.h"
#include "readset.h"
#include "fmt.h"
#include "ui.h"
#include "netdrv.h"

extern const char *feeds_note;
/* In LOWBSS (not cleared at startup): written before it is used. Main
 * memory is short (cfg/newsiie-app.cfg). */
#pragma bss-name (push, "LOWBSS")
static char note[40];
#pragma bss-name (pop)

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
    uint8_t view = V_FEEDS;
    const char *why;

    if (os < APPLE_IIE) {
        fail40("NEWSIIE NEEDS AN APPLE //E");
        return 1;
    }
    has_mousetext = (uint8_t)(os != APPLE_IIE);

    scr_init();
    splash_show();
    why = store_init();
    if (why) {
        status_msg(why);
        key_get();
        scr_done();
        return 1;
    }
    /* The feed list, before anything can open a stream. */
    if (fl_load() == FL_E_READ) {
        fmt_hex8(fmt_str(note, "COULD NOT READ NEWSIIE.FEEDS, ERROR $"), fl_oserror);
        feeds_note = note;
    } else if (fl_skipped) {
        fmt_str(fmt_u32(note, fl_skipped),
                fl_skipped == 1 ? " LINE OF NEWSIIE.FEEDS NOT USED" : " LINES OF NEWSIIE.FEEDS NOT USED");
        feeds_note = note;
    }
    rs_load();
    store_is_read = rs_has;
    net_init();                 /* load the driver for the card there */
    net_check();                /* while the splash is up */
    key_get();                  /* consumed: it must not act on the view */

    while (view != V_QUIT) {
        switch (view) {
        case V_FEEDS:   view = feeds_run();   break;
        case V_HEADS:   view = heads_run();   break;
        default:        view = article_run(); break;
        }
    }

    rs_save();
    scr_done();
    return 0;
}
