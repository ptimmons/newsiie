/*
 * Fetching a feed into the store: from the network through the CoGS card,
 * or from a ProDOS file when the "URL" is a pathname (it starts with '/').
 * Either way the bytes go through the parser chunk by chunk, so nothing
 * but the parsed items is ever held.
 *
 * Progress and the outcome go on the status bar; Esc stops a fetch and
 * keeps the items that arrived. The two sources are never mixed: no ProDOS
 * call is made while a stream is open (PLAN.md section 1).
 */
#ifndef SOURCE_H
#define SOURCE_H

#include "port.h"

/* Results beyond cogs.h's. */
#define SRC_E_NOFILE    0xF8    /* the file would not open */
#define SRC_E_NOTFEED   0xF9    /* it parsed, but it is not a feed */
#define SRC_E_EMPTY     0xFA    /* a feed with no items */

/* Fetch url into the store (clearing it first). Returns 0, COGS_E_ABORT
 * (stopped, items so far kept), or an error. */
uint8_t source_fetch(const char *url);

/* A short uppercase line for a result, for the status bar. */
const char *source_error(uint8_t rc);

extern uint32_t source_bytes;       /* bytes the last fetch read */
extern uint8_t  source_rc;          /* what the last fetch returned */

#endif
