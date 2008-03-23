/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2004-2008 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <windowsx.h>
#include "stdafx.h" 
#include <commdlg.h>

#include "cemain.h" 
#include "cesvdgms.h" 
#include "ceutil.h" 
#include "cedebug.h" 

typedef struct SaveGameNameState {
    CEAppGlobals* globals;
/*     HWND hDlg; */
    wchar_t* buf; 
    XP_U16 buflen;
    XP_Bool cancelled;
    XP_Bool inited;
} SaveGameNameState;

static void
notImpl( CEAppGlobals* globals )
{
    messageBoxChar( globals, "To be implemented soon....",
                    L"Notice", MB_OK );
}

static XP_Bool
ceFileExists( const wchar_t* name )
{
    wchar_t buf[128];
    DWORD attributes;
    swprintf( buf, DEFAULT_DIR_NAME L"\\%s.xwg", name );

    attributes = GetFileAttributes( buf );
    return attributes != 0xFFFFFFFF;
}

static void
makeUniqueName( wchar_t* buf, XP_U16 bufLen )
{
    XP_U16 i;
    for ( i = 1; i < 100; ++i ) {
        swprintf( buf, L"Untitled%d", i );
        if ( !ceFileExists( buf ) ) {
            break;
        }
    }
    /* If we fall out of the loop, the user will be asked to confirm delete
       of Untitled99 or somesuch.  That's ok.... */
} /* makeUniqueName */

static LRESULT CALLBACK
SaveNameDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    SaveGameNameState* state;
    XP_U16 wid;
    BOOL result = FALSE;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );

        state = (SaveGameNameState*)lParam;
        state->cancelled = XP_TRUE;
        state->inited = XP_FALSE;

        ceDlgSetup( state->globals, hDlg, XP_FALSE );

        result = TRUE;
    } else {
        state = (SaveGameNameState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!state ) {
            if ( !state->inited ) {
                (void)SetDlgItemText( hDlg, IDC_SVGN_EDIT, state->buf );
                state->inited = XP_TRUE;
            }

            switch (message) {
            case WM_COMMAND:
                wid = LOWORD(wParam);
                switch( wid ) {
                case IDOK: {
                    wchar_t buf[128];
                    (void)GetDlgItemText( hDlg, IDC_SVGN_EDIT, buf, 
                                          VSIZE(buf) );
                    if ( ceFileExists( buf ) ) {
                        messageBoxChar( state->globals, "File exists", 
                                        L"Oops!", MB_OK );
                        break;
                    }
                    swprintf( state->buf, DEFAULT_DIR_NAME L"\\%s.xwg", buf );
                    XP_LOGW( __func__, state->buf );
                    /* fallthru */
                    state->cancelled = XP_FALSE;
                }
                case IDCANCEL:
                    EndDialog(hDlg, wid);
                    result = TRUE;
                    break;
                }
            }
        }
    }
    return result;
}

XP_Bool
ceConfirmUniqueName( CEAppGlobals* globals, wchar_t* buf, XP_U16 buflen )
{
    SaveGameNameState state;

    LOG_FUNC();

    makeUniqueName( buf, buflen );

    XP_MEMSET( &state, 0, sizeof(state) );
    state.globals = globals;
    state.buf = buf;
    state.buflen = buflen;
    (void)DialogBoxParam( globals->hInst, (LPCTSTR)IDD_SAVENAMEDLG, 
                          globals->hWnd, 
                          (DLGPROC)SaveNameDlg, (long)&state );
    XP_LOGW( __func__, buf );
    return !state.cancelled;
} /* ceConfirmUniqueName */

typedef struct SavedGamesState {
    CEAppGlobals* globals;
    HWND hDlg;
    wchar_t* buf; 
    XP_U16 buflen;
    XP_S16 sel;
    wchar_t curName[128];

/*     wchar_t** names; */
/*     XP_U16 nNamesUsed; */
/*     XP_U16 nNamesAllocd; */

    XP_Bool cancelled;
    XP_Bool inited;
} SavedGamesState;

/* Probably belongs as a utility */
static void
getCBText( HWND hDlg, XP_U16 id, XP_U16 sel, wchar_t* buf, XP_U16* lenp )
{
    XP_U16 len = SendDlgItemMessage( hDlg, id, GETLBTEXTLEN, sel, 0L );
    if ( len < *lenp ) {
        (void)SendDlgItemMessage( hDlg, id, GETLBTEXT, sel, (LPARAM)buf );
    } else {
        XP_ASSERT( 0 );
    }
    *lenp = len;
} /* getCBText */

static void
setEditFromSel( SavedGamesState* state )
{
    wchar_t buf[64];
    XP_U16 len = VSIZE(buf);
    getCBText( state->hDlg, IDC_SVGM_GAMELIST, state->sel, buf, &len );
    if ( len <= VSIZE(buf) ) {
        (void)SetDlgItemText( state->hDlg, IDC_SVGM_EDIT, buf );
    }
} /*  */

