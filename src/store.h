/*
 * The item store: one feed's items, in auxiliary memory.
 *
 * Records are packed one after another in aux $0800-$BFFF (47K), and a
 * small index in main memory holds where each starts. Items are added as
 * the parser closes them and the store is cleared before each fetch, so
 * there is no deletion and no fragmentation. When the next item would not
 * fit, store_add() says so and the fetch stops: what fitted is kept.
 *
 * A record:
 *   u16 length of the whole record
 *   u8  flags (STORE_READ)
 *   u16 hash of the guid, for read markers
 *   u8  title length, title
 *   u8  date length (0 or 16), date
 *   u16 link length, link
 *   u16 body length, body
 */
#ifndef STORE_H
#define STORE_H

#include "port.h"
#include "feed.h"

#define STORE_MAX_ITEMS 200
#define STORE_READ      0x01

/* What a list row needs, read back from aux in one copy. */
typedef struct {
    uint8_t  flags;
    uint16_t hash;
    char     title[FEED_TITLE_MAX + 1];
    char     date[17];
} store_head_t;

extern uint16_t store_count;
extern uint8_t  store_full;         /* a fetch stopped because aux filled */

/* Check for 128K and claim auxiliary memory, disconnecting ProDOS's /RAM
 * disk, which lives there. Returns 0, or a message saying why not. */
const char *store_init(void);

void store_reset(void);

/* A feed_sink: copy the item into aux. Returns 1 (stop) when full. */
uint8_t store_add(const feed_item_t *it);

void store_head(uint16_t i, store_head_t *h);

/* Copy item i's body into dst (up to cap - 1 bytes, NUL-terminated).
 * Returns its length. */
uint16_t store_body(uint16_t i, char *dst, uint16_t cap);
uint16_t store_link(uint16_t i, char *dst, uint16_t cap);

/* aux bytes still free */
uint16_t store_free(void);

#endif
