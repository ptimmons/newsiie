/*
 * RSS 2.0, RSS 1.0 (RDF) and Atom, mapped onto one item shape.
 *
 * Feed the bytes of a feed through feed_chunk() in any pieces. Each item
 * is cleaned (see clean.h) and handed to feed_sink as it closes; the sink
 * returns nonzero to stop the parse (a full store, say). Nothing else is
 * kept, so memory does not grow with the feed.
 *
 * Which element fills which field, for direct children of item/entry:
 *   title    <title>
 *   link     RSS <link> text; Atom <link href> with rel="alternate" or no rel
 *   body     <description> or <summary>, preferred because they are short;
 *            <content:encoded> or <content> only if neither comes
 *   date     <pubDate>, <published> or <dc:date>; <updated> if none
 *   guid     <guid> or <id>
 */
#ifndef FEED_H
#define FEED_H

#include "port.h"

#define FEED_UNKNOWN    0   /* no root element yet */
#define FEED_RSS        1
#define FEED_ATOM       2
#define FEED_RDF        3
#define FEED_NOT        4   /* the root is something else: HTML, say */

/* Caps on what is kept per item, after cleaning. */
#define FEED_TITLE_MAX  160
#define FEED_LINK_MAX   511     /* BBC links run past 400 with tracking */
#define FEED_GUID_MAX   255
#define FEED_BODY_MAX   2000    /* summaries are rarely past 1K */

typedef struct {
    const char *title;              /* never empty: "(no title)" if need be */
    const char *link;
    const char *date;               /* "YYYY-MM-DD HH:MM", or "" */
    const char *guid;
    const char *body;               /* lines separated by '\n' */
    uint16_t    body_len;
} feed_item_t;

extern uint8_t  feed_kind;
extern char     feed_title[FEED_TITLE_MAX + 1];
extern uint16_t feed_items;         /* items handed to the sink */
extern uint8_t (*feed_sink)(const feed_item_t *it);

void feed_begin(void);
void feed_chunk(const uint8_t *p, uint16_t n);
uint8_t feed_end(void);             /* returns feed_kind */

#endif