static void
initSavedGamesData( SavedGamesState* state )
{
    HANDLE fileH;
    WIN32_FIND_DATA data;
    wchar_t path[256];
    XP_U16 curSel = 0, ii;

    XP_MEMSET( &data, 0, sizeof(data) );
    lstrcpy( path, DEFAULT_DIR_NAME L"\\" );
    lstrcat( path, L"*.xwg" );

    fileH = FindFirstFile( path, &data );
    for ( ii = 0; fileH != INVALID_HANDLE_VALUE; ++ii ) {
        XP_U16 len = wcslen( data.cFileName );

        XP_LOGW( "comp1", state->curName );
        XP_LOGW( "comp2", data.cFileName );
        if ( curSel == 0 && 0 == wcscmp( state->curName, data.cFileName ) ) {
            curSel = ii;
        }

        XP_ASSERT( data.cFileName[len-4] == '.');
        data.cFileName[len-4] = 0;
        SendDlgItemMessage( state->hDlg, IDC_SVGM_GAMELIST, ADDSTRING,
                            0, (LPARAM)data.cFileName );

        if ( !FindNextFile( fileH, &data ) ) {
            XP_ASSERT( GetLastError() == ERROR_NO_MORE_FILES );
            break;
        }
    }

    SendDlgItemMessage( state->hDlg, IDC_SVGM_GAMELIST, SETCURSEL, curSel, 0 );
    state->sel = curSel;
    setEditFromSel( state );

    LOG_RETURN_VOID();
} /* initSavedGamesData */

static LRESULT CALLBACK
SavedGamesDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    SavedGamesState* state;
    XP_U16 wid;
    BOOL result = FALSE;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );

        state = (SavedGamesState*)lParam;
        state->hDlg = hDlg;
        state->cancelled = XP_TRUE;
        state->inited = XP_FALSE;

        ceDlgSetup( state->globals, hDlg, XP_TRUE );

        result = TRUE;
    } else {
        state = (SavedGamesState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!state ) {

            if ( !state->inited ) {
                state->inited = XP_TRUE;
                initSavedGamesData( state );
            }

            switch (message) {

            case WM_VSCROLL:
                if ( !IS_SMARTPHONE(state->globals) ) {
                    ceDoDlgScroll( state->globals, hDlg, wParam );
                }
                break;

            case WM_COMMAND:
                if ( !IS_SMARTPHONE(state->globals) ) {
                    ceDoDlgFocusScroll( state->globals, hDlg );
                }
                wid = LOWORD(wParam);
                switch( wid ) {

                case IDC_SVGM_GAMELIST:
                    if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                        XP_S16 sel = SendDlgItemMessage( state->hDlg, 
                                                         IDC_SVGM_GAMELIST, 
                                                         CB_GETCURSEL, 0, 0L);
                        if ( sel >= 0 ) {
                            state->sel = sel;
                            setEditFromSel( state );
                        }
                    }
                    break;

                case IDC_SVGM_DUP:
                case IDC_SVGM_DEL:
                case IDC_SVGM_CHANGE:
                    notImpl( state->globals );
                    break;

                case IDC_SVGM_OPEN: {
                    wchar_t buf[128];
                    XP_U16 len = sizeof(buf);
                    getCBText( state->hDlg, IDC_SVGM_GAMELIST, state->sel, 
                               buf, &len );
                    swprintf( state->buf, DEFAULT_DIR_NAME L"\\%s.xwg", buf );
                    XP_LOGW( "returning", state->buf );
                }
                    /* fallthrough */
                case IDOK:
                    state->cancelled = XP_FALSE;
                    /* fallthrough */

                case IDCANCEL:
                    EndDialog(hDlg, wid);
                    result = TRUE;
                    break;
                }
            }
        }
    }

    return result;
} /* SavedGamesDlg */

XP_Bool
ceSavedGamesDlg( CEAppGlobals* globals, const XP_UCHAR* curPath,
                 wchar_t* buf, XP_U16 buflen )
{
    SavedGamesState state;

    LOG_FUNC();

    XP_MEMSET( &state, 0, sizeof(state) ); /* sets cancelled */
    state.globals = globals;
    state.buf = buf;
    state.buflen = buflen;

    if ( !!curPath ) {
        wchar_t shortName[128];
        XP_U16 len;
        XP_LOGF( curPath );

        len = (XP_U16)XP_STRLEN( curPath );
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, curPath, len + 1, 
                             shortName, len + 1 );
        len = wcslen( DEFAULT_DIR_NAME L"\\" );
        lstrcpy( state.curName, shortName+len );
        XP_LOGW( "shortName", state.curName );
    }

    (void)DialogBoxParam( globals->hInst, (LPCTSTR)IDD_SAVEDGAMESDLG, 
                          globals->hWnd, 
                          (DLGPROC)SavedGamesDlg, (long)&state );

    XP_LOGW( __func__, buf );

    return !state.cancelled;
} /*ceSavedGamesDlg  */
