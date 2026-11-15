/*
 * newsiie -- an RSS and Atom reader for the Enhanced Apple //e, with the
 * network done by a CoGS card.
 */
#include <apple2.h>
#include <stdint.h>
#include "screen.h"
#include "kbd.h"
#include "store.h"
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
    net_check();                /* while the splash is up */
    key_get();                  /* consumed: it must not act on the view */

    while (view != V_QUIT) {
        switch (view) {
        case V_FEEDS:   view = feeds_run();   break;
        case V_HEADS:   view = heads_run();   break;
        default:        view = article_run(); break;
        }
    }

    scr_done();
    return 0;
}
