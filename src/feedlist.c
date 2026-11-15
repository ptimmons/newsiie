/*
 * The feed list. See feedlist.h.
 */
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "feedlist.h"
#include "feed.h"

#ifdef __CC65__
#include <apple2.h>                 /* _filetype, _auxtype */
#include "tick.h"                   /* io_retry's pause */
#endif

void __fastcall__ aux_put(uint16_t aux_dst, const void *src, uint16_t n);
void __fastcall__ aux_get(void *dst, uint16_t aux_src, uint16_t n);

#define SCRATCH     feed_scratch
#define SCRATCH_MAX (FEED_SCRATCH_SIZE - 1)

uint8_t fl_count;
uint8_t fl_skipped;
uint8_t fl_oserror;
char    fl_name[FL_NAME + 1];
char    fl_url[FL_URL + 1];

static const char header[] =
    "# Newsiie feeds: one a line, a name, a '|', then the URL.\r"
    "# Add and delete them in Newsiie with Open-Apple-A and Open-Apple-D,\r"
    "# or edit this file. Comments like these are not kept.\r";

static uint16_t entry(uint8_t i)
{
    return (uint16_t)(FL_BASE + (uint16_t)i * FL_ENTRY);
}

void fl_get(uint8_t i)
{
    aux_get(fl_name, entry(i), FL_NAME + 1);
    aux_get(fl_url, (uint16_t)(entry(i) + FL_NAME + 1), FL_URL + 1);
}

static void put(uint8_t i)
{
    aux_put(entry(i), fl_name, FL_NAME + 1);
    aux_put((uint16_t)(entry(i) + FL_NAME + 1), fl_url, FL_URL + 1);
}

uint8_t fl_add(const char *name, const char *url)
{
    if (fl_count >= FL_MAX) {
        return FL_E_FULL;
    }
    strncpy(fl_name, name, FL_NAME);
    fl_name[FL_NAME] = 0;
    strncpy(fl_url, url, FL_URL);
    fl_url[FL_URL] = 0;
    put(fl_count++);
    return 0;
}

void fl_delete(uint8_t i)
{
    for (; i + 1 < fl_count; ++i) {
        fl_get((uint8_t)(i + 1));
        put(i);
    }
    if (fl_count) {
        --fl_count;
    }
}

void fl_host(const char *url, char *name)
{
    const char *p = strstr(url, "://");
    uint8_t n = 0;

    p = p ? p + 3 : url;
    if (!strncmp(p, "www.", 4)) {
        p += 4;
    }
    while (*p && *p != '/' && *p != ':' && n < FL_NAME) {
        name[n++] = *p++;
    }
    name[n] = 0;
}

void fl_fix_url(char *url)
{
    uint8_t n = (uint8_t)strlen(url);

    if (url[0] == '/' || strstr(url, "://") || n + 8 > FL_URL) {
        return;                     /* a path, has a scheme, or no room */
    }
    memmove(url + 8, url, n + 1);
    memcpy(url, "https://", 8);
}

static uint8_t is_space(char c)
{
    return (uint8_t)(c == ' ' || c == '\t');
}

uint8_t fl_parse(const char *line, uint16_t n, char *name, char *url)
{
    const char *bar, *u;
    uint16_t un, nn;

    while (n && is_space(*line)) {
        ++line;
        --n;
    }
    while (n && is_space(line[n - 1])) {
        --n;
    }
    if (!n || line[0] == '#') {
        return 0;
    }
    bar = memchr(line, '|', n);
    if (bar) {
        nn = (uint16_t)(bar - line);
        u = bar + 1;
        un = (uint16_t)(n - nn - 1);
        while (nn && is_space(line[nn - 1])) {
            --nn;
        }
        while (un && is_space(*u)) {
            ++u;
            --un;
        }
    } else {
        nn = 0;
        u = line;
        un = n;
    }
    if (!un || un > FL_URL) {
        return 2;                   /* no URL, or too long to open */
    }
    memcpy(url, u, un);
    url[un] = 0;
    if (nn > FL_NAME) {
        nn = FL_NAME;
    }
    memcpy(name, line, nn);
    name[nn] = 0;
    if (!nn) {
        fl_host(url, name);
    }
    return 1;
}

uint8_t fl_load(void)
{
    int fd, got;
    uint16_t n = 0, i, start;

    fl_count = 0;
    fl_skipped = 0;
    fl_oserror = 0;
    fd = open(FL_FILE, O_RDONLY);
    if (fd < 0) {
#ifdef __CC65__
        fl_oserror = _oserror;
        return _oserror == 0x46 ? FL_E_NOFILE : FL_E_READ;
#else
        return errno == ENOENT ? FL_E_NOFILE : FL_E_READ;
#endif
    }
    while (n < SCRATCH_MAX && (got = read(fd, SCRATCH + n, SCRATCH_MAX - n)) > 0) {
        n += (uint16_t)got;
    }
    close(fd);

    /* Lines end in CR (ProDOS) or LF (a Mac), and the last may not end.
     * A line that cannot be used, or a feed past the 21st, is counted. */
    for (start = i = 0; i <= n; ++i) {
        if (i == n || SCRATCH[i] == '\r' || SCRATCH[i] == '\n') {
            switch (fl_parse(SCRATCH + start, (uint16_t)(i - start), fl_name, fl_url)) {
            case 1:
                if (fl_count < FL_MAX) {
                    put(fl_count++);
                } else {
                    ++fl_skipped;
                }
                break;
            case 2:
                ++fl_skipped;
                break;
            }
            start = (uint16_t)(i + 1);
        }
    }
    return 0;
}

uint8_t io_retry(uint8_t (*fn)(void))
{
    uint8_t rc, n = IO_TRIES;

    while ((rc = fn()) == IO_ERR && --n) {
#ifdef __CC65__
        tick_wait(TICKS_PER_SEC);
#endif
    }
    return rc;
}

/* One attempt: 0, or the ProDOS error (also in fl_oserror). */
static uint8_t fl_write(void)
{
    uint16_t n;
    uint8_t i, l;
    int fd, rc;

    n = (uint16_t)strlen(header);
    memcpy(SCRATCH, header, n);
    for (i = 0; i < fl_count; ++i) {
        fl_get(i);
        l = (uint8_t)strlen(fl_name);
        memcpy(SCRATCH + n, fl_name, l);
        n += l;
        SCRATCH[n++] = '|';
        l = (uint8_t)strlen(fl_url);
        memcpy(SCRATCH + n, fl_url, l);
        n += l;
        SCRATCH[n++] = '\r';
    }

    fl_oserror = 0;
#ifdef __CC65__
    _filetype = 0x04;               /* TXT */
    _auxtype = 0;
#endif
    fd = open(FL_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        goto failed;
    }
    rc = write(fd, SCRATCH, n);
    if (rc != (int)n) {
        close(fd);
        goto failed;
    }
    if (!close(fd)) {
        return 0;
    }
failed:
#ifdef __CC65__
    fl_oserror = _oserror;
#endif
    if (!fl_oserror) {
        fl_oserror = 0xFF;          /* failed, but ProDOS gave no reason */
    }
    return fl_oserror;
}

uint8_t fl_save(void)
{
    uint8_t rc = io_retry(fl_write);

    return !rc ? 0 : rc == 0x2B ? FL_E_LOCKED : FL_E_WRITE;
}
