/* network.h
 * Some network functions
 * Copyright (C) 2013 Anton Pirogov
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdio.h>

#define RCVBUFSIZE 8192

FILE *open_tcp_stream(const char *host, int port);
void close_tcp_stream(FILE *stream);
int puts_tcp_stream(const char *str, FILE *s); //put string into tcp stream
//to read use fgets

#endif /* _NETWORK_H_ */
