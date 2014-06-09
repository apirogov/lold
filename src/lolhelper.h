/* lolhelper.h
 * Common shared functions
 * Copyright (C) 2013 Anton Pirogov
 */
#ifndef _LOLHELPER_H_
#define _LOLHELPER_H_

//default server port
#define DEF_LOLD_PORT 10101

#define BUFSIZE 128

//representation of empty frame, null-terminated, with newline
extern const char EMPTY_FRAME[];

//generic linked list
struct LolList {
  struct LolList *next;
  struct LolList *prev;
  void *value;
};
typedef struct LolList LolList;

//linked list generic functions
void lollist_free(LolList *list);
LolList *lollist_push(LolList *list, void *val);
LolList *lollist_add(LolList *list, void *val);
void *lollist_shift(LolList **list);
void *lollist_insert(LolList **start, LolList *pos, void *val);
void *lollist_remove(LolList *pos);


//sleep function with ms
void sleep_ms(unsigned int ms);

//Find passed argument, return value or default
char *eval_arg(int argc, char *argv[], const char *arg, char *def);
//wrapper for eval arg for numeric values
int int_arg(const char *str, int def);

//tests whether a flag is present
int eval_flag(int argc, char *argv[], const char *arg);

#endif /* _LOLHELPER_H_ */
