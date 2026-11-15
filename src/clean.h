/*
 * Turning captured feed text into what the //e can show.
 *
 * Feed text is UTF-8 by the time it gets here (xml.c converts Latin-1),
 * and titles and bodies are often HTML that was escaped once for the XML.
 * clean() strips the tags, decodes HTML entities, folds Unicode to the
 * nearest ASCII (curly quotes to straight ones, accented letters to plain,
 * dashes to hyphens; anything with no fair equivalent to '?'), and
 * collapses whitespace.
 */
#ifndef CLEAN_H
#define CLEAN_H

#include "port.h"

#define CLEAN_LINE  0   /* one line: every break becomes a space */
#define CLEAN_BODY  1   /* block tags become '\n' (a paragraph), <br> '\r'
                         * (a line inside one), <li> "- " */

/* Clean n bytes of src into dst (at most cap - 1 bytes, NUL-terminated).
 * src and dst must not overlap. Returns the length written. */
uint16_t clean(char *dst, uint16_t cap, const char *src, uint16_t n, uint8_t mode);

/* Strip leading and trailing whitespace in place (links and guids). */
void clean_trim(char *s);

/* "YYYY-MM-DD HH:MM" from an RFC 822 date ("Fri, 13 Nov 2026 16:39:33
 * +0000") or an ISO 8601 one ("2026-11-14T16:44:44-05:00"), as written:
 * the time zone is dropped, not applied. dst holds 17 bytes. Returns 0 and
 * an empty string if neither form fits. */
uint8_t clean_date(char *dst, const char *src);

/* The ASCII stand-in for a code point, for the tests. */
const char *clean_fold(uint16_t cp);
extern const uint16_t clean_latin1_len;     /* 128, checked by the tests */

#endif
