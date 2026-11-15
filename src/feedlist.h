/*
 * The feed list: up to 21 feeds (one a screen row), kept in auxiliary
 * memory at $B000-$BFFF, above the item store, and saved in NEWSIIE.FEEDS.
 *
 * NEWSIIE.FEEDS is a ProDOS text file, one feed a line, editable in ZipEdit:
 *
 *     # comments and blank lines are ignored
 *     Hacker News|https://hnrss.org/frontpage
 *     https://xkcd.com/atom.xml          <- no name: the host is used
 *
 * It is read once at startup and written whole after each change, and
 * never while a feed is being fetched (PLAN.md section 1). Both go through
 * the parser's idle scratch buffer, so the file costs no main memory.
 */
#ifndef FEEDLIST_H
#define FEEDLIST_H

#include "port.h"

#define FL_MAX      21
#define FL_NAME     31              /* characters, not counting the NUL */
#define FL_URL      127
#define FL_FILE     "NEWSIIE.FEEDS"

/* aux memory the list lives in: FL_MAX entries of FL_ENTRY bytes */
#define FL_BASE     0xB000
#define FL_ENTRY    (FL_NAME + 1 + FL_URL + 1)

extern uint8_t fl_count;
extern uint8_t fl_skipped;          /* lines fl_load() could not use */

/* The entry fl_get() last read. */
extern char fl_name[FL_NAME + 1];
extern char fl_url[FL_URL + 1];

/* Results beyond 0: */
#define FL_E_NOFILE     1           /* no NEWSIIE.FEEDS: an empty list */
#define FL_E_READ       2
#define FL_E_LOCKED     3           /* the disk is write-protected */
#define FL_E_WRITE      4
#define FL_E_FULL       5

uint8_t fl_load(void);
uint8_t fl_save(void);

/* Every save goes through this. On a CoGS network disk each block written
 * is an HTTP PUT, and the slot ROM's wait for the reply is bounded: a slow
 * one comes back as $27, I/O error, and unlike a read the ROM does not
 * retry it. fn writes a whole file and returns 0 or a ProDOS error; a $27
 * is tried again, from the start, after a second's pause, up to IO_TRIES
 * times in all. */
#define IO_TRIES    3
#define IO_ERR      0x27
uint8_t io_retry(uint8_t (*fn)(void));
extern uint8_t fl_oserror;          /* the ProDOS error behind FL_E_* */

void fl_get(uint8_t i);
uint8_t fl_add(const char *name, const char *url);  /* 0 or FL_E_FULL */
void fl_delete(uint8_t i);

/* One line of the file into a name and URL (both NUL-terminated, the name
 * cut to size). Returns 1 for a feed, 0 for a blank line or a comment, and
 * 2 for a line that cannot be used: no URL, or one past FL_URL. */
uint8_t fl_parse(const char *line, uint16_t n, char *name, char *url);

/* A name from a URL: its host, without "www.". */
void fl_host(const char *url, char *name);

/* "https://" in front of a URL typed without a scheme. */
void fl_fix_url(char *url);

#endif
