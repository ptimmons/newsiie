/*
 * Fetching from the network through the CoGS card's byte streams: the
 * CoGS build's half of source.h.
 */
#include <string.h>
#include "source.h"
#include "cogs.h"
#include "feed.h"
#include "xml.h"
#include "tick.h"
#include "kbd.h"
#include "fmt.h"
#include "ui.h"

#define WAIT_LIMIT  (60u * TICKS_PER_SEC)   /* per reply; the card has its own */

static uint8_t  stopping;           /* Esc while a reply was awaited */
static uint16_t wait_start;

const char *source_net_error(uint8_t rc)
{
    if (rc == COGS_E_REFUSED) {
        return "THE SERVER REFUSED IT (OR NOT FOUND)";
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
            fmt_str(fmt_u32(fmt_str(source_msg, "WAITING FOR THE SERVER, "),
                            waited / TICKS_PER_SEC), " S   ESC STOPS");
            status_msg(source_msg);
        }
    }
    return (uint16_t)(ticks - wait_start) > WAIT_LIMIT ? COGS_E_TIMEOUT : 0;
}

/* Wait (up to SOURCE_READY seconds, Esc to give up) until the card has a
 * network link and, for https, a clock: the card refuses a verified TLS
 * connection until NTP has set it. Both are usual just after power-on. */
static uint8_t ready(const char *url)
{
    uint32_t t = 0;
    uint8_t secs;

    for (secs = 0; !(cogs_status() & COGS_ST_LINK_UP); ++secs) {
        if (secs >= SOURCE_READY) {
            return COGS_E_NOT_JOINED;
        }
        status_msg("WAITING FOR THE NETWORK   ESC STOPS");
        if (tick_wait(TICKS_PER_SEC) && key_get() == K_ESC) {
            return SRC_E_ABORT;
        }
    }
    if (strncmp(url, "https:", 6)) {
        return 0;
    }
    for (secs = 0; !cogs_time(&t) && !t; ++secs) {
        if (secs >= SOURCE_READY) {
            return SRC_E_NOCLOCK;
        }
        status_msg("WAITING FOR THE CARD'S CLOCK (HTTPS NEEDS IT)   ESC STOPS");
        if (tick_wait(TICKS_PER_SEC) && key_get() == K_ESC) {
            return SRC_E_ABORT;
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

uint8_t source_stream(const char *url)
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
            rc = SRC_E_ABORT;
            break;
        }
        wait_start = ticks;
        rc = cogs_stream_read(source_buf, SOURCE_CHUNK, &c);
        if (rc) {
            break;
        }
        if (c.len) {
            quiet = 0;
            source_bytes += c.len;
            feed_chunk(c.data, c.len);
            source_progress();
            if (xml_stop) {
                break;              /* store full, or not a feed */
            }
        }
        if (c.flags & COGS_SF_END) {
            break;
        }
        if (!c.len) {
            tick_wait(1);
            if (++quiet > SOURCE_STALL) {
                rc = SRC_E_STALLED;
            }
        }
        if (source_esc()) {
            rc = SRC_E_ABORT;
        }
    }
    cogs_stream_close();
    cogs_idle = NULL;
    if (t0) {
        source_secs = (uint16_t)(card_uptime() - t0);
    }
    return rc;
}
