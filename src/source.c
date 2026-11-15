/*
 * Fetching into the store. See source.h.
 */
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "source.h"
#include "cogs.h"
#include "feed.h"
#include "xml.h"
#include "store.h"
#include "tick.h"
#include "kbd.h"
#include "fmt.h"
#include "ui.h"

#define CHUNK       512
#define WAIT_LIMIT  (60u * TICKS_PER_SEC)   /* per reply; the card has its own */
#define STALL_LIMIT (30u * TICKS_PER_SEC)   /* empty reads in a row */

uint32_t source_bytes;
uint8_t  source_rc;
uint16_t source_secs;

#define READY_LIMIT 20              /* seconds to wait for a link or a clock */

static uint8_t  stopping;           /* Esc while a reply was awaited */

static uint8_t  chunk[COGS_STREAM_HDR + CHUNK];
static uint16_t wait_start;
static uint16_t nchunks;
static char     msg[48];

const char *source_error(uint8_t rc)
{
    switch (rc) {
    case 0:              return "";
    case COGS_E_REFUSED: return "THE SERVER REFUSED IT (OR NOT FOUND)";
    case COGS_E_ABORT:   return "STOPPED";
    case SRC_E_NOFILE:   return "CANNOT OPEN THAT FILE";
    case SRC_E_NOTFEED:  return "THAT IS NOT A FEED";
    case SRC_E_EMPTY:    return "THE FEED HAS NO ITEMS";
    case SRC_E_NOCLOCK:  return "THE CARD'S CLOCK IS NOT SET, AND HTTPS NEEDS IT";
    }
    return cogs_strerror(rc);
}

/* While a reply has not started: keep the clock, and give up after a
 * minute. The card ends every command itself, so this guards only against
 * a card that stopped answering.
 *
 * Esc here cannot cut the wait short: the card would still send its reply
 * later, and a late STREAM_OPEN reply could be taken for the next one. So
 * Esc is remembered, the wait goes on to the reply, and the fetch stops
 * the moment it arrives. */
static uint8_t idle(void)
{
    uint16_t waited;

    if (!stopping && key_ready() && key_get() == K_ESC) {
        stopping = 1;
        status_msg("STOPPING WHEN THE SERVER ANSWERS");
    }
    if (tick_poll() && !stopping) {
        waited = (uint16_t)(ticks - wait_start);
        if (waited >= 2 * TICKS_PER_SEC && !(waited % TICKS_PER_SEC)) {
            fmt_str(fmt_u32(fmt_str(msg, "WAITING FOR THE SERVER, "),
                            waited / TICKS_PER_SEC), " S   ESC STOPS");
            status_msg(msg);
        }
    }
    return (uint16_t)(ticks - wait_start) > WAIT_LIMIT ? COGS_E_TIMEOUT : 0;
}

/* Wait (up to READY_LIMIT seconds, Esc to give up) until the card has a
 * network link and, for https, a clock: the card refuses a verified TLS
 * connection until NTP has set it. Both are usual just after power-on. */
static uint8_t ready(const char *url)
{
    uint32_t t = 0;
    uint8_t secs;

    for (secs = 0; !(cogs_status() & COGS_ST_LINK_UP); ++secs) {
        if (secs >= READY_LIMIT) {
            return COGS_E_NOT_JOINED;
        }
        status_msg("WAITING FOR THE NETWORK   ESC STOPS");
        if (tick_wait(TICKS_PER_SEC) && key_get() == K_ESC) {
            return COGS_E_ABORT;
        }
    }
    if (strncmp(url, "https:", 6)) {
        return 0;
    }
    for (secs = 0; !cogs_time(&t) && !t; ++secs) {
        if (secs >= READY_LIMIT) {
            return SRC_E_NOCLOCK;
        }
        status_msg("WAITING FOR THE CARD'S CLOCK (HTTPS NEEDS IT)   ESC STOPS");
        if (tick_wait(TICKS_PER_SEC) && key_get() == K_ESC) {
            return COGS_E_ABORT;
        }
    }
    return 0;
}

