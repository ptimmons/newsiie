/*
 * The FujiNet, reached over SmartPort: finding it (fuji.c), the N: network
 * device's stream calls, and the adapter's Wi-Fi state. The FujiNet build
 * links this in place of the CoGS files.
 *
 * This is our own driver, written from the firmware's documented SmartPort
 * protocol, so that newsiie stays MIT licensed: fujinet-lib is GPLv3.
 *
 * The protocol, as the FujiNet firmware implements it:
 *   - STATUS unit 0, code 0: the number of SmartPort units. STATUS code 3
 *     on a unit: its device information block, whose name is "NETWORK"
 *     for the network device and "THE_FUJI" for the adapter itself.
 *   - CONTROL 'O' on NETWORK opens a URL. The control list is a two-byte
 *     length, then the access mode (4, read), the translation (0, none)
 *     and the device spec "N:<url>", ending in a zero byte.
 *   - STATUS 'S' on NETWORK returns four bytes: the bytes waiting (16-bit),
 *     connected, and an error code (1 success, 136 end of file, others
 *     an error). The first STATUS after an HTTP open runs the request, so
 *     it can block for as long as the server takes to answer.
 *   - READ on NETWORK takes up to 512 bytes; ask for no more than waiting,
 *     and no more than FUJI_READ_MAX (below).
 *   - CONTROL 'C' on NETWORK closes it.
 *   - STATUS $FA on THE_FUJI: Wi-Fi state, 3 when connected. STATUS $E8:
 *     the adapter configuration, 140 bytes, the address at offset 97.
 */
#ifndef FUJI_H
#define FUJI_H

#include "port.h"

/* sp.s */
void __fastcall__ sp_set_entry(uint16_t entry);
uint8_t __fastcall__ sp_call(uint8_t command);
extern uint8_t  sp_parms[9];
extern uint16_t sp_count;
extern uint8_t  sp_raw_a;       /* A as the card left it, for the probe */

/* Results, kept clear of source.h's and of 0 (success). */
#define FN_E_NODEV      0x30    /* no FujiNet network device found */
#define FN_E_BUS        0x31    /* a SmartPort call failed: fuji_err holds the code */
#define FN_E_NET        0x32    /* the device reported an error: fuji_err holds it */
#define FN_E_NOWIFI     0x33    /* the adapter is not on Wi-Fi */

/* The most one READ asks for. For HTTP the firmware's "waiting" is the
 * body length still to come, not bytes it holds; it holds one 512-byte
 * piece of the response at a time. A READ for more than that piece's rest
 * makes it wait on the network inside the SmartPort call. With firmware
 * v1.6.1 on the bench, 512-byte reads of large feeds from the internet
 * ended the transfer early, and sometimes rebooted the FujiNet in the
 * middle of the call, which hangs the //e; 256 and 64 did not (PLAN.md
 * section 12). */
#define FUJI_READ_MAX   256

/* The network device's own codes (fuji_err for FN_E_NET). */
#define FN_ST_OK        1
#define FN_ST_EOF       136

extern uint8_t fuji_slot;       /* 0 until fuji_find() succeeds */
extern uint8_t fuji_net;        /* the NETWORK unit */
extern uint8_t fuji_dev;        /* the THE_FUJI unit, 0 if none */
extern uint8_t fuji_err;        /* detail for FN_E_BUS and FN_E_NET */

/* Look through the SmartPort cards, slot 7 down, for one with a NETWORK
 * unit, and remember it. 1 if found. Cheap after the first success. */
uint8_t fuji_find(void);

typedef struct {
    uint16_t avail;             /* bytes waiting */
    uint8_t  conn;
    uint8_t  err;               /* FN_ST_* or another device code */
} fuji_status_t;

/* The network stream. url is a full http:// or https:// URL. */
uint8_t fuji_open(const char *url);
uint8_t fuji_status(fuji_status_t *s);
uint8_t fuji_read(uint8_t *buf, uint16_t n);    /* sp_count: bytes read */
extern uint8_t fuji_read_pcount;                /* READ parameter count in use */
uint8_t fuji_close(void);

/* The adapter: 1 if it is on Wi-Fi. */
uint8_t fuji_wifi(void);

/* The adapter's address (4 bytes) and its firmware version (up to 14
 * characters and a zero), from its configuration. 0, or an FN_E_* code. */
uint8_t fuji_config(uint8_t ip[4], char version[15]);

/* Short uppercase text for an FN_E_* result, for the status bar. */
const char *fuji_strerror(uint8_t rc);

#endif
