/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 - 2012 by Eric House (xwords@eehouse.org).  All
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

import android.app.Dialog;
import android.app.ExpandableListActivity;
import android.content.DialogInterface;
import android.os.Bundle;

import junit.framework.Assert;


public class XWExpandableListActivity extends ExpandableListActivity 
    implements DlgDelegate.DlgClickNotify, DlgDelegate.HasDlgDelegate,
               MultiService.MultiEventListener {

    private DlgDelegate m_delegate;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        DbgUtils.logf( "%s.onCreate(this=%H)", getClass().getName(), this );
        super.onCreate( savedInstanceState );
        m_delegate = new DlgDelegate( this, this, savedInstanceState );
    }

    @Override
    protected void onResume()
    {
        DbgUtils.logf( "%s.onResume(this=%H)", getClass().getName(), this );
        XWService.setListener( this );
        super.onResume();
    }

    @Override
    protected void onPause()
    {
        DbgUtils.logf( "%s.onPause(this=%H)", getClass().getName(), this );
        XWService.setListener( null );
        super.onPause();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        m_delegate.onSaveInstanceState( outState );
    }

    @Override
    protected Dialog onCreateDialog( final int id )
    {
        Dialog dialog = m_delegate.onCreateDialog( id );
        if ( null == dialog ) {
            dialog = super.onCreateDialog( id );
        }
        return dialog;
    }

    protected boolean post( Runnable runnable )
    {
        return m_delegate.post( runnable );
    }

    protected void doSyncMenuitem()
    {
        m_delegate.doSyncMenuitem();
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey,
                                        int action, Object... params )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey, action, params );
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey,
                                        int action )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey, action );
    }

    protected void showNotAgainDlg( int msgID, int prefsKey )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey );
    }

    // It sucks that these must be duplicated here and XWActivity
    protected void showAboutDialog()
    {
        m_delegate.showAboutDialog();
    }

    public void showOKOnlyDialog( int msgID )
    {
        m_delegate.showOKOnlyDialog( msgID );
    }

    public void showOKOnlyDialog( String msg )
    {
        m_delegate.showOKOnlyDialog( msg );
    }

    protected void showConfirmThen( String msg, int action, Object... params )
    {
        m_delegate.showConfirmThen( msg, action, params );
    }

    protected void showConfirmThen( String msg, int posButton, int action,
                                    Object... params )
    {
        m_delegate.showConfirmThen( msg, posButton, action, params );
    }

    protected void showConfirmThen( int msg, int posButton, int action, 
                                    Object... params )
    {
        m_delegate.showConfirmThen( getString(msg), posButton, action, params );
    }

    // DlgDelegate.DlgClickNotify interface
    public void dlgButtonClicked( int id, int which, Object[] params )
    {
        Assert.fail();
    }

    // BTService.BTEventListener interface
    public void eventOccurred( MultiService.MultiEvent event, 
                               final Object ... args )
    {
        m_delegate.eventOccurred( event, args );
    }

}
