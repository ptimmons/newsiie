/*
 * Feed mapping: xml.c's element events to items. See feed.h.
 */
#include <string.h>
#include "feed.h"
#include "xml.h"
#include "clean.h"

/* Raw captures are larger than what is kept: escaped HTML shrinks a lot
 * when cleaned. */
#define RAW_TITLE   400
#define RAW_LINK    FEED_LINK_MAX
#define RAW_DATE    64
#define RAW_GUID    FEED_GUID_MAX
#define RAW_BODY    3072

enum { F_NONE, F_FTITLE, F_TITLE, F_LINK, F_DATE, F_GUID, F_BODY };

uint8_t  feed_kind;
char     feed_title[FEED_TITLE_MAX + 1];
uint16_t feed_items;
uint8_t (*feed_sink)(const feed_item_t *it);

/* The big buffers live in LOWBSS, $0800-$1FFF on the Apple (see
 * cfg/newsiie.cfg): never zeroed, and every one is written before it is
 * read. */
#ifdef __CC65__
#pragma bss-name (push, "LOWBSS")
#endif
static char raw_body[RAW_BODY + 1];
/* Lent out between fetches (see feed.h). */
char *const feed_scratch = raw_body;
static char out_body[FEED_BODY_MAX + 1];
static char raw_link[RAW_LINK + 1];
static char raw_title[RAW_TITLE + 1];
#ifdef __CC65__
#pragma bss-name (pop)
#endif

static char raw_date[RAW_DATE + 1];
static char raw_guid[RAW_GUID + 1];
static uint16_t len_title, len_link, len_date, len_guid, len_body;

static char out_title[FEED_TITLE_MAX + 1];
static char out_date[17];

static uint8_t cap_field, cap_depth;
static uint8_t chan_depth;
static uint8_t in_item, item_depth;
static uint8_t body_prio, date_prio;
static uint8_t have_ftitle;
static feed_item_t item;

void feed_begin(void)
{
    xml_reset();
    xml_href = out_body;            /* never live at the same time */
    feed_kind = FEED_UNKNOWN;
    feed_title[0] = 0;
    feed_items = 0;
    cap_field = F_NONE;
    chan_depth = 0;
    in_item = 0;
    have_ftitle = 0;
}

void feed_chunk(const uint8_t *p, uint16_t n)
{
    xml_feed(p, n);
}

uint8_t feed_end(void)
{
    /* An item cut off by the end of the data is dropped: its fields may
     * be half there. */
    xml_cap = NULL;
    return feed_kind;
}

static void capture(uint8_t field, char *buf, uint16_t max, uint8_t depth)
{
    cap_field = field;
    cap_depth = depth;
    xml_cap = buf;
    xml_cap_len = 0;
    xml_cap_max = max;
}

static void end_capture(void)
{
    switch (cap_field) {
    case F_FTITLE: raw_title[xml_cap_len] = 0; len_title = xml_cap_len; break;
    case F_TITLE:  raw_title[xml_cap_len] = 0; len_title = xml_cap_len; break;
    case F_LINK:   raw_link[xml_cap_len] = 0;  len_link = xml_cap_len;  break;
    case F_DATE:   raw_date[xml_cap_len] = 0;  len_date = xml_cap_len;  break;
    case F_GUID:   raw_guid[xml_cap_len] = 0;  len_guid = xml_cap_len;  break;
    case F_BODY:   raw_body[xml_cap_len] = 0;  len_body = xml_cap_len;  break;
    }
    if (cap_field == F_FTITLE) {
        clean(feed_title, sizeof feed_title, raw_title, len_title, CLEAN_LINE);
        have_ftitle = 1;
    }
    cap_field = F_NONE;
    xml_cap = NULL;
}

static void start_item(uint8_t depth)
{
    in_item = 1;
    item_depth = depth;
    len_title = len_link = len_date = len_guid = len_body = 0;
    raw_title[0] = raw_link[0] = raw_date[0] = raw_guid[0] = raw_body[0] = 0;
    body_prio = date_prio = 0;
}

/* A summary cut at a cap says so, as its own last paragraph. */
static const char cut_note[] = "\n[Shortened: too long for Newsiie to show in full.]";
#define CUT_LEN     (sizeof cut_note - 1)

static uint16_t mark_cut(uint16_t n)
{
    uint8_t back;

    if (n > FEED_BODY_MAX - CUT_LEN) {
        n = FEED_BODY_MAX - CUT_LEN;
    }
    /* End on a word if one ends nearby. */
    for (back = 0; back < 40 && n && out_body[n - 1] != ' '; ++back) {
        --n;
    }
    if (back == 40) {
        n += back;
    }
    while (n && (out_body[n - 1] == ' ' || out_body[n - 1] == '\n' || out_body[n - 1] == '\r')) {
        --n;
    }
    memcpy(out_body + n, cut_note, CUT_LEN + 1);
    return (uint16_t)(n + CUT_LEN);
}

