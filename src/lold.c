/* lold.c
 * Lolshield Daemon
 * Copyright (C) 2013 Anton Pirogov
 */
#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
//non-blocking stream
#include <fcntl.h>

//threading
#include <pthread.h>

//to trap INT signal
#include <signal.h>

//directory listing
#include <sys/types.h>
#include <dirent.h>

//network
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "arduino-serial-lib.h"
#include "lolhelper.h"
#include "loltask.h"
#include "lold.h"
#include "network.h"

#define DEBUG 0

int s_port = -1; //serial port descriptor
int svr_sock = 0; //server socket

int to_stdout = 0; //stdout flag set?
char *device = NULL; //path of device to be used
int delay = 0; //delay between frames in ms
int port = 0; //server listening port

int streaming = 0; //server is in streaming mode, flag RO for main
pthread_mutex_t imutex; //mutex for interrupted flag (RW for both threads)
int interrupted = 0; //animation is interrupted
int shutting_down = 0; //server is shutting down, flag RO for server thread
pthread_mutex_t qmutex; //mutex for queue access (RW for both threads)
LolList *queue = NULL; //animation queue

//Prototypes
static void error_exit(char *error_message);
void dummy_output(const char *frame); //frame output to stdout
void render_frame(const char *dev, const char *frame); //render a frame to device
char *autodetect(void); //detect usb serial device

void sig_handler(int sig);    //handling SIGINT
void *server_thread(void *t); //listening server
void *client_thread(void *t); //every client
void read_task(FILE *stream);
void read_stream(FILE *stream);

void insert_task(LolTask *task);
void clean_tasks(void);

/* Die Funktion gibt den aufgetretenen Fehler aus und
 * beendet die Anwendung. */
static void error_exit(char *error_message) {
  fprintf(stderr, "%s: %s\n", error_message, strerror(errno));
  exit(EXIT_FAILURE);
}

//mock output to stdout for testing/ no arduino present
void dummy_output(const char *frame) {
  char buff[100];
  strncpy(buff, frame, strlen(frame)+1);

  char * pch;
  pch = strtok (buff,",");
  while (pch != NULL)
  {
    int num = atoi(pch);
    for (int i=0; i<13; i++) {
      if ((num & (1<<i)) != 0)
        printf("%c[31;1mO%c[0m", 27, 27); //ON (ANSI red)
      else
        printf("O"); //OFF
    }
    printf("\n");
    pch = strtok (NULL, ",");
  }
  for (int i=0; i<9; i++)
    printf("%c[A", 27); //ANSI cursor up
  fflush(stdout);
}

//render a frame to real device or stdout
void render_frame(const char *dev, const char *frame) {
  if (dev==NULL) { //dummy
    dummy_output(frame);
    return;
  }

  //open port if no open port
  if (s_port == -1) {
    s_port = serialport_init(dev, 9600);
    if (s_port == -1) {
      fprintf(stderr, "Could not open serial device file! Aborting.\n");
      exit(EXIT_FAILURE);
    }
  }

  //write frame
  serialport_write(s_port, frame);
  serialport_write(s_port, "\n");
}

//returns first ttyUSB/ttyACM device name string pointer, or NULL
char *autodetect(void) {
  DIR *dp;
  struct dirent *ep;

  dp = opendir ("/dev/");
  if (dp != NULL) {
    while (ep = readdir(dp)) {
      if (strncmp(ep->d_name, "ttyUSB", 6)==0 || strncmp(ep->d_name, "ttyACM", 6)==0) {
        int len = strlen(ep->d_name);

        //build path to file
        char *ret = malloc(sizeof(char)*len+6);
        memcpy(ret, "/dev/", 5);
        memcpy(ret+5, ep->d_name, len);
        ret[len+6] = '\0';

        (void)closedir(dp);
        return ret;
      }
    }
    (void)closedir(dp);
    return NULL; //not found
  }
  //could not open directory
  return NULL;
}

//server is asked to shutdown by SIGINT
void sig_handler(int sig) {
  if (sig == SIGINT)
    shutting_down = 1;
}

