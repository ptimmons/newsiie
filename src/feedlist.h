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
extern uint8_t fl_oserror;          /* the ProDOS error behind FL_E_* */

void fl_get(uint8_t i);
uint8_t fl_add(const char *name, const char *url);  /* 0 or FL_E_FULL */
void fl_delete(uint8_t i);

/* One line of the file into a name and URL (both NUL-terminated, cut to
 * size). Returns 0 for a line with no feed (blank, comment, no URL). */
uint8_t fl_parse(const char *line, uint16_t n, char *name, char *url);

/* A name from a URL: its host, without "www.". */
void fl_host(const char *url, char *name);

/* "https://" in front of a URL typed without a scheme. */
void fl_fix_url(char *url);

#endif
