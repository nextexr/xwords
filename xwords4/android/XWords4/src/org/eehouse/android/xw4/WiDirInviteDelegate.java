/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2016 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import java.util.Iterator;
import java.util.Map;

import junit.framework.Assert;

public class WiDirInviteDelegate extends InviteDelegate
    implements WiDirService.DevSetListener {
    private static final String SAVE_NAME = "SAVE_NAME";
    private Map<String, String> m_macsToName;
    private Activity m_activity;

    public static void launchForResult( Activity activity, int nMissing,
                                        RequestCode requestCode )
    {
        Intent intent = new Intent( activity, WiDirInviteActivity.class );
        intent.putExtra( INTENT_KEY_NMISSING, nMissing );
        activity.startActivityForResult( intent, requestCode.ordinal() );
    }

    public WiDirInviteDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.inviter );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        String msg = getString( R.string.button_invite );
        msg = getQuantityString( R.plurals.invite_p2p_desc_fmt, m_nMissing,
                                 m_nMissing, msg );
        msg += "\n\n" + getString( R.string.invite_p2p_desc_extra );
        super.init( R.id.button_invite, R.id.invite_desc, msg );
        findViewById( R.id.button_rescan ).setVisibility( View.GONE );
        findViewById( R.id.button_clear ).setVisibility( View.GONE );
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        WiDirService.registerDevSetListener( this );
    }

    @Override
    protected void onPause()
    {
        super.onPause();
        WiDirService.unregisterDevSetListener( this );
    }

    // DevSetListener interface
    public void setChanged( Map<String, String> macToName )
    {
        m_macsToName = macToName;
        runOnUiThread( new Runnable() {
                @Override
                public void run() {
                    rebuildList();
                }
            } );
    }

    private void rebuildList()
    {
        int count = m_macsToName.size();
        String[] names = new String[count];
        String[] addrs = new String[count];
        Iterator<String> iter = m_macsToName.keySet().iterator();
        for ( int ii = 0; ii < count; ++ii ) {
            String mac = iter.next();
            addrs[ii] = mac;
            names[ii] = m_macsToName.get(mac);
        }

        updateListAdapter( R.layout.inviter_item, names, addrs, false );
    }
}