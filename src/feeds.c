/*
 * The Feeds view: a list of feeds, one a row; Return fetches the chosen
 * one and opens its headlines.
 *
 * Until milestone 6 reads NEWSIIE.FEEDS, the list is built in. A test
 * build (make TEST=1) adds feed files on a second disk, /FEEDS, so the
 * emulator, which has no CoGS card, can drive every screen.
 */
#include <string.h>
#include "screen.h"
#include "kbd.h"
#include "fmt.h"
#include "store.h"
#include "source.h"
#include "ui.h"

#define FIRST_ROW   2
#define NAME_COL    3
#define URL_COL     26

struct feed_def {
    const char *name;
    const char *url;
};

static const struct feed_def feeds[] = {
#ifdef NEWSIIE_TEST
    { "Hacker News (disk)",  "/FEEDS/HN.XML" },
    { "Ars Technica (disk)", "/FEEDS/ARS.XML" },
    { "BBC News (disk)",     "/FEEDS/BBC.XML" },
    { "xkcd (disk)",         "/FEEDS/XKCD.XML" },
    { "Not there (disk)",    "/FEEDS/NONE.XML" },
#endif
    { "Hacker News",         "https://hnrss.org/frontpage" },
    { "BBC News",            "https://feeds.bbci.co.uk/news/rss.xml" },
    { "Ars Technica",        "https://feeds.arstechnica.com/arstechnica/index" },
    { "NASA",                "https://www.nasa.gov/feed/" },
    { "New York Times",      "https://rss.nytimes.com/services/xml/rss/nyt/HomePage.xml" },
    { "Slashdot",            "http://rss.slashdot.org/Slashdot/slashdotMain" },
    { "xkcd",                "https://xkcd.com/atom.xml" },
};
#define NFEEDS  ((uint8_t)(sizeof feeds / sizeof feeds[0]))

static uint8_t sel;
static char count[16];

static void draw_row(uint8_t i)
{
    uint8_t inv = (uint8_t)(i == sel);

    lb_clear(inv ? SC_ISPACE : SC_SPACE);
    lb_text(NAME_COL, feeds[i].name, inv);
    lb_text(URL_COL, feeds[i].url, inv);
    scr_blit((uint8_t)(FIRST_ROW + i));
}

static void draw(void)
{
    uint8_t i;

    fmt_str(fmt_u32(count, NFEEDS), " feeds");
    ui_heading("Feeds", count);
    for (i = 0; i < NFEEDS; ++i) {
        draw_row(i);
    }
    scr_clear((uint8_t)(FIRST_ROW + NFEEDS), BODY_ROWS - 1);
    fmt_str(fmt_u32(count, NFEEDS), " FEEDS");
    status_main("NEWSIIE", count);
}

static void move(uint8_t to)
{
    uint8_t was = sel;

    sel = to;
    draw_row(was);
    draw_row(sel);
}

uint8_t feeds_run(void)
{
    uint8_t k, rc;

    draw();
    for (;;) {
        k = key_get();
        switch (ui_common(k)) {
        case UI_QUIT:
            return V_QUIT;
        case UI_REDRAW:
            draw();
            continue;
        }
        if (k == K_UP && sel) {
            move((uint8_t)(sel - 1));
        } else if (k == K_DOWN && sel + 1 < NFEEDS) {
            move((uint8_t)(sel + 1));
        } else if (k == K_RETURN) {
            cur_name = feeds[sel].name;
            cur_url = feeds[sel].url;
            rc = source_fetch(cur_url);
            if (store_count) {
                /* Stopped or not, what arrived can be read. */
                return V_HEADS;
            }
            status_msg(source_error(rc));
            key_get();
            draw();
        }
    }
}
