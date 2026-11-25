/*
 * Fetching into the store: the shared half. See source.h.
 */
#include <fcntl.h>
#include <unistd.h>
#include "source.h"
#include "feed.h"
#include "xml.h"
#include "store.h"
#include "tick.h"
#include "kbd.h"
#include "fmt.h"
#include "ui.h"

uint32_t source_bytes;
uint8_t  source_rc;
uint16_t source_secs;

uint8_t source_buf[SOURCE_BUF];
/* In LOWBSS (not cleared at startup): always written before it is shown.
 * Main memory is short (cfg/newsiie-app.cfg). */
#pragma bss-name (push, "LOWBSS")
char    source_msg[48];
#pragma bss-name (pop)

static uint16_t nchunks;

const char *source_error(uint8_t rc)
{
    switch (rc) {
    case 0:              return "";
    case SRC_E_ABORT:    return "STOPPED";
    case SRC_E_NOFILE:   return "CANNOT OPEN THAT FILE";
    case SRC_E_NOTFEED:  return "THAT IS NOT A FEED";
    case SRC_E_EMPTY:    return "THE FEED HAS NO ITEMS";
    case SRC_E_NOCLOCK:  return "THE CARD'S CLOCK IS NOT SET, AND HTTPS NEEDS IT";
    case SRC_E_STALLED:  return "THE SERVER STOPPED SENDING";
    }
    return source_net_error(rc);
}

void source_progress(void)
{
    char *p;

    if (++nchunks & 7) {
        return;
    }
    p = fmt_str(fmt_u32(fmt_str(source_msg, "FETCHING "), source_bytes >> 10), "K, ");
    p = fmt_str(fmt_u32(p, store_count), store_count == 1 ? " ITEM" : " ITEMS");
    fmt_str(p, "   ESC STOPS");
    status_msg(source_msg);
}

uint8_t source_esc(void)
{
    return (uint8_t)(key_ready() && key_get() == K_ESC);
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
    while ((n = read(fd, source_buf, SOURCE_CHUNK)) > 0) {
        source_bytes += (uint16_t)n;
        feed_chunk(source_buf, (uint16_t)n);
        source_progress();
        if (xml_stop) {
            break;
        }
        if (source_esc()) {
            rc = SRC_E_ABORT;
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
    rc = url[0] == '/' ? from_file(url) : source_stream(url);
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
