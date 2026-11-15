/*
 * The views. Each owns rows 0..22; the status bar (row 23) belongs to
 * status.c. A view runs until the user leaves it and returns the next one.
 */
#ifndef UI_H
#define UI_H

#include <stdint.h>

#define NEWSIIE_VERSION "1.0"

#define V_FEEDS     0
#define V_HEADS     1
#define V_ARTICLE   2
#define V_QUIT      3

void splash_show(void);         /* draws the splash; caller waits for a key */
uint8_t feeds_run(void);
uint8_t heads_run(void);
uint8_t article_run(void);
void help_show(void);           /* draws the help box over the body rows */

/* Rows 0 and 1: a heading at the left, a note at the right, and a rule. */
void ui_heading(const char *left, const char *right);

/* Keys every view shares. UI_REDRAW: handled, redraw the view. UI_QUIT:
 * quit. 0: not ours. */
#define UI_REDRAW   1
#define UI_QUIT     2
uint8_t ui_common(uint8_t k);

/* The feed being read, set by the Feeds view, and the item open in the
 * Article view (kept when it returns, so Headlines can follow Left and
 * Right). heads_fresh tells Headlines a new fetch replaced its items. */
extern const char *cur_name;
extern const char *cur_url;
extern uint16_t    cur_item;
extern uint8_t     heads_fresh;

/* The status bar. status_main() is the resting state for a view: a left
 * field, a middle field, the card's address, and the help hint at the
 * right. status_msg() takes the whole bar for a message until the next
 * status_main(). */
void status_main(const char *left, const char *mid);
void status_msg(const char *msg);

/* Ask on the status bar. prompt() edits buf (up to max characters) and
 * returns 1 on Return with something typed, 0 on Esc. confirm() asks a
 * yes-or-no question and returns 1 for Y. */
uint8_t prompt(const char *label, char *buf, uint8_t max);
uint8_t confirm(const char *question);

/* A bar with three fields of the caller's choosing (the probes'). */
void status_fields(const char *left, const char *mid, const char *right);

/* The card's state, shown in status_main()'s network field: "COGS
 * 192.168.1.60", "NO COGS CARD", ... Set by net_check(). */
extern char status_net[24];

/* Find the card and read its link state into status_net. */
void net_check(void);

#endif
