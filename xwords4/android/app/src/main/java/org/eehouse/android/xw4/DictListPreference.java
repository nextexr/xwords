/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All
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

import java.util.ArrayList;

public class DictListPreference extends XWListPreference {

    public DictListPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );

        setEntriesForLang();
    }

    protected void invalidate()
    {
        String[] values = setEntriesForLang();
        setValue( values[0] );
    }

    private String[] setEntriesForLang()
    {
        Context context = getContext();
        String curLang = XWPrefs.getPrefsString( context, R.string.key_default_language );
        int langCode = DictLangCache.getLangLangCode( context, curLang );

        DictUtils.DictAndLoc[] dals = DictUtils.dictList( context  );
        ArrayList<String> dictEntries = new ArrayList<String>();
        ArrayList<String> values = new ArrayList<String>();
        for ( int ii = 0; ii < dals.length; ++ii ) {
            String name = dals[ii].name;
            if ( langCode == DictLangCache.getDictLangCode( context, name ) ) {
                values.add( name );
                dictEntries.add( DictLangCache.annotatedDictName( context,
                                                                  dals[ii] ) );
            }
        }
        setEntries( dictEntries.toArray( new String[dictEntries.size()] ) );
        String[] result = values.toArray( new String[values.size()] );
        setEntryValues( result );
        return result;
    }
}