//lold server main routine
void *server_thread(void *t) {
  struct sockaddr_in server, client;
  int fd;
  unsigned int len;

  svr_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  int optval=1;
  setsockopt(svr_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

  if (svr_sock < 0)
    error_exit("Error creating socket.");

  /* Erzeuge die Socketadresse des Servers. */
  memset( &server, 0, sizeof (server));
  /* IPv4-Verbindung */
  server.sin_family = AF_INET;
  /* INADDR_ANY: jede IP-Adresse annehmen */
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  /* Portnummer */
  server.sin_port = htons(port);
  /* Erzeuge die Bindung an die Serveradresse
   * (genauer: an einen bestimmten Port). */
  if(bind(svr_sock,(struct sockaddr*)&server, sizeof( server)) < 0)
    error_exit("Can not bind socket.");

  /* Teile dem Socket mit, dass Verbindungswünsche
   * von Clients entgegengenommen werden. */
  if(listen(svr_sock, 5) == -1 )
    error_exit("Error on listen.");

  if (DEBUG) fprintf(stderr, "> Server ready, waiting for clients\n");

  while (!shutting_down) {
    len = sizeof(client);
    fd = accept(svr_sock, (struct sockaddr*)&client, &len);
    if (fd < 0)
      error_exit("Error on accept");
    if (DEBUG) fprintf(stderr, "> Handle client with address: %s\n", inet_ntoa(client.sin_addr));

    //start new thread for each incoming client
    pthread_t clientThread;
    int *arg = malloc(sizeof(int));
    *arg = fd;
    pthread_create(&clientThread, NULL, client_thread, (void*)arg);
    pthread_detach(clientThread);

    //---- test ani ----
    /*
    sleep_ms(2000);
    LolTask *tsk = loltask_new();
    //tsk->delay = 200;

    for (int i=0; i<14; i++) {
      char *frame = malloc(sizeof(char)*100);
      snprintf(frame, 100, "%i,%i,%i,%i,%i,%i,%i,%i,%i", 1<<i, 1<<i, 1<<i, 1<<i, 1<<i, 1<<i, 1<<i, 1<<i, 1<<i);
      tsk->frames = lollist_add(tsk->frames, frame);
    }

    insert_task(tsk);

    if (DEBUG)
      fprintf(stderr, "-> Insert ani\n");
    */
    //---- end test ani ----
  }

  close(svr_sock);
  pthread_exit(NULL);
}

//called for every client
void *client_thread(void *t) {
  int fd = *(int*)t;
  free(t); //free argument int
  FILE *stream = fdopen(fd, "r");

  char buff[100];
  buff[99] = '\0';

  if (streaming) { //locked by some stream -> tell client
    if (DEBUG) fprintf(stderr, "> Animation rejected. Server streaming.\n");

    puts_tcp_stream(LOLD_SYM_BSY "\n\0", stream);
  } else { //accept client
    puts_tcp_stream(LOLD_SYM_OK "\n\0", stream);

    //read head - task or stream?
    //tcp_recv_string(fd, 100, buff);
    fgets(buff, 100, stream);
    if (strncmp(buff, LOLD_SYM_TSK, LOLD_SYM_TSK_LEN)==0) //task
      read_task(stream);
    else if (strncmp(buff, LOLD_SYM_STM, LOLD_SYM_STM_LEN)==0) { //streaming
      streaming = 1;
      read_stream(stream);
      render_frame(device, EMPTY_FRAME); // clear lolshield
      streaming = 0;
    } else { //invalid
      puts_tcp_stream(LOLD_SYM_ERR "\n\0", stream);
    }
  }

  //close file and socket, exit thread
  fclose(stream);
  close(fd);
  pthread_exit(NULL);
}

//read async animation task
void read_task(FILE *stream) {
  char buff[100];
  buff[99] = '\0';
  LolTask *tsk = loltask_new();

  //read parameters
  while (1) {
    fgets(buff, 100, stream);

    //start of data section
    if (strncmp(buff, LOLD_SYM_DAT, LOLD_SYM_DAT_LEN) == 0)
      break;

    //read parameter and throw error if out of range
    strtok(buff," "); //param
    int val = atoi(strtok(NULL, " \n")); //value

    if (strncmp(buff, LOLD_SYM_DEL, LOLD_SYM_DEL_LEN) == 0
            && val>=50 && val<=1000)
      tsk->delay = val;
    else if (strncmp(buff, LOLD_SYM_TTL, LOLD_SYM_TTL_LEN) == 0
            && val>=0 && val<=600)
      tsk->ttl = val;
    else if (strncmp(buff, LOLD_SYM_PRI, LOLD_SYM_PRI_LEN) == 0
            && val>=0)
      tsk->pri = val;
    else {
      loltask_free(tsk);
      puts_tcp_stream(LOLD_SYM_ERR"\n\0", stream);
      return;
    }
  }

  //read frames
  while (1) {
    fgets(buff, 100, stream);

    //end of frames
    if (strncmp(buff, LOLD_SYM_END"\0", LOLD_SYM_END_LEN) == 0)
      break;

    char *save_ptr;
    strtok_r(buff,"\n", &save_ptr); //strip newline

    //check frame (9 comma sep ints?)
    int commas=0;
    for (unsigned int i=0; i<strlen(buff); i++)
      if (buff[i]==',')
        commas++;
    if (commas!=8) {
      loltask_free(tsk);
      puts_tcp_stream(LOLD_SYM_ERR"\n\0", stream);
      return;
    }

    //copy frame into task
    char *frame = malloc(sizeof(char)*strlen(buff)+1);
    memcpy(frame, buff, strlen(buff)+1);
    tsk->frames = lollist_add(tsk->frames, frame);
  }

  //insert task into list
  insert_task(tsk);

  if (DEBUG) fprintf(stderr, "> Animation with priority %i added.\n", tsk->pri);

  //tcp_send_string(fd, LOLD_SYM_OK"\n\0");
  puts_tcp_stream(LOLD_SYM_OK"\n\0", stream);
}

//read stream, just act as proxy to lolshield
void read_stream(FILE *stream) {
  char buff[100];
  buff[99] = '\0';

  if (DEBUG) fprintf(stderr, "> Stream started.\n");

  //set fgets non-blocking
  int fd = fileno(stream);
  int flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);

  //read frames until 10 sec nothing happens
  long last_frame_time = time(NULL);
  while(1) {
    if (time(NULL)>last_frame_time+10) {
      if (DEBUG) fprintf(stderr, "> Stream Timeout. Kill stream.\n");

      puts_tcp_stream(LOLD_SYM_ERR"\n\0", stream); //timeout
      return;
    }

    if (fgets(buff, 100, stream) == NULL) { //nothing in buffer?
      sleep_ms(30);
      continue;
    }

    //assume there is some input -> new time
    last_frame_time = time(NULL);

    //end of frames
    if (strncmp(buff, LOLD_SYM_END"\0", LOLD_SYM_END_LEN) == 0)
      break;

    char *save_ptr;
    strtok_r(buff,"\n", &save_ptr); //strip newline

    //check frame (9 comma sep ints?)
    int commas=0;
    for (unsigned int i=0; i<strlen(buff); i++)
      if (buff[i]==',')
        commas++;
    if (commas!=8) {
      if (DEBUG) fprintf(stderr, "> Invalid stream frame. Kill stream.\n");

      puts_tcp_stream(LOLD_SYM_ERR"\n\0", stream); //invalid frame
      return;
    }

    render_frame(device, buff); //directly output frame
  }

  if (DEBUG) fprintf(stderr, "> Stream ended.\n");

  puts_tcp_stream(LOLD_SYM_OK"\n\0", stream); //ok
}

