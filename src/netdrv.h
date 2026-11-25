/*
 * The network driver: the CoGS card's or the FujiNet's, chosen at startup.
 *
 * Each driver is linked with the app but written to a file of its own,
 * NEWSIIE.COGS or NEWSIIE.FUJI, to run in one slot of memory just under
 * the C stack (cfg/newsiie-app.cfg). net_init() loads NEWSIIE.COGS and asks
 * it whether there is a CoGS card; if not, NEWSIIE.FUJI, the same way. The
 * one that finds its device stays loaded; if neither does, there is no
 * network and the app works from disk files only.
 *
 * A driver begins with a table (below). The app reaches the driver only
 * through it (netcall.s): it must never call a driver's functions or use its
 * variables directly, since the other driver may be in the slot. The
 * drivers call the app freely (feed_chunk, status_msg, ...), which is why
 * they are linked with it: tools/drvcheck.sh checks both rules hold.
 */
#ifndef NETDRV_H
#define NETDRV_H

#include "port.h"

#define NETDRV_MAGIC    0xD5    /* the table's first byte */

/* The table (drv_cogs.s, drv_fuji.s): the magic byte; the stamp, the
 * address of __ONCE_RUN__ (the app's end) as linked, so a driver from
 * another build, where everything has moved, is refused; then a JMP to
 * each of find (nonzero if the device is there), check (net_check(), fill
 * status_net), stream (source_stream()) and error (source_net_error()). */
#define NETDRV_HEAD     15

/* The drivers' halves, under their own names. */
uint8_t     cogs_find(void);
void        cogs_net_check(void);
uint8_t     cogs_stream(const char *url);
const char *cogs_net_error(uint8_t rc);
uint8_t     fuji_find(void);
void        fuji_net_check(void);
uint8_t     fuji_stream(const char *url);
const char *fuji_net_error(uint8_t rc);

/* The app's half: netdrv.c loads, netcall.s passes calls on. */
void net_init(void);            /* load the driver that finds its device */
extern uint8_t net_loaded;      /* $80 once a driver is in the slot */
uint8_t net_find(void);         /* the slot's find */
void net_none_check(void);      /* the stand-ins with no driver */
const char *net_none_error(uint8_t rc);

/* A linker symbol: the app's end, for the stamp. */
extern char _ONCE_RUN__[];

#endif
