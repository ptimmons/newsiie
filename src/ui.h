/*
 * The views. Each owns rows 0..22; the status bar (row 23) belongs to
 * status.c. A view runs until the user leaves it and returns the next one.
 */
#ifndef UI_H
#define UI_H

#include <stdint.h>

#define NEWSIIE_VERSION "0.1"

#define V_FEEDS     0
#define V_HEADS     1
#define V_QUIT      2

void splash_show(void);         /* draws the splash; caller waits for a key */
uint8_t feeds_run(void);
uint8_t heads_run(void);
void help_show(void);           /* draws the help box over the body rows */

/* Rows 0 and 1: a heading at the left, a note at the right, and a rule. */
void ui_heading(const char *left, const char *right);

/* Keys every view shares. UI_REDRAW: handled, redraw the view. UI_QUIT:
 * quit. 0: not ours. */
#define UI_REDRAW   1
#define UI_QUIT     2
uint8_t ui_common(uint8_t k);

/* The feed being read, set by the Feeds view. */
extern const char *cur_name;
extern const char *cur_url;

/* The status bar. status_main() is the resting state for a view: a left
 * field, a middle field, the card's address, and the help hint at the
 * right. status_msg() takes the whole bar for a message until the next
 * status_main(). */
void status_main(const char *left, const char *mid);
void status_msg(const char *msg);

/* A bar with three fields of the caller's choosing (the probes'). */
void status_fields(const char *left, const char *mid, const char *right);

/* The card's state, shown in status_main()'s network field: "COGS
 * 192.168.1.60", "NO COGS CARD", ... Set by net_check(). */
extern char status_net[24];

/* Find the card and read its link state into status_net. */
void net_check(void);

#endif
