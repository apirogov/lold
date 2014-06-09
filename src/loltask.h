/* loltask.h
 * Object to store animation tasks, works like queue
 * Copyright (C) 2013 Anton Pirogov
 */
#ifndef _LOLTASK_H_
#define _LOLTASK_H_

#include "lolfactory.h"

#define DEF_DELAY	50
#define DEF_TTL		60
#define DEF_PRI		0

typedef struct {
  int delay; //ms between frames. valid: 20-1000
  int ttl; //seconds the animation is kept. valid: 0-600
  int pri; //Priority, higher overrides lower messages and discards them, >= 0

  LolList *frames;
  long timestamp; //creation timestamp (unix), should not be changed manually
} LolTask;

LolTask *loltask_new(void);
void loltask_free(LolTask *task);

int loltask_send(const char *host, int port, const LolTask *task);

#endif /* _LOLTASK_H_ */
