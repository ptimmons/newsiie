/*
 * The parser's inner loop: find the first byte of a class.
 *
 * scan() returns the offset of the first byte in p[0..n) whose class bits
 * meet mask, or n if there is none. On the Apple it is scan.s, about 18
 * cycles a byte; on the Mac it is scan_host.c. Both use scan_class.
 */
#ifndef SCAN_H
#define SCAN_H

#include "port.h"

#define SC_LT       0x01    /* < */
#define SC_AMP      0x02    /* & */
#define SC_RBRK     0x04    /* ] */
#define SC_DQ       0x08    /* " */
#define SC_SQ       0x10    /* ' */
#define SC_GT       0x20    /* > */
#define SC_DASH     0x40    /* - */
#define SC_QM       0x80    /* ? */

extern const uint8_t scan_class[256];

uint16_t __fastcall__ scan(const uint8_t *p, uint16_t n, uint8_t mask);

#endif
