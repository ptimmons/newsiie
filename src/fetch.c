/*
 * FETCH.SYSTEM -- milestone 2's real-card test of the CoGS byte streams.
 *
 * Opens a URL as a stream, pulls it to the end, and reports what newsiie
 * needs to know before building on streams:
 *   - do http, https, an http-to-https redirect and a 404 behave as hoped?
 *   - how fast do the bytes arrive, at each read size?
 *   - is the body plain XML, or did a server send gzip anyway?
 *   - does ProDOS still read the boot volume afterwards? On a CoGS network
 *     disk that read shares the card's FIFOs with the stream.
 *
 * Nothing is parsed or kept; only the first few hundred bytes are shown.
 */
#include <apple2.h>
#include <string.h>
#include "screen.h"
#include "kbd.h"
#include "tick.h"
#include "cogs.h"
#include "fmt.h"
#include "ui.h"

uint8_t disk_unit(void);
uint8_t __fastcall__ disk_read_block(uint8_t unit, uint16_t block, void *buf);

#define MENU_ROW    2           /* the six tests, then U, A, D, C */
#define RES_COL     62          /* each test's one-word result */
#define RES_W       17
#define DETAIL_ROW  13          /* result, timing, reads, content */
#define PREV_ROW    17          /* preview of the first bytes */
#define PREV_ROWS   5
#define DISK_ROW    22
#define VAL_COL     11

#define OPEN_LIMIT  (60u * TICKS_PER_SEC)   /* the card has its own timeouts */
#define STALL_LIMIT (30u * TICKS_PER_SEC)

struct test {
    const char *kind;
    const char *url;
    uint8_t expect;             /* the result that counts as a pass */
};

static const struct test tests[] = {
    { "http",     "http://rss.cnn.com/rss/edition.rss",               0 },
    { "https",    "https://hnrss.org/frontpage",                      0 },
    { "redirect", "http://hnrss.org/frontpage",                       0 },
    { "404",      "https://httpbin.org/status/404",                   COGS_E_REFUSED },
    { "75K",      "https://feeds.arstechnica.com/arstechnica/index",  0 },
    { "250K",     "https://www.nasa.gov/feed/",                       0 },
};
#define NTESTS  (sizeof tests / sizeof tests[0])

static uint8_t chunk[COGS_STREAM_HDR + 4096];
static uint16_t chunk_max = 512;
static char preview[481];
static uint16_t npreview;
static char buf[100];
static char url[128];
static uint8_t block[512];

/* What one fetch measured. */
static struct {
    uint8_t  rc;
    uint32_t total;
    uint16_t reads, empties;
    uint8_t  peak;
    uint32_t open_secs, xfer_secs;  /* by the card's uptime clock */
    uint8_t  have_clock;
    uint16_t fail_at;               /* 0 at open, else the read that failed */
    uint8_t  lost_why, lost_op;     /* cogs_lost_* when rc is a timeout */
    uint16_t longest;               /* longest wait for one reply, in frames */
} r;

/* ---- drawing ---- */

static void row_text(uint8_t row, uint8_t col, const char *label, const char *value, uint8_t inv)
{
    lb_clear(SC_SPACE);
    lb_text(1, label, 0);
    lb_text(col, value, inv);
    scr_blit(row);
}

static void status(const char *mid)
{
    char right[24];

    fmt_u32(fmt_str(right, "READS OF "), chunk_max);
    status_fields("FETCH PROBE", mid, right);
}

static const char *wait_label;

static void p_status_wait(uint16_t waited)
{
    char *p = fmt_str(buf, wait_label);

    fmt_str(fmt_u32(fmt_str(p, ", WAITED "), waited / TICKS_PER_SEC), " S");
    status(buf);
}

