/*
 * Fetching a feed into the store: from the network, or from a ProDOS file
 * when the "URL" is a pathname (it starts with '/'). Either way the bytes
 * go through the parser chunk by chunk, so nothing but the parsed items is
 * ever held.
 *
 * The network is the driver netdrv.c loaded at startup: the CoGS card's
 * (src_cogs.c, net.c) or the FujiNet's (src_fuji.c, net_fuji.c).
 * source.c is the app's own half.
 *
 * Progress and the outcome go on the status bar; Esc stops a fetch and
 * keeps the items that arrived. The two sources are never mixed: no ProDOS
 * call is made while a stream is open (PLAN.md section 1).
 */
#ifndef SOURCE_H
#define SOURCE_H

#include "port.h"

/* Results beyond the network's own (cogs.h, fuji.h). */
#define SRC_E_ABORT     0xF3    /* the user stopped it (== COGS_E_ABORT) */
#define SRC_E_NONET     0xF7    /* no network driver found its device */
#define SRC_E_NOFILE    0xF8    /* the file would not open */
#define SRC_E_NOTFEED   0xF9    /* it parsed, but it is not a feed */
#define SRC_E_EMPTY     0xFA    /* a feed with no items */
#define SRC_E_NOCLOCK   0xFB    /* https, but the card's clock is not set */
#define SRC_E_STALLED   0xFC    /* 30 s with no new data from the server */

/* Fetch url into the store (clearing it first). Returns 0, SRC_E_ABORT
 * (stopped, items so far kept), or an error. */
uint8_t source_fetch(const char *url);

/* A short uppercase line for a result, for the status bar. */
const char *source_error(uint8_t rc);

extern uint32_t source_bytes;       /* bytes the last fetch read */
extern uint8_t  source_rc;          /* what the last fetch returned */
extern uint16_t source_secs;        /* how long it took, by the card's
                                     * clock; 0 for a file or no clock */

/* ---- between source.c and the network's half ---- */

#define SOURCE_CHUNK    512
#define SOURCE_BUF      (6 + SOURCE_CHUNK)  /* room for a CoGS reply header */
#define SOURCE_READY    20                  /* seconds to wait for a link */
#define SOURCE_STALL    (30u * 60)          /* frames of empty reads in a row */

extern uint8_t source_buf[SOURCE_BUF];
extern char    source_msg[48];

void    source_progress(void);      /* every 8th chunk, on the status bar */
uint8_t source_esc(void);           /* 1 if Esc was pressed (and eaten) */

/* The network's half, through the loaded driver (netdrv.c): open url, feed
 * what arrives to feed_chunk(), adding to source_bytes and calling
 * source_progress(), until the end, Esc, xml_stop or an error; close. Then
 * the text for its result codes. */
uint8_t     source_stream(const char *url);
const char *source_net_error(uint8_t rc);

#endif
