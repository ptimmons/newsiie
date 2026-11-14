/*
 * The CoGS card: register primitives (cogs_io.s), the frame protocol and
 * the commands newsiie uses (cogs.c), and the slot scan (slot.c).
 *
 * Every command returns a result code: 0 for success, a card code from the
 * Technical Reference Appendix A (1..20), or one of the COGS_E_* codes below
 * for failures the card itself never reports.
 */
#ifndef COGS_H
#define COGS_H

#include "port.h"

/* ---- cogs_io.s (replaced by a software model in the host tests) ---- */
void __fastcall__ cogs_set_slot(uint8_t slot);
uint8_t __fastcall__ cogs_probe(uint8_t slot);
uint8_t cogs_status(void);
int cogs_get(void);
uint8_t __fastcall__ cogs_getn(uint8_t *dst, uint8_t n);
uint8_t __fastcall__ cogs_skip(uint16_t n);
uint8_t __fastcall__ cogs_putn(const uint8_t *src, uint16_t n);
uint8_t cogs_resync(void);
void cogs_irq_off(void);

/* STATUS register bits */
#define COGS_ST_RX_AVAIL    0x80
#define COGS_ST_TX_READY    0x40
#define COGS_ST_LINK_UP     0x10
#define COGS_ST_ERROR       0x08

/* ---- result codes ---- */
#define COGS_OK             0
#define COGS_E_NOT_JOINED   1
#define COGS_E_UNSUPPORTED  11
#define COGS_E_LAST_CARD    20      /* highest code the card defines */
#define COGS_E_TIMEOUT      0xF0    /* the card stopped answering mid-frame */
#define COGS_E_PROTO        0xF1    /* a reply too short, or none at all */
#define COGS_E_NOCARD       0xF2

/* Short uppercase text for a result code, for the status bar. */
const char *cogs_strerror(uint8_t code);

/* ---- slot.c (Apple only) ---- */

/* The highest slot holding a CoGS card, skipping Disk II controllers.
 * 0 if there is none. */
uint8_t cogs_find(void);

/* Nonzero if slot s holds a Disk II controller, whose I/O switches must
 * never be read: they move the drive head. */
uint8_t slot_is_disk2(uint8_t s);

/* ---- cogs.c ---- */

extern uint8_t cogs_slot;           /* 0 until cogs_open() succeeds */

/* Select the card in slot s, mask IRQs, and reset its frame parser and
 * both FIFOs (never the soft reset, which would also abort a network disk
 * ProDOS may be reading). */
uint8_t cogs_open(uint8_t s);

/* Send one command frame (pre and body are concatenated as the payload)
 * and read frames until its reply arrives. Events and unrelated replies
 * that arrive first are skipped. Up to cap reply bytes are copied to buf
 * and the rest are discarded; *got (if not NULL) is the number copied. */
uint8_t cogs_request(uint8_t op, const uint8_t *pre, uint8_t npre,
                     const uint8_t *body, uint16_t nbody,
                     uint8_t *buf, uint16_t cap, uint16_t *got);

typedef struct {
    uint8_t fw_major, fw_minor;
    uint8_t proto;                  /* $04 */
    uint8_t caps;                   /* bit 0 NET, 1 TLS, 2 HTTP, 3 HASH, ... */
} cogs_ping_t;

typedef struct {
    uint8_t  link;                  /* 0 down, 1 joining, 2 up with an address */
    uint8_t  ip[4];
    int8_t   rssi;                  /* dBm */
    uint16_t heap_kb;
    uint8_t  sockets;
    uint8_t  has_uptime;            /* the 17-byte form of the reply */
    uint32_t uptime;                /* seconds */
} cogs_netstat_t;

#define COGS_LINK_DOWN      0
#define COGS_LINK_JOINING   1
#define COGS_LINK_UP        2

uint8_t cogs_ping(cogs_ping_t *p);
uint8_t cogs_netstat(cogs_netstat_t *s);
uint8_t cogs_time(uint32_t *unix_utc);             /* 0 until NTP has synced */
uint8_t cogs_localtime(uint8_t *valid, uint32_t *secs_1904);
uint8_t cogs_random(uint8_t *dst, uint8_t n);      /* n is 1..255 */
uint8_t cogs_sha256(const uint8_t *src, uint16_t n, uint8_t digest[32]);

#endif
