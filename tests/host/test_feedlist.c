/*
 * Host tests for src/feedlist.c: parsing lines, names from hosts, URLs
 * without a scheme, add and delete in (modelled) aux memory, and a save
 * then load through a real file. Run in an empty scratch directory: it
 * writes NEWSIIE.FEEDS there.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "feedlist.h"
#include "feed.h"

static int fails, checks;
#define CHECK(c) do { ++checks; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static uint8_t aux[0x10000];
static int out_of_bounds;

static void bounds(uint16_t at, uint16_t n)
{
    if (at < FL_BASE || (uint32_t)at + n > 0xC000) {
        ++out_of_bounds;
    }
}

void aux_put(uint16_t dst, const void *src, uint16_t n) { bounds(dst, n); memcpy(aux + dst, src, n); }
void aux_get(void *dst, uint16_t src, uint16_t n) { bounds(src, n); memcpy(dst, aux + src, n); }

/* feed.c's scratch buffer, which feedlist.c borrows. */
static char scratch[FEED_SCRATCH_SIZE];
char *const feed_scratch = scratch;

static char name[FL_NAME + 1], url[FL_URL + 1];

static int parse(const char *line)
{
    return fl_parse(line, (uint16_t)strlen(line), name, url);
}

int main(void)
{
    FILE *f;
    uint8_t i;
    char u[FL_URL + 1], longurl[200];

    /* Lines */
    CHECK(parse("Hacker News|https://hnrss.org/frontpage"));
    CHECK(!strcmp(name, "Hacker News") && !strcmp(url, "https://hnrss.org/frontpage"));
    CHECK(parse("  BBC  |  https://feeds.bbci.co.uk/news/rss.xml  "));
    CHECK(!strcmp(name, "BBC") && !strcmp(url, "https://feeds.bbci.co.uk/news/rss.xml"));
    CHECK(parse("https://www.xkcd.com/atom.xml"));
    CHECK(!strcmp(name, "xkcd.com") && !strcmp(url, "https://www.xkcd.com/atom.xml"));
    CHECK(parse("|http://host:8080/feed"));
    CHECK(!strcmp(name, "host"));
    CHECK(parse("Disk|/FEEDS/HN.XML") && !strcmp(url, "/FEEDS/HN.XML") && !strcmp(name, "Disk"));
    CHECK(!parse(""));
    CHECK(!parse("   "));
    CHECK(!parse("# a comment"));
    CHECK(parse("Name only|") == 2);
    memset(longurl, 'x', sizeof longurl - 1);
    longurl[sizeof longurl - 1] = 0;
    CHECK(parse(longurl) == 2);                     /* past FL_URL: refused */
    CHECK(parse("An extremely long feed name that goes on and on|https://a.b/"));
    CHECK(strlen(name) == FL_NAME);

    /* URLs typed without a scheme */
    strcpy(u, "hnrss.org/frontpage");
    fl_fix_url(u);
    CHECK(!strcmp(u, "https://hnrss.org/frontpage"));
    strcpy(u, "http://example.com/");
    fl_fix_url(u);
    CHECK(!strcmp(u, "http://example.com/"));
    strcpy(u, "/FEEDS/HN.XML");
    fl_fix_url(u);
    CHECK(!strcmp(u, "/FEEDS/HN.XML"));

    /* Add, get, delete */
    fl_count = 0;
    CHECK(fl_add("One", "https://one/") == 0);
    CHECK(fl_add("Two", "https://two/") == 0);
    CHECK(fl_add("Three", "https://three/") == 0);
    fl_get(1);
    CHECK(!strcmp(fl_name, "Two") && !strcmp(fl_url, "https://two/"));
    fl_delete(0);
    CHECK(fl_count == 2);
    fl_get(0);
    CHECK(!strcmp(fl_name, "Two"));
    fl_get(1);
    CHECK(!strcmp(fl_name, "Three"));
    fl_delete(1);
    CHECK(fl_count == 1);
    for (i = 0; i < FL_MAX; ++i) {
        fl_add("x", "https://x/");
    }
    CHECK(fl_count == FL_MAX);
    CHECK(fl_add("y", "https://y/") == FL_E_FULL);

    /* Save, then load through a real file */
    unlink(FL_FILE);
    CHECK(fl_load() == FL_E_NOFILE && fl_count == 0);
    fl_add("Hacker News", "https://hnrss.org/frontpage");
    fl_add("Disk", "/FEEDS/HN.XML");
    CHECK(fl_save() == 0);
    fl_count = 0;
    CHECK(fl_load() == 0 && fl_count == 2);
    fl_get(0);
    CHECK(!strcmp(fl_name, "Hacker News") && !strcmp(fl_url, "https://hnrss.org/frontpage"));
    fl_get(1);
    CHECK(!strcmp(fl_name, "Disk") && !strcmp(fl_url, "/FEEDS/HN.XML"));

    /* A file from a Mac: LF endings, comments, no final newline */
    f = fopen(FL_FILE, "w");
    fputs("# mine\n\nA|https://a/\nhttps://www.b.org/rss\n# done\nC|https://c/", f);
    fclose(f);
    CHECK(fl_load() == 0 && fl_count == 3 && fl_skipped == 0);
    fl_get(1);
    CHECK(!strcmp(fl_name, "b.org") && !strcmp(fl_url, "https://www.b.org/rss"));
    fl_get(2);
    CHECK(!strcmp(fl_name, "C"));

    /* Lines it cannot use, and feeds past the 21st, are counted. */
    f = fopen(FL_FILE, "w");
    fputs("A|https://a/\nNo address|\n", f);
    for (i = 0; i < FL_MAX + 2; ++i) {
        fprintf(f, "F%u|https://f%u/\n", i, i);
    }
    fclose(f);
    CHECK(fl_load() == 0 && fl_count == FL_MAX && fl_skipped == 1 + 3);
    unlink(FL_FILE);

    CHECK(out_of_bounds == 0);

    if (fails) {
        printf("%d of %d feed list checks FAILED\n", fails, checks);
        return 1;
    }
    printf("all %d feed list checks passed\n", checks);
    return 0;
}
