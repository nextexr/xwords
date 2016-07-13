/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2014 by Eric House (xwords@eehouse.org).  All
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
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager.BackStackEntry;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.LinearLayout;

import org.eehouse.android.xw4.jni.CurGameInfo;

import junit.framework.Assert;

public class MainActivity extends XWActivity 
    implements FragmentManager.OnBackStackChangedListener {
    private DelegateBase m_dlgt;
    private boolean m_dpEnabled;

    // Used only if m_dpEnabled is true
    private LinearLayout m_root;
    private int m_maxPanes;
    private int m_nextID = 0x00FFFFFF;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        m_dpEnabled = XWPrefs.dualpaneEnabled( this );
        if ( m_dpEnabled ) {
            m_dlgt = new DualpaneDelegate( this, savedInstanceState );
            Utils.showToast( this, "dualpane mode" );
        } else {
            m_dlgt = new GamesListDelegate( this, savedInstanceState );
        }
        super.onCreate( savedInstanceState, m_dlgt );

        if ( m_dpEnabled ) {
            m_root = (LinearLayout)findViewById( R.id.main_container );
            getSupportFragmentManager().addOnBackStackChangedListener( this );

            m_maxPanes = maxPanes();

            // Nothing to do if we're restarting
            if ( savedInstanceState == null ) {
                // In case this activity was started with special instructions from an Intent,
                // pass the Intent's extras to the fragment as arguments
                addFragmentImpl( new GamesListFrag(), getIntent().getExtras(), null );
            }
        }

        // Trying to debug situation where two of this activity are running at
        // once. finish()ing when Intent.FLAG_ACTIVITY_BROUGHT_TO_FRONT is
        // passed is not the fix, but perhaps there's another
        // int flags = getIntent().getFlags();
        // DbgUtils.logf( "MainActivity.onCreate(this=%H): flags=0x%x", 
        //                this, flags );
    } // onCreate

    // called when we're brought to the front (probably as a result of
    // notification)
    @Override
    protected void onNewIntent( Intent intent )
    {
        super.onNewIntent( intent );

        // HACK!!!!! FIXME
        if ( m_dlgt instanceof GamesListDelegate ) {
            ((GamesListDelegate)m_dlgt).onNewIntent( intent );
        }
    }

    //////////////////////////////////////////////////////////////////////
    // Delegator interface
    //////////////////////////////////////////////////////////////////////
    @Override
    public boolean inDPMode() {
        return m_dpEnabled;
    }

    @Override
    public void addFragment( XWFragment fragment, Bundle extras ) 
    {
        addFragmentImpl( fragment, extras, this );
    }

    @Override
    public void addFragmentForResult( XWFragment fragment, Bundle extras, 
                                      RequestCode requestCode )
    {
        DbgUtils.logf( "addFragmentForResult(): dropping requestCode" );
        addFragmentImpl( fragment, extras, this );
    }

    //////////////////////////////////////////////////////////////////////
    // FragmentManager.OnBackStackChangedListener
    //////////////////////////////////////////////////////////////////////
    public void onBackStackChanged()
    {
        DbgUtils.logf( "MainActivity.onBackStackChanged()" );
        // make sure the right-most are visible
        int fragCount = getSupportFragmentManager().getBackStackEntryCount();
        if ( 0 == fragCount ) {
            finish();
        } else if ( fragCount == m_root.getChildCount() - 1 ) {
            m_root.removeViewAt( fragCount );
            setVisiblePanes();
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // Dualpane mode stuff
    ////////////////////////////////////////////////////////////////////////

    private int maxPanes()
    {
        int result;
        int orientation = getResources().getConfiguration().orientation;
        if ( XWPrefs.getIsTablet( this ) 
             && Configuration.ORIENTATION_LANDSCAPE == orientation ) {
            result = 2;
        } else {
            result = 1;
        }
        return result;
    }

    private void setVisiblePanes()
    {
        // hide all but the right-most m_maxPanes children
        int nPanes = m_root.getChildCount();
        for ( int ii = 0; ii < nPanes; ++ii ) {
            View child = m_root.getChildAt( ii );
            boolean visible = ii >= nPanes - m_maxPanes;
            DbgUtils.logf( "pane %d: visible=%b", ii, visible );
            child.setVisibility( visible ? View.VISIBLE : View.GONE );
            setMenuVisibility( child, visible );
        }
    }

    private void setMenuVisibility( View cont, boolean visible )
    {
        FrameLayout layout = (FrameLayout)cont;
        FragmentManager fm = getSupportFragmentManager();
        int hidingId = layout.getId();
        Fragment frag = fm.findFragmentById( hidingId );
        if ( null != frag ) {   // hasn't been popped?
            frag.setMenuVisibility( visible );
        }
    }

    private void addFragmentImpl( Fragment fragment, Bundle bundle, 
                                  Delegator parent ) 
    {
        fragment.setArguments( bundle );
        addFragmentImpl( fragment, parent );
    }

    private void addFragmentImpl( Fragment fragment, Delegator delegator )
    {
        String newName = fragment.getClass().getName();
        boolean replace = false;
        FragmentManager fm = getSupportFragmentManager();
        int fragCount = fm.getBackStackEntryCount();
        int containerCount = m_root.getChildCount();
        DbgUtils.logf( "fragCount: %d; containerCount: %d", fragCount, containerCount );
        // Assert.assertTrue( fragCount == containerCount );

        // Replace IF we're adding something of the same class at right OR if
        // we're adding something with the existing left pane as its parent
        // (delegator)
        if ( 0 < fragCount ) {
            FragmentManager.BackStackEntry entry = fm.getBackStackEntryAt( fragCount - 1 );
            String curName = entry.getName();
            DbgUtils.logf( "name of last entry: %s", curName );
            replace = curName.equals( newName );

            if ( !replace && 1 < fragCount ) {
                entry = fm.getBackStackEntryAt( fragCount - 2 );
                curName = entry.getName();
                String delName = delegator.getClass().getName();
                DbgUtils.logf( "comparing %s, %s", curName, delName );
                replace = curName.equals( delName );
            }

            if ( replace ) {
                fm.popBackStack();
            }
        }

        // Replace doesn't seem to work with generated IDs, so we'll create a
        // new FrameLayout each time.  If we're replacing, we'll replace the
        // current rightmost FrameLayout.  Otherwise we'll add a new one.
        FrameLayout cont = new FrameLayout( this );
        cont.setLayoutParams( new LayoutParams(0, LayoutParams.MATCH_PARENT, 1.0f) );
        int id = --m_nextID;
        cont.setId( id );
        m_root.addView( cont, replace ? containerCount - 1 : containerCount );

        if ( !replace && containerCount >= m_maxPanes ) {
            int indx = containerCount - m_maxPanes;
            View child = m_root.getChildAt( indx );
            child.setVisibility( View.GONE );

            setMenuVisibility( child, false );

            DbgUtils.logf( "hiding %dth container", indx );
        }

        fm.beginTransaction()
            .add( id, fragment )
            .addToBackStack( newName )
            .commit();
        fm.executePendingTransactions();
    }
}
