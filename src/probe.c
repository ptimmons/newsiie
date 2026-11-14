/*
 * PROBE.SYSTEM -- checks a CoGS card from end to end and reports on one
 * screen, for a photo of the result. Milestone 1's real-hardware test.
 *
 *   1. scan the slots (never touching a Disk II's switches)
 *   2. reset the card's frame parser and FIFOs
 *   3. PING: firmware, protocol, features
 *   4. wait up to 20 s for a network link, then STATUS
 *   5. TIME and LOCALTIME
 *   6. data integrity both ways: RANDOM (card to Apple) and SHA-256 of
 *      known data checked against digests computed on the Mac (Apple to card)
 *   7. 100 PING round trips, timed
 */
#include <apple2.h>
#include <string.h>
#include "screen.h"
#include "kbd.h"
#include "tick.h"
#include "cogs.h"
#include "fmt.h"
#include "ui.h"

#define FIRST_ROW   2
#define LAST_ROW    22
#define VAL_COL     12          /* where a line's value starts */

static uint8_t row = FIRST_ROW;
static uint8_t failures;
static char buf[80];
static uint8_t data[1000];

/* Expected SHA-256 digests, from Python's hashlib. */
static const uint8_t sha_abc[32] = {
    0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde,
    0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
    0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
};
/* bytes (i * 7 + 3) & 255 for i = 0..999 */
static const uint8_t sha_pat[32] = {
    0x1e, 0x9b, 0xc3, 0x8c, 0xbf, 0x86, 0x0b, 0x9e, 0xc3, 0x19, 0x18, 0xb0,
    0x65, 0xf9, 0xb5, 0x24, 0x76, 0xc5, 0x49, 0xa7, 0x82, 0xe0, 0xe7, 0x99,
    0x0b, 0xed, 0x8c, 0xe3, 0x86, 0x8d, 0x23, 0x71
};

static const char *const capname[8] = {
    "NET", "TLS", "HTTP", "HASH", "RANDOM", "JSON", "NONCE", "NETTOOLS"
};

/* One report line: a label, and a value that may be redrawn in place. */
static void line(const char *label, const char *value, uint8_t inverse)
{
    lb_clear(SC_SPACE);
    lb_text(1, label, 0);
    lb_text(VAL_COL, value, inverse);
    scr_blit(row);
}

static void next(void)
{
    if (row < LAST_ROW) {
        ++row;
    }
}

static void say(const char *label, const char *value)
{
    line(label, value, 0);
    next();
}

static void fail(const char *label, const char *what, uint8_t rc)
{
    char *p = fmt_str(buf, what);

    if (rc) {
        p = fmt_str(p, ": ");
        fmt_str(p, cogs_strerror(rc));
    }
    line(label, buf, 1);
    next();
    ++failures;
}

static uint8_t scan_slots(void)
{
    uint8_t s, found = 0;
    char *p = buf;

    for (s = 7; s >= 1; --s) {
        p = fmt_u32(p, s);
        p = fmt_str(p, ": ");
        if (slot_is_disk2(s)) {
            p = fmt_str(p, "Disk II");
        } else if (cogs_probe(s)) {
            p = fmt_str(p, "CoGS");
            if (!found) {
                found = s;
            }
        } else {
            p = fmt_str(p, "-");
        }
        p = fmt_str(p, s > 1 ? "   " : "");
    }
    say("Slots", buf);
    return found;
}

/* Raw ID, VERSION and STATUS of every slot that is not a Disk II, for a
 * card that did not identify itself. */
static void raw_slots(void)
{
    uint8_t s;
    const volatile uint8_t *r;
    char *p;

    for (s = 7; s >= 1; --s) {
        if (slot_is_disk2(s)) {
            continue;
        }
        r = (const volatile uint8_t *)(0xC080 + ((uint16_t)s << 4));
        p = fmt_str(buf, "slot ");
        p = fmt_u32(p, s);
        p = fmt_str(p, ": ID $");
        p = fmt_hex8(p, r[5]);
        p = fmt_str(p, "  VERSION $");
        p = fmt_hex8(p, r[6]);
        p = fmt_str(p, "  STATUS $");
        fmt_hex8(p, r[1]);
        say("", buf);
    }
}

static void show_ping(void)
{
    cogs_ping_t pg;
    uint8_t rc, i;
    char *p;

    rc = cogs_ping(&pg);
    if (rc) {
        fail("Firmware", "PING failed", rc);
        return;
    }
    p = fmt_u32(buf, pg.fw_major);
    *p++ = '.';
    if (pg.fw_minor < 10) {
        *p++ = '0';
    }
    p = fmt_u32(p, pg.fw_minor);
    p = fmt_str(p, ", protocol $");
    p = fmt_hex8(p, pg.proto);
    if (pg.fw_major < 1 || (pg.fw_major == 1 && pg.fw_minor < 39)) {
        fmt_str(p, "  (older than 1.39, which newsiie targets)");
    }
    say("Firmware", buf);

    p = buf;
    *p = 0;
    for (i = 0; i < 8; ++i) {
        if (pg.caps & (1 << i)) {
            p = fmt_str(p, capname[i]);
            p = fmt_str(p, " ");
        }
    }
    say("Features", buf[0] ? buf : "none reported");
}

