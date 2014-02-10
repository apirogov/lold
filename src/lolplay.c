/* lolplay.c
 * lolplay toolset
 * Copyright (C) 2013 Anton Pirogov
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lolhelper.h"
#include "loltask.h"
#include "lolfactory.h"

#define DEF_LOLD_HOST "localhost"

/* Prototypes */
int get_delay_from_pde(LolList *lines);
LolList *pde2frames(LolList *lines);
LolList *ascii2frames(LolList *lines);

//scans lines for delay
int get_delay_from_pde(LolList *lines) {
  while(lines!=NULL && strncmp("\tdelay(", (char*)lines->value,7)!=0)
	lines = lines->next;
  return atoi(((char*)lines->value)+7);
}

//takes and destroys lines, returns frames
LolList *pde2frames(LolList *lines) {
  LolList *curr = lines;
  LolList *frames = NULL;

  while (curr!=NULL) {
    while(curr!=NULL && strncmp("\tdelay(", (char*)curr->value,7)!=0) //find delay statement
      curr = curr->next;
    if (curr!=NULL) //next line->first number
      curr = curr->next;

    //read line numbers
    int num[9];
    for(int i=0; i<9; i++) {
      if (curr==NULL)
    	break;
      num[i] = atoi(((char*)curr->value)+15); //\tDisplayBitMap(NUM...
      curr = curr->next;
    }

    //generate string and append to list
    char *frame = malloc(100*sizeof(char));
    frame[0] = '\0';
    snprintf(frame, 100, "%i,%i,%i,%i,%i,%i,%i,%i,%i",
    		 num[0], num[1], num[2], num[3], num[4],
    		 num[5], num[6], num[7], num[8]);
    frames = lollist_add(frames, frame);
  }

  lollist_free(lines);
  return frames;
}

//takes and destroys lines, returns frames
LolList *ascii2frames(LolList *lines) {
  LolFactory *fac = lolfac_new();

  LolList *curr = lines;
  char *line = NULL;
  while (curr != NULL) {
    //9 lines representing frame
    for(int i=0; i<9; i++) {
      if (curr==NULL)
        break;

      line = curr->value;
      if (line == NULL || strlen(line)<14)
        break;

      for (int j=0; j<14; j++) {
        fac->frame[i][j] = (line[j]=='.') ? 0 : 1;
      }

      curr = curr->next;
    }
    if (curr != NULL)
      curr = curr->next; //empty line

    lolfac_flip(fac); //generate frame
  }

  lollist_free(lines);
  curr = fac->frames;
  fac->frames = NULL;
  lolfac_free(fac);
  return curr;
}

int main(int argc, char *argv[]) {
  if (argc==1) {
    printf("Usage: lolplay OPTIONS [-F|-P] FILE\n");
    printf("       lolplay OPTIONS -m \"message\"\n");
    printf("FLAGS:\n");
    printf("  -h: lold host, default: %s\n", DEF_LOLD_HOST);
    printf("  -p: lold port, default: %i\n", DEF_LOLD_PORT);
    printf("  -D: Frame delay (50-1000),  default: %i\n", DEF_DELAY);
    printf("  -T: TTL in sec (0-600),     default: %i\n", DEF_TTL);
    printf("  -C: Channel/Priority (>=0), default: %i\n", DEF_PRI);
    printf("\n");
    printf("-A: File is in AsciiFrame format\n");
    printf("-P: File is PDE sketch file from Lol Shield Theatre Homepage\n");
    printf("nothing: File is raw animation file\n");
    printf("\n");
    printf("-m: Send text message\n");
    printf("\n");
    printf("-o: Output frames to stdout, no sending\n");

    exit(EXIT_FAILURE);
  }

  int del = int_arg(eval_arg(argc, argv, "-D\0", NULL), DEF_DELAY);
  int ttl = int_arg(eval_arg(argc, argv, "-T\0", NULL), DEF_TTL);
  int pri = int_arg(eval_arg(argc, argv, "-C\0", NULL), DEF_PRI);
  int port = int_arg(eval_arg(argc, argv, "-p\0", NULL), DEF_LOLD_PORT);
  char *host = eval_arg(argc, argv, "-h\0", DEF_LOLD_HOST);

  int oflag = eval_flag(argc, argv, "-o\0");

  //Initialize loltask
  LolTask *task = loltask_new();
  task->delay = del;
  task->ttl = ttl;
  task->pri = pri;

  if (eval_flag(argc, argv, "-m\0")) { //message
    char *msg = eval_arg(argc, argv, "-m\0", NULL);
    if (msg == NULL) {
      printf("No message passed!\n");
      exit(EXIT_FAILURE);
    }

    //create lolfactory, generate banner
    LolFactory *fac = lolfac_new();
    int len = strlen(msg);
    lolfac_banner(fac, len, msg);

    //free lolfactory rescuing frames
    LolList *frames = fac->frames;
    fac->frames = NULL;
    lolfac_free(fac);

    //fill loltask with frames
    task->frames = frames;

  } else { //animation file
    //open file
    char *filename = argv[argc-1];
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
      exit(EXIT_FAILURE); //failed opening file

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    //read lines
    LolList *lines = NULL;
    while ((read = getline(&line, &len, fp)) != -1) {
      lines = lollist_add(lines, line);
      line = NULL;
    }

    int type = 0; //raw compressed frames
    if (eval_flag(argc, argv, "-A\0")) //ascii frames
      type = 1;
    else if (eval_flag(argc, argv, "-P\0")) //PDE sketch
      type = 2;

    if (type==0) { //raw
      LolList *curr = lines;
      while (curr!=NULL) {
        char *str = curr->value;
        len = strlen(str);
      if (str[len-1] == '\n')
        str[len-1] = '\0';
      curr = curr->next;
      }
      task->frames = lines;
    } else if (type==1) { //ascii
      task->frames = ascii2frames(lines);
    } else if (type==2) { //pde
      task->delay = get_delay_from_pde(lines);
      task->frames = pde2frames(lines);
    }
  }

  //output flag set - no sending, but output
  if (oflag) {
    LolList *curr = task->frames;
    while (curr!=NULL) {
      printf("%s\n", (char*)curr->value);
      curr = curr->next;
    }
    loltask_free(task);
    return EXIT_SUCCESS;
  }

  if (!loltask_send(host, port, task)) //error sending
    exit(EXIT_FAILURE);

  loltask_free(task);
  return EXIT_SUCCESS;
}