static void end_item(void)
{
    uint16_t n;

    in_item = 0;

    n = clean(out_title, sizeof out_title, raw_title, len_title, CLEAN_LINE);
    if (len_title >= RAW_TITLE || n >= FEED_TITLE_MAX) {
        /* Cut: say so with "...". */
        if (n > FEED_TITLE_MAX - 3) {
            n = FEED_TITLE_MAX - 3;
        }
        memcpy(out_title + n, "...", 4);
    }
    if (!out_title[0]) {
        strcpy(out_title, "(no title)");
    }
    clean_trim(raw_link);
    clean_trim(raw_guid);
    clean_date(out_date, raw_date);
    item.title = out_title;
    item.link = raw_link;
    item.date = out_date;
    item.guid = raw_guid[0] ? raw_guid : raw_link;
    n = clean(out_body, sizeof out_body, raw_body, len_body, CLEAN_BODY);
    if (len_body >= RAW_BODY || n >= FEED_BODY_MAX) {
        n = mark_cut(n);
    }
    item.body_len = n;
    item.body = out_body;

    ++feed_items;
    if (feed_sink && feed_sink(&item)) {
        xml_stop = 1;
    }
}

void xml_open(uint8_t id, uint8_t depth)
{
    if (depth == 1) {
        switch (id) {
        case XE_RSS:  feed_kind = FEED_RSS; break;
        case XE_FEED: feed_kind = FEED_ATOM; chan_depth = 1; break;
        case XE_RDF:  feed_kind = FEED_RDF; break;
        default:
            feed_kind = FEED_NOT;
            xml_stop = 1;
            return;
        }
    }

    if (cap_field) {
        /* An element inside captured text: Atom type="xhtml" markup. Its
         * block elements become breaks; clean() makes the rest whitespace. */
        if (cap_field == F_BODY && xml_cap_len + 4 < xml_cap_max) {
            if (id == XE_P || id == XE_BR || id == XE_DIV) {
                xml_cap[xml_cap_len++] = '<';
                xml_cap[xml_cap_len++] = 'p';
                xml_cap[xml_cap_len++] = '>';
            } else if (id == XE_LI) {
                xml_cap[xml_cap_len++] = '<';
                xml_cap[xml_cap_len++] = 'l';
                xml_cap[xml_cap_len++] = 'i';
                xml_cap[xml_cap_len++] = '>';
            }
        }
        return;
    }

    if (id == XE_CHANNEL) {
        chan_depth = depth;
        return;
    }
    if (id == XE_ITEM || id == XE_ENTRY) {
        start_item(depth);
        return;
    }

    if (!in_item) {
        if (id == XE_TITLE && !have_ftitle && chan_depth && depth == chan_depth + 1) {
            capture(F_FTITLE, raw_title, RAW_TITLE, depth);
        }
        return;
    }
    if (depth != item_depth + 1) {
        return;                     /* <source><title>, <media:group>, ... */
    }

    switch (id) {
    case XE_TITLE:
        capture(F_TITLE, raw_title, RAW_TITLE, depth);
        break;
    case XE_LINK:
        if (xml_href[0]) {
            /* Atom: the link is an attribute, and the element is empty. */
            if (!raw_link[0] && (!xml_rel[0] || !strcmp(xml_rel, "alternate"))) {
                strncpy(raw_link, xml_href, RAW_LINK);
                raw_link[RAW_LINK] = 0;
            }
        } else if (!raw_link[0]) {
            capture(F_LINK, raw_link, RAW_LINK, depth);
        }
        break;
    case XE_DESCRIPTION:
    case XE_SUMMARY:
        if (body_prio < 2) {
            body_prio = 2;
            capture(F_BODY, raw_body, RAW_BODY, depth);
        }
        break;
    case XE_CONTENT:
    case XE_CONTENT_ENCODED:
        if (body_prio < 1) {
            body_prio = 1;
            capture(F_BODY, raw_body, RAW_BODY, depth);
        }
        break;
    case XE_PUBDATE:
    case XE_PUBLISHED:
    case XE_DC_DATE:
        if (date_prio < 2) {
            date_prio = 2;
            capture(F_DATE, raw_date, RAW_DATE, depth);
        }
        break;
    case XE_UPDATED:
        if (date_prio < 1) {
            date_prio = 1;
            capture(F_DATE, raw_date, RAW_DATE, depth);
        }
        break;
    case XE_GUID:
    case XE_ID:
        if (!raw_guid[0]) {
            capture(F_GUID, raw_guid, RAW_GUID, depth);
        }
        break;
    }
}

void xml_close(uint8_t id, uint8_t depth)
{
    if (cap_field) {
        if (depth > cap_depth) {
            if (cap_field == F_BODY && (id == XE_P || id == XE_DIV || id == XE_LI) &&
                xml_cap_len + 3 < xml_cap_max) {
                xml_cap[xml_cap_len++] = '<';
                xml_cap[xml_cap_len++] = 'p';
                xml_cap[xml_cap_len++] = '>';
            }
            return;
        }
        end_capture();
    }
    if (in_item && depth <= item_depth && (id == XE_ITEM || id == XE_ENTRY)) {
        end_item();
    } else if (in_item && depth < item_depth) {
        in_item = 0;                /* malformed: left the item unclosed */
    }
}
