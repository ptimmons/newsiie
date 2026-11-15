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
    }
    return cogs_strerror(rc);
}

/* While a reply has not started: keep the clock, and give up after a
 * minute. The card ends every command itself, so this guards only against
 * a card that stopped answering. */
static uint8_t idle(void)
{
    uint16_t waited;

    if (tick_poll()) {
        waited = (uint16_t)(ticks - wait_start);
        if (waited >= 2 * TICKS_PER_SEC && !(waited % TICKS_PER_SEC)) {
            fmt_str(fmt_u32(fmt_str(msg, "WAITING FOR THE SERVER, "),
                            waited / TICKS_PER_SEC), " S");
            status_msg(msg);
        }
    }
    return (uint16_t)(ticks - wait_start) > WAIT_LIMIT ? COGS_E_TIMEOUT : 0;
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

    if (!cogs_slot) {
        return COGS_E_NOCARD;
    }
    cogs_stream_close();            /* one left open would make us BUSY */
    status_msg("CONNECTING");
    cogs_idle = idle;
    wait_start = ticks;
    rc = cogs_stream_open(url, (uint8_t)strlen(url));
    while (!rc) {
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
    return rc;
}
