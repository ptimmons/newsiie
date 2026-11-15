/*
 * Word wrap. See wrap.h.
 */
#include "wrap.h"

wrap_line_t wrap_lines[WRAP_MAX];
uint16_t    wrap_count;
uint8_t     wrap_full;

void wrap_reset(void)
{
    wrap_count = 0;
    wrap_full = 0;
}

static void add(const char *p, uint8_t len, uint8_t kind)
{
    if (wrap_count >= WRAP_MAX) {
        wrap_full = 1;
        return;
    }
    wrap_lines[wrap_count].p = p;
    wrap_lines[wrap_count].len = len;
    wrap_lines[wrap_count].kind = kind;
    ++wrap_count;
}

void wrap_blank(void)
{
    add("", 0, WK_TEXT);
}

void wrap_rule(void)
{
    add("", 0, WK_RULE);
}

void wrap_line(const char *s, uint16_t n, uint8_t width)
{
    add(s, (uint8_t)(n > width ? width : n), WK_TEXT);
}

/* One line of a paragraph: no '\n' or '\r' inside. */
static void lines(const char *s, uint16_t n, uint8_t width)
{
    uint16_t k;

    if (!n) {
        wrap_blank();
        return;
    }
    while (n > width) {
        /* The last space at or before the width: a space exactly at the
         * width still lets the line use every column. */
        for (k = width; k && s[k] != ' '; --k) {
        }
        if (k) {
            add(s, (uint8_t)k, WK_TEXT);
            s += k + 1;
            n -= k + 1;
        } else {
            add(s, width, WK_TEXT);  /* one word wider than the screen */
            s += width;
            n -= width;
        }
    }
    if (n) {
        add(s, (uint8_t)n, WK_TEXT);
    }
}

/* One paragraph: no '\n' inside, but a '\r' (an HTML <br>) starts a new
 * line with no blank before it. */
static void para(const char *s, uint16_t n, uint8_t width)
{
    uint16_t k;

    for (;;) {
        for (k = 0; k < n && s[k] != '\r'; ++k) {
        }
        lines(s, k, width);
        if (k >= n) {
            return;
        }
        s += k + 1;
        n -= k + 1;
    }
}

void wrap_add(const char *s, uint16_t n, uint8_t width, uint8_t spaced)
{
    uint16_t k;
    uint8_t first = 1;

    while (n) {
        for (k = 0; k < n && s[k] != '\n'; ++k) {
        }
        if (spaced && !first && !(k >= 2 && s[0] == '-' && s[1] == ' ')) {
            wrap_blank();
        }
        para(s, k, width);
        first = 0;
        if (k < n) {
            ++k;                    /* the '\n' */
        }
        s += k;
        n -= k;
    }
}
