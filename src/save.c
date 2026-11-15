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
#include "feedlist.h"               /* io_retry */

#ifdef __CC65__
#include <apple2.h>                 /* _filetype, _auxtype */
void lc_ram(void);                  /* blit.s */
#else
#define lc_ram()
#endif

static int fd;
static uint8_t failed;              /* 0, or the first ProDOS error */

/* The error behind a failed call; $FF if ProDOS gave none. */
static uint8_t os_err(void)
{
#ifdef __CC65__
    return _oserror ? _oserror : 0xFF;
#else
    return 0xFF;
#endif
}

static void put(const char *s, uint16_t n)
{
    if (!failed && n && write(fd, s, n) != (int)n) {
        failed = os_err();
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

/* The article being saved, for each attempt. */
static const char *s_name, *s_title, *s_date, *s_link, *s_body;
static uint16_t s_n;

/* One attempt: 0, or the ProDOS error. */
static uint8_t write_md(void)
{
    uint16_t i, start;

#ifdef __CC65__
    _filetype = 0x04;               /* TXT */
    _auxtype = 0;
#endif
    failed = 0;
    fd = open(s_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return os_err();
    }
    put("# ", 2);
    puts_cr(s_title);
    put("\r", 1);
    if (*s_date) {
        puts_cr(s_date);
    }
    if (*s_link) {
        puts_cr(s_link);
    }
    put("\r", 1);
    /* Paragraphs ('\n') get a blank line between them; a line break inside
     * one (an HTML <br>, '\r') is a plain new line. */
    for (start = i = 0; i <= s_n; ++i) {
        if (i == s_n || s_body[i] == '\n' || s_body[i] == '\r') {
            put(s_body + start, (uint16_t)(i - start));
            if (i < s_n) {
                put("\r\r", s_body[i] == '\n' ? 2 : 1);
            }
            start = (uint16_t)(i + 1);
        }
    }
    put("\r", 1);
    if (close(fd) && !failed) {
        failed = os_err();
    }
    return failed;
}

uint8_t save_markdown(const char *name, const char *title, const char *date,
                      const char *link, const char *body, uint16_t n)
{
    uint8_t rc;

    s_name = name;
    s_title = title;
    s_date = date;
    s_link = link;
    s_body = body;
    s_n = n;
    rc = io_retry(write_md);
    lc_ram();
    return rc;
}
