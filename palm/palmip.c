/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001-2005 by Eric House (fixin@peak.org).  All rights reserved.
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

#ifdef BEYOND_IR

#include <TimeMgr.h>

#include "palmip.h"
#include "memstream.h"

#define NETLIB_TIMEOUT (5 * sysTicksPerSecond)
#define NAMELOOKUP_TIMEOUT (60*sysTicksPerSecond)

void
palm_ip_setup( PalmAppGlobals* globals )
{
    globals->nlStuff.socket = -1;
    globals->nlStuff.netLibRef = 0; /* probably unnecessary */
}

void
palm_ip_close( PalmAppGlobals* globals )
{
    if ( globals->nlStuff.netLibRef != 0 ) {
        
        if ( globals->nlStuff.socket != -1 ) {
            Err ignore;
            NetLibSocketClose( globals->nlStuff.netLibRef,
                               globals->nlStuff.socket, 0, &ignore);
            globals->nlStuff.socket = -1;
        }

        NetLibClose(globals->nlStuff.netLibRef, 0);

        globals->nlStuff.netLibRef = 0;
    }
} /* palm_ip_close */

static XP_Bool
openNetLibIfNot( PalmAppGlobals* globals )
{
    Err err;
    XP_Bool done = globals->nlStuff.netLibRef != 0;

    if ( !done ) {
        UInt16 libRef;
        err = SysLibFind( "Net.lib", &libRef);
        if ( err == errNone ) {
            UInt16 ifErrs;
            err = NetLibOpen( libRef, &ifErrs );
            if ( err == errNone ) {
                globals->nlStuff.netLibRef = libRef;
                done = XP_TRUE;
                XP_LOGF( "successful netlib open" );
            } else {
                XP_LOGF( "NetLibOpen failed: err=%d; ifErrs=%d",
                         err, ifErrs );
            }
        }
    }

    return done;
} /* openNetLibIfNot */

static XP_Bool
openSocketIfNot( PalmAppGlobals* globals )
{
    XP_Bool open = globals->nlStuff.socket != -1;
    
    if ( !open ) {
        Err err;
        NetSocketRef socket;

        XP_ASSERT( globals->nlStuff.netLibRef != 0 );

        socket = NetLibSocketOpen( globals->nlStuff.netLibRef,
                                   netSocketAddrINET, 
                                   netSocketTypeStream,
                                   0, /* protocol (ignored) */
                                   NETLIB_TIMEOUT, &err );
        if ( err == errNone ) {
            NetSocketLingerType lt;

            open = XP_TRUE;
            globals->nlStuff.socket = socket;
            XP_LOGF( "Opened socket %d", socket );

            /* Just for grins, turn linger off; suggested by
             * http://tomfrauen.blogspot.com/2005/01/some-palm-os-network-programming.html
             */
            lt.onOff = true;
            lt.time = 0;
            NetLibSocketOptionSet( globals->nlStuff.netLibRef, socket, 
                                   netSocketOptLevelSocket,
                                   netSocketOptSockLinger, &lt, sizeof(lt),
                                   NETLIB_TIMEOUT, &err );
        } else {
            XP_LOGF( "Failed to open socket: %d", err );
        }
    }
    return open;
} /* openSocketIfNot */

static XP_Bool
connectSocket( PalmAppGlobals* globals, CommsAddrRec* addr )
{
    XP_Bool success;
    Err err;
    NetSocketAddrINType socketAddr;

    socketAddr.family = netSocketAddrINET;
    socketAddr.port = XP_HTONS( addr->u.ip_relay.port );
    socketAddr.addr = XP_HTONL( addr->u.ip_relay.ipAddr );

    success = ( 0 == NetLibSocketConnect( globals->nlStuff.netLibRef,
                                          globals->nlStuff.socket,
                                          (NetSocketAddrType*)&socketAddr,
                                          sizeof(socketAddr), NETLIB_TIMEOUT, 
                                          &err ) );
    if ( !success ) {
        XP_LOGF( "NetLibSocketConnect => %d", err );
    }
    return success;
} /* connectSocket */

