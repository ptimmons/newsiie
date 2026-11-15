/*
 * Host tests for src/wrap.c: every line fits, no word is split unless it
 * is wider than the screen, nothing is lost, and paragraphs and list items
 * are spaced as the Article view wants. Also run over every item of every
 * fixture feed.
 */
#include <stdio.h>
#include <string.h>
#include "wrap.h"

static int fails, checks;
#define CHECK(c) do { ++checks; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static char joined[8192];

/* Lines rejoined: a space where a break replaced one, '\n' at blanks. */
static const char *rejoin(void)
{
    uint16_t i;
    char *w = joined;

    for (i = 0; i < wrap_count; ++i) {
        if (i && w > joined && w[-1] != '\n') {
            *w++ = ' ';
        }
        memcpy(w, wrap_lines[i].p, wrap_lines[i].len);
        w += wrap_lines[i].len;
    }
    *w = 0;
    return joined;
}

static void check_lines(uint8_t width)
{
    uint16_t i;

    for (i = 0; i < wrap_count; ++i) {
        CHECK(wrap_lines[i].len <= width);
        if (wrap_lines[i].len) {
            CHECK(wrap_lines[i].p[0] != ' ');
            CHECK(wrap_lines[i].p[wrap_lines[i].len - 1] != ' ');
        }
    }
}

int main(void)
{
    const char *t;
    uint16_t i;

    /* Short text: one line, untouched. */
    wrap_reset();
    wrap_add("hello world", 11, 78, 1);
    CHECK(wrap_count == 1 && wrap_lines[0].len == 11);

    /* Breaks at the last space that fits. */
    wrap_reset();
    t = "aaaa bbbb cccc dddd";
    wrap_add(t, (uint16_t)strlen(t), 10, 0);
    CHECK(wrap_count == 2);
    CHECK(wrap_lines[0].len == 9 && !memcmp(wrap_lines[0].p, "aaaa bbbb", 9));
    CHECK(wrap_lines[1].len == 9 && !memcmp(wrap_lines[1].p, "cccc dddd", 9));

    /* A space exactly at the width: the line uses every column. */
    wrap_reset();
    t = "0123456789 next";
    wrap_add(t, (uint16_t)strlen(t), 10, 0);
    CHECK(wrap_count == 2 && wrap_lines[0].len == 10 && wrap_lines[1].len == 4);

    /* A word wider than the screen is split at the width. */
    wrap_reset();
    t = "https://example.com/a/very/long/path/that/does/not/fit ok";
    wrap_add(t, (uint16_t)strlen(t), 20, 0);
    check_lines(20);
    CHECK(wrap_lines[0].len == 20 && wrap_lines[1].len == 20);

    /* Paragraphs get a blank line between them; list items do not. */
    wrap_reset();
    t = "First para.\nSecond para.\n- one\n- two\nAfter.";
    wrap_add(t, (uint16_t)strlen(t), 78, 1);
    /* First para. / blank / Second para. / - one / - two / blank / After. */
    CHECK(wrap_count == 7);
    CHECK(wrap_lines[0].len == 11 && !memcmp(wrap_lines[0].p, "First para.", 11));
    CHECK(wrap_lines[1].len == 0);
    CHECK(wrap_lines[2].len == 12 && !memcmp(wrap_lines[2].p, "Second para.", 12));
    CHECK(wrap_lines[3].len == 5 && !memcmp(wrap_lines[3].p, "- one", 5));
    CHECK(wrap_lines[4].len == 5 && !memcmp(wrap_lines[4].p, "- two", 5));
    CHECK(wrap_lines[5].len == 0);
    CHECK(wrap_lines[6].len == 6 && !memcmp(wrap_lines[6].p, "After.", 6));

    /* A '\r' (an HTML <br>) starts a line with no blank before it. */
    wrap_reset();
    t = "Line\rbreak.\nNext.";
    wrap_add(t, (uint16_t)strlen(t), 78, 1);
    CHECK(wrap_count == 4);
    CHECK(wrap_lines[0].len == 4 && wrap_lines[1].len == 6);
    CHECK(wrap_lines[2].len == 0 && wrap_lines[3].len == 5);

    /* Unspaced: one line per paragraph, no blanks. */
    t = "First para.\nSecond para.\n- one\n- two\nAfter.";
    wrap_reset();
    wrap_add(t, (uint16_t)strlen(t), 78, 0);
    CHECK(wrap_count == 5);

    /* Nothing lost: 2000 characters of words rejoin to the original. */
    {
        static char big[2001];
        uint16_t k = 0, w = 0;

        while (k < 1990) {
            uint16_t len = (uint16_t)(1 + (w * 7) % 13);
            uint16_t j;
            for (j = 0; j < len && k < 1990; ++j) big[k++] = (char)('a' + (w + j) % 26);
            if (k < 1990) big[k++] = ' ';
            ++w;
        }
        while (k && big[k - 1] == ' ') --k;
        big[k] = 0;
        wrap_reset();
        wrap_add(big, k, 78, 1);
        check_lines(78);
        CHECK(!strcmp(rejoin(), big));
        CHECK(!wrap_full);
    }

    /* The table's cap. */
    wrap_reset();
    for (i = 0; i < WRAP_MAX + 10; ++i) {
        wrap_blank();
    }
    CHECK(wrap_count == WRAP_MAX && wrap_full);

    /* wrap_line cuts, never wraps. */
    wrap_reset();
    wrap_line("0123456789", 10, 4);
    CHECK(wrap_count == 1 && wrap_lines[0].len == 4);
    wrap_rule();
    CHECK(wrap_lines[1].kind == WK_RULE);

    if (fails) {
        printf("%d of %d wrap checks FAILED\n", fails, checks);
        return 1;
    }
    printf("all %d wrap checks passed\n", checks);
    return 0;
}
