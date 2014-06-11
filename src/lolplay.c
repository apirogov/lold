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
LolList *ascii2frames(LolList *lines, int grayscale);

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
LolList *ascii2frames(LolList *lines, int grayscale) {
  LolFactory *fac = lolfac_new();
  fac->grayscale = grayscale;

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
        if (line[j] == '.' || line[j] == '0')
          fac->frame[i][j] = 0;
        else if (grayscale && line[j]>='1' && line[j]<='7')
          fac->frame[i][j] = line[j]-'0';
        else
          fac->frame[i][j] = grayscale ? 7 : 1;
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
    printf("Usage: lolplay OPTIONS [-A|-P] [FILE (if not stdin)]\n");
    printf("       lolplay OPTIONS [-m|-M] \"message\"\n");
    printf("       lolplay OPTIONS -b VALUE\n");
    printf("       lolplay OPTIONS -t\n");
    printf("FLAGS:\n");
    printf("  -h: lold host, default: %s\n", DEF_LOLD_HOST);
    printf("  -p: lold port, default: %i\n", DEF_LOLD_PORT);
    printf("\n");
    printf("  -D: Frame delay (20-1000),  default: %i\n", DEF_DELAY);
    printf("  -T: TTL in sec (0-600),     default: %i\n", DEF_TTL);
    printf("  -C: Channel/Priority (>=0), default: %i\n", DEF_PRI);
    printf("\n");
    printf("  -A: File is in AsciiFrame format (monochrome)\n");
    printf("  -P: File is PDE sketch file from Lol Shield Theatre Homepage\n");
    printf("  -R: File is raw animation data (as sent to sketch)\n");
    printf("\n");
    printf("  -m: Send scrolling text message (pre-rendered)\n");
    printf("  -M: Send scrolling text message (rendered on Lol Shield)\n");
    printf("\n");
    printf("  -b: Set maximum brightness (1-7)\n");
    printf("  -t: Toggle Lolshield mode (loop ROM or pass through mode)\n");
    printf("\n");
    printf("  -B: with '-[mAPR]' - burn this animation to Lolshield ROM\n");
    printf("  -g: with '-A' - render in grayscale (not with '-B' !)\n");
    printf("  -o: added to any command - output raw frames to stdout, no sending\n");
    printf("\n");

    exit(EXIT_FAILURE);
  }

  int del = int_arg(eval_arg(argc, argv, "-D\0", NULL), DEF_DELAY);
  int ttl = int_arg(eval_arg(argc, argv, "-T\0", NULL), DEF_TTL);
  int pri = int_arg(eval_arg(argc, argv, "-C\0", NULL), DEF_PRI);
  int port = int_arg(eval_arg(argc, argv, "-p\0", NULL), DEF_LOLD_PORT);
  char *host = eval_arg(argc, argv, "-h\0", DEF_LOLD_HOST);

  int bflag = eval_flag(argc, argv, "-b\0");
  int gflag = eval_flag(argc, argv, "-g\0");
  int oflag = eval_flag(argc, argv, "-o\0");
  int tflag = eval_flag(argc, argv, "-t\0");
  int burnFlag = eval_flag(argc, argv, "-B\0");

  //Initialize loltask
  LolTask *task = loltask_new();
  task->delay = burnFlag ? 100 : del;
  task->ttl = ttl;
  task->pri = pri;

  if (tflag) { //just toggle
    char *frame = malloc(100*sizeof(char));
    frame[0] = '\0';
    snprintf(frame, 100, "16384,0,0,0,0,0,0,0,0");
    task->frames = lollist_add(task->frames, frame);
    goto send;
  } else if (bflag) { //just set brightness
    char *val = eval_arg(argc, argv, "-b\0", NULL);
    char *frame = malloc(100*sizeof(char));
    frame[0] = '\0';
    snprintf(frame, 100, "16387,%s",val);
    task->frames = lollist_add(task->frames, frame);
    goto send;
  }

  if (eval_flag(argc, argv, "-m\0")) { //software text message
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

  } else if (eval_flag(argc,argv,"-M\0")) {
    char *msg = eval_arg(argc, argv, "-M\0", NULL);
    char *frame = malloc(100*sizeof(char));
    frame[0] = '\0';
    snprintf(frame, BUFSIZE-1, "16385,%i,0,%s", del, msg);
    task->frames = lollist_add(task->frames, frame);

  } else { //animation file
    //open file
    char *filename = argv[argc-1];
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
      // exit(EXIT_FAILURE); //failed opening file
      fp = stdin; //use standard input if no file given
    }

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
      task->frames = ascii2frames(lines, gflag);
    } else if (type==2) { //pde
      task->delay = get_delay_from_pde(lines);
      task->frames = pde2frames(lines);
    }
  }

  //burn flag set
  if (burnFlag) {
    if (gflag) {
        printf("Can not burn grayscale animation!\n");
        exit(EXIT_FAILURE);
    }

    //check message length
    int cnt = 0;
    LolList *curr = task->frames;
    while (curr!=NULL) {
        curr = curr->next;
        cnt++;
        if (cnt>56) {
          printf("Message too long to store (max. frames: 56)!\n");
          exit(EXIT_FAILURE);
        }
    }
    //TODO: check invalid stuff which can not be recorded (all non-regular frames)

    //prepend and append recording signal commands (start/stop recording)
    char *frame = malloc(100*sizeof(char));
    snprintf(frame, 100, "16384,1,%i,0,0,0,0,0,0", del);
    lollist_insert(&task->frames, task->frames, frame);

    frame = malloc(100*sizeof(char));
    snprintf(frame, 100, "16384,0,0,0,0,0,0,0,0");
    lollist_add(task->frames, frame);
  }

send:
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

  //try to send
  if (!loltask_send(host, port, task)) //error sending
    exit(EXIT_FAILURE);

  //done
  loltask_free(task);
  return EXIT_SUCCESS;
}
