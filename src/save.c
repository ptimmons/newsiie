/*
 * Saving an article as a Markdown text file, for ZipEdit:
 *
 *     # The title
 *
 *     2026-11-14 16:42
 *     https://the.link/in/full
 *
 *     The first paragraph.
 *
 *     The second.
 *
 * ProDOS TXT, CR line endings. In main memory, not the language card: it
 * calls ProDOS, which code running from the language card must not do.
 * ProDOS restores the language card on its way out; lc_ram() makes sure,
 * since the caller (the Article view) runs from there.
 */
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "save.h"

#ifdef __CC65__
#include <apple2.h>                 /* _filetype, _auxtype */
void lc_ram(void);                  /* blit.s */
#else
#define lc_ram()
#endif

static int fd;
static uint8_t failed;

static void put(const char *s, uint16_t n)
{
    if (!failed && n && write(fd, s, n) != (int)n) {
        failed = 1;
    }
}

static void puts_cr(const char *s)
{
    put(s, (uint16_t)strlen(s));
    put("\r", 1);
}

void save_name(const char *title, char *name)
{
    uint8_t n = 0;
    char c;

    for (; *title && n < 12; ++title) {
        c = *title;
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 32);
        }
        if ((c >= 'A' && c <= 'Z') || (n && c >= '0' && c <= '9')) {
            name[n++] = c;
        }
    }
    if (!n) {
        memcpy(name, "ARTICLE", 7);
        n = 7;
    }
    memcpy(name + n, ".MD", 4);
}

uint8_t save_exists(const char *name)
{
    int f = open(name, O_RDONLY);

    if (f >= 0) {
        close(f);
    }
    lc_ram();
    return (uint8_t)(f >= 0);
}

uint8_t save_markdown(const char *name, const char *title, const char *date,
                      const char *link, const char *body, uint16_t n)
{
    uint16_t i, start;
    uint8_t rc = 0;

#ifdef __CC65__
    _filetype = 0x04;               /* TXT */
    _auxtype = 0;
#endif
    failed = 0;
    fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
#ifdef __CC65__
        rc = _oserror ? _oserror : 0xFF;
#else
        rc = 0xFF;
#endif
        lc_ram();
        return rc;
    }
    put("# ", 2);
    puts_cr(title);
    put("\r", 1);
    if (*date) {
        puts_cr(date);
    }
    if (*link) {
        puts_cr(link);
    }
    put("\r", 1);
    /* Paragraphs ('\n') get a blank line between them; a line break inside
     * one (an HTML <br>, '\r') is a plain new line. */
    for (start = i = 0; i <= n; ++i) {
        if (i == n || body[i] == '\n' || body[i] == '\r') {
            put(body + start, (uint16_t)(i - start));
            if (i < n) {
                put(body[i] == '\n' ? "\r\r" : "\r", body[i] == '\n' ? 2 : 1);
            }
            start = (uint16_t)(i + 1);
        }
    }
    put("\r", 1);
    if (failed) {
#ifdef __CC65__
        rc = _oserror ? _oserror : 0xFF;
#else
        rc = 0xFF;
#endif
    }
    if (close(fd) && !rc) {
        rc = 0xFF;
    }
    lc_ram();
    return rc;
}