/* Seconds by the card's clock, or 0 if it does not report one. */
static uint32_t card_uptime(void)
{
    cogs_netstat_t ns;

    return cogs_netstat(&ns) || !ns.has_uptime ? 0 : ns.uptime;
}

static void progress(void)
{
    char *p;

    if (++nchunks & 7) {
        return;
    }
    p = fmt_str(fmt_u32(fmt_str(msg, "FETCHING "), source_bytes >> 10), "K, ");
    p = fmt_str(fmt_u32(p, store_count), store_count == 1 ? " ITEM" : " ITEMS");
    fmt_str(p, "   ESC STOPS");
    status_msg(msg);
}

static uint8_t esc_pressed(void)
{
    return (uint8_t)(key_ready() && key_get() == K_ESC);
}

static uint8_t from_stream(const char *url)
{
    cogs_chunk_t c;
    uint16_t quiet = 0;
    uint8_t rc;

    uint32_t t0;

    if (!cogs_slot) {
        return COGS_E_NOCARD;
    }
    rc = ready(url);
    if (rc) {
        return rc;
    }
    t0 = card_uptime();
    cogs_stream_close();            /* one left open would make us BUSY */
    status_msg("CONNECTING   ESC STOPS");
    stopping = 0;
    cogs_idle = idle;
    wait_start = ticks;
    rc = cogs_stream_open(url, (uint8_t)strlen(url));
    while (!rc) {
        if (stopping) {
            rc = COGS_E_ABORT;
            break;
        }
        wait_start = ticks;
        rc = cogs_stream_read(chunk, CHUNK, &c);
        if (rc) {
            break;
        }
        if (c.len) {
            quiet = 0;
            source_bytes += c.len;
            feed_chunk(c.data, c.len);
            progress();
            if (xml_stop) {
                break;              /* store full, or not a feed */
            }
        }
        if (c.flags & COGS_SF_END) {
            break;
        }
        if (!c.len) {
            tick_wait(1);
            if (++quiet > STALL_LIMIT) {
                rc = COGS_E_TIMEOUT;
            }
        }
        if (esc_pressed()) {
            rc = COGS_E_ABORT;
        }
    }
    cogs_stream_close();
    cogs_idle = NULL;
    if (t0) {
        source_secs = (uint16_t)(card_uptime() - t0);
    }
    return rc;
}

static uint8_t from_file(const char *path)
{
    char upper[65];
    uint8_t i, rc = 0;
    int fd, n;

    /* ProDOS 8 wants pathnames in uppercase. */
    for (i = 0; path[i] && i < sizeof upper - 1; ++i) {
        upper[i] = (char)(path[i] >= 'a' && path[i] <= 'z' ? path[i] - 32 : path[i]);
    }
    upper[i] = 0;
    status_msg("READING");
    fd = open(upper, O_RDONLY);
    if (fd < 0) {
        return SRC_E_NOFILE;
    }
    while ((n = read(fd, chunk, CHUNK)) > 0) {
        source_bytes += (uint16_t)n;
        feed_chunk(chunk, (uint16_t)n);
        progress();
        if (xml_stop) {
            break;
        }
        if (esc_pressed()) {
            rc = COGS_E_ABORT;
            break;
        }
    }
    close(fd);
    return rc;
}

uint8_t source_fetch(const char *url)
{
    uint8_t rc;

    store_reset();
    source_bytes = 0;
    source_secs = 0;
    nchunks = 0;
    tick_reset();
    feed_sink = store_add;
    feed_begin();
    rc = url[0] == '/' ? from_file(url) : from_stream(url);
    if (feed_end() == FEED_NOT) {
        rc = SRC_E_NOTFEED;
    } else if (!rc && !store_count) {
        rc = feed_kind == FEED_UNKNOWN ? SRC_E_NOTFEED : SRC_E_EMPTY;
    }
    source_rc = rc;
    if (url[0] != '/') {
        net_check();                /* the link may have changed */
    }
    return rc;
}
