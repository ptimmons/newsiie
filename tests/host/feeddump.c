/*
 * feeddump [-c chunk] [-n max_items] file.xml
 *
 * Parse a feed with the same C that runs on the Apple and print what it
 * found, one field a line. The fixture tests diff this against expected
 * output at several chunk sizes; the corpus check reads it too.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "feed.h"

static unsigned max_items;

static uint8_t sink(const feed_item_t *it)
{
    const char *b = it->body, *e;

    printf("ITEM\n");
    printf("  title: %s\n", it->title);
    printf("  link:  %s\n", it->link);
    printf("  date:  %s\n", it->date);
    printf("  guid:  %s\n", it->guid);
    /* "| " starts a paragraph, ": " a line inside one (an HTML <br>). */
    {
        char mark = '|';

        while (*b) {
            e = b + strcspn(b, "\n\r");
            printf("  %c %.*s\n", mark, (int)(e - b), b);
            mark = *e == '\r' ? ':' : '|';
            b = *e ? e + 1 : e;
        }
    }
    return (uint8_t)(max_items && feed_items >= max_items);
}

int main(int argc, char **argv)
{
    static const char *kinds[] = { "unknown", "rss", "atom", "rdf", "not a feed" };
    static uint8_t data[1 << 20];
    size_t len, off, chunk = 512;
    const char *path = NULL;
    FILE *f;
    int i;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            chunk = (size_t)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-n") && i + 1 < argc) {
            max_items = (unsigned)atoi(argv[++i]);
        } else {
            path = argv[i];
        }
    }
    if (!path || !chunk || !(f = fopen(path, "rb"))) {
        fprintf(stderr, "usage: feeddump [-c chunk] [-n max] file.xml\n");
        return 2;
    }
    len = fread(data, 1, sizeof data, f);
    fclose(f);

    feed_sink = sink;
    feed_begin();
    for (off = 0; off < len; off += chunk) {
        feed_chunk(data + off, (uint16_t)(len - off < chunk ? len - off : chunk));
    }
    printf("KIND %s\n", kinds[feed_end()]);
    printf("FEED %s\n", feed_title);
    printf("ITEMS %u\n", (unsigned)feed_items);
    return 0;
}
