/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005 - 2012 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _XWRELAY_PRIV_H_
#define _XWRELAY_PRIV_H_

#include <string>
#include <time.h>
#include <netinet/in.h>
#include <stdint.h>

#include "lstnrmgr.h"
#include "xwrelay.h"
#include "addrinfo.h"

typedef uint8_t HostID;   /* see HOST_ID_SERVER */
typedef uint32_t DevIDRelay;

typedef enum {
    XW_LOGERROR
    ,XW_LOGINFO
    ,XW_LOGVERBOSE0
    ,XW_LOGVERBOSE1
} XW_LogLevel;

void logf( XW_LogLevel level, const char* format, ... );
bool willLog( XW_LogLevel level );

void denyConnection( const AddrInfo* addr, XWREASON err );
bool send_with_length_unsafe( const AddrInfo* addr, 
                              const uint8_t* buf, size_t bufLen,
                              uint32_t* packetIDP );
void send_havemsgs( const AddrInfo* addr );

typedef void (*OnMsgAckProc)( bool acked, DevIDRelay devid, uint32_t packetID, 
                              void* data );
bool post_message( DevIDRelay destDevID, const char* message,
                   OnMsgAckProc proc, void* data );
void post_upgrade( DevIDRelay devid );

time_t uptime(void);

void blockSignals( void );      /* call from all but main thread */

int GetNSpawns(void);

int make_socket( unsigned long addr, unsigned short port );

int read_packet( int sock, uint8_t* buf, int buflen );

void onMsgAcked( bool acked, uint32_t packetID, void* data );

const char* cmdToStr( XWRELAY_Cmd cmd );
const char* msgToStr( XWRelayReg msg );

extern class ListenerMgr g_listeners;

#endif
