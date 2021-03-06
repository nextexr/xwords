/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package org.eehouse.android.xw4;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;

public class RelayReceiver extends BroadcastReceiver {
    private static final String TAG = RelayReceiver.class.getSimpleName();

    @Override
    public void onReceive( Context context, Intent intent )
    {
        Log.d( TAG, "onReceive(intent=%s)", intent );
        RelayService.timerFired( context );
    }

    public static void setTimer( Context context )
    {
        setTimer( context, 1000 * XWPrefs.getProxyIntervalSeconds( context ) );
    }

    public static void setTimer( Context context, long interval_millis )
    {
        AlarmManager am =
            (AlarmManager)context.getSystemService( Context.ALARM_SERVICE );

        Intent intent = new Intent( context, RelayReceiver.class );
        PendingIntent pi = PendingIntent.getBroadcast( context, 0, intent, 0 );

        // Check if we have any relay IDs, since we'll be using them to
        // identify connected games for which we can fetch messages
        if ( interval_millis > 0 && DBUtils.haveRelayIDs( context ) ) {
            long fire_millis = SystemClock.elapsedRealtime() + interval_millis;
            am.set( AlarmManager.ELAPSED_REALTIME_WAKEUP, fire_millis, pi );
        } else {
            // will happen if user's set getProxyIntervalSeconds to return 0
            am.cancel( pi );
        }
    }

}