static void show_network(void)
{
    cogs_netstat_t ns;
    uint8_t rc, secs = 0;
    char *p;

    /* LINK_UP in the STATUS register costs no frame round trip. */
    tick_reset();
    while (!(cogs_status() & COGS_ST_LINK_UP) && secs < 20) {
        p = fmt_str(buf, "waiting for a link... ");
        p = fmt_u32(p, secs);
        fmt_str(p, " s  (any key skips)");
        line("Network", buf, 0);
        if (tick_wait(TICKS_PER_SEC)) {
            key_get();
            break;
        }
        ++secs;
    }

    rc = cogs_netstat(&ns);
    if (rc) {
        fail("Network", "STATUS failed", rc);
        return;
    }
    if (ns.link != COGS_LINK_UP) {
        fail("Network", ns.link == COGS_LINK_JOINING ? "still joining" : "down", 0);
        return;
    }
    p = fmt_str(buf, "up, ");
    p = fmt_ip(p, ns.ip);
    p = fmt_str(p, ", signal ");
    p = fmt_i8(p, ns.rssi);
    fmt_str(p, " dBm (Wi-Fi only)");
    say("Network", buf);

    p = fmt_u32(buf, ns.heap_kb);
    p = fmt_str(p, "K free, ");
    p = fmt_u32(p, ns.sockets);
    p = fmt_str(p, ns.sockets == 1 ? " socket open" : " sockets open");
    if (ns.has_uptime) {
        p = fmt_str(p, ", up ");
        fmt_dur(p, ns.uptime);
    }
    say("Card", buf);
}

static void show_time(void)
{
    uint32_t t;
    uint8_t valid, rc;

    rc = cogs_time(&t);
    if (rc) {
        fail("UTC", "TIME failed", rc);
    } else if (!t) {
        /* Verified https is refused until this is set. */
        fail("UTC", "not set yet: the card has not reached NTP", 0);
    } else {
        fmt_date1904(buf, t + SECS_1904_TO_1970);
        say("UTC", buf);
    }

    rc = cogs_localtime(&valid, &t);
    if (rc) {
        fail("Local", "LOCALTIME failed", rc);
    } else if (!valid) {
        say("Local", "not set yet");
    } else {
        fmt_date1904(buf, t);
        say("Local", buf);
    }
}

static void check_data(void)
{
    uint8_t dig[32], rc, i;
    uint16_t k;

    /* Card to Apple: eight full-size RANDOM replies, length-checked. */
    for (i = 0; i < 8; ++i) {
        rc = cogs_random(data, 255);
        if (rc) {
            fail("Random", "RANDOM failed", rc);
            return;
        }
    }
    say("Random", "8 x 255 bytes, every reply complete");

    /* Apple to card: the card's digest of what it received. */
    rc = cogs_sha256((const uint8_t *)"abc", 3, dig);
    if (rc) {
        fail("SHA-256", "HASH failed", rc);
        return;
    }
    if (memcmp(dig, sha_abc, 32)) {
        fail("SHA-256", "\"abc\" hashed WRONG", 0);
        return;
    }
    for (k = 0; k < sizeof data; ++k) {
        data[k] = (uint8_t)(k * 7 + 3);
    }
    rc = cogs_sha256(data, sizeof data, dig);
    if (rc) {
        fail("SHA-256", "HASH of 1000 bytes failed", rc);
        return;
    }
    if (memcmp(dig, sha_pat, 32)) {
        fail("SHA-256", "1000 bytes hashed WRONG: data damaged on the way", 0);
        return;
    }
    say("SHA-256", "\"abc\" and 1000 bytes arrived intact");
}

static void time_pings(void)
{
    cogs_ping_t pg;
    uint8_t i, rc;
    uint16_t start;
    char *p;

    tick_reset();
    start = ticks;
    for (i = 0; i < 100; ++i) {
        rc = cogs_ping(&pg);
        if (rc) {
            fail("Round trip", "PING failed", rc);
            return;
        }
        tick_poll();
    }
    p = fmt_str(buf, "100 PINGs in ");
    p = fmt_u32(p, (uint16_t)(ticks - start));
    fmt_str(p, " frames (60 a second; a busy loop may miss some)");
    say("Round trip", buf);
}

int main(void)
{
    uint8_t os = get_ostype();
    uint8_t slot, rc;

    if (os < APPLE_IIE) {
        return 1;
    }
    has_mousetext = (uint8_t)(os != APPLE_IIE);
    scr_init();
    scr_clear(0, SCR_ROWS - 1);

    lb_clear(SC_SPACE);
    lb_text(1, "CoGS probe", 0);
    lb_text((uint8_t)(SCR_COLS - 1 - txt_len("newsiie " NEWSIIE_VERSION)),
            "newsiie " NEWSIIE_VERSION, 0);
    scr_blit(0);
    lb_clear(has_mousetext ? MT_RULE : SC_DASH);
    scr_blit(1);
    status_fields("COGS PROBE", "RUNNING", "");

    slot = scan_slots();
    if (!slot) {
        fail("Card", "no CoGS card found", 0);
        raw_slots();
        goto done;
    }

    rc = cogs_open(slot);
    if (rc) {
        fail("Reset", "the card's SYNC register never moved", rc);
        goto done;
    }
    fmt_u32(fmt_str(buf, "slot "), slot);
    say("Card", buf);

    show_ping();
    show_network();
    show_time();
    check_data();
    time_pings();

done:
    next();
    if (failures) {
        fmt_str(fmt_u32(buf, failures), failures == 1 ? " check failed" : " checks failed");
        line("", buf, 1);
    } else {
        line("", "All checks passed.", 0);
    }
    status_fields("COGS PROBE", failures ? "FAILED" : "PASSED", "ANY KEY QUITS");
    key_get();
    scr_done();
    return 0;
}