//insert a task into the queue on correct location, exclusive queue access
void insert_task(LolTask *task) {
  pthread_mutex_lock(&qmutex);

  int inserted = 0;
  LolList *curr = queue;
  while (curr!=NULL) {
    LolTask *t = curr->value;
    if (t->pri <= task->pri && t->timestamp <= task->timestamp) {
      lollist_insert(&queue, curr, task);
      inserted = 1;
      break;
    }
    curr = curr->next;
  }

  //on the end of the list without result -> just append/new list
  if (!inserted)
    queue = lollist_add(queue, task);

  pthread_mutex_lock(&imutex);
  interrupted = 1; //notify frame render thread
  pthread_mutex_unlock(&imutex);

  pthread_mutex_unlock(&qmutex);
}

//remove aged tasks, exclusive queue access
void clean_tasks(void) {
  pthread_mutex_lock(&qmutex);

  long currtime = time(NULL);

  LolList *pos = queue;
  while (pos!=NULL) {
    LolTask *curr = pos->value;
    if (curr->frames == NULL) {
      if (DEBUG) fprintf(stderr, "Task cleaned (done)\n");
      loltask_free(curr);
      if (pos->prev == NULL) //first elem of queue removed -> adjust queue
        queue = pos = lollist_remove(pos);
      else
        pos = lollist_remove(pos);
    } else if (curr->ttl > 0 && currtime > (curr->timestamp+curr->ttl)) {
      if (DEBUG) fprintf(stderr, "Task cleaned (age)\n");
      loltask_free(curr);
      if (pos->prev == NULL) //first elem of queue removed -> adjust queue
        queue = pos = lollist_remove(pos);
      else
        pos = lollist_remove(pos);
    }
    if (pos)
      pos = pos->next;
  }

  pthread_mutex_unlock(&qmutex);
}

