/* network.c
 * Network library (modified copypasta from galileocomputing)
 * Copyright (C) 2013 Anton Pirogov
 */

#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "network.h"

FILE *open_tcp_stream(const char *host, int port) {
    struct sockaddr_in server;
    struct hostent *host_info;
    unsigned long addr;
    int sock;

    sock = socket( AF_INET, SOCK_STREAM, 0 );

    if (sock < 0)
    	return 0; //error initializing socket

    memset( &server, 0, sizeof (server));
    if ((addr = inet_addr(host)) != INADDR_NONE) {
        /* host ist eine numerische IP-Adresse. */
        memcpy( (char *)&server.sin_addr, &addr, sizeof(addr));
    }
    else {
        host_info = gethostbyname(host);
        if (NULL == host_info)
            return 0; //unknown server
        /* Server-IP-Adresse */
        memcpy( (char *)&server.sin_addr,
                host_info->h_addr_list[0], host_info->h_length );
    }
    /* IPv4-Verbindung */
    server.sin_family = AF_INET;
    /* Portnummer */
    server.sin_port = htons( port );

    /* Baue die Verbindung zum Server auf. */
    if(connect(sock,(struct sockaddr*)&server,sizeof(server)) <0)
      return 0; //failure on connecting

    //behandle als datei-strom
    FILE *stream = fdopen(sock, "r");
    if (stream == NULL) {
      close(sock);
      return NULL;
    }

    return stream;
}

void close_tcp_stream(FILE *stream) {
  close(fileno(stream));
  fclose(stream);
}

//immediate send without buffering... to send linewise use fgets
int puts_tcp_stream(const char *str, FILE *s) {
	//printf(">send: %s", str); //DEBUG
	int len = strlen(str);
  // den String inkl. Nullterminator an den Server senden
  if (send(fileno(s), str, len, 0) != len)
    return 0; //failure - sent unexpected amount of bytes
  return 1; //success
}

/*
//send a string over an open socket, 1 on success, 0 on failure
int tcp_send_string(int sock, const char *str) {
	//printf(">send: %s", str); //DEBUG

	int len = strlen(str);
  // den String inkl. Nullterminator an den Server senden
  if (send(sock, str, len, 0) != len)
    return 0; //failure - sent unexpected amount of bytes
  return 1; //success
}

//receive a string into a buffer
int tcp_recv_string(int socket, int len, char *str) {
  if((len = recv(socket, str, len, 0)) < 0)
    return 0; //recv error
  str[len] = '\0';
	//printf(">recv: %s", str); //DEBUG
  return 1; //success
}
*/
