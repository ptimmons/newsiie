/*
 * The Feeds view. Milestone 0 has no feeds file yet, so it draws the empty
 * state: a heading, a rule under it, and a hint in the middle of the body.
 */
#include "screen.h"
#include "ui.h"

#define HEAD_ROW    0
#define RULE_ROW    1

void feeds_draw(void)
{
    lb_clear(SC_SPACE);
    lb_text(1, "Feeds", 0);
    scr_blit(HEAD_ROW);

    /* A MouseText rule draws along the top of its cell, so a row of them
     * sits directly under the heading. */
    lb_clear(has_mousetext ? MT_RULE : SC_DASH);
    scr_blit(RULE_ROW);

    scr_clear(RULE_ROW + 1, BODY_ROWS - 1);

    lb_clear(SC_SPACE);
    lb_center("No feeds yet.", 0);
    scr_blit(10);
    lb_clear(SC_SPACE);
    lb_center("Press " OA "-A to add one, or " OA "-? for help.", 0);
    scr_blit(12);

    status_main("NEWSIIE", "NO FEEDS");
}
