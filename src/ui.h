/*
 * The views. Each draw function owns rows 0..22; the status bar (row 23)
 * belongs to status.c.
 */
#ifndef UI_H
#define UI_H

#include <stdint.h>

#define NEWSIIE_VERSION "0.1"

void splash_show(void);         /* draws the splash; caller waits for a key */
void feeds_draw(void);
void help_show(void);           /* draws the help box over the body rows */

/* The status bar. status_main() is the resting state for a view: a left
 * field, a middle field, and the help hint at the right. status_msg() takes
 * the whole bar for a message until the next status_main(). */
void status_main(const char *left, const char *mid);
void status_msg(const char *msg);

#endif
