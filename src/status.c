/*
 * The status bar: row 24, inverse, uppercase, as fixed fields.
 *
 *   col  1   program or feed name
 *   col 20   counts and position
 *   col 60   the key hints, ending one column short of the edge
 */
#include "screen.h"
#include "ui.h"

#define COL_LEFT    1
#define COL_MID     20
#define COL_HINT    60

void status_main(const char *left, const char *mid)
{
    lb_clear(SC_ISPACE);
    lb_text(COL_LEFT, left, 1);
    lb_text(COL_MID, mid, 1);
    lb_text(COL_HINT, OA "-? HELP   " OA "-Q QUIT", 1);
    scr_blit(STATUS_ROW);
}

void status_msg(const char *msg)
{
    lb_clear(SC_ISPACE);
    lb_center(msg, 1);
    scr_blit(STATUS_ROW);
}
