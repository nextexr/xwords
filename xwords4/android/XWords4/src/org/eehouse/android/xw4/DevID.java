/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 - 2015 by Eric House (xwords@eehouse.org).  All rights
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

/* The relay issues an identifier for a registered device. It's a string
 * representation of a 32-bit hex number. When devices register, they pass
 * what's meant to be a unique identifier of their own. GCM-aware devices (for
 * which this was originally conceived) pass their GCM IDs (which can change,
 * and require re-registration). Other devices generate an ID however they
 * choose, or can pass "", meaning "I'm anonymous; just give me an ID based on
 * nothing."
 */

import android.content.Context;
import com.google.android.gcm.GCMRegistrar;

import junit.framework.Assert;

public class DevID {

    private static final String DEVID_KEY = "DevID.devid_key";
    private static final String DEVID_ACK_KEY = "key_relay_regid_ackd";
    private static final String GCM_REGVERS_KEY = "key_gcmvers_regid";

    private static String s_relayDevID;
    private static int s_asInt;

    // Called, likely on DEBUG builds only, when the relay hostname is
    // changed. DevIDs are invalid at that point.
    public static void hostChanged( Context context )
    {
        clearRelayDevID( context );
        RelayService.reset( context );
    }

    public static int getRelayDevIDInt( Context context )
    {
        if ( 0 == s_asInt ) {
            String asStr = getRelayDevID( context );
            if ( null != asStr && 0 < asStr.length() ) {
                s_asInt = Integer.valueOf( asStr, 16 );
            }
        }
        return s_asInt;
    }

    public static String getRelayDevID( Context context, boolean insistAckd )
    {
        String result = getRelayDevID( context );
        if ( insistAckd && null != result && 0 < result.length()
             && ! DBUtils.getBoolFor( context, DEVID_ACK_KEY, false ) ) {
            result = null;
        }
        return result;
    }

    public static String getRelayDevID( Context context )
    {
        if ( null == s_relayDevID ) {
            String asStr = DBUtils.getStringFor( context, DEVID_KEY, "" );
            // TRANSITIONAL: If it's not there, see if it's stored the old way
            if ( 0 == asStr.length() ) {
                asStr = XWPrefs.getPrefsString( context, R.string.key_relay_regid );
            }

            if ( null != asStr && 0 != asStr.length() ) {
                s_relayDevID = asStr;
            }
        }
        DbgUtils.logdf( "DevID.getRelayDevID() => %s", s_relayDevID );
        return s_relayDevID;
    }

    public static void setRelayDevID( Context context, String devID )
    {
        DbgUtils.logdf( "DevID.setRelayDevID()" );
        if ( BuildConfig.DEBUG ) {
            String oldID = getRelayDevID( context );
            if ( null != oldID && 0 < oldID.length()
                 && ! devID.equals( oldID ) ) {
                DbgUtils.logdf( "devID changing!!!: %s => %s", oldID, devID );
            }
        }
        DBUtils.setStringFor( context, DEVID_KEY, devID );
        s_relayDevID = devID;

        DBUtils.setBoolFor( context, DEVID_ACK_KEY, true );
        // DbgUtils.printStack();
    }

    public static void clearRelayDevID( Context context )
    {
        DbgUtils.logf( "DevID.clearRelayDevID()" );
        DBUtils.setStringFor( context, DEVID_KEY, "" );
        // DbgUtils.printStack();
    }

    public static void setGCMDevID( Context context, String devID )
    {
        int curVers = Utils.getAppVersion( context );
        DBUtils.setIntFor( context, GCM_REGVERS_KEY, curVers );
        DBUtils.setBoolFor( context, DEVID_ACK_KEY, false );
    }

    public static String getGCMDevID( Context context )
    {
        int curVers = Utils.getAppVersion( context );
        int storedVers = DBUtils.getIntFor( context, GCM_REGVERS_KEY, 0 );
        // TRANSITIONAL
        if ( 0 == storedVers ) {
            storedVers = XWPrefs.getPrefsInt( context, 
                                              R.string.key_gcmvers_regid, 0 );
            if ( 0 != storedVers ) {
                DBUtils.setIntFor( context, GCM_REGVERS_KEY, storedVers );
            }
        }

        String result;
        if ( 0 != storedVers && storedVers < curVers ) {
            result = "";        // Don't trust what registrar has
        } else {
            result = GCMRegistrar.getRegistrationId( context );
        }
        return result;
    }

    public static void clearGCMDevID( Context context )
    {
        DBUtils.setBoolFor( context, DEVID_ACK_KEY, false );
    }
}