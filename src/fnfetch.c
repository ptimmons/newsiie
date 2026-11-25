/*
 * FETCH.SYSTEM for the FujiNet build: the transport check PLAN.md section
 * 12 asks for before trusting the FujiNet with newsiie's feeds.
 *
 * It finds the FujiNet's units, shows its firmware and address, then
 * fetches each test URL through the N: device, parsing as the bytes come,
 * and records the raw device results: what the open returned, the final
 * STATUS (bytes waiting, connected, error), how many reads and empty
 * polls it took, and the feed it parsed. D reads a block of the boot
 * volume, which on a FujiNet-mounted disk goes to the same SmartPort card.
 */
#include <apple2.h>
#include <string.h>
#include "screen.h"
#include "kbd.h"
#include "tick.h"
#include "fuji.h"
#include "fmt.h"
#include "ui.h"
#include "feed.h"
#include "xml.h"

#ifndef NEWSIIE_BUILD
#define NEWSIIE_BUILD ""
#endif

uint8_t disk_unit(void);
uint8_t __fastcall__ disk_read_block(uint8_t unit, uint16_t block, void *buf);

#define DEV_ROW     2
#define MENU_ROW    5
#define RES_COL     63
#define KEYS_ROW    14
#define LIVE_ROW    15
#define DETAIL_ROW  16
#define TITLE_ROW   19
#define NTITLES     3
#define TITLE_W     74
#define DISK_ROW    22
#define CHUNK       512
#define STALL       (30u * TICKS_PER_SEC)

struct test {
    const char *kind;
    const char *url;
    uint8_t     feed;           /* 1: a feed with items counts as a pass */
};

static const struct test tests[] = {
    { "http",     "http://rss.cnn.com/rss/edition.rss",              1 },
    { "https",    "https://hnrss.org/frontpage",                     1 },
    { "redirect", "http://hnrss.org/frontpage",                      1 },
    { "404",      "https://httpbin.org/status/404",                  0 },
    { "75K",      "https://feeds.arstechnica.com/arstechnica/index", 1 },
    { "250K",     "https://www.nasa.gov/feed/",                      1 },
    { "delay 5",  "https://httpbin.org/delay/5",                     0 },
#ifdef PROBE_HOST
    /* A feed served from the bench Mac, to watch the FujiNet's side. */
    { "local len", "http://" PROBE_HOST "/len.xml",                  1 },
    { "local chk", "http://" PROBE_HOST "/chunk.xml",                1 },
#endif
};
#define NTESTS  (sizeof tests / sizeof tests[0])

static uint8_t chunk[CHUNK];
static uint8_t parse = 1;       /* P: feed the bytes to the parser */
static uint16_t chunk_n = FUJI_READ_MAX;    /* C: the most one READ asks for */
static uint8_t block[512];
static char buf[100];
static char titles[NTITLES][TITLE_W + 1];

static struct {
    uint8_t  open_rc, open_err;
    uint8_t  rc, err;           /* FN_E_* and fuji_err at the end */
    uint8_t  conn, last_err;    /* the last STATUS */
    uint16_t last_avail;
    uint32_t bytes;
    uint16_t reads, empties;
    uint16_t frames;            /* frames counted between calls */
    uint8_t  esc;
    uint8_t  raw_a;             /* the last READ: A and the count, as returned */
    uint16_t raw_n;
} r;

static void row(uint8_t y, const char *s, uint8_t inv)
{
    lb_clear(SC_SPACE);
    lb_text(1, s, inv);
    scr_blit(y);
}

static void status(const char *mid)
{
    status_fields("FUJINET FETCH PROBE", mid, "ESC STOPS A FETCH");
}

static uint8_t sink(const feed_item_t *it)
{
    uint16_t i = (uint16_t)(feed_items - 1);

    if (i < NTITLES) {
        strncpy(titles[i], it->title, TITLE_W);
        titles[i][TITLE_W] = 0;
    }
    return 0;
}

/* Redrawn before every SmartPort call during a fetch, so a call that
 * never returns leaves on screen which one it was and how far it got. */
static void live(char phase, uint16_t n)
{
    char *p;

    tick_poll();
    p = buf;
    *p++ = phase;
    p = fmt_u32(fmt_str(p, " read "), r.reads);
    p = fmt_u32(fmt_str(p, " bytes "), r.bytes);
    p = fmt_u32(fmt_str(p, " waiting "), r.last_avail);
    p = fmt_u32(fmt_str(p, " ask "), n);
    p = fmt_u32(fmt_str(p, " empty "), r.empties);
    p = fmt_u32(fmt_str(p, " s "), ticks / TICKS_PER_SEC);
    fmt_str(p, "      ");
    row(LIVE_ROW, buf, 0);
}

static void draw_keys(void)
{
    char *p;

    p = fmt_str(buf, "1-9 one test  A all  D disk  Q quit   P parse: ");
    p = fmt_str(p, parse ? "on" : "off");
    fmt_u32(fmt_str(p, "  C read size: "), chunk_n);
    row(KEYS_ROW, buf, 0);
}

static void draw_device(void)
{
    uint8_t ip[4];
    char version[15];
    char *p;

    if (!fuji_find()) {
        row(DEV_ROW, "No FujiNet: no SmartPort card has a NETWORK unit.", 1);
        return;
    }
    p = fmt_u32(fmt_str(buf, "FujiNet in slot "), fuji_slot);
    p = fmt_u32(fmt_str(p, ", NETWORK unit "), fuji_net);
    p = fmt_u32(fmt_str(p, ", THE_FUJI unit "), fuji_dev);
    fmt_str(p, fuji_wifi() ? ", on Wi-Fi" : ", NOT on Wi-Fi");
    row(DEV_ROW, buf, 0);
    if (fuji_config(ip, version)) {
        fmt_str(buf, "No adapter configuration.");
    } else {
        fmt_ip(fmt_str(fmt_str(fmt_str(buf, "Firmware "), version), ", address "), ip);
    }
    row(DEV_ROW + 1, buf, 0);
}

static void draw_test(uint8_t i, const char *result, uint8_t inv)
{
    lb_clear(SC_SPACE);
    linebuf[1] = sc_char((char)('1' + i), 0);
    lb_text(3, tests[i].kind, 0);
    lb_text(12, tests[i].url, 0);
    lb_text(RES_COL, result, inv);
    scr_blit((uint8_t)(MENU_ROW + i));
}

static void draw_menu(void)
{
    uint8_t i;

    for (i = 0; i < NTESTS; ++i) {
        draw_test(i, "", 0);
    }
    draw_keys();
}

static void draw_detail(void)
{
    char *p;
    uint8_t i;

    p = fmt_hex8(fmt_str(buf, "open $"), r.open_rc);
    p = fmt_u32(fmt_str(p, " (device "), r.open_err);
    p = fmt_hex8(fmt_str(p, ")   result $"), r.rc);
    p = fmt_u32(fmt_str(p, " detail "), r.err);
    p = fmt_u32(fmt_str(p, "   last STATUS: waiting "), r.last_avail);
    p = fmt_u32(fmt_str(p, " conn "), r.conn);
    fmt_u32(fmt_str(p, " err "), r.last_err);
    row(DETAIL_ROW, buf, 0);

    p = fmt_u32(fmt_str(buf, "bytes "), r.bytes);
    p = fmt_u32(fmt_str(p, "  reads "), r.reads);
    p = fmt_u32(fmt_str(p, "  empty "), r.empties);
    p = fmt_u32(fmt_str(p, "  frames "), r.frames);
    p = fmt_u32(fmt_str(p, "  pcount "), fuji_read_pcount);
    p = fmt_hex8(fmt_str(p, " A $"), r.raw_a);
    p = fmt_u32(fmt_str(p, " n "), r.raw_n);
    p = fmt_u32(fmt_str(p, "  items "), feed_items);
    p = fmt_str(p, "   kind ");
    p = fmt_str(p, feed_kind == FEED_RSS ? "RSS" : feed_kind == FEED_ATOM ? "Atom" :
                   feed_kind == FEED_RDF ? "RDF" : feed_kind == FEED_NOT ? "not a feed" : "unknown");
    fmt_str(p, r.esc ? "   (Esc)" : "");
    row(DETAIL_ROW + 1, buf, 0);

    lb_clear(SC_SPACE);
    lb_text(1, feed_title, 0);
    scr_blit(DETAIL_ROW + 2);
    for (i = 0; i < NTITLES; ++i) {
        lb_clear(SC_SPACE);
        if (i < feed_items) {
            linebuf[1] = sc_char('-', 0);
            lb_text(3, titles[i], 0);
        }
        scr_blit((uint8_t)(TITLE_ROW + i));
    }
}

static void fetch(uint8_t i)
{
    fuji_status_t st;
    uint16_t quiet = 0, n;
    uint8_t k;

    memset(&r, 0, sizeof r);
    for (k = 0; k < NTITLES; ++k) {
        titles[k][0] = 0;
    }
    feed_sink = sink;
    feed_begin();
    draw_test(i, "RUNNING", 1);
    status(tests[i].url);
    tick_reset();

    if (!fuji_find()) {
        r.rc = FN_E_NODEV;
        goto done;
    }
    fuji_close();
    r.open_rc = fuji_open(tests[i].url);
    if (r.open_rc) {
        r.open_err = fuji_err;
        r.rc = r.open_rc;
        r.err = fuji_err;
        if (!fuji_status(&st)) {
            r.last_avail = st.avail;
            r.conn = st.conn;
            r.last_err = st.err;
        }
        goto close;
    }
    for (;;) {
        if (key_ready() && key_get() == K_ESC) {
            r.esc = 1;
            break;
        }
        live('S', 0);
        if (fuji_status(&st)) {
            r.rc = FN_E_BUS;
            r.err = fuji_err;
            break;
        }
        r.last_avail = st.avail;
        r.conn = st.conn;
        r.last_err = st.err;
        if (st.avail) {
            n = st.avail > chunk_n ? chunk_n : st.avail;
            live('R', n);
            k = fuji_read(chunk, n);
            r.raw_a = sp_raw_a;
            r.raw_n = sp_count;
            if (k) {
                r.rc = FN_E_BUS;
                r.err = fuji_err;
                break;
            }
            ++r.reads;
            quiet = 0;
            r.bytes += sp_count;
            if (parse) {
                feed_chunk(chunk, sp_count);
            }
            if (!(r.reads & 7)) {
                fmt_str(fmt_u32(buf, r.bytes >> 10), "K");
                draw_test(i, buf, 1);
            }
            if (xml_stop) {
                break;
            }
        } else if (st.err == FN_ST_EOF || (st.err == FN_ST_OK && !st.conn)) {
            break;
        } else if (st.err != FN_ST_OK) {
            r.rc = FN_E_NET;
            r.err = st.err;
            break;
        } else {
            ++r.empties;
            tick_wait(1);
            if (++quiet > STALL) {
                r.rc = 0xFC;
                break;
            }
        }
    }
close:
    fuji_close();
done:
    feed_end();
    tick_poll();
    r.frames = ticks;
    live('.', 0);
    if (r.esc) {
        fmt_str(buf, "STOPPED");
    } else if (r.rc) {
        fmt_str(buf, r.rc == FN_E_NET ? "DEVICE ERROR" : "FAIL");
    } else if (tests[i].feed && !parse) {
        fmt_u32(fmt_str(buf, "READ "), r.bytes);
    } else if (tests[i].feed) {
        fmt_str(fmt_u32(fmt_str(buf, feed_items ? "PASS " : "NO ITEMS "), r.bytes), "");
    } else {
        fmt_u32(fmt_str(buf, "DONE "), r.bytes);
    }
    draw_test(i, buf, (uint8_t)(r.rc || r.esc));
    draw_detail();
    status("");
}

static void disk_check(void)
{
    uint8_t unit = disk_unit(), rc, n;
    char *p;

    rc = disk_read_block(unit, 2, block);
    p = fmt_hex8(fmt_str(buf, "Disk: block 2 of unit $"), unit);
    if (rc) {
        fmt_hex8(fmt_str(p, ": ProDOS ERROR $"), rc);
        row(DISK_ROW, buf, 1);
        return;
    }
    p = fmt_str(p, ": read OK, volume /");
    n = (uint8_t)(block[4] & 15);
    memcpy(p, block + 5, n);
    p[n] = 0;
    row(DISK_ROW, buf, 0);
}

/* X: open a feed, then try SmartPort READ several ways, showing what the
 * card returns for each (rows DETAIL_ROW on). For finding out which READ
 * a SmartPort card accepts for the network device. */
static uint8_t xrow;

static void try_read(const char *label, uint8_t cmd, uint8_t pcount, uint16_t n)
{
    char *p;
    uint8_t rc, i;

    sp_parms[0] = pcount;
    sp_parms[1] = fuji_net;
    sp_parms[2] = (uint8_t)(uint16_t)chunk;
    sp_parms[3] = (uint8_t)((uint16_t)chunk >> 8);
    sp_parms[4] = (uint8_t)n;
    sp_parms[5] = (uint8_t)(n >> 8);
    sp_parms[6] = (uint8_t)(pcount == 5);
    sp_parms[7] = sp_parms[8] = 0;
    sp_count = 0;
    chunk[0] = 0;
    rc = sp_call(cmd);
    p = fmt_str(buf, label);
    p = fmt_hex8(fmt_str(p, ": rc $"), rc);
    p = fmt_hex8(fmt_str(p, " A $"), sp_raw_a);
    p = fmt_u32(fmt_str(p, " XY "), sp_count);
    p = fmt_str(p, "  ");
    for (i = 0; i < 24 && i < sp_count; ++i) {
        *p++ = (char)(chunk[i] >= 32 && chunk[i] < 127 ? chunk[i] : '.');
    }
    *p = 0;
    row(xrow++, buf, 0);
}

static void experiment(void)
{
    fuji_status_t st;
    char *p;
    uint8_t rc;

    scr_clear(DETAIL_ROW, DISK_ROW);
    xrow = DETAIL_ROW;
    if (!fuji_find()) {
        return;
    }
    fuji_close();
    rc = fuji_open("https://hnrss.org/frontpage");
    p = fmt_hex8(fmt_str(buf, "open $"), rc);
    rc = fuji_status(&st);
    p = fmt_hex8(fmt_str(p, "  status $"), rc);
    p = fmt_u32(fmt_str(p, " waiting "), st.avail);
    p = fmt_u32(fmt_str(p, " conn "), st.conn);
    fmt_u32(fmt_str(p, " err "), st.err);
    row(xrow++, buf, 0);
    try_read("READ pc4 n1", 0x08, 4, 1);
    try_read("READ pc4 n64", 0x08, 4, 64);
    try_read("READ pc5 n64", 0x08, 5, 64);
    try_read("OPEN pc1", 0x06, 1, 0);
    try_read("READ pc4 n64 after OPEN", 0x08, 4, 64);
    fuji_close();
}

int main(void)
{
    uint8_t k, i;

    scr_init();
    has_mousetext = (uint8_t)(get_ostype() != APPLE_IIE);
    scr_clear(0, SCR_ROWS - 1);
    row(0, "FujiNet fetch probe " NEWSIIE_VERSION NEWSIIE_BUILD, 0);
    lb_clear(has_mousetext ? MT_RULE : SC_DASH);
    scr_blit(1);
    status("LOOKING FOR THE FUJINET");
    draw_device();
    draw_menu();
    status("");
    for (;;) {
        k = key_get();
        if (k >= 'a') {
            k = (uint8_t)(k - 32);
        }
        if (k >= '1' && k < '1' + NTESTS) {
            fetch((uint8_t)(k - '1'));
        } else if (k == 'A') {
            for (i = 0; i < NTESTS; ++i) {
                fetch(i);
                if (r.esc) {
                    break;
                }
            }
            disk_check();
        } else if (k == 'P') {
            parse = (uint8_t)!parse;
            draw_keys();
        } else if (k == 'C') {
            chunk_n = chunk_n == 256 ? 64 : chunk_n == 64 ? CHUNK : 256;
            draw_keys();
        } else if (k == 'X') {
            experiment();
        } else if (k == 'D') {
            disk_check();
        } else if (k == 'Q' || k == K_ESC) {
            break;
        }
    }
    scr_done();
    return 0;
}
