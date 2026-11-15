/* The Mac's scan(); the Apple uses scan.s. Same name, so not scan.c:
 * both would build to build/scan.o. */
#include "scan.h"

uint16_t scan(const uint8_t *p, uint16_t n, uint8_t mask)
{
    uint16_t i;

    for (i = 0; i < n; ++i) {
        if (scan_class[p[i]] & mask) {
            break;
        }
    }
    return i;
}
