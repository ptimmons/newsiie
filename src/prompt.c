/*
 * Asking on the status bar: a one-line editor, and a yes-or-no question.
 *
 * The field is a box of normal video inside the inverse bar, with an
 * inverse block where the next character lands. Text longer than the box
 * scrolls, so the end being typed is always in view. Delete or Left
 * erases; Return accepts; Esc cancels.
 */
#include <string.h>
#include "screen.h"
#include "kbd.h"
#include "ui.h"

/* This file's code runs from the language card ($D400-$DFFF, bank 2),
 * which cc65 fills at startup: main memory is full (PLAN.md section 5).
 * Code there must not call ProDOS or the ROM while it runs. */
#ifdef __CC65__
#pragma code-name ("LC")
#endif

#define HINT        "ESC CANCELS"
#define HINT_COL    (SCR_COLS - 1 - 11)

uint8_t prompt(const char *label, char *buf, uint8_t max)
{
    uint8_t n = (uint8_t)strlen(buf), col, width, start, k;

    for (;;) {
        lb_clear(SC_ISPACE);
        col = (uint8_t)(lb_text(1, label, 1) + 1);
        width = (uint8_t)(HINT_COL - 1 - col);      /* the box */
        start = n >= width ? (uint8_t)(n - width + 1) : 0;
        lb_fill(col, width, SC_SPACE);
        lb_text(col, buf + start, 0);
        linebuf[col + n - start] = SC_ISPACE;       /* the cursor */
        lb_text(HINT_COL, HINT, 1);
        scr_blit(STATUS_ROW);

        k = key_get();
        if (k == K_ESC) {
            return 0;
        }
        if (k == K_RETURN) {
            return n != 0;
        }
        if (k == K_DELETE || k == K_LEFT) {
            if (n) {
                buf[--n] = 0;
            }
        } else if (k >= 0x20 && k < 0x7F && n < max) {
            buf[n++] = (char)k;
            buf[n] = 0;
        }
    }
}

uint8_t confirm(const char *question)
{
    uint8_t k;

    lb_clear(SC_ISPACE);
    lb_text(lb_text(1, question, 1), "  Y OR N", 1);
    scr_blit(STATUS_ROW);
    k = key_get();
    return (uint8_t)(k == 'y' || k == 'Y');
}
