/*
 * The Article view: one item, word-wrapped to the screen.
 *
 *   the title
 *   the date and the link (one line each; a long link is cut, marked '>')
 *   a rule
 *   the summary, a blank line between paragraphs
 *
 * The text and link sit in the parser's scratch buffer (feed_scratch),
 * idle while no feed is being fetched; the wrapped lines point into it.
 * Left and Right move to the previous and next item without going back to
 * the list.
 */
#include <string.h>
#include "screen.h"
#include "kbd.h"
#include "fmt.h"
#include "feed.h"
#include "store.h"
#include "wrap.h"
#include "ui.h"

/* This file's code runs from the language card ($D400-$DFFF, bank 2),
 * which cc65 fills at startup: main memory is full (PLAN.md section 5).
 * Code there must not call ProDOS or the ROM while it runs. */
#ifdef __CC65__
#pragma code-name ("LC")
#endif

#define FIRST_ROW   2
#define NROWS       (BODY_ROWS - FIRST_ROW)     /* 21 */
#define TEXT_COL    1
#define WIDTH       78

uint16_t cur_item;

static store_head_t h;
static uint16_t top;
static char note[24];
static char where[24];

#define BODY    feed_scratch
#define LINK    (feed_scratch + FEED_BODY_MAX + 1)

static void compose(void)
{
    uint16_t n, ln;

    store_head(cur_item, &h);
    n = store_body(cur_item, BODY, FEED_BODY_MAX + 1);
    ln = store_link(cur_item, LINK, FEED_LINK_MAX + 1);
    if (ln > WIDTH) {
        LINK[WIDTH - 1] = '>';      /* cut: the rest is in the store */
    }

    wrap_reset();
    wrap_add(h.title, (uint16_t)strlen(h.title), WIDTH, 0);
    if (h.date[0]) {
        wrap_line(h.date, (uint16_t)strlen(h.date), WIDTH);
    }
    if (ln) {
        wrap_line(LINK, ln, WIDTH);
    }
    wrap_rule();
    if (n) {
        wrap_add(BODY, n, WIDTH, 1);
    } else {
        wrap_line("(This item has no summary.)", 27, WIDTH);
    }
    top = 0;
}

static void draw_line(uint8_t row, uint16_t i)
{
    lb_clear(SC_SPACE);
    if (i < wrap_count) {
        if (wrap_lines[i].kind == WK_RULE) {
            lb_fill(TEXT_COL, WIDTH, has_mousetext ? MT_RULE : SC_DASH);
        } else {
            lb_textn(TEXT_COL, wrap_lines[i].p, wrap_lines[i].len, 0);
        }
    }
    scr_blit(row);
}

static void draw_status(void)
{
    uint16_t last = top + NROWS;
    char *p;

    if (last > wrap_count) {
        last = wrap_count;
    }
    p = fmt_u32(fmt_str(where, "LINE "), last);
    fmt_u32(fmt_str(p, " OF "), wrap_count);
    status_main(cur_name, where);
}

static void draw_text(void)
{
    uint8_t r;

    for (r = 0; r < NROWS; ++r) {
        draw_line((uint8_t)(FIRST_ROW + r), top + r);
    }
    draw_status();
}

static void draw(void)
{
    char *p = fmt_u32(fmt_str(note, "Item "), cur_item + 1);

    fmt_u32(fmt_str(p, " of "), store_count);
    ui_heading(feed_title[0] ? (const char *)feed_title : cur_name, note);
    draw_text();
}

/* The furthest top that still fills the screen. */
static uint16_t max_top(void)
{
    return wrap_count > NROWS ? (uint16_t)(wrap_count - NROWS) : 0;
}

static void scroll_to(uint16_t t)
{
    if (t > max_top()) {
        t = max_top();
    }
    if (t != top) {
        top = t;
        draw_text();
    } else {
        draw_status();              /* already there: retire any message */
    }
}

uint8_t article_run(void)
{
    uint8_t k;

    compose();
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
        if (k == K_ESC) {
            return V_HEADS;
        }
        if (k == K_UP) {
            scroll_to(key_oa ? (top > NROWS ? (uint16_t)(top - NROWS) : 0)
                             : (top ? (uint16_t)(top - 1) : 0));
        } else if (k == K_DOWN) {
            scroll_to((uint16_t)(top + (key_oa ? NROWS : 1)));
        } else if (k == ' ') {
            scroll_to((uint16_t)(top + NROWS));
        } else if (key_oa && (k == '<' || k == ',')) {
            scroll_to(0);
        } else if (key_oa && (k == '>' || k == '.')) {
            scroll_to(max_top());
        } else if (k == K_LEFT || k == K_RIGHT) {
            if (k == K_LEFT && cur_item) {
                --cur_item;
            } else if (k == K_RIGHT && cur_item + 1 < store_count) {
                ++cur_item;
            } else {
                continue;
            }
            compose();
            draw();
        } else {
            draw_status();
        }
    }
}
