/*
 * Host tests for src/clean.c: folding, entities, tags, whitespace, caps,
 * broken UTF-8, and dates.
 */
#include <stdio.h>
#include <string.h>
#include "clean.h"

static int fails, checks;

static void check_str(int line, const char *got, const char *want)
{
    ++checks;
    if (strcmp(got, want)) {
        printf("FAIL line %d:\n  got  \"%s\"\n  want \"%s\"\n", line, got, want);
        ++fails;
    }
}
#define CHECK_STR(g, w) check_str(__LINE__, (g), (w))
#define CHECK(c) do { ++checks; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static char out[256];

static const char *line(const char *s)
{
    clean(out, sizeof out, s, (uint16_t)strlen(s), CLEAN_LINE);
    return out;
}

static const char *body(const char *s)
{
    clean(out, sizeof out, s, (uint16_t)strlen(s), CLEAN_BODY);
    return out;
}

static const char *date(const char *s)
{
    clean_date(out, s);
    return out;
}

int main(void)
{
    char small[6];

    /* tables */
    CHECK(clean_latin1_len == 128);
    CHECK_STR(clean_fold(0xE9), "e");
    CHECK_STR(clean_fold(0xC6), "AE");
    CHECK_STR(clean_fold(0xDF), "ss");
    CHECK_STR(clean_fold(0xFE), "th");
    CHECK_STR(clean_fold(0xFF), "y");
    CHECK_STR(clean_fold(0xD7), "x");
    CHECK_STR(clean_fold(0x141), "L");
    CHECK_STR(clean_fold(0x17E), "z");
    CHECK_STR(clean_fold(0x2019), "'");
    CHECK_STR(clean_fold(0x2014), "--");
    CHECK_STR(clean_fold(0x2026), "...");
    CHECK_STR(clean_fold(0xFEFF), "");
    CHECK_STR(clean_fold(0x301), "");          /* combining acute */
    CHECK_STR(clean_fold(0xFFFD), "?");        /* stands in for U+10000 up */
    CHECK_STR(clean_fold(0x4E2D), "?");        /* CJK */

    /* whitespace */
    CHECK_STR(line("  a \n\t b  "), "a b");
    CHECK_STR(line(""), "");
    CHECK_STR(line("   "), "");
    CHECK_STR(body("<p>a</p>\n\n<p>b</p>"), "a\nb");
    CHECK_STR(body("<p></p><p></p>x<br><br>y<p>"), "x\ny");
    CHECK_STR(line("<p>a</p><p>b</p>"), "a b");

    /* tags */
    CHECK_STR(line("a<b>b</b>c"), "abc");
    CHECK_STR(line("x <img src=\"a.png\" alt=\"pic\"> y"), "x y");
    CHECK_STR(line("1 < 2 and 3 > 2"), "1 < 2 and 3 > 2");
    CHECK_STR(line("cut off <a href=\"http://exa"), "cut off");
    CHECK_STR(line("ends with <"), "ends with <");
    CHECK_STR(line("x <3 y"), "x <3 y");
    CHECK_STR(line("<!-- gone -->kept<!-- unterminated"), "kept");
    CHECK_STR(line("a<SCRIPT>bad()</SCRIPT>b"), "ab");
    CHECK_STR(line("a<style>p{}</Style >b"), "ab");
    CHECK_STR(line("<table><tr><td>1</td><td>2</td></tr></table>"), "1 2");
    CHECK_STR(body("<ul><li>a</li><li>b</li></ul>after"), "- a\n- b\nafter");

    /* entities */
    CHECK_STR(line("&amp;&lt;&gt;&quot;&apos;"), "&<>\"'");
    CHECK_STR(line("&#8217;&#x2019;&rsquo;"), "'''");
    CHECK_STR(line("a&nbsp;&nbsp;b"), "a b");
    CHECK_STR(line("&copy; 2026"), "(c) 2026");
    CHECK_STR(line("AT&T & co &bogus; &#xZZ; &"), "AT&T & co &bogus; &#xZZ; &");
    CHECK_STR(line("&verylongentityname;"), "&verylongentityname;");

    /* UTF-8, good and broken */
    CHECK_STR(line("caf\xc3\xa9 na\xc3\xafve \xe2\x80\x9cq\xe2\x80\x9d"), "cafe naive \"q\"");
    CHECK_STR(line("e\xcc\x81"), "e");                  /* decomposed accent */
    CHECK_STR(line("\xf0\x9f\x9a\x80!"), "?!");          /* emoji */
    CHECK_STR(line("a\x80" "b"), "a?b");                 /* stray continuation */
    CHECK_STR(line("a\xe2\x80"), "a?");                  /* cut short at the end */
    CHECK_STR(line("a\xe2\x80z"), "a?z");                /* cut short mid-text */
    CHECK_STR(line("tab\there\x01x"), "tab herex");      /* control dropped */

    /* the cap: at most cap - 1 bytes, NUL-terminated */
    CHECK(clean(small, sizeof small, "abcdefgh", 8, CLEAN_LINE) == 5);
    CHECK_STR(small, "abcde");
    CHECK(clean(small, sizeof small, "ab&hellip;", 10, CLEAN_LINE) == 5);
    CHECK_STR(small, "ab...");

    /* trim */
    strcpy(out, "  \n https://x/ \t");
    clean_trim(out);
    CHECK_STR(out, "https://x/");

    /* dates */
    CHECK_STR(date("Fri, 13 Nov 2026 16:39:33 +0000"), "2026-11-13 16:39");
    CHECK_STR(date("  Sat, 14 Nov 2026 09:05:00 GMT"), "2026-11-14 09:05");
    CHECK_STR(date("13 Nov 2026 16:39 EST"), "2026-11-13 16:39");
    CHECK_STR(date("Wed, 2 Oct 02 08:00:00 EST"), "2002-10-02 08:00");
    CHECK_STR(date("1 jan 99 00:00"), "1999-01-01 00:00");
    CHECK_STR(date("Tue, 10 Jun 2003"), "2003-06-10 00:00");
    CHECK_STR(date("2026-11-14T16:44:44-05:00"), "2026-11-14 16:44");
    CHECK_STR(date("2026-11-13T00:00:00Z"), "2026-11-13 00:00");
    CHECK_STR(date("2026-11-14 16:44"), "2026-11-14 16:44");
    CHECK_STR(date("2026-11-13"), "2026-11-13 00:00");
    CHECK_STR(date("Nov 2026"), "");
    CHECK_STR(date("2026-13-01"), "");
    CHECK_STR(date("Fri, 32 Nov 2026 10:00:00"), "");
    CHECK_STR(date(""), "");
    CHECK_STR(date("bogus"), "");

    if (fails) {
        printf("%d of %d clean checks FAILED\n", fails, checks);
        return 1;
    }
    printf("all %d clean checks passed\n", checks);
    return 0;
}
