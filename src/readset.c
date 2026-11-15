/*
 * The read set. See readset.h.
 */
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "readset.h"

#ifdef __CC65__
#include <apple2.h>                 /* _filetype, _auxtype, _oserror */
#endif

uint16_t rs_count;

static uint16_t set[RS_MAX];        /* oldest first */
static uint8_t  dirty;
static uint8_t  locked;             /* the disk refused a write: read-only */

uint8_t rs_has(uint16_t h)
{
    uint16_t i;

    for (i = 0; i < rs_count; ++i) {
        if (set[i] == h) {
            return 1;
        }
    }
    return 0;
}

void rs_add(uint16_t h)
{
    if (rs_has(h)) {
        return;
    }
    if (rs_count == RS_MAX) {
        /* Forget the oldest. */
        memmove(set, set + 1, (RS_MAX - 1) * sizeof set[0]);
        --rs_count;
    }
    set[rs_count++] = h;
    dirty = 1;
}

/* The file is little-endian words; so is the 6502. On the Mac, too. */
void rs_load(void)
{
    int fd, n;

    rs_count = 0;
    dirty = 0;
    fd = open(RS_FILE, O_RDONLY);
    if (fd < 0) {
        return;
    }
    n = read(fd, set, sizeof set);
    close(fd);
    rs_count = n > 0 ? (uint16_t)n / 2 : 0;
}

uint8_t rs_save(void)
{
    int fd, n;

    if (!dirty || locked) {
        return 0;
    }
#ifdef __CC65__
    _filetype = 0x06;               /* BIN */
    _auxtype = 0;
#endif
    fd = open(RS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
#ifdef __CC65__
        if (_oserror == 0x2B) {
            locked = 1;             /* write-protected: stop trying */
        }
        return _oserror ? _oserror : 0xFF;
#else
        return 0xFF;
#endif
    }
    n = write(fd, set, rs_count * 2);
    close(fd);
    if (n != (int)(rs_count * 2)) {
#ifdef __CC65__
        return _oserror ? _oserror : 0xFF;
#else
        return 0xFF;
#endif
    }
    dirty = 0;
    return 0;
}
