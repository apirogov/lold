/* loltask.c
 * Object storing one animation task
 * Copyright (C) 2013 Anton Pirogov
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lold.h"
#include "loltask.h"
#include "network.h"

//Init LolTask
LolTask *loltask_new(void) {
  LolTask *task = malloc(sizeof(LolTask));
  task->delay = DEF_DELAY;
  task->ttl = DEF_TTL;
  task->pri = DEF_PRI;
  task->frames = NULL;
  task->timestamp = time(NULL);
  return task;
}

//Destroy LolTask
void loltask_free(LolTask *task) {
  lollist_free(task->frames);
  free(task);
}

//Send an animation to a lold server over a TCP connection
//host: where the lold server is (may be name to be resolved)
//port: lold server port
//task: LolTask structure to be transferred
//returns non-zero on success
int loltask_send(const char *host, int port, const LolTask *task) {
  char buffer[128];
  buffer[127] = '\0';
  FILE *stream = open_tcp_stream(host, port); //open stream
  if (stream==NULL)
    return 0; //error

  fgets(buffer, 128, stream);
  if (strncmp(buffer, LOLD_SYM_OK, 2)!=0) {
	  close_tcp_stream(stream);
    return 0; //server busy
  }

  snprintf(buffer, 128, "%s\n", LOLD_SYM_TSK);
  puts_tcp_stream(buffer, stream);
  snprintf(buffer, 128, "%s %i\n", LOLD_SYM_DEL, task->delay);
  puts_tcp_stream(buffer, stream);
  snprintf(buffer, 128, "%s %i\n", LOLD_SYM_TTL, task->ttl);
  puts_tcp_stream(buffer, stream);
  snprintf(buffer, 128, "%s %i\n", LOLD_SYM_PRI, task->pri);
  puts_tcp_stream(buffer, stream);

  snprintf(buffer, 128, "%s\n", LOLD_SYM_DAT);
  puts_tcp_stream(buffer, stream);

  LolList *curr = task->frames;
  while(curr!=NULL) {
	if (curr->value != NULL) {
	  snprintf(buffer, 127, "%s\n", (char*)curr->value);
    puts_tcp_stream(buffer, stream);
	}
    curr = curr->next;
  }

  snprintf(buffer, 128, "%s\n", LOLD_SYM_END);
  puts_tcp_stream(buffer, stream);

  fgets(buffer, 128, stream);
  if (strncmp(buffer, LOLD_SYM_OK, 2)!=0) {
	  close_tcp_stream(stream);
	  return 0; //error
  }

	close_tcp_stream(stream);
  return 1; //success
}
