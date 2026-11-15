/*
 * The Feeds view: the feed list, one a row. Return fetches the chosen feed
 * and opens its headlines; Open-Apple-A adds a feed and Open-Apple-D
 * deletes one, and either change is saved to NEWSIIE.FEEDS at once.
 *
 * While a feed is open, cur_name and cur_url point at fl_name and fl_url,
 * which hold the chosen entry: nothing else calls fl_get() until the user
 * comes back here, and this view redraws (and re-reads) on return.
 */
#include <string.h>
#include "screen.h"
#include "kbd.h"
#include "fmt.h"
#include "feed.h"
#include "feedlist.h"
#include "store.h"
#include "source.h"
#include "ui.h"

#define FIRST_ROW   2
#define NAME_COL    3
#define URL_COL     26

/* While adding, the typed URL and name sit in the parser's idle buffer. */
#define NEW_URL     feed_scratch
#define NEW_NAME    (feed_scratch + FL_URL + 1)

const char *feeds_note;             /* a message for the next draw */

static uint8_t sel;
static uint8_t told_locked;
static char text[48];

static void draw_row(uint8_t i)
{
    uint8_t inv = (uint8_t)(i == sel);

    fl_get(i);
    lb_clear(inv ? SC_ISPACE : SC_SPACE);
    lb_text(NAME_COL, fl_name, inv);
    lb_text(URL_COL, fl_url, inv);
    scr_blit((uint8_t)(FIRST_ROW + i));
}

static void draw_status(void)
{
    fmt_str(fmt_u32(text, fl_count), fl_count == 1 ? " FEED" : " FEEDS");
    status_main("NEWSIIE", text);
}

static void draw(void)
{
    uint8_t i;

    fmt_str(fmt_u32(text, fl_count), fl_count == 1 ? " feed" : " feeds");
    ui_heading("Feeds", text);
    scr_clear(FIRST_ROW, BODY_ROWS - 1);
    if (!fl_count) {
        lb_clear(SC_SPACE);
        lb_center("No feeds yet.", 0);
        scr_blit(10);
        lb_clear(SC_SPACE);
        lb_center("Press " OA "-A to add one, or " OA "-? for help.", 0);
        scr_blit(12);
    }
    for (i = 0; i < fl_count; ++i) {
        draw_row(i);
    }
    draw_status();
    if (feeds_note) {
        status_msg(feeds_note);
        feeds_note = 0;
    }
}

static void move(uint8_t to)
{
    uint8_t was = sel;

    sel = to;
    draw_row(was);
    draw_row(sel);
}

/* Write the list out. A write-protected disk (a read-only network image,
 * say) is a normal state: say so once, and keep the list in memory. */
static void save(void)
{
    uint8_t rc = fl_save();

    if (rc == FL_E_LOCKED) {
        if (!told_locked) {
            feeds_note = "THE DISK IS WRITE-PROTECTED: FEEDS NOT SAVED";
            told_locked = 1;
        }
    } else if (rc) {
        fmt_hex8(fmt_str(text, "COULD NOT SAVE THE FEEDS, PRODOS ERROR $"), fl_oserror);
        feeds_note = text;
    }
}

static void add(void)
{
    NEW_URL[0] = 0;
    if (!prompt("FEED URL:", NEW_URL, FL_URL)) {
        return;
    }
    fl_fix_url(NEW_URL);
    fl_host(NEW_URL, NEW_NAME);
    if (!prompt("NAME:", NEW_NAME, FL_NAME)) {
        return;
    }
    if (fl_add(NEW_NAME, NEW_URL)) {
        feeds_note = "THE LIST IS FULL: 21 FEEDS";
        return;
    }
    sel = (uint8_t)(fl_count - 1);
    save();
}

static void delete(void)
{
    uint8_t i;
    char *p;

    fl_get(sel);
    p = fmt_str(text, "DELETE ");
    for (i = 0; fl_name[i] && i < 30; ++i) {
        *p++ = (char)(fl_name[i] >= 'a' && fl_name[i] <= 'z' ? fl_name[i] - 32 : fl_name[i]);
    }
    fmt_str(p, "?");
    if (!confirm(text)) {
        return;
    }
    fl_delete(sel);
    if (sel && sel >= fl_count) {
        --sel;
    }
    save();
}

uint8_t feeds_run(void)
{
    uint8_t k, rc;

    if (sel >= fl_count) {
        sel = 0;
    }
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
        if (key_oa && (k == 'a' || k == 'A')) {
            add();
            draw();
        } else if (key_oa && (k == 'd' || k == 'D') && fl_count) {
            delete();
            draw();
        } else if (k == K_UP && sel) {
            move((uint8_t)(sel - 1));
        } else if (k == K_DOWN && sel + 1 < fl_count) {
            move((uint8_t)(sel + 1));
        } else if (k == K_RETURN && fl_count) {
            fl_get(sel);
            cur_name = fl_name;
            cur_url = fl_url;
            rc = source_fetch(cur_url);
            if (store_count) {
                /* Stopped or not, what arrived can be read. */
                heads_fresh = 1;
                return V_HEADS;
            }
            feeds_note = source_error(rc);
            draw();
        } else {
            draw_status();          /* retire any message */
        }
    }
}
