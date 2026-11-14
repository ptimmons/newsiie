/*
 * The CoGS frame protocol (Technical Reference chapters 3 to 5).
 *
 *   frame = OPCODE LEN_LO LEN_HI PAYLOAD[LEN]
 *
 * Commands are $00-$7F. A reply is the command's opcode with bit 7 set.
 * $90-$9F are events, which can arrive ahead of any reply and are skipped
 * here; nothing newsiie does yet has a socket open, so none matter.
 * A command the card cannot parse gets no reply of its own: it gets an
 * ERROR event ($93) with socket $FF and a code.
 *
 * The card never interleaves two frames, so the reader handles one whole
 * frame at a time. If a frame stops arriving partway (a byte never comes),
 * our view of the stream is lost: reset the parser and both FIFOs.
 *
 * Plain C over the cogs_io primitives, so the host tests can run it against
 * a model of the card.
 */
#include <stddef.h>
#include "cogs.h"

#define OP_PING         0x00
#define OP_STATUS       0x01
#define OP_TIME         0x05
#define OP_LOCALTIME    0x0F
#define OP_HASH         0x30
#define OP_RANDOM       0x31

#define EV_ERROR        0x93
#define SOCK_NONE       0xFF

/* Frames to skip while waiting for a reply before deciding it is not
 * coming. Events are rare while no socket is open. */
#define MAX_SKIPPED     64

uint8_t cogs_slot;

static uint8_t hdr[3];

static const char *const card_err[COGS_E_LAST_CARD + 1] = {
    "OK",
    "NO NETWORK",               /* 1  NOT_JOINED */
    "DNS FAILED",               /* 2  DNS_FAIL */
    "TIMED OUT",                /* 3  CONN_TIMEOUT */
    "CONNECTION REFUSED",       /* 4  CONN_REFUSED */
    "TLS FAILED",               /* 5  TLS_HANDSHAKE */
    "BAD CERTIFICATE",          /* 6  CERT_VERIFY */
    "NO FREE SOCKET",           /* 7  NO_FREE_SOCK */
    "SOCKET STATE",             /* 8  SOCKET_STATE */
    "CARD BUFFER OVERRUN",      /* 9  BUF_OVERRUN */
    "BAD FRAME",                /* 10 BAD_FRAME */
    "NOT SUPPORTED BY CARD",    /* 11 UNSUPPORTED */
    "CARD INTERNAL ERROR",      /* 12 INTERNAL */
    "NOTHING RETAINED",         /* 13 NO_RETAINED */
    "NO SUCH JSON PATH",        /* 14 JSON_NOPATH */
    "CARD BUSY",                /* 15 BUSY */
    "NO VOLUME",                /* 16 VOL_NOMOUNT */
    "BLOCK OUT OF RANGE",       /* 17 VOL_RANGE */
    "NO STREAM OPEN",           /* 18 STREAM_NONE */
    "WINDOW SHORT",             /* 19 WIN_SHORT */
    "ALREADY MOUNTED",          /* 20 VOL_DUP */
};

const char *cogs_strerror(uint8_t code)
{
    if (code <= COGS_E_LAST_CARD) {
        return card_err[code];
    }
    switch (code) {
    case COGS_E_TIMEOUT: return "CARD STOPPED ANSWERING";
    case COGS_E_PROTO:   return "CARD PROTOCOL ERROR";
    case COGS_E_NOCARD:  return "NO COGS CARD";
    }
    return "CARD ERROR";
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)le16(p) | ((uint32_t)le16(p + 2) << 16);
}

/* Lost frame alignment: start again from a clean parser and empty FIFOs. */
static uint8_t lost(void)
{
    cogs_resync();
    return COGS_E_TIMEOUT;
}

uint8_t cogs_open(uint8_t s)
{
    cogs_slot = 0;
    cogs_set_slot(s);
    cogs_irq_off();
    if (cogs_resync()) {
        return COGS_E_TIMEOUT;
    }
    cogs_slot = s;
    return COGS_OK;
}

/* Read n bytes in pieces cogs_getn can take. Nonzero on timeout. */
static uint8_t get_bytes(uint8_t *dst, uint16_t n)
{
    uint8_t piece;

    while (n) {
        piece = n > 255 ? 255 : (uint8_t)n;
        if (cogs_getn(dst, piece)) {
            return 1;
        }
        dst += piece;
        n -= piece;
    }
    return 0;
}

uint8_t cogs_request(uint8_t op, const uint8_t *pre, uint8_t npre,
                     const uint8_t *body, uint16_t nbody,
                     uint8_t *buf, uint16_t cap, uint16_t *got)
{
    uint16_t len, n;
    uint8_t rop, skipped, err[2];

    if (got) {
        *got = 0;
    }
    len = (uint16_t)(npre + nbody);
    hdr[0] = op;
    hdr[1] = (uint8_t)len;
    hdr[2] = (uint8_t)(len >> 8);
    if (cogs_putn(hdr, 3) ||
        (npre && cogs_putn(pre, npre)) ||
        (nbody && cogs_putn(body, nbody))) {
        return lost();
    }

    for (skipped = 0; skipped < MAX_SKIPPED; ++skipped) {
        if (get_bytes(hdr, 3)) {
            /* Nothing at all, or a header cut short. Either way the card
             * is not where we think it is. */
            return lost();
        }
        rop = hdr[0];
        len = le16(hdr + 1);

        if (rop == (uint8_t)(op | 0x80)) {
            n = len < cap ? len : cap;
            if (get_bytes(buf, n) || (len > n && cogs_skip(len - n))) {
                return lost();
            }
            if (got) {
                *got = n;
            }
            return COGS_OK;
        }

        if (rop == EV_ERROR && len >= 2) {
            if (get_bytes(err, 2) || cogs_skip(len - 2)) {
                return lost();
            }
            if (err[0] == SOCK_NONE) {
                /* The card refused the command we just sent. */
                return err[1] ? err[1] : COGS_E_PROTO;
            }
            continue;
        }

        /* Every card-to-host opcode has bit 7 set. Anything else means we
         * are reading the middle of a frame. */
        if (!(rop & 0x80)) {
            return lost();
        }
        if (len && cogs_skip(len)) {
            return lost();
        }
    }
    return COGS_E_PROTO;
}

uint8_t cogs_ping(cogs_ping_t *p)
{
    uint8_t r[4];
    uint16_t got;
    uint8_t rc = cogs_request(OP_PING, NULL, 0, NULL, 0, r, sizeof r, &got);

    if (rc) {
        return rc;
    }
    if (got < 4) {
        return COGS_E_PROTO;
    }
    p->fw_major = r[0];
    p->fw_minor = r[1];
    p->proto = r[2];
    p->caps = r[3];
    return COGS_OK;
}

uint8_t cogs_netstat(cogs_netstat_t *s)
{
    uint8_t r[17];
    uint16_t got;
    uint8_t rc = cogs_request(OP_STATUS, NULL, 0, NULL, 0, r, sizeof r, &got);

    if (rc) {
        return rc;
    }
    if (got < 9) {
        return COGS_E_PROTO;
    }
    s->link = r[0];
    s->ip[0] = r[1];
    s->ip[1] = r[2];
    s->ip[2] = r[3];
    s->ip[3] = r[4];
    s->rssi = (int8_t)r[5];
    s->heap_kb = le16(r + 6);
    s->sockets = r[8];
    s->has_uptime = (uint8_t)(got >= 17);
    s->uptime = s->has_uptime ? le32(r + 13) : 0;
    return COGS_OK;
}

uint8_t cogs_time(uint32_t *unix_utc)
{
    uint8_t r[4];
    uint16_t got;
    uint8_t rc = cogs_request(OP_TIME, NULL, 0, NULL, 0, r, sizeof r, &got);

    if (rc) {
        return rc;
    }
    if (got < 4) {
        return COGS_E_PROTO;
    }
    *unix_utc = le32(r);
    return COGS_OK;
}

uint8_t cogs_localtime(uint8_t *valid, uint32_t *secs_1904)
{
    uint8_t r[6];
    uint16_t got;
    uint8_t rc = cogs_request(OP_LOCALTIME, NULL, 0, NULL, 0, r, sizeof r, &got);

    if (rc) {
        return rc;
    }
    if (got < 6) {
        return COGS_E_PROTO;
    }
    *valid = r[0];
    *secs_1904 = le32(r + 2);
    return COGS_OK;
}

uint8_t cogs_random(uint8_t *dst, uint8_t n)
{
    uint16_t got;
    uint8_t rc = cogs_request(OP_RANDOM, &n, 1, NULL, 0, dst, n, &got);

    if (rc) {
        return rc;
    }
    return got == n ? COGS_OK : COGS_E_PROTO;
}

uint8_t cogs_sha256(const uint8_t *src, uint16_t n, uint8_t digest[32])
{
    uint8_t pre[3];
    uint16_t got;
    uint8_t rc;

    pre[0] = 0;                     /* algo: single SHA-256 */
    pre[1] = (uint8_t)n;
    pre[2] = (uint8_t)(n >> 8);
    rc = cogs_request(OP_HASH, pre, 3, src, n, digest, 32, &got);
    if (rc) {
        return rc;
    }
    return got == 32 ? COGS_OK : COGS_E_PROTO;
}
