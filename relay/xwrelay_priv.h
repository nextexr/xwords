/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

#ifndef _XWRELAY_PRIV_H_
#define _XWRELAY_PRIV_H_

#include <time.h>

typedef unsigned short HostID;
typedef unsigned long CookieID; /* stands in for string after connection established */

void logf( const char* format, ... );

void killSocket( int socket, char* why );

time_t now();

#endif
