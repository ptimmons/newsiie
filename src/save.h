/*
 * Saving an article as Markdown. See save.c.
 */
#ifndef SAVE_H
#define SAVE_H

#include "port.h"

/* A ProDOS file name from a title: its first 12 letters and digits,
 * uppercase, a letter first, then ".MD". name holds 16 bytes. */
void save_name(const char *title, char *name);

uint8_t save_exists(const char *name);

/* Write the article. 0, or a ProDOS error ($2B: write-protected). */
uint8_t save_markdown(const char *name, const char *title, const char *date,
                      const char *link, const char *body, uint16_t n);

#endif
