/* -*- compile-command: "find-and-gradle.sh installXw4Debug"; -*- */
/*
 * Copyright 2009 - 2016 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CheckBox;
import android.widget.ScrollView;
import android.widget.TextView;

public class NotAgainView extends ScrollView {

    public NotAgainView( Context cx, AttributeSet as ) {
        super( cx, as );
    }

    public void setMessage( String msg )
    {
        ((TextView)findViewById( R.id.msg )).setText( msg );
    }

    public boolean getChecked()
    {
        CheckBox cbx = (CheckBox)findViewById( R.id.not_again_check );
        return cbx.isChecked();
    }

    public void setShowNACheckbox( boolean show )
    {
        findViewById( R.id.not_again_check )
            .setVisibility( show ? View.VISIBLE : View.GONE );
    }
}