static void draw_menu(void)
{
    uint8_t i;

    for (i = 0; i < NTESTS; ++i) {
        lb_clear(SC_SPACE);
        linebuf[1] = sc_char((char)('1' + i), 0);
        lb_text(4, tests[i].kind, 0);
        lb_text(13, tests[i].url, 0);
        scr_blit((uint8_t)(MENU_ROW + i));
    }
    row_text(MENU_ROW + 6, 4, "U", "type a URL", 0);
    row_text(MENU_ROW + 7, 4, "A", "run 1 to 6, then check the disk", 0);
    row_text(MENU_ROW + 8, 4, "D", "check the disk: read the boot volume's directory block", 0);
    fmt_str(fmt_u32(fmt_str(buf, "read size: "), chunk_max), " bytes (cycles 512, 1024, 2048, 4096)");
    row_text(MENU_ROW + 9, 4, "C", buf, 0);
}

/* The result field of a menu row, without disturbing the rest of it. */
static void put_result(uint8_t row, const char *s, uint8_t inv)
{
    uint8_t i = (uint8_t)(row - MENU_ROW);

    lb_clear(SC_SPACE);
    if (i < NTESTS) {
        linebuf[1] = sc_char((char)('1' + i), 0);
        lb_text(4, tests[i].kind, 0);
        lb_text(13, tests[i].url, 0);
    } else {
        lb_text(1, "U", 0);
        lb_text(4, url, 0);
    }
    lb_fill(RES_COL - 1, RES_W + 1, SC_SPACE);
    lb_text(RES_COL, s, inv);
    scr_blit(row);
}

/* ---- the fetch ---- */

/* Every stream command waits here until its reply starts. On the real
 * card the first STREAM_READ is held back until the server has answered,
 * which a slow server stretches past the primitives' one-second wait. */
static uint16_t wait_start;

static void wait_begin(const char *label)
{
    wait_start = ticks;
    wait_label = label;
}

static uint8_t idle(void)
{
    uint16_t waited;

    if (tick_poll()) {
        waited = (uint16_t)(ticks - wait_start);
        if (waited > r.longest) {
            r.longest = waited;
        }
        if (waited >= TICKS_PER_SEC && !(waited % TICKS_PER_SEC)) {
            p_status_wait(waited);
        }
    }
    if ((uint16_t)(ticks - wait_start) > OPEN_LIMIT) {
        return COGS_E_TIMEOUT;
    }
    return 0;
}

/* Seconds by the card's uptime, which needs the 17-byte STATUS reply. */
static uint8_t card_clock(uint32_t *t)
{
    cogs_netstat_t ns;

    if (cogs_netstat(&ns) || !ns.has_uptime) {
        return 0;
    }
    *t = ns.uptime;
    return 1;
}

static void fetch(const char *u)
{
    cogs_chunk_t c;
    uint32_t t0 = 0, t1 = 0, t2 = 0;
    uint16_t quiet = 0, n;
    uint8_t clk;

    memset(&r, 0, sizeof r);
    npreview = 0;
    preview[0] = 0;

    clk = card_clock(&t0);
    cogs_stream_close();            /* a stream left open would make us BUSY */

    status("OPENING");
    tick_reset();
    cogs_idle = idle;
    wait_begin("OPENING");
    r.rc = cogs_stream_open(u, (uint8_t)strlen(u));
    if (r.rc) {
        r.lost_why = cogs_lost_why;
        r.lost_op = cogs_lost_op;
        cogs_idle = NULL;
        clk &= card_clock(&t1);
        r.open_secs = t1 - t0;
        r.have_clock = clk;
        return;
    }
    clk &= card_clock(&t1);

    for (;;) {
        wait_begin("READING");
        r.rc = cogs_stream_read(chunk, chunk_max, &c);
        if (r.rc) {
            r.fail_at = (uint16_t)(r.reads + r.empties + 1);
            r.lost_why = cogs_lost_why;
            r.lost_op = cogs_lost_op;
            break;
        }
        if (c.level > r.peak) {
            r.peak = c.level;
        }
        if (c.len) {
            ++r.reads;
            quiet = 0;
            if (npreview < sizeof preview - 1) {
                n = (uint16_t)(sizeof preview - 1 - npreview);
                if (n > c.len) {
                    n = c.len;
                }
                memcpy(preview + npreview, c.data, n);
                npreview += n;
                preview[npreview] = 0;
            }
            r.total += c.len;
            if (!(r.reads & 7)) {
                fmt_str(fmt_u32(fmt_str(buf, "FETCHING "), r.total >> 10), "K");
                status(buf);
            }
        }
        if (c.flags & COGS_SF_END) {
            break;
        }
        if (!c.len) {
            ++r.empties;
            if (tick_wait(1)) {
                if (key_get() == K_ESC) {
                    r.rc = COGS_E_ABORT;
                    break;
                }
            }
            if (++quiet > STALL_LIMIT) {
                r.rc = COGS_E_TIMEOUT;
                break;
            }
        } else if (key_ready() && key_get() == K_ESC) {
            r.rc = COGS_E_ABORT;
            break;
        }
    }
    cogs_stream_close();
    cogs_idle = NULL;
    clk &= card_clock(&t2);
    r.have_clock = clk;
    r.open_secs = t1 - t0;
    r.xfer_secs = t2 - t1;
}

