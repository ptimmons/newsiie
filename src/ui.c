/*
 * What the views share: the heading, and the keys that work everywhere.
 */
#include "screen.h"
#include "kbd.h"
#include "ui.h"

const char *cur_name;
const char *cur_url;

void ui_heading(const char *left, const char *right)
{
    lb_clear(SC_SPACE);
    lb_text(1, left, 0);
    lb_text((uint8_t)(SCR_COLS - 1 - txt_len(right)), right, 0);
    scr_blit(0);
    /* A MouseText rule draws along the top of its cell, so a row of them
     * sits directly under the heading. */
    lb_clear(has_mousetext ? MT_RULE : SC_DASH);
    scr_blit(1);
}

uint8_t ui_common(uint8_t k)
{
    if (!key_oa) {
        return 0;
    }
    if (k == '?' || k == '/' || k == 'h' || k == 'H') {
        help_show();
        key_get();
        return UI_REDRAW;
    }
    if (k == 'q' || k == 'Q') {
        return UI_QUIT;
    }
    return 0;
}
