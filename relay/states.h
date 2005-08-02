/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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

#ifndef _STATES_H_
#define _STATES_H_


/* states */
typedef enum {
    XW_ST_NONE

    ,XW_ST_INITED               /* Relay's running and the object's been
                                   created, but nobody's signed up yet.  This
                                   is a very short-lived state since an
                                   incoming connection is why the object was
                                   created.  */

    ,XW_ST_CONNECTING           /* At least one device has connected, but no
                                   packets have yet arrived to be
                                   forwarded. */

    ,XW_ST_CHECKING_CONN        /* While we're still not fully connected a
                                   message comes in */

    ,XW_ST_ALLCONNECTED         /* All devices are connected and ready for the
                                   relay to do its work.  This is the state
                                   we're in most of the time.  */

    ,XW_ST_WAITING_RECON        /* At least one device has been timed out or
                                   sent a disconnect message.  We can't flow
                                   messages in this state, and will be killing
                                   all connections if we don't hear back from
                                   the missing guy soon.  */


    ,XW_ST_HEARTCHECK_CONNECTING      /* In the middle of checking heartbeat situation. */
    ,XW_ST_HEARTCHECK_CONNECTED       /* Need two states so we know where to
                                         go back to. */

    ,XW_ST_SENDING_DISCON       /* We're in the process of pulling the plug on
                                   all devices that remain connected. */

    ,XW_ST_DISCONNECTED         /* We're about to kill this object. */

    ,XW_ST_CHECKINGDEST         /* Checking for valid socket */

    ,XW_ST_DEAD                 /* About to kill the object */
} XW_RELAY_STATE;


/* events */
typedef enum {
    XW_EVENT_NONE

    ,XW_EVENT_CONNECTMSG        /* A device is connecting using the cookie for
                                   this object */

    ,XW_EVENT_RECONNECTMSG      /* A device is re-connecting using the
                                   connID for this object */

    ,XW_EVENT_FORWARDMSG        /* A message needs forwarding */

    ,XW_EVENT_HEARTMSG          /* A heartbeat message arrived */

    ,XW_EVENT_HEARTTIMER        /* Time to check for missing heartbeats */

    ,XW_EVENT_DISCONTIMER       /* No reconnect received: time to kill the
                                   remaining connections */

    ,XW_EVENT_CONNTIMER         /* timer for did we get all players hooked
                                   up  */

    ,XW_EVENT_ALLHEREMSG        /* message from server that all expected
                                   player reg messages have been received and
                                   no new hosts should be registering selvs
                                   with cookie. */

    ,XW_EVENT_HEARTOK

    ,XW_EVENT_DESTOK

    ,XW_EVENT_DESTBAD

    ,XW_EVENT_HEARTFAILED
} XW_RELAY_EVENT;


/* actions */
typedef enum {
    XW_ACTION_NONE

    ,XW_ACTION_SENDRSP          /* Send a connection response */

    ,XW_ACTION_FWD              /* Forward a message */

    ,XW_ACTION_NOTEHEART        /* Record heartbeat received */

    ,XW_ACTION_CHECKHEART       /* Check for heartbeats */

    ,XW_ACTION_DISCONNECTALL

    ,XW_ACTION_HEARTOK          /* allows transition back to stationary state */

    ,XW_ACTION_LOCKGAME         /* lock the cref/cookie session to new hosts */

    ,XW_ACTION_CHECKDEST        /* check that a given hostID has a socket */

} XW_RELAY_ACTION;


int getFromTable( XW_RELAY_STATE curState, XW_RELAY_EVENT curEvent,
                  XW_RELAY_ACTION* takeAction, XW_RELAY_STATE* nextState );


char* stateString( XW_RELAY_STATE state );
char* eventString( XW_RELAY_EVENT evt );

#endif