/* Only returns true if the name resolves and the machine on the other end is
 * accepting connections on our port.  Might be better to break the process
 * up.
 */
static XP_Bool
resolveAddressIfNot( PalmAppGlobals* globals, CommsAddrRec* addr, 
                     XP_Bool* resolvedP )
{
    XP_Bool resolved = addr->u.ip_relay.ipAddr != 0;
    *resolvedP = XP_FALSE;

    if ( !resolved ) {
        NetHostInfoBufType niBuf;
        NetHostInfoPtr result;
        Err err;

        result = NetLibGetHostByName( globals->nlStuff.netLibRef,
                                      addr->u.ip_relay.hostName, 
                                      &niBuf, NAMELOOKUP_TIMEOUT, &err );
        if ( result == NULL ) {
            XP_LOGF( "NetLibGetHostByName => %d", err );
        } else {
            if ( openSocketIfNot( globals ) ) {

                XP_ASSERT( result->addrLen == sizeof(addr->u.ip_relay.ipAddr) );
                /* Addresses are in host byte order.  So just copy. */
                XP_MEMCPY( &addr->u.ip_relay.ipAddr, result->addrListP[0],
                           sizeof( addr->u.ip_relay.ipAddr ) );
                XP_LOGF( "got address 0x%lx for %s", addr->u.ip_relay.ipAddr,
                         addr->u.ip_relay.hostName );

                if ( connectSocket( globals, addr ) ) {
                    *resolvedP = resolved = XP_TRUE;
                }
            }
        }
    }
    return resolved;
} /* resolveAddressIfNot */

/* Deal with NetLibSend's willingness to send less than the full buffer */
static XP_Bool
sendLoop( PalmAppGlobals* globals, XP_U8* buf, XP_U16 len )
{
    XP_U16 totalSent = 0;

    do {
        XP_S16 thisSent;
        Err err;
        thisSent = NetLibSend( globals->nlStuff.netLibRef, 
                               globals->nlStuff.socket,
                               (void*)(buf + totalSent), 
                               len - totalSent, 0, /* flags */
                               NULL, 0, NETLIB_TIMEOUT, &err );

        if ( thisSent == 0 ) {
            globals->nlStuff.socket = -1;  /* mark socket closed */
            return XP_FALSE;
        } else if ( thisSent < 0 ) {
            XP_LOGF( "NetLibSend => %d", err );
            return XP_FALSE;
        } else {
            totalSent += thisSent;
            if ( totalSent < len ) {
                XP_LOGF( "looping in sendLoop: %d of %d sent so far", 
                         totalSent, len );
            }
        }
    } while ( totalSent < len );

    XP_LOGF( "sendLoop sent %d bytes", len );
    return XP_TRUE;
} /* sendLoop */

XP_S16
palm_ip_send( XP_U8* buf, XP_U16 len, CommsAddrRec* addr,
              PalmAppGlobals* globals )
{
    CommsAddrRec localRec;
    XP_S16 nSent = 0;
    XP_Bool resolved = XP_FALSE;

    XP_LOGF( "palm_ip_send: len=%d", len );
    XP_ASSERT( len < MAX_MSG_LEN );

    if ( !!addr ) {
        XP_MEMCPY( &localRec, addr, sizeof(localRec) );
    } else {
        comms_getAddr( globals->game.comms, &localRec );
    }
    addr = &localRec;

    if ( openNetLibIfNot( globals ) ) {
        if ( resolveAddressIfNot( globals, addr, &resolved ) ) {
            XP_U16 netlen;

            if ( resolved ) {
                comms_setAddr( globals->game.comms, addr );
            }

            /* Send the length */
            netlen = XP_HTONS( len );
            if ( sendLoop( globals, (XP_U8*)&netlen, sizeof(netlen) ) 
                 && sendLoop( globals, buf, len ) ) {
                nSent = len;
            }
        }
    }
    return nSent;
} /* palm_ip_send */