int main(int argc, char *argv[]) {
  if (eval_flag(argc,argv,"-h\0")) {
    printf("Usage: lold [-p PORT] [-D DELAY] [-d DEVICE]\n\n");
    return EXIT_FAILURE;
  }

  delay = int_arg(eval_arg(argc, argv, "-D\0", NULL), DEF_DELAY);
  port = int_arg(eval_arg(argc, argv, "-p\0", NULL), DEF_LOLD_PORT);
  device = eval_arg(argc, argv, "-d\0", NULL);
  to_stdout = eval_flag(argc, argv, "--stdout\0");

  if (!to_stdout && device==NULL) //no device provided?
    device = autodetect();
  if (!device && !to_stdout) //no device found by autodetect?
    fprintf(stderr, "No serial USB device found! (Try -d flag?) Output to stdout.\n");

  pthread_mutex_init(&imutex, NULL); //init mutex for interrupted flag
  pthread_mutex_init(&qmutex, NULL); //init mutex for queue

  //Start web server thread listening for clients sending stuff
  pthread_t serverThread;
  pthread_create(&serverThread, NULL, server_thread, NULL);
  pthread_detach(serverThread);

  //trap INT
  signal(SIGINT, sig_handler);

  LolTask *currtask = NULL; //task currently being executed

  //clean lolshield
  render_frame(device, EMPTY_FRAME);

  //loop outputting animation frames to device
  while (!shutting_down) {
    //do nothing if some client is streaming
    if (streaming) {
      sleep_ms(100);
      continue;
    }

    //New task arrived -> check stuff
    pthread_mutex_lock(&imutex);
    if (interrupted) {
      if (DEBUG) fprintf(stderr, "New task inserted!\n");

      if (queue==NULL)
        exit(EXIT_FAILURE); //can not happen!

      LolTask *first = (LolTask*)(queue->value);

      //new arrived has higher priority? cancel current ani
      if (currtask != NULL && first->pri > currtask->pri) {
        loltask_free(currtask);
        currtask = NULL;

        if (DEBUG) fprintf(stderr, "Ani cancelled\n");
      }

      interrupted = 0;
    }
    pthread_mutex_unlock(&imutex);

    //load new task if neccessary
    if (currtask == NULL) {
      currtask = lollist_shift(&queue);

      if (DEBUG) if (currtask != NULL) fprintf(stderr, "Ani start\n");
    }

    //nothing in queue -> just wait
    if (currtask == NULL) {
      sleep_ms(delay);
      continue;
    }

    //animation delay
    sleep_ms(currtask->delay);

    //get next frame
    char *frame = lollist_shift(&(currtask->frames));

    //current animation done?
    if (frame == NULL) {
      currtask = NULL;
      clean_tasks(); //remove aged tasks

      //render empty frame to clean lolshield
      render_frame(device, EMPTY_FRAME);
      sleep_ms(delay);

      if (DEBUG) fprintf(stderr, "Ani done\n");
      continue;
    }

    //render next frame
    render_frame(device, frame);
    free(frame);
  }
  if (DEBUG) fprintf(stderr, "Shutting down\n");

  //shutting down
  render_frame(device, EMPTY_FRAME); //clean lolshield
  serialport_close(port); //close port
  pthread_cancel(serverThread); //kill server
  pthread_mutex_destroy(&qmutex); //uninit mutex
  pthread_mutex_destroy(&imutex); //uninit mutex

  close(svr_sock); //close server listening socket
  pthread_exit(NULL);
  return EXIT_SUCCESS;
}

