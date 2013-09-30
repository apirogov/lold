/* lolhelper.c
 * Common shared functions
 * Copyright (C) 2013 Anton Pirogov
 */
//for usleep warning
#define _BSD_SOURCE

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lolhelper.h"

const char EMPTY_FRAME[] = "0,0,0,0,0,0,0,0,0\0";

//Free all contents of a linked list and list itself, always returns NULL
void lollist_free(LolList *list) {
  LolList *tmp = NULL;
  while (list != NULL) {
	if (list->value != NULL)
      free(list->value);
    tmp = list->next;
    free(list);
    list = tmp;
  }
  list = NULL;
}

//Push a value to the end of a linked list
//If linked list=NULL, create one
//returns pointer to end of list -> new element
LolList *lollist_push(LolList *list, void *val) {
  if (list==NULL) {
    list = malloc(sizeof(LolList));
    list->next = list->prev = NULL;
    list->value = val;
    return list;
  } else {
    while (list->next != NULL)
      list = list->next;
    list->next = malloc(sizeof(LolList));
    list->next->next = NULL;
    list->next->prev = list;
    list->next->value = val;
    return list->next;
  }
}

//same as push, but returns beginning of list
LolList *lollist_add(LolList *list, void *val) {
  LolList *ret = lollist_push(list, val);
  if (list != NULL)
    return list;
  return ret;
}

//Pop from the beginning, return value if list not empty, or NULL
void *lollist_shift(LolList **list) {
  LolList *lst = *list;

  if (list==NULL || lst==NULL)
    return NULL;

  LolList *next = lst->next;
  void *val = lst->value;
  free(lst);

  if (next != NULL)
    next->prev = NULL;
  (*list) = next;

  return val;
}

//input: position pointer within a lollist, new value
//returns: pointer to position of new value
void *lollist_insert(LolList **start, LolList *pos, void *val) {
  if (pos == NULL)
    return NULL; //no reference element given -> failed

  LolList *n = lollist_add(NULL, val); //create one element list
  //insert element right before current one
  n->next = pos;
  n->prev = pos->prev;
  if (n->prev == NULL)
    (*start) = n;
  else {
    pos->prev->next = n;
    pos->prev = n;
  }
  return n;
}

//remove current element, returns pointer to next element
void *lollist_remove(LolList *pos) {
  void *ret = NULL;

  if (pos==NULL)
    return NULL;

  if (pos->prev != NULL)
    pos->prev->next = pos->next;
  if (pos->next != NULL)
    pos->next->prev = pos->prev;

  ret = pos->next;
  pos->next = pos->prev = NULL;
  free(pos); //remove entry... free value is job of user
  return ret; //return next entry
}

//Sleep amount of ms
void sleep_ms(unsigned int ms) {
  usleep(1000*ms);
}

//Evaluate argument from argv, return value or default given
//input: arguments from main and null terminated argument to search, default value
char *eval_arg(int argc, char *argv[], const char *arg, char *def) {
  int index = -1;
  for(int i=1; i<argc; i++) {
    if (strncmp(argv[i], arg, strlen(arg))==0) {
      index = i;
      break;
    }
  }
  if (index != -1 && index+1 < argc)
    return argv[index+1];
  return def;
}

//convert str to int, if str NULL, return default -> eval arg wrapper
int int_arg(const char *str, int def) {
  if (str == NULL)
    return def;
  return atoi(str);
}

int eval_flag(int argc, char *argv[], const char *arg) {
  for(int i=1; i<argc; i++)
    if (strncmp(argv[i], arg, strlen(arg))==0)
      return 1;
  return 0; //not found
}
