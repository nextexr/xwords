/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2015 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.view.View;


import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.loc.LocUtils;

public class XWConnAddrPreference extends DialogPreference {

    private Context m_context;
    private ConnViaViewLayout m_view;

    public XWConnAddrPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        m_context = context;

        setDialogLayoutResource( R.layout.conn_types_display );

        setNegativeButtonText( LocUtils.getString( context, android.R.string.cancel ) );

        CommsConnTypeSet curSet = XWPrefs.getAddrTypes( context );
        setSummary( curSet.toString( context, true ) );
    }

    @Override
    protected void onBindDialogView( View view )
    {
        LocUtils.xlateView( m_context, view );
        m_view = (ConnViaViewLayout)view.findViewById( R.id.conn_types );
        final PrefsActivity activity = (PrefsActivity)m_context;
        m_view.configure( XWPrefs.getAddrTypes( m_context ),
                          new ConnViaViewLayout.CheckEnabledWarner() {
                              public void warnDisabled( CommsConnType typ ) {
                                  switch( typ ) {
                                  case COMMS_CONN_SMS:
                                      activity
                                          .makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                                                   Action.ENABLE_NBS_ASK )
                                          .setPosButton( R.string.button_enable_sms )
                                          .setNegButton( R.string.button_later )
                                          .show();
                                      break;
                                  case COMMS_CONN_BT:
                                      activity
                                          .makeConfirmThenBuilder( R.string.warn_bt_disabled,
                                                                   Action.ENABLE_BT_DO )
                                          .setPosButton( R.string.button_enable_bt )
                                          .setNegButton( R.string.button_later )
                                          .show();
                                      break;
                                  case COMMS_CONN_RELAY:
                                      String msg = LocUtils
                                          .getString( m_context, R.string
                                                      .warn_relay_disabled );
                                      msg += "\n\n" + LocUtils
                                          .getString( m_context,
                                                      R.string.warn_relay_later );
                                      activity
                                          .makeConfirmThenBuilder( msg, Action
                                                                   .ENABLE_RELAY_DO )
                                          .setPosButton( R.string.button_enable_relay )
                                          .setNegButton( R.string.button_later )
                                          .show();
                                      break;
                                  default:
                                      Assert.fail();
                                      break;
                                  }
                              }
                          },
                          new ConnViaViewLayout.SetEmptyWarner() {
                              public void typeSetEmpty() {
                                  PrefsActivity activity = (PrefsActivity)m_context;
                                  activity
                                      .makeOkOnlyBuilder( R.string.warn_no_comms )
                                      .show();
                              }
                          }, activity );
    }

    @Override
    public void onClick( DialogInterface dialog, int which )
    {
        if ( AlertDialog.BUTTON_POSITIVE == which && null != m_view ) {
            CommsConnTypeSet curSet = m_view.getTypes();
            XWPrefs.setAddrTypes( m_context, curSet );
            setSummary( curSet.toString( m_context, true ) );
        }
        super.onClick( dialog, which );
    }
}
