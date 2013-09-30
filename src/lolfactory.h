/* lolfactory.h
 * Helper functions to create animations
 * Copyright (C) 2013 Anton Pirogov
 */
#ifndef _LOLFACTORY_H_
#define _LOLFACTORY_H_

#include "lolhelper.h"

//constants of lolshield dimensions
#define MAX_X 14
#define MAX_Y 9

typedef struct {
  LolList *frames; //double linked list containing compiled frames of the animation
  LolList *lastFrame; //for speedup-access last directly
  char **frame; //9*14 2D array of chars representing a lolshield state
} LolFactory;

int lolfac_main(void);

LolFactory *lolfac_new(void);
void lolfac_free(LolFactory *fac);

void lolfac_set(LolFactory *fac, int x, int y, char v);
char lolfac_get(LolFactory *fac, int x, int y);
void lolfac_toggle(LolFactory *fac, int x, int y);

void lolfac_clear(LolFactory *fac);
void lolfac_empty(LolFactory *fac);
void lolfac_flip(LolFactory *fac);

void lolfac_draw(LolFactory *fac, int xval, char chr);
void lolfac_banner(LolFactory *fac, int len, const char *text);

char *lolfac_compile(char **frame); //may be used separately of a lolfactory object

#endif /* _LOLFACTORY_H_ */
