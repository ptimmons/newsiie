/*
 * Fetching from the network through the FujiNet's N: device: the FujiNet
 * driver's half of source.h, in NEWSIIE.FUJI (netdrv.h).
 *
 * Every SmartPort call runs to its end; nothing can cut one short. The
 * first STATUS after an open is the one that waits for the server (DNS,
 * TLS, the response header), so Esc pressed then takes effect when it
 * returns: the keyboard holds the key until we look.
 */
#include "source.h"
#include "netdrv.h"
#include "fuji.h"
#include "feed.h"
#include "xml.h"
#include "tick.h"
#include "kbd.h"
#include "fmt.h"
#include "ui.h"

static uint8_t last_err;            /* fuji_err when the fetch ended */

const char *fuji_net_error(uint8_t rc)
{
    fuji_err = last_err;            /* other calls since may have reset it */
    return fuji_strerror(rc);
}

/* Up to SOURCE_READY seconds for the adapter to be on Wi-Fi: usual just
 * after power-on. */
static uint8_t ready(void)
{
    uint8_t secs;

    for (secs = 0; !fuji_wifi(); ++secs) {
        if (secs >= SOURCE_READY) {
            return FN_E_NOWIFI;
        }
        status_msg("WAITING FOR THE FUJINET'S WI-FI   ESC STOPS");
        if (tick_wait(TICKS_PER_SEC) && key_get() == K_ESC) {
            return SRC_E_ABORT;
        }
    }
    return 0;
}

static uint8_t pull(void)
{
    fuji_status_t st;
    uint16_t quiet = 0, n;
    uint8_t rc;

    for (;;) {
        rc = fuji_status(&st);
        if (rc) {
            return rc;
        }
        if (st.avail) {
            /* Drain what is buffered, even past the end of the source. */
            n = st.avail > FUJI_READ_MAX ? FUJI_READ_MAX : st.avail;
            rc = fuji_read(source_buf, n);
            if (rc) {
                return rc;
            }
            if (sp_count) {
                quiet = 0;
                source_bytes += sp_count;
                feed_chunk(source_buf, sp_count);
                source_progress();
                if (xml_stop) {
                    return 0;       /* store full, or not a feed */
                }
            }
        } else if (st.err == FN_ST_EOF) {
            return 0;
        } else if (st.err != FN_ST_OK) {
            fuji_err = st.err;
            return FN_E_NET;
        } else if (!st.conn) {
            return 0;               /* closed, with nothing left */
        } else {
            tick_wait(1);
            if (++quiet > SOURCE_STALL) {
                return SRC_E_STALLED;
            }
            if (quiet >= 2 * TICKS_PER_SEC && !(quiet % TICKS_PER_SEC)) {
                fmt_str(fmt_u32(fmt_str(source_msg, "WAITING FOR THE SERVER, "),
                                quiet / TICKS_PER_SEC), " S   ESC STOPS");
                status_msg(source_msg);
            }
        }
        if (source_esc()) {
            return SRC_E_ABORT;
        }
    }
}

uint8_t fuji_stream(const char *url)
{
    fuji_status_t st;
    uint8_t rc;

    if (!fuji_find()) {
        return FN_E_NODEV;
    }
    rc = ready();
    if (rc) {
        return rc;
    }
    fuji_close();                   /* one left open from before */
    status_msg("CONNECTING (ESC STOPS ONCE THE SERVER ANSWERS)");
    rc = fuji_open(url);
    if (rc) {
        /* Refused at once: a bad URL, or no network. STATUS may say why;
         * if not, keep the SmartPort error. */
        uint8_t sp_err = fuji_err;

        if (!fuji_status(&st) && st.err > FN_ST_OK) {
            fuji_err = st.err;
            rc = FN_E_NET;
        } else {
            fuji_err = sp_err;
        }
    } else if (source_esc()) {
        rc = SRC_E_ABORT;
    } else {
        rc = pull();
    }
    last_err = fuji_err;            /* the close (and net_check) reset it */
    fuji_close();
    return rc;
}
