#include "screen.h"
#include "ui.h"

/* The commit it was built from (Makefile), so a test, or a bug report, can
 * tell which build booted. */
#ifndef NEWSIIE_BUILD
#define NEWSIIE_BUILD ""
#endif

static void row_center(uint8_t row, const char *s, uint8_t inverse)
{
    lb_clear(SC_SPACE);
    lb_center(s, inverse);
    scr_blit(row);
}

void splash_show(void)
{
    scr_clear(0, SCR_ROWS - 1);

    row_center(7, "         ", 1);
    row_center(8, " NEWSIIE ", 1);
    row_center(9, "         ", 1);
    row_center(11, "A news reader for the Apple //e", 0);
    row_center(17, "Version " NEWSIIE_VERSION, 0);
    if (NEWSIIE_BUILD[0]) {
        row_center(19, "Build" NEWSIIE_BUILD, 0);
    }
    status_msg("PRESS ANY KEY");
}
