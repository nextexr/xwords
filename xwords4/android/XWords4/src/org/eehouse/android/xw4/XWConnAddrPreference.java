/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 - 2014 by Eric House (xwords@eehouse.org).  All
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
import android.widget.CheckBox;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.CompoundButton;
import android.widget.LinearLayout;

import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;

public class XWConnAddrPreference extends DialogPreference {

    private CommsConnTypeSet m_curSet;
    private Context m_context;
    // This stuff probably belongs in CommsConnType
    private static CommsConnTypeSet s_supported;
    static {
        s_supported = new CommsConnTypeSet();
        s_supported.add( CommsConnType.COMMS_CONN_RELAY );
        s_supported.add( CommsConnType.COMMS_CONN_BT );
        s_supported.add( CommsConnType.COMMS_CONN_SMS );
    }

    public static CommsConnTypeSet addConnections( Context context, 
                                                   LinearLayout view, 
                                                   CommsConnTypeSet curTypes )
    {
        LinearLayout list = (LinearLayout)view.findViewById( R.id.conn_types );
        final CommsConnTypeSet tmpTypes = (CommsConnTypeSet)curTypes.clone();

        for ( CommsConnType typ : s_supported.getTypes() ) {
            LinearLayout layout = (LinearLayout)
                LocUtils.inflate( context, R.layout.btinviter_item );
            CheckBox box = (CheckBox)layout.findViewById( R.id.inviter_check );
            box.setText( typ.longName( context ) );
            box.setChecked( curTypes.contains( typ ) );
            list.addView( layout ); // failed!!!
            
            final CommsConnType typf = typ;
            box.setOnCheckedChangeListener( new OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton buttonView, 
                                                  boolean isChecked ) {
                        if ( isChecked ) {
                            tmpTypes.add( typf );
                        } else {
                            tmpTypes.remove( typf );
                        }
                    }
                } );
        }
        return tmpTypes;
    }

    public XWConnAddrPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        m_context = context;

        setDialogLayoutResource( R.layout.conn_types_display );

        setNegativeButtonText( LocUtils.getString( context, R.string.button_cancel ) );

        m_curSet = XWPrefs.getAddrTypes( context );
        setSummary( m_curSet.toString( context ) );
    }

    @Override
    protected void onBindDialogView( View view )
    {
        LocUtils.xlateView( m_context, view );

        m_curSet = addConnections( m_context, (LinearLayout)view, m_curSet );
    }
    
    @Override
    public void onClick( DialogInterface dialog, int which )
    {
        if ( AlertDialog.BUTTON_POSITIVE == which ) {
            DbgUtils.logf( "ok pressed" );
            XWPrefs.setAddrTypes( m_context, m_curSet );
            setSummary( m_curSet.toString( m_context ) );
        }
        super.onClick( dialog, which );
    }
}