/*
 * Which items have been read: the 16-bit guid hashes of the last 256
 * opened, in main memory, saved to NEWSIIE.READ (a ProDOS BIN file of
 * little-endian words, oldest first).
 *
 * It is a hint, not a ledger: two different guids share a hash about once
 * in 65,536, and the oldest read item is forgotten when the 257th arrives.
 * Loaded at startup and saved when leaving a feed's headlines and at quit,
 * never while a stream is open (PLAN.md section 1).
 */
#ifndef READSET_H
#define READSET_H

#include "port.h"

#define RS_MAX      256
#define RS_FILE     "NEWSIIE.READ"

uint8_t rs_has(uint16_t h);
void    rs_add(uint16_t h);         /* marks the set changed */

void    rs_load(void);              /* no file: an empty set */
uint8_t rs_save(void);              /* only if changed; 0 or a ProDOS error */

extern uint16_t rs_count;

#endif