/* ---- reporting ---- */

static const char *content_kind(void)
{
    const char *p = preview;

    if (npreview >= 2 && (uint8_t)p[0] == 0x1F && (uint8_t)p[1] == 0x8B) {
        return "GZIP-COMPRESSED: newsiie could not read this";
    }
    if (npreview >= 3 && (uint8_t)p[0] == 0xEF && (uint8_t)p[1] == 0xBB && (uint8_t)p[2] == 0xBF) {
        p += 3;
    }
    while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') {
        ++p;
    }
    if (strstr(p, "<rss")) {
        return "XML, an RSS 2.0 feed";
    }
    if (strstr(p, "<feed")) {
        return "XML, an Atom feed";
    }
    if (strstr(p, "<rdf:RDF")) {
        return "XML, an RSS 1.0 (RDF) feed";
    }
    if (!strncmp(p, "<!DOCTYPE h", 11) || !strncmp(p, "<!doctype h", 11) ||
        !strncmp(p, "<html", 5) || !strncmp(p, "<HTML", 5)) {
        return "HTML, not a feed";
    }
    if (!strncmp(p, "<?xml", 5)) {
        return "XML, feed type not in the first bytes";
    }
    return npreview ? "not recognised" : "empty";
}

static void show_preview(void)
{
    uint8_t row = PREV_ROW, col = 1;
    uint16_t i;
    char ch;

    lb_clear(SC_SPACE);
    for (i = 0; i < npreview && row < PREV_ROW + PREV_ROWS; ++i) {
        ch = preview[i];
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n' || col >= SCR_COLS - 1) {
            scr_blit(row++);
            lb_clear(SC_SPACE);
            col = 1;
            if (ch == '\n') {
                continue;
            }
        }
        if (ch == '\t') {
            ch = ' ';
        }
        if ((uint8_t)ch < 0x20 || (uint8_t)ch >= 0x7F) {
            ch = '.';
        }
        linebuf[col++] = sc_char(ch, 0);
    }
    while (row < PREV_ROW + PREV_ROWS) {
        scr_blit(row++);
        lb_clear(SC_SPACE);
    }
}

static const char *lost_text(uint8_t why)
{
    switch (why) {
    case COGS_LOST_SEND:    return "card took no more bytes";
    case COGS_LOST_HEADER:  return "no reply header";
    case COGS_LOST_PAYLOAD: return "reply cut short";
    case COGS_LOST_OPCODE:  return "stray byte";
    case COGS_LOST_IDLE:    return "no reply in 60 s";
    }
    return "?";
}