/* Deal with NetLibReceive's willingness to return will fewer bytes than
   requested. */
static XP_Bool
recvLoop( PalmAppGlobals* globals, XP_U8* buf, XP_U16 lenSought )
{
    XP_U32 timeout = TimGetSeconds() + 5;
    XP_U16 totalRead = 0;
    NetSocketAddrINType fromAddr;
    void* fromAddrP;
    UInt16 fromLen;

    if ( globals->romVersion >= 50 ) {
        fromAddrP = NULL;
        fromLen = 0;
    } else {
        fromAddrP = (void*)&fromAddr;
        fromLen = sizeof( fromAddr );
    }

    /* Be sure there's a way to timeout quickly here!!! */
    while ( totalRead < lenSought && TimGetSeconds() < timeout ) {
        Err err;
        Int16 nRead = NetLibReceive( globals->nlStuff.netLibRef,
                                     globals->nlStuff.socket,
                                     buf, lenSought, 0, /* flags */
                                     fromAddrP, &fromLen, 
                                     NETLIB_TIMEOUT, &err );

        if ( (nRead < 0) && (err != netErrTimeout) ) {
            XP_LOGF( "NetLibReceive => %d", err );
            return XP_FALSE;
        } else if ( nRead == 0 ) {
            XP_LOGF( "NetLibReceive; socket close" );
            globals->nlStuff.socket = -1;
            return XP_FALSE;
        } else {
            totalRead += nRead;
        }
    }

    XP_LOGF( "recvLoop got %d bytes", totalRead );
    return totalRead == lenSought;
} /* recvLoop */

static XWStreamCtxt*
packetToStream( PalmAppGlobals* globals, CommsAddrRec* retAddr )
{
    XP_U8 buf[MAX_MSG_LEN];
    XWStreamCtxt* result = NULL;
    XP_U16 netlen;
    
    if ( recvLoop( globals, (XP_U8*)&netlen, sizeof(netlen) ) ) {
        netlen = XP_NTOHS( netlen );
        XP_LOGF( "netlen = %d", netlen );
        if ( recvLoop( globals, buf, netlen ) ) {

            result = mem_stream_make( MEMPOOL globals->vtMgr, 
                                      globals, 0, NULL);
            stream_open( result );
            stream_putBytes( result, buf, netlen );
        }
    }

    return result;
} /* packetToStream */

void
checkHandleNetEvents( PalmAppGlobals* globals )
{
    if ( socketIsOpen( globals ) ) {
        NetFDSetType readFDs;
        NetFDSetType writeFDs;
        NetFDSetType ignoreFDs;
        XP_S16 nSockets;
        UInt16 width;
        Err err;

        netFDZero( &readFDs );
        netFDZero( &writeFDs );
        netFDZero( &ignoreFDs );

        netFDSet( globals->nlStuff.socket, &readFDs );
        netFDSet( sysFileDescStdIn, &readFDs );
        width = XP_MAX( globals->nlStuff.socket, sysFileDescStdIn );

        nSockets = NetLibSelect( globals->nlStuff.netLibRef,
                                 width + 1,
                                 &readFDs, &writeFDs, &ignoreFDs,
                                 NETLIB_TIMEOUT,     /* timeout */
                                 &err );

        if ( nSockets > 0 && err == errNone ) {
            if ( netFDIsSet( globals->nlStuff.socket, &readFDs ) ) {

                XWStreamCtxt* instream;
                CommsAddrRec raddr; 

                XP_MEMSET( &raddr, 0, sizeof(raddr) );

                instream = packetToStream( globals, &raddr );
                if ( !!instream ) {
                    checkAndDeliver( globals, instream, &raddr );
                }
            }
        }
    }
} /* checkHandleNetEvents */

#endif
