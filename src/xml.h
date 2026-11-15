/*
 * A streaming XML scanner, just enough for feeds.
 *
 * Fed any number of chunks of any size; every state survives a chunk
 * boundary. It knows the elements feeds use (XE_*), tracks depth, and
 * reports each element's open and close to feed.c. Text is copied only
 * while feed.c has asked for it, by pointing xml_cap at a buffer; all
 * other text is skipped at scan() speed, entities and all.
 *
 * Not a validating parser: mismatched tags, stray '&' and unknown entities
 * are passed over, never fatal. A feed truncated mid-way yields every item
 * that finished.
 */
#ifndef XML_H
#define XML_H

#include "port.h"

/* Element ids. "atom:" prefixes are dropped before lookup. */
enum {
    XE_OTHER = 0,
    XE_RSS, XE_CHANNEL, XE_ITEM, XE_FEED, XE_ENTRY, XE_RDF, XE_HTML,
    XE_TITLE, XE_LINK, XE_DESCRIPTION, XE_SUMMARY, XE_CONTENT,
    XE_CONTENT_ENCODED, XE_PUBDATE, XE_PUBLISHED, XE_UPDATED, XE_DC_DATE,
    XE_GUID, XE_ID,
    /* Block elements inside Atom type="xhtml" content: each is a break. */
    XE_P, XE_BR, XE_DIV, XE_LI
};

/* Set by feed.c to capture text. While xml_cap is not NULL, text and CDATA
 * are appended (entities decoded, numeric ones as UTF-8) until xml_cap_max;
 * the rest is dropped. */
extern char    *xml_cap;
extern uint16_t xml_cap_len;
extern uint16_t xml_cap_max;

/* An Atom <link>'s attributes, valid during xml_open for XE_LINK. */
extern char xml_href[512];
extern char xml_rel[16];

/* Set from <?xml encoding="ISO-8859-1"?> (or windows-1252): bytes from $80
 * up are then Latin-1 characters, not UTF-8. */
extern uint8_t xml_latin1;

/* Set nonzero (by feed.c) to stop the scan at once. */
extern uint8_t xml_stop;

/* Called every few hundred bytes, for a caller that keeps a clock. */
extern void (*xml_tick)(void);

/* feed.c implements these. depth is 1 for the root element. */
void xml_open(uint8_t id, uint8_t depth);
void xml_close(uint8_t id, uint8_t depth);

void xml_reset(void);
void xml_feed(const uint8_t *p, uint16_t n);

#endif