static void report(uint8_t menu_row, uint8_t expect)
{
    char *p;
    uint32_t rate10;

    if (r.rc == expect && r.rc) {
        put_result(menu_row, "REFUSED, AS HOPED", 0);
        row_text(DETAIL_ROW, VAL_COL, "Result", "refused at open, as a 404 should be", 0);
    } else if (expect && !r.rc) {
        /* The card let a 404 through: whatever arrived is the error page. */
        put_result(menu_row, "NOT REFUSED", 1);
        p = fmt_str(buf, "a 404 was NOT refused: open succeeded, ");
        fmt_str(fmt_u32(p, r.total), " bytes of error page");
        row_text(DETAIL_ROW, VAL_COL, "Result", buf, 1);
    } else if (r.rc) {
        put_result(menu_row, cogs_strerror(r.rc), 1);
        if (r.fail_at) {
            p = fmt_str(fmt_u32(fmt_str(buf, "read "), r.fail_at), ": ");
        } else {
            p = fmt_str(buf, "open: ");
        }
        p = fmt_str(p, cogs_strerror(r.rc));
        if (r.rc == COGS_E_REFUSED) {
            p = fmt_str(p, " (or HTTP status not 2xx)");
        }
        if (r.rc == COGS_E_TIMEOUT) {
            p = fmt_str(p, " - ");
            p = fmt_str(p, lost_text(r.lost_why));
            p = fmt_hex8(fmt_str(p, " $"), r.lost_op);
        }
        if (r.total) {
            fmt_str(fmt_u32(fmt_str(p, ", after "), r.total), " bytes");
        }
        row_text(DETAIL_ROW, VAL_COL, "Result", buf, 1);
    } else {
        p = fmt_str(fmt_u32(fmt_str(buf, "OK "), (r.total + 512) >> 10), "K");
        put_result(menu_row, buf, 0);
        fmt_str(fmt_u32(fmt_str(buf, "OK, "), r.total), " bytes");
        row_text(DETAIL_ROW, VAL_COL, "Result", buf, 0);
    }

    if (r.have_clock) {
        p = fmt_str(fmt_u32(fmt_str(buf, "open "), r.open_secs), " s, transfer ");
        p = fmt_str(fmt_u32(p, r.xfer_secs), " s");
        if (r.xfer_secs && r.total) {
            rate10 = r.total * 10 / r.xfer_secs / 1024;
            p = fmt_u32(fmt_str(p, ", "), rate10 / 10);
            *p++ = '.';
            p = fmt_str(fmt_u32(p, rate10 % 10), " KB/s");
        }
        fmt_str(p, "  (card clock, whole seconds)");
    } else {
        fmt_str(buf, "no card clock in this firmware's STATUS reply");
    }
    row_text(DETAIL_ROW + 1, VAL_COL, "Timing", buf, 0);

    p = fmt_str(fmt_u32(buf, r.reads), " reads with data, ");
    p = fmt_str(fmt_u32(p, r.empties), " empty, ring peak ");
    p = fmt_str(fmt_u32(p, r.peak), "/255, longest wait ");
    fmt_str(fmt_u32(p, (r.longest + TICKS_PER_SEC / 2) / TICKS_PER_SEC), " s");
    row_text(DETAIL_ROW + 2, VAL_COL, "Reads", buf, 0);

    row_text(DETAIL_ROW + 3, VAL_COL, "Content", content_kind(), 0);
    show_preview();
}

static void disk_check(void)
{
    uint8_t unit = disk_unit(), rc, n;
    char *p;

    rc = disk_read_block(unit, 2, block);
    p = fmt_str(buf, "block 2 of unit $");
    p = fmt_hex8(p, unit);
    p = fmt_str(fmt_u32(fmt_str(p, " (slot "), (unit >> 4) & 7), unit & 0x80 ? " drive 2)" : " drive 1)");
    if (rc) {
        fmt_hex8(fmt_str(p, ": ProDOS ERROR $"), rc);
        row_text(DISK_ROW, VAL_COL, "Disk", buf, 1);
        return;
    }
    p = fmt_str(p, ": read OK, volume /");
    n = (uint8_t)(block[4] & 15);
    if ((block[4] >> 4) != 0xF) {
        fmt_str(p, "?  (not a volume directory)");
    } else {
        memcpy(p, block + 5, n);
        p[n] = 0;
    }
    row_text(DISK_ROW, VAL_COL, "Disk", buf, 0);
}

