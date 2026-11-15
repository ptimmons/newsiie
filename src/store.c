/*
 * The item store. See store.h.
 */
#include <string.h>
#include "store.h"

/* auxmem.s on the Apple; a plain array in the host tests. */
void aux_init(void);
void __fastcall__ aux_put(uint16_t aux_dst, const void *src, uint16_t n);
void __fastcall__ aux_get(void *dst, uint16_t aux_src, uint16_t n);

#define AUX_BASE    0x0800
#define AUX_END     0xC000      /* never read at or past this: it is I/O */
#define HEAD_MAX    (6 + FEED_TITLE_MAX + 1 + 16)

uint16_t store_count;
uint8_t  store_full;

static uint16_t idx[STORE_MAX_ITEMS];
static uint16_t next;
static uint8_t  b[HEAD_MAX];

#ifdef __CC65__
/* ProDOS 8's global page. */
#define MACHID      (*(volatile uint8_t *)0xBF98)
#define DEVCNT      (*(volatile uint8_t *)0xBF31)
#define DEVLST      ((volatile uint8_t *)0xBF32)
#define DEVADR32    (*(volatile uint16_t *)0xBF26)  /* slot 3, drive 2 */
#define DEVADR01    (*(volatile uint16_t *)0xBF10)  /* slot 0: "no device" */
#define RAM_DRIVER  0xFF00                          /* ProDOS's /RAM */
#endif

const char *store_init(void)
{
#ifdef __CC65__
    uint8_t i, n;

    if ((MACHID & 0x30) != 0x30) {
        return "NEWSIIE NEEDS 128K";
    }
    /* On a 128K machine ProDOS puts /RAM in the auxiliary memory the store
     * uses. Disconnect it: point slot 3 drive 2 at the no-device routine
     * and take its entry out of the device list. Something other than
     * /RAM there (a RAM card's own driver) is left alone, and refused. */
    if (DEVADR32 != DEVADR01) {
        if (DEVADR32 != RAM_DRIVER) {
            return "AUX MEMORY IS IN USE BY A RAM DISK";
        }
        n = DEVCNT;                 /* the count less one */
        for (i = 0; i <= n; ++i) {
            if ((DEVLST[i] & 0xF0) == 0xB0) {
                for (; i < n; ++i) {
                    DEVLST[i] = DEVLST[i + 1];
                }
                DEVLST[n] = 0;
                DEVCNT = (uint8_t)(n - 1);
                break;
            }
        }
        DEVADR32 = DEVADR01;
    }
#endif
    aux_init();
    store_reset();
    return 0;
}

void store_reset(void)
{
    store_count = 0;
    store_full = 0;
    next = AUX_BASE;
}

uint16_t store_free(void)
{
    return (uint16_t)(AUX_END - next);
}

static uint16_t hash16(const char *s)
{
    uint16_t h = 0x2B5D;

    while (*s) {
        h = (uint16_t)((h << 5) + h + (uint8_t)*s++);    /* h * 33 + c */
    }
    return h;
}

static uint16_t put(uint16_t at, const void *src, uint16_t n)
{
    if (n) {
        aux_put(at, src, n);
    }
    return (uint16_t)(at + n);
}

uint8_t store_add(const feed_item_t *it)
{
    uint8_t tl = (uint8_t)strlen(it->title);
    uint8_t dl = (uint8_t)strlen(it->date);
    uint16_t ll = (uint16_t)strlen(it->link);
    uint16_t bl = it->body_len;
    uint16_t len = (uint16_t)(6 + tl + 1 + dl + 2 + ll + 2 + bl);
    uint16_t h = hash16(it->guid), at;

    if (store_count >= STORE_MAX_ITEMS || len > (uint16_t)(AUX_END - next)) {
        store_full = 1;
        return 1;
    }
    b[0] = (uint8_t)len;
    b[1] = (uint8_t)(len >> 8);
    b[2] = 0;
    b[3] = (uint8_t)h;
    b[4] = (uint8_t)(h >> 8);
    b[5] = tl;
    at = put(next, b, 6);
    at = put(at, it->title, tl);
    b[0] = dl;
    at = put(at, b, 1);
    at = put(at, it->date, dl);
    b[0] = (uint8_t)ll;
    b[1] = (uint8_t)(ll >> 8);
    at = put(at, b, 2);
    at = put(at, it->link, ll);
    b[0] = (uint8_t)bl;
    b[1] = (uint8_t)(bl >> 8);
    at = put(at, b, 2);
    put(at, it->body, bl);

    idx[store_count++] = next;
    next = (uint16_t)(next + len);
    return 0;
}

/* Read the front of record i into b. Never past AUX_END. */
static uint16_t front(uint16_t i)
{
    uint16_t at = idx[i];
    uint16_t n = (uint16_t)(AUX_END - at);

    if (n > HEAD_MAX) {
        n = HEAD_MAX;
    }
    aux_get(b, at, n);
    return at;
}

void store_head(uint16_t i, store_head_t *h)
{
    /* Held in a static: with h left as a parameter, this cc65 reloaded it
     * from the C stack with Y = 0 (see tools/cc65check.py) and memcpy()
     * wrote the title into the program's code. */
    static store_head_t *hp;
    uint8_t tl, dl;

    hp = h;
    front(i);
    hp->flags = b[2];
    hp->hash = (uint16_t)(b[3] | (b[4] << 8));
    tl = b[5];
    memcpy(hp->title, b + 6, tl);
    hp->title[tl] = 0;
    dl = b[6 + tl];
    memcpy(hp->date, b + 7 + tl, dl);
    hp->date[dl] = 0;
}

/* The aux address of item i's link length, from the record's front. */
static uint16_t link_at(uint16_t i)
{
    uint16_t at = front(i);
    uint8_t tl = b[5];

    return (uint16_t)(at + 7 + tl + b[6 + tl]);
}

static uint16_t copy_field(uint16_t at, char *dst, uint16_t cap)
{
    uint8_t l[2];
    uint16_t n;

    aux_get(l, at, 2);
    n = (uint16_t)(l[0] | (l[1] << 8));
    if (n > cap - 1) {
        n = (uint16_t)(cap - 1);
    }
    if (n) {
        aux_get(dst, (uint16_t)(at + 2), n);
    }
    dst[n] = 0;
    return n;
}

uint16_t store_link(uint16_t i, char *dst, uint16_t cap)
{
    return copy_field(link_at(i), dst, cap);
}

uint16_t store_body(uint16_t i, char *dst, uint16_t cap)
{
    uint16_t at = link_at(i);
    uint8_t l[2];

    aux_get(l, at, 2);
    return copy_field((uint16_t)(at + 2 + (l[0] | (l[1] << 8))), dst, cap);
}
