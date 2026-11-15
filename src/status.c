/*
 * The status bar: row 24, inverse, uppercase, as fixed fields.
 *
 *   col  1   program or feed name
 *   col 20   counts and position
 *   col 36   the card and its address
 *   col 60   the key hints, ending one column short of the edge
 */
#include "screen.h"
#include "ui.h"

#define COL_LEFT    1
#define COL_MID     20
#define COL_NET     36
#define COL_HINT    60

char status_net[24];

void status_fields(const char *left, const char *mid, const char *right)
{
    lb_clear(SC_ISPACE);
    lb_text(COL_LEFT, left, 1);
    lb_text(COL_MID, mid, 1);
    lb_text((uint8_t)(SCR_COLS - 1 - txt_len(right)), right, 1);
    scr_blit(STATUS_ROW);
}

void status_main(const char *left, const char *mid)
{
    char up[COL_MID - COL_LEFT];
    uint8_t i;

    /* The bar is uppercase, and a long feed name stops short of the next
     * field. */
    for (i = 0; left[i] && i < sizeof up - 1; ++i) {
        up[i] = (char)(left[i] >= 'a' && left[i] <= 'z' ? left[i] - 32 : left[i]);
    }
    up[i] = 0;
    lb_clear(SC_ISPACE);
    lb_text(COL_LEFT, up, 1);
    lb_text(COL_MID, mid, 1);
    lb_text(COL_NET, status_net, 1);
    lb_text(COL_HINT, OA "-? HELP   " OA "-Q QUIT", 1);
    scr_blit(STATUS_ROW);
}

void status_msg(const char *msg)
{
    lb_clear(SC_ISPACE);
    lb_center(msg, 1);
    scr_blit(STATUS_ROW);
}