static void run_test(uint8_t i)
{
    put_result((uint8_t)(MENU_ROW + i), "...", 0);
    fetch(tests[i].url);
    report((uint8_t)(MENU_ROW + i), tests[i].expect);
    disk_check();
}

/* A line editor on the URL row. 0 if cancelled. */
static uint8_t ask_url(void)
{
    uint8_t n = (uint8_t)strlen(url), k;
    const uint8_t row = MENU_ROW + 6, col = 7, max = RES_COL - 2 - col;

    for (;;) {
        lb_clear(SC_SPACE);
        lb_text(1, "U", 0);
        lb_text(4, "> ", 0);
        lb_text(col, url, 0);
        linebuf[col + n] = SC_ISPACE;       /* the cursor */
        scr_blit(row);
        status_fields("FETCH PROBE", "TYPE A URL", "RETURN FETCHES  ESC CANCELS");

        k = key_get();
        if (k == K_ESC) {
            return 0;
        }
        if (k == K_RETURN) {
            return n != 0;
        }
        if (k == K_DELETE || k == K_LEFT) {
            if (n) {
                url[--n] = 0;
            }
        } else if (k >= 0x20 && k < 0x7F && n < max) {
            url[n++] = (char)k;
            url[n] = 0;
        }
    }
}

int main(void)
{
    uint8_t os = get_ostype();
    uint8_t slot, k, i, card;

    if (os < APPLE_IIE) {
        return 1;
    }
    has_mousetext = (uint8_t)(os != APPLE_IIE);
    scr_init();
    scr_clear(0, SCR_ROWS - 1);

    lb_clear(SC_SPACE);
    lb_text(1, "Fetch probe", 0);
    lb_text((uint8_t)(SCR_COLS - 1 - txt_len("newsiie " NEWSIIE_VERSION)),
            "newsiie " NEWSIIE_VERSION, 0);
    scr_blit(0);
    lb_clear(has_mousetext ? MT_RULE : SC_DASH);
    scr_blit(1);
    draw_menu();

    /* Without a card only the disk check can run; the menu stays up so
     * the rest of the screen can still be tried in an emulator. */
    slot = cogs_find();
    card = (uint8_t)(slot && !cogs_open(slot));
    if (card) {
        fmt_u32(fmt_str(buf, "CoGS in slot "), slot);
        row_text(DETAIL_ROW, VAL_COL, "Card", buf, 0);
    } else {
        row_text(DETAIL_ROW, VAL_COL, "Card",
                 slot ? "the card did not answer: only D works" : "no CoGS card found: only D works", 1);
    }

    for (;;) {
        status("PRESS 1-6, U, A, D, C   ESC QUITS");
        k = key_get();
        if (k >= 'a' && k <= 'z') {
            k = (uint8_t)(k - 32);
        }
        if (k == K_ESC) {
            break;
        }
        if (!card && k != 'D' && k != 'C') {
            continue;
        }
        if (k >= '1' && k < '1' + NTESTS) {
            run_test((uint8_t)(k - '1'));
        } else if (k == 'A') {
            for (i = 0; i < NTESTS; ++i) {
                run_test(i);
            }
        } else if (k == 'D') {
            disk_check();
        } else if (k == 'C') {
            chunk_max = chunk_max >= 4096 ? 512 : (uint16_t)(chunk_max * 2);
            draw_menu();
        } else if (k == 'U') {
            if (ask_url()) {
                put_result(MENU_ROW + 6, "...", 0);
                fetch(url);
                report(MENU_ROW + 6, 0);
                disk_check();
            } else {
                draw_menu();
            }
        }
    }
    scr_done();
    return 0;
}
