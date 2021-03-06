/* -*- compile-command: "find-and-gradle.sh inXw4dDebug"; -*- */
/*
 * Copyright 2017 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.view.View;
import android.view.View;
import android.widget.TextView;

import java.text.DateFormat;
import java.util.Date;


import org.eehouse.android.xw4.loc.LocUtils;

public class AboutAlert extends XWDialogFragment {
    private static final String TAG = AboutAlert.class.getSimpleName();

    public static AboutAlert newInstance()
    {
        return new AboutAlert();
    }

    @Override
    public Dialog onCreateDialog( Bundle sis )
    {
        Context context = getActivity();
        View view = LocUtils.inflate( context, R.layout.about_dlg );
        TextView vers = (TextView)view.findViewById( R.id.version_string );

        DateFormat df = DateFormat.getDateTimeInstance( DateFormat.FULL,
                                                        DateFormat.FULL );
        String dateString
            = df.format( new Date( BuildConfig.BUILD_STAMP * 1000 ) );
        vers.setText( getString( R.string.about_vers_fmt,
                                 BuildConfig.VARIANT_NAME,
                                 BuildConfig.VERSION_NAME,
                                 BuildConfig.GIT_REV,
                                 dateString ) );

        TextView xlator = (TextView)view.findViewById( R.id.about_xlator );
        String str = getString( R.string.xlator );
        if ( str.length() > 0 && !str.equals("[empty]") ) {
            xlator.setText( str );
        } else {
            xlator.setVisibility( View.GONE );
        }

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( context )
            .setIcon( R.drawable.icon48x48 )
            .setTitle( R.string.app_name )
            .setView( view )
            .setPositiveButton( android.R.string.ok, null );

        if ( context instanceof XWActivity ) {
            final XWActivity activity = (XWActivity)context;
            builder.setNegativeButton( R.string.changes_button, new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int which ) {
                        activity.show( FirstRunDialog.newInstance() );
                    }
                } );
        }

        return builder.create();
    }

    @Override
    protected String getFragTag() { return TAG; }
}
