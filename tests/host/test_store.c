/*
 * Host tests for src/store.c, with auxiliary memory modelled as an array.
 * The model fails any access outside the store's aux, $0800-$AFFF: above
 * it is the feed list, and at $C000 and up a read would hit I/O switches.
 */
#include <stdio.h>
#include <string.h>
#include "store.h"

static int fails, checks;
#define CHECK(c) do { ++checks; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static uint8_t aux[0x10000];
static int out_of_bounds;

static void bounds(uint16_t at, uint16_t n)
{
    if (at < STORE_BASE || (uint32_t)at + n > STORE_END) {
        ++out_of_bounds;
    }
}

void aux_init(void) {}
void aux_put(uint16_t dst, const void *src, uint16_t n) { bounds(dst, n); memcpy(aux + dst, src, n); }
void aux_get(void *dst, uint16_t src, uint16_t n) { bounds(src, n); memcpy(dst, aux + src, n); }

static char body[FEED_BODY_MAX + 1], link[FEED_LINK_MAX + 1], title[FEED_TITLE_MAX + 1];

static feed_item_t make(unsigned i, unsigned blen, unsigned llen)
{
    feed_item_t it;
    unsigned k;

    snprintf(title, sizeof title, "Item %u title", i);
    for (k = 0; k < llen; ++k) link[k] = (char)('a' + (i + k) % 26);
    link[llen] = 0;
    for (k = 0; k < blen; ++k) body[k] = (char)('A' + (i * 7 + k) % 26);
    body[blen] = 0;
    it.title = title;
    it.link = link;
    it.date = i % 3 ? "2026-11-14 16:42" : "";
    it.guid = link;
    it.body = body;
    it.body_len = (uint16_t)blen;
    return it;
}

int main(void)
{
    static char got[FEED_BODY_MAX + 1];
    store_head_t h;
    feed_item_t it;
    unsigned i, k, n;

    CHECK(store_init() == 0);
    CHECK(store_count == 0 && store_free() == STORE_END - STORE_BASE);

    /* Round trip: every field of every item comes back as it went in. */
    for (i = 0; i < 20; ++i) {
        it = make(i, 100 + i * 37, 20 + i * 11);
        CHECK(store_add(&it) == 0);
    }
    CHECK(store_count == 20 && !store_full);
    for (i = 0; i < 20; ++i) {
        it = make(i, 100 + i * 37, 20 + i * 11);
        store_head((uint16_t)i, &h);
        CHECK(!strcmp(h.title, it.title));
        CHECK(!strcmp(h.date, it.date));
        CHECK(h.flags == 0);
        n = store_body((uint16_t)i, got, sizeof got);
        CHECK(n == it.body_len && !strcmp(got, it.body));
        n = store_link((uint16_t)i, got, sizeof got);
        CHECK(n == strlen(it.link) && !strcmp(got, it.link));
    }
    /* Same guid, same hash; different guid, (here) different hash. */
    store_head(0, &h);
    k = h.hash;
    store_head(1, &h);
    CHECK(k != h.hash);

    /* A short buffer gets a truncated, terminated copy. */
    n = store_body(19, got, 10);
    CHECK(n == 9 && strlen(got) == 9);

    /* Fill it: big items until one does not fit. The store says full, keeps
     * what fitted, and reading the last record (near $BFFF) stays inside
     * aux. */
    store_reset();
    for (i = 0; i < 1000; ++i) {
        it = make(i, FEED_BODY_MAX, FEED_LINK_MAX);
        if (store_add(&it)) {
            break;
        }
    }
    CHECK(store_full && store_count == i && store_count > 15);
    CHECK(store_free() < 6 + 16 + 1 + 16 + 2 + FEED_LINK_MAX + 2 + FEED_BODY_MAX);
    for (i = 0; i < store_count; ++i) {
        store_head((uint16_t)i, &h);
        store_body((uint16_t)i, got, sizeof got);
    }
    it = make(store_count - 1, FEED_BODY_MAX, FEED_LINK_MAX);
    CHECK(!strcmp(got, it.body));

    /* The item cap. */
    store_reset();
    for (i = 0; i < STORE_MAX_ITEMS + 5; ++i) {
        it = make(i, 0, 0);
        if (store_add(&it)) {
            break;
        }
    }
    CHECK(store_count == STORE_MAX_ITEMS && store_full);
    store_head(STORE_MAX_ITEMS - 1, &h);
    {
        char want[32];
        snprintf(want, sizeof want, "Item %u title", STORE_MAX_ITEMS - 1);
        CHECK(!strcmp(h.title, want));
    }

    CHECK(out_of_bounds == 0);

    if (fails) {
        printf("%d of %d store checks FAILED\n", fails, checks);
        return 1;
    }
    printf("all %d store checks passed\n", checks);
    return 0;
}
