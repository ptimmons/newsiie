/*
 * Word wrap for the Article view: text in, a table of screen lines out.
 *
 * Lines point into the caller's buffers, so nothing is copied. Each '\n'
 * in the text ends a paragraph, and each '\r' a line within one. A line breaks at the last space that fits;
 * a word longer than the width is split. Plain C, tested on the Mac.
 */
#ifndef WRAP_H
#define WRAP_H

#include "port.h"

#define WRAP_MAX    160             /* lines kept; more are dropped */

#define WK_TEXT     0
#define WK_RULE     1               /* a horizontal rule, no text */

typedef struct {
    const char *p;
    uint8_t     len;
    uint8_t     kind;
} wrap_line_t;

extern wrap_line_t wrap_lines[WRAP_MAX];
extern uint16_t    wrap_count;
extern uint8_t     wrap_full;       /* lines were dropped */

void wrap_reset(void);

/* Add n bytes of s, wrapped to width. With spaced set, paragraphs after
 * the first get a blank line before them, except list items ("- ..."),
 * which stay together. */
void wrap_add(const char *s, uint16_t n, uint8_t width, uint8_t spaced);

/* Add one line as it is, cut at width: no wrapping. */
void wrap_line(const char *s, uint16_t n, uint8_t width);

void wrap_blank(void);
void wrap_rule(void);

#endif
