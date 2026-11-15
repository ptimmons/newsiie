/*
 * The Headlines view: the fetched feed's items, one a row, newest first as
 * the feed gives them, with an inverse bar on the chosen one.
 *
 *   col 2   the date, "MM-DD HH:MM"
 *   col 14  the title, clipped at the right edge
 *
 * Rows come from the store in aux memory one at a time, so moving the bar
 * redraws two rows and scrolling redraws the page.
 */
#include <string.h>
#include "screen.h"
#include "kbd.h"
#include "fmt.h"
#include "feed.h"
#include "store.h"
#include "source.h"
#include "cogs.h"
#include "readset.h"
#include "ui.h"

#define FIRST_ROW   2
#define NROWS       (BODY_ROWS - FIRST_ROW)     /* 21 */
#define MARK_COL    0                           /* unread: a diamond */
#define DATE_COL    2
#define TITLE_COL   14

static uint16_t top, sel;
uint8_t heads_fresh;
static store_head_t h;
static char note[40];
static char where[24];

static void draw_row(uint16_t i)
{
    uint8_t inv = (uint8_t)(i == sel);
    uint8_t row = (uint8_t)(FIRST_ROW + (i - top));

    lb_clear(inv ? SC_ISPACE : SC_SPACE);
    if (i < store_count) {
        store_head(i, &h);
        if (!(h.flags & STORE_READ)) {
            linebuf[MARK_COL] = has_mousetext ? MT_DIAMOND : sc_char('*', inv);
        }
        if (h.date[0]) {
            lb_text(DATE_COL, h.date + 5, inv);     /* drop the year */
        }
        lb_text(TITLE_COL, h.title, inv);
    }
    scr_blit(row);
}

static void draw_status(void)
{
    char *p = fmt_u32(where, sel + 1);

    fmt_u32(fmt_str(p, " OF "), store_count);
    status_main(cur_name, where);
}

static void draw(void)
{
    uint16_t i;
    char *p;

    p = fmt_str(fmt_u32(note, store_count), store_count == 1 ? " item" : " items");
    if (store_full) {
        fmt_str(p, ", no room for more");
    } else if (source_rc == COGS_E_ABORT) {
        fmt_str(p, ", stopped early");
    } else if (source_rc) {
        fmt_str(p, ", cut short");
    }
    ui_heading(feed_title[0] ? (const char *)feed_title : cur_name, note);
    for (i = top; i < top + NROWS; ++i) {
        draw_row(i);
    }
    draw_status();
}

/* Put the bar on item to, scrolling as little as possible. */
static void move(uint16_t to)
{
    uint16_t was = sel;

    if (to >= store_count) {
        to = (uint16_t)(store_count - 1);
    }
    sel = to;
    if (sel < top) {
        top = sel;
    } else if (sel >= top + NROWS) {
        top = (uint16_t)(sel - NROWS + 1);
    } else {
        draw_row(was);
        draw_row(sel);
        draw_status();
        return;
    }
    draw();
}

/* "20 ITEMS, 74K IN 9 S" on the status bar until the first key. */
static void summary(void)
{
    char *p = fmt_u32(note, store_count);

    p = fmt_str(p, store_count == 1 ? " ITEM, " : " ITEMS, ");
    p = fmt_str(fmt_u32(p, (source_bytes + 512) >> 10), "K");
    if (source_secs) {
        fmt_str(fmt_u32(fmt_str(p, " IN "), source_secs), " S");
    }
    status_msg(note);
}

uint8_t heads_run(void)
{
    uint8_t k, rc;

    if (heads_fresh) {
        heads_fresh = 0;
        top = sel = 0;
        draw();
        if (source_rc) {
            status_msg(source_error(source_rc));
        } else {
            summary();
        }
    } else {
        /* Back from an article, maybe a different one (Left, Right). */
        sel = cur_item;
        if (sel < top || sel >= top + NROWS) {
            top = sel >= NROWS / 2 ? (uint16_t)(sel - NROWS / 2) : 0;
        }
        draw();
    }
    for (;;) {
        k = key_get();
        switch (ui_common(k)) {
        case UI_QUIT:
            return V_QUIT;
        case UI_REDRAW:
            draw();
            continue;
        }
        if (k == K_ESC) {
            rs_save();              /* no stream is open here */
            return V_FEEDS;
        }
        if (k == K_RETURN) {
            cur_item = sel;
            return V_ARTICLE;
        }
        if (k == K_UP) {
            if (key_oa) {
                move(sel > NROWS ? (uint16_t)(sel - NROWS) : 0);
            } else if (sel) {
                move((uint16_t)(sel - 1));
            } else {
                draw_status();
            }
        } else if (k == K_DOWN) {
            move(key_oa ? (uint16_t)(sel + NROWS) : (uint16_t)(sel + 1));
        } else if (key_oa && (k == '<' || k == ',')) {
            move(0);
        } else if (key_oa && (k == '>' || k == '.')) {
            move((uint16_t)(store_count - 1));
        } else if (key_oa && (k == 'r' || k == 'R')) {
            rc = source_fetch(cur_url);
            if (!store_count) {
                status_msg(source_error(rc));
                key_get();
                return V_FEEDS;
            }
            top = sel = 0;
            draw();
            if (rc) {
                status_msg(source_error(rc));
            } else {
                summary();
            }
        } else {
            draw_status();          /* retire any message */
        }
    }
}
