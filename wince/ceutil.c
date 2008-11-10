/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2002-2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "stdafx.h" 
#include <commctrl.h>
#include <shlobj.h>

#include "ceutil.h"
#include "cedefines.h"
#include "cedebug.h"
#include "debhacks.h"

#define BUF_SIZE 128
#define VPADDING 4
#define HPADDING_L 2
#define HPADDING_R 3

static XP_Bool ceDoDlgScroll( CeDlgHdr* dlgHdr, WPARAM wParam );
static void ceDoDlgFocusScroll( CeDlgHdr* dlgHdr, HWND nextCtrl );

void
ceSetDlgItemText( HWND hDlg, XP_U16 id, const XP_UCHAR* buf )
{
    wchar_t widebuf[BUF_SIZE];
    XP_U16 len;

    XP_ASSERT( buf != NULL );

    len = (XP_U16)XP_STRLEN( buf );

    if ( len >= BUF_SIZE ) {
        len = BUF_SIZE - 1;
    }

    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, buf, len, widebuf, len );
    widebuf[len] = 0;
    SendDlgItemMessage( hDlg, id, WM_SETTEXT, 0, (long)widebuf );
} /* ceSetDlgItemText */

void
ceSetDlgItemFileName( HWND hDlg, XP_U16 id, XP_UCHAR* str )
{
    XP_UCHAR* stripstart;
    XP_UCHAR buf[BUF_SIZE];
    XP_U16 len = XP_STRLEN(str);

    if ( len >= BUF_SIZE ) {
        len = BUF_SIZE - 1;
    }

    XP_MEMCPY( buf, str, len );
    buf[len] = '\0';

    stripstart = strrchr( (const char*)buf, '.' );
    if ( !!stripstart ) {
        *stripstart = '\0';
    }

    ceSetDlgItemText( hDlg, id, buf );
} /* ceSetDlgItemFileName */

void
ceGetDlgItemText( HWND hDlg, XP_U16 id, XP_UCHAR* buf, XP_U16* bLen )
{
    XP_U16 len = *bLen;
    XP_U16 gotLen;
    wchar_t wbuf[BUF_SIZE];

    XP_ASSERT( len <= BUF_SIZE );

    gotLen = (XP_U16)SendDlgItemMessage( hDlg, id, WM_GETTEXT, len, 
                                         (long)wbuf );
    if ( gotLen > 0 ) {
        XP_ASSERT( gotLen < len );
        if ( gotLen >= len ) {
            gotLen = len - 1;
        }
        gotLen = WideCharToMultiByte( CP_ACP, 0, wbuf, gotLen,
                                      buf, len, NULL, NULL );
        *bLen = gotLen;
        buf[gotLen] = '\0';
    } else {
        buf[0] = '\0';
        *bLen = 0;
    }
} /* ceGetDlgItemText */

void
ceSetDlgItemNum( HWND hDlg, XP_U16 id, XP_S32 num )
{
    XP_UCHAR buf[20];
    XP_SNPRINTF( buf, sizeof(buf), "%ld", num );
    ceSetDlgItemText( hDlg, id, buf );
} /* ceSetDlgItemNum */

XP_S32
ceGetDlgItemNum( HWND hDlg, XP_U16 id )
{
    XP_S32 result = 0;
    XP_UCHAR buf[24];
    XP_U16 len = sizeof(buf);
    ceGetDlgItemText( hDlg, id, buf, &len );

    result = atoi( buf );
    return result;
} /* ceGetDlgItemNum */

void
ceShowOrHide( HWND hDlg, XP_U16 resID, XP_Bool visible )
{
    HWND itemH = GetDlgItem( hDlg, resID );
    if ( !!itemH ) {
        ShowWindow( itemH, visible? SW_SHOW: SW_HIDE );
    }
} /* ceShowOrHide */

void
ceEnOrDisable( HWND hDlg, XP_U16 resID, XP_Bool enable )
{
    HWND itemH = GetDlgItem( hDlg, resID );
    if ( !!itemH ) {
        EnableWindow( itemH, enable );
    }
} /* ceShowOrHide */

void
ceSetChecked( HWND hDlg, XP_U16 resID, XP_Bool check )
{
    SendDlgItemMessage( hDlg, resID, BM_SETCHECK, 
                        check? BST_CHECKED:BST_UNCHECKED, 0L );
} /* ceSetBoolCheck */

XP_Bool
ceGetChecked( HWND hDlg, XP_U16 resID )
{
    XP_U16 checked;
    checked = (XP_U16)SendDlgItemMessage( hDlg, resID, BM_GETCHECK, 0, 0L );
    return checked == BST_CHECKED;
} /* ceGetChecked */

/* Return dlg-relative rect. 
 */
static void
GetItemRect( HWND hDlg, XP_U16 resID, RECT* rect )
{
    RECT dlgRect;
    HWND itemH = GetDlgItem( hDlg, resID );
    XP_U16 clientHt, winHt;

    GetClientRect( hDlg, &dlgRect );
    clientHt = dlgRect.bottom;
    GetWindowRect( hDlg, &dlgRect );
    winHt = dlgRect.bottom - dlgRect.top;
    GetWindowRect( itemH, rect );

    /* GetWindowRect includes the title bar, but functions like MoveWindow
       set relative to the client area below it.  So subtract out the
       difference between window ht and client rect ht -- the title bar --
       when returning the item's rect. */
    (void)OffsetRect( rect, -dlgRect.left, 
                      -(dlgRect.top + winHt - clientHt) );
} /* GetItemRect */

void
ceCenterCtl( HWND hDlg, XP_U16 resID )
{
    RECT buttonR, dlgR;
    HWND itemH = GetDlgItem( hDlg, resID );
    XP_U16 newX, buttonWidth;
    
    GetClientRect( hDlg, &dlgR );
    XP_ASSERT( dlgR.left == 0 && dlgR.top == 0 );

    GetItemRect( hDlg, resID, &buttonR );

    buttonWidth = buttonR.right - buttonR.left;
    newX = ( dlgR.right - buttonWidth ) / 2;

    if ( !MoveWindow( itemH, newX, buttonR.top,
                      buttonWidth, 
                      buttonR.bottom - buttonR.top, TRUE ) ) {
        XP_LOGF( "MoveWindow=>%ld", GetLastError() );
    }
} /* ceCenterCtl */

/* XP_Bool */
/* ceIsLandscape( CEAppGlobals* globals ) */
/* { */
/*     XP_U16 width, height; */
/*     XP_Bool landscape; */

/*     XP_ASSERT( !!globals ); */
/*     XP_ASSERT( !!globals->hWnd ); */

/*     if ( 0 ) { */
/* #if defined DEBUG && !defined _WIN32_WCE */
/*     } else if ( globals->dbWidth != 0 ) { */
/*         width = globals->dbWidth; */
/*         height = globals->dbHeight; */
/* #endif */
/*     } else { */
/*         RECT rect; */
/*         GetClientRect( globals->hWnd, &rect ); */
/*         width = (XP_U16)(rect.right - rect.left); */
/*         height = (XP_U16)(rect.bottom - rect.top); */
/*     } */

/*     landscape = (height - CE_SCORE_HEIGHT)  */
/*         < (width - CE_MIN_SCORE_WIDTH); */
/*     return landscape; */
/* } /\* ceIsLandscape *\/ */

#ifdef _WIN32_WCE
static XP_Bool
ceIsFullScreen( CEAppGlobals* globals, HWND hWnd )
{
    XP_S16 screenHt;
    XP_U16 winHt;
    RECT rect;

    GetClientRect( hWnd, &rect );
    winHt = rect.bottom - rect.top; /* top should always be 0 */

    screenHt = GetSystemMetrics( SM_CYSCREEN );
    XP_ASSERT( screenHt >= winHt );

    screenHt -= winHt;
    
    if ( !!globals->hwndCB ) {
        RECT rect;
        GetWindowRect( globals->hwndCB, &rect );
        screenHt -= rect.bottom - rect.top;
    }

    XP_ASSERT( screenHt >= 0 );
    return screenHt == 0;
} /* ceIsFullScreen */

static void
ceSize( CEAppGlobals* globals, HWND hWnd, XP_Bool fullScreen )
{
    RECT rect;
    XP_U16 cbHeight = 0;
    if ( !!globals->hwndCB ) {
        GetWindowRect( globals->hwndCB, &rect );
        cbHeight = rect.bottom - rect.top;
    }

    /* I'm leaving the SIP/cmdbar in place until I can figure out how to
       get menu events with it hidden -- and also the UI for making sure
       users don't get stuck in fullscreen mode not knowing how to reach
       menus to get out.  Later, add SHFS_SHOWSIPBUTTON and
       SHFS_HIDESIPBUTTON to the sets shown and hidden below.*/
    if ( fullScreen ) {
        SHFullScreen( hWnd, SHFS_HIDETASKBAR | SHFS_HIDESTARTICON );

        SetRect( &rect, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                 GetSystemMetrics(SM_CYSCREEN) );

    } else {
        SHFullScreen( hWnd, SHFS_SHOWTASKBAR | SHFS_SHOWSTARTICON );
        SystemParametersInfo( SPI_GETWORKAREA, 0, &rect, FALSE );
        if ( IS_SMARTPHONE(globals) ) {
            cbHeight = 0;
        }
    }

    rect.bottom -= cbHeight;
    MoveWindow( hWnd, rect.left, rect.top, rect.right - rect.left, 
                rect.bottom - rect.top, TRUE );
} /* ceSize */

void
ceSizeIfFullscreen( CEAppGlobals* globals, HWND hWnd )
{
    if ( globals->appPrefs.fullScreen != ceIsFullScreen(globals, hWnd) ) {
        ceSize( globals, hWnd, globals->appPrefs.fullScreen );
    }
}

static XP_Bool
mkFullscreenWithSoftkeys( CEAppGlobals* globals, HWND hDlg, XP_U16 curHt,
                          XP_Bool doneOnly )
{
    XP_Bool success = XP_FALSE;

    SHMENUBARINFO mbi;
    XP_MEMSET( &mbi, 0, sizeof(mbi) );
    mbi.cbSize = sizeof(mbi);
    mbi.hwndParent = hDlg;
    mbi.nToolBarId = doneOnly? IDM_DONE_MENUBAR:IDM_OKCANCEL_MENUBAR;
    mbi.hInstRes = globals->hInst;
    success = SHCreateMenuBar( &mbi );
    if ( !success ) {
        XP_LOGF( "SHCreateMenuBar failed: %ld", GetLastError() );
    } else {

        if ( IS_SMARTPHONE(globals) ) {
            SHINITDLGINFO info;
            XP_MEMSET( &info, 0, sizeof(info) );
            info.dwMask = SHIDIM_FLAGS;
            info.dwFlags = SHIDIF_SIZEDLGFULLSCREEN;
            info.hDlg = hDlg;
            success = SHInitDialog( &info );
            if ( !success ) {
                XP_LOGF( "SHInitDialog failed: %ld", GetLastError() );
            }
        } else {
            XP_U16 screenHt = GetSystemMetrics(SM_CYFULLSCREEN);
            RECT rect;
            GetWindowRect( mbi.hwndMB, &rect );
            screenHt -= (rect.bottom - rect.top);
            if ( screenHt < curHt ) {
                ceSize( globals, hDlg, XP_TRUE );
            }
        }
    }

    return success;
} /* mkFullscreenWithSoftkeys */
#endif

#define TITLE_HT 20            /* Need to get this from the OS */
void
ceDlgSetup( CeDlgHdr* dlgHdr, HWND hDlg, DlgStateTask doWhat )
{
    RECT rect;
    XP_U16 fullHeight;
    CEAppGlobals* globals = dlgHdr->globals;

    dlgHdr->hDlg = hDlg;

    XP_ASSERT( !!globals );
    XP_ASSERT( !!hDlg );

    GetClientRect( hDlg, &rect );
    XP_ASSERT( rect.top == 0 );
    fullHeight = rect.bottom;         /* This is before we've resized it */

#ifdef _WIN32_WCE
    (void)mkFullscreenWithSoftkeys( globals, hDlg, fullHeight,
                                    (doWhat & DLG_STATE_DONEONLY) != 0);
#elif defined DEBUG
    /* Force it to be small so we can test scrolling etc. */
    if ( globals->dbWidth > 0 && globals->dbHeight > 0) {
        MoveWindow( hDlg, 0, 0, globals->dbWidth, globals->dbHeight, TRUE );
        rect.bottom = globals->dbHeight;
    }
#endif

    /* Measure again post-resize */
    GetClientRect( hDlg, &rect );

    /* Set up the scrollbar if we're on PPC */
    if ( !IS_SMARTPHONE(globals) ) {
        SCROLLINFO sinfo;
     
        XP_MEMSET( &sinfo, 0, sizeof(sinfo) );
        sinfo.cbSize = sizeof(sinfo);

        sinfo.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
        if ( rect.bottom < fullHeight ) {
            sinfo.nMax = fullHeight;
            dlgHdr->nPage = sinfo.nPage = rect.bottom - 1;
        }
        
        (void)SetScrollInfo( hDlg, SB_VERT, &sinfo, FALSE );
    }
    dlgHdr->doWhat = doWhat;

#ifdef _WIN32_WCE
    /* Need to trap this for all dialogs, even if they don't have edit
       controls.  The need goes away if the main window stops trapping it,
       but I don't understand why: trapping here is still required. */
    if ( IS_SMARTPHONE(globals) ) {
        trapBackspaceKey( hDlg );
    }
#endif
} /* ceDlgSetup */

void
ceDlgComboShowHide( CeDlgHdr* dlgHdr, XP_U16 baseId )
{
    HWND hDlg = dlgHdr->hDlg;

    if ( IS_SMARTPHONE(dlgHdr->globals) ) {
        ceShowOrHide( hDlg, baseId+2, XP_FALSE );
    } else {
        ceShowOrHide( hDlg, baseId, XP_FALSE );
        ceShowOrHide( hDlg, baseId+1, XP_FALSE );
    } 
}

#ifdef OVERRIDE_BACKKEY
static XP_Bool
editHasFocus( void )
{
    HWND focus = GetFocus();
    wchar_t buf[32];
    XP_Bool isEdit = !!focus
        && ( 0 != GetClassName( focus, buf, VSIZE(buf) ) )
        && !wcscmp( L"Edit", buf );
    return isEdit;
} /* editHasFocus */
#endif

XP_Bool
ceDoDlgHandle( CeDlgHdr* dlgHdr, UINT message, WPARAM wParam, LPARAM lParam )
{
    XP_Bool handled = XP_FALSE;

    switch( message ) {
#ifdef OVERRIDE_BACKKEY
    case WM_HOTKEY:
        if ( VK_TBACK == HIWORD(lParam) ) {
            if ( editHasFocus() ) {
                SHSendBackToFocusWindow( message, wParam, lParam );
            } else if ( 0 != (BACK_KEY_UP_MAYBE & LOWORD(lParam) ) ) {
                WPARAM cmd = (0 != (dlgHdr->doWhat & DLG_STATE_DONEONLY)) ?
                    IDOK : IDCANCEL;
                SendMessage( dlgHdr->hDlg, WM_COMMAND, cmd, 0L );
            }
            handled = TRUE;
        }
        break;
#endif
    case WM_VSCROLL:
        handled = ceDoDlgScroll( dlgHdr, wParam );
        break;

    case WM_COMMAND:
        if ( BN_SETFOCUS == HIWORD(wParam) ) {
            ceDoDlgFocusScroll( dlgHdr, (HWND)lParam );
            handled = TRUE;
        } else if ( BN_KILLFOCUS == HIWORD(wParam) ) { /* dialogs shouldn't have to handle this */
            handled = TRUE;
        }
        break;
    }
    return handled;
}

static void
setScrollPos( HWND hDlg, XP_S16 newPos )
{
    SCROLLINFO sinfo;
    XP_S16 vertChange;

    XP_MEMSET( &sinfo, 0, sizeof(sinfo) );
    sinfo.cbSize = sizeof(sinfo);
    sinfo.fMask = SIF_POS;
    GetScrollInfo( hDlg, SB_VERT, &sinfo );

    if ( sinfo.nPos != newPos ) {
        XP_U16 oldPos = sinfo.nPos;
        sinfo.nPos = newPos;
        SetScrollInfo( hDlg, SB_VERT, &sinfo, XP_TRUE );

        GetScrollInfo( hDlg, SB_VERT, &sinfo );
        vertChange = oldPos - sinfo.nPos;
        if ( 0 != vertChange ) {
            RECT updateR;
            ScrollWindowEx( hDlg, 0, vertChange, NULL, NULL, NULL,
                            &updateR, SW_SCROLLCHILDREN|SW_ERASE);
            InvalidateRect( hDlg, &updateR, TRUE );
            (void)UpdateWindow( hDlg );
        } else {
            XP_LOGF( "%s: change dropped",  __func__ );
        }
    }
} /* setScrollPos */

static void
adjustScrollPos( HWND hDlg, XP_S16 vertChange )
{
    if ( vertChange != 0 ) {
        SCROLLINFO sinfo;

        XP_MEMSET( &sinfo, 0, sizeof(sinfo) );
        sinfo.cbSize = sizeof(sinfo);
        sinfo.fMask = SIF_POS;
        GetScrollInfo( hDlg, SB_VERT, &sinfo );

        setScrollPos( hDlg, sinfo.nPos + vertChange );
    }
    LOG_RETURN_VOID();
} /* adjustScrollPos */

static XP_Bool
ceDoDlgScroll( CeDlgHdr* dlgHdr, WPARAM wParam )
{
    XP_Bool handled = !IS_SMARTPHONE(dlgHdr->globals);
    if ( handled ) {
        XP_S16 vertChange = 0;
    
        switch ( LOWORD(wParam) ) {

        case SB_LINEUP: // Scrolls one line up 
            vertChange = -1;
            break;
        case SB_PAGEUP: // 
            vertChange = -dlgHdr->nPage;
            break;

        case SB_LINEDOWN: // Scrolls one line down 
            vertChange = 1;
            break;
        case SB_PAGEDOWN: // Scrolls one page down 
            vertChange = dlgHdr->nPage;
            break;

        case SB_THUMBTRACK:     /* still dragging; don't redraw */
        case SB_THUMBPOSITION:
            setScrollPos( dlgHdr->hDlg, HIWORD(wParam) );
            break;
        }

        if ( 0 != vertChange ) {
            adjustScrollPos( dlgHdr->hDlg, vertChange );
        }
    }
    return handled;
} /* ceDoDlgScroll */


/*     wParam */
/*         If lParam is TRUE, this parameter identifies the control that
           receives the focus. If lParam is FALSE, this parameter indicates
           whether the next or previous control with the WS_TABSTOP style
           receives the focus. If wParam is zero, the next control receives
           the focus; otherwise, the previous control with the WS_TABSTOP
           style receives the focus.  */
/*     lParam */
/*         The low-order word indicates how the system uses wParam. If the
           low-order word is TRUE, wParam is a handle associated with the
           control that receives the focus; otherwise, wParam is a flag that
           indicates whether the next or previous control with the WS_TABSTOP
           style receives the focus.  */

static void
ceDoDlgFocusScroll( CeDlgHdr* dlgHdr, HWND nextCtrl )
{
    /* Scroll the current focus owner into view.
     *
     * There's nothing passed in to tell us who it is, so look it up.
     *
     * What's in view?  First, a window has a scroll position, nPos, that
     * tells how many pixels are scrolled out of view above the window.  Then
     * a control has an offset within the containing rect (which shifts as
     * it's scrolled.)  Finally, all rects are relative to the screen, so we
     * need to get the containing rect to figure out what the control's
     * position is.  
     *
     * The first question, which can be answered without reference to
     * scrolling, is "Are we in view?"  If we're not, then we need to look at
     * scrolling to see how to fix it.
     */

    if ( !IS_SMARTPHONE(dlgHdr->globals) ) {
        HWND hDlg = dlgHdr->hDlg;

        if ( !!nextCtrl ) {
            RECT rect;
            XP_U16 dlgHeight, ctrlHeight, dlgTop;
            XP_S16 ctrlPos;

            GetClientRect( hDlg, &rect );
            dlgHeight = rect.bottom - rect.top;
            XP_LOGF( "dlgHeight: %d", dlgHeight );

            GetWindowRect( hDlg, &rect );
            dlgTop = rect.top;

            GetWindowRect( nextCtrl, &rect );
            ctrlPos = rect.top - dlgTop - TITLE_HT;
            ctrlHeight = rect.bottom - rect.top;

            if ( ctrlPos < 0 ) {
                XP_LOGF( "need to scroll it DOWN into view" );
                adjustScrollPos( hDlg, ctrlPos );
            } else if ( (ctrlPos + ctrlHeight) > dlgHeight ) {
                XP_LOGF( "need to scroll it UP into view" );
                setScrollPos( hDlg, ctrlPos - ctrlHeight );
            }
        }
    }
} /* ceDoDlgFocusScroll */

static XP_Bool
ceFindMenu( HMENU menu, XP_U16 id, 
#ifndef _WIN32_WCE
            HMENU* foundMenu, XP_U16* foundPos,
#endif
            wchar_t* foundBuf, XP_U16 bufLen )
{
    XP_Bool found = XP_FALSE;
    XP_U16 pos;
    MENUITEMINFO minfo;

    XP_MEMSET( &minfo, 0, sizeof(minfo) );
    minfo.cbSize = sizeof(minfo);

    for ( pos = 0; !found; ++pos ) {
        /* Set these each time through loop.  GetMenuItemInfo can change
           some of 'em. */
        minfo.fMask = MIIM_SUBMENU | MFT_STRING | MIIM_ID | MIIM_TYPE;
        minfo.dwTypeData = foundBuf;
        minfo.fType = MFT_STRING;
        minfo.cch = bufLen;

        if ( !GetMenuItemInfo( menu, pos, TRUE, &minfo ) ) {
            break;              /* pos is too big */
        } else if ( NULL != minfo.hSubMenu ) {
            found = ceFindMenu( minfo.hSubMenu, id, 
#ifndef _WIN32_WCE
                                foundMenu, foundPos,
#endif
                                foundBuf, bufLen );
        } else if ( MFT_SEPARATOR == minfo.fType ) {
            continue;
        } else if ( minfo.wID == id ) {
            found = XP_TRUE;
#ifndef _WIN32_WCE
            *foundPos = pos;
            *foundMenu = menu;
#endif
        }
    }
    return found;
} /* ceFindMenu */

#ifndef _WIN32_WCE
static void
setW32DummyMenu( CEAppGlobals* globals, HMENU menu, XP_U16 id, wchar_t* oldNm )
{
    XP_LOGW( __func__, oldNm );
    if ( globals->dummyMenu == NULL ) {
        HMENU tmenu;
        XP_U16 tpos;
        wchar_t ignore[32];
        if ( ceFindMenu( menu, W32_DUMMY_ID, &tmenu, &tpos, ignore, 
                         VSIZE(ignore) ) ) {
            globals->dummyMenu = tmenu;
            globals->dummyPos = tpos;
        }
    }

    if ( globals->dummyMenu != NULL ) {
        MENUITEMINFO minfo;
        XP_MEMSET( &minfo, 0, sizeof(minfo) );
        minfo.cbSize = sizeof(minfo);
        minfo.fMask = MFT_STRING | MIIM_TYPE | MIIM_ID;
        minfo.fType = MFT_STRING;
        minfo.dwTypeData = oldNm;
        minfo.cch = wcslen( oldNm );
        minfo.wID = id;

        if ( !SetMenuItemInfo( globals->dummyMenu, globals->dummyPos, 
                               TRUE, &minfo ) ) {
            XP_LOGF( "SetMenuItemInfo failed" );
        }
    }
}
#endif

static HMENU
ceGetMenu( const CEAppGlobals* globals )
{
#ifdef _WIN32_WCE
    TBBUTTONINFO info;
    XP_MEMSET( &info, 0, sizeof(info) );
    info.cbSize = sizeof(info);
    info.dwMask = TBIF_LPARAM;
    SendMessage( globals->hwndCB, TB_GETBUTTONINFO, IDM_MENU, 
                 (LPARAM)&info );
    return (HMENU)info.lParam;
#else
    return GetMenu( globals->hWnd );
#endif
}

void
ceSetLeftSoftkey( CEAppGlobals* globals, XP_U16 newId )
{
    if ( newId != globals->softKeyId ) {
        wchar_t menuTxt[32];    /* text of newId menu */
        HMENU menu = ceGetMenu( globals );
#ifdef _WIN32_WCE
        TBBUTTONINFO info;
#else
        HMENU prevMenu;
        XP_U16 prevPos;
#endif
        XP_U16 oldId = globals->softKeyId;
        if ( 0 == oldId ) {
            oldId = ID_INITIAL_SOFTID;
        }

        /* Look up the text... */
        if ( ceFindMenu( menu, newId, 
#ifndef _WIN32_WCE
                          &prevMenu, &prevPos,
#endif
                          menuTxt, VSIZE(menuTxt) ) ) {
            globals->softKeyId = newId;
#ifndef _WIN32_WCE
            globals->softKeyMenu = prevMenu;
#endif
        } else {
            XP_LOGF( "%s: ceFindMenu failed", __func__ );
        }

        /* Make it the button */
#ifdef _WIN32_WCE
        XP_MEMSET( &info, 0, sizeof(info) );
        info.cbSize = sizeof(info);
        info.dwMask = TBIF_TEXT | TBIF_COMMAND;
        info.idCommand = newId;
        info.pszText = menuTxt;
        SendMessage( globals->hwndCB, TB_SETBUTTONINFO, oldId, (LPARAM)&info );
#else
        setW32DummyMenu( globals, menu, newId, menuTxt );
#endif
    }
    ceCheckMenus( globals );  /* in case left key was or should be checked */
} /* ceSetLeftSoftkey */

static void
checkOneItem( const CEAppGlobals* globals, XP_U16 id, XP_Bool check )
{
    UINT uCheck = check ? MF_CHECKED : MF_UNCHECKED;
    HMENU menu = ceGetMenu( globals );

    (void)CheckMenuItem( menu, id, uCheck );
#ifndef _WIN32_WCE
    if ( id == globals->softKeyId ) {
        (void)CheckMenuItem( globals->softKeyMenu, id, uCheck );
    }
#endif
}

void
ceCheckMenus( const CEAppGlobals* globals )
{
    const BoardCtxt* board = globals->game.board;

    checkOneItem( globals, ID_MOVE_VALUES, board_get_showValues( board ));
    checkOneItem( globals, ID_MOVE_FLIP, board_get_flipped( board ) );
    checkOneItem( globals, ID_FILE_FULLSCREEN, globals->appPrefs.fullScreen );
    checkOneItem( globals, ID_MOVE_HIDETRAY,
                  TRAY_REVEALED != board_getTrayVisState( board ) );
} /* ceCheckMenus */

#ifdef OVERRIDE_BACKKEY
void
trapBackspaceKey( HWND hDlg )
{
    /* Override back key so we can pass it to edit controls */
    SendMessage( SHFindMenuBar(hDlg), SHCMBM_OVERRIDEKEY, VK_TBACK, 
                 MAKELPARAM (SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                             SHMBOF_NODEFAULT | SHMBOF_NOTIFY));
    /* To undo the above
    SendMessage( SHFindMenuBar(hDlg), SHCMBM_OVERRIDEKEY, VK_TBACK, 
                 MAKELPARAM( SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                             0 ) );
    */
}
#endif

/* Bugs in mingw32ce headers force defining _WIN32_IE, which causes
 * SHGetSpecialFolderPath to be defined as SHGetSpecialFolderPathW which
 * is not on Wince.  Once I turn off _WIN32_IE this can go away. */
#ifdef  _WIN32_IE
# ifdef SHGetSpecialFolderPath
#  undef SHGetSpecialFolderPath
# endif
BOOL SHGetSpecialFolderPath( HWND hwndOwner,
                             LPTSTR lpszPath,
                             int nFolder,
                             BOOL fCreate );
#endif

static void
lookupSpecialDir( wchar_t* bufW, XP_U16 indx )
{
    bufW[0] = 0;
#ifdef _WIN32_WCE
    SHGetSpecialFolderPath( NULL, bufW, 
                            (indx == MY_DOCS_CACHE)? 
                            CSIDL_PERSONAL : CSIDL_PROGRAM_FILES,
                            TRUE );
    if ( 0 == bufW[0] ) {
        XP_WARNF( "SHGetSpecialFolderPath failed" );
        wcscpy( bufW, L"\\My Documents" );
    }
#else
    wcscat( bufW, L"." );
#endif
    if ( indx == PROGFILES_CACHE ) {
        wcscat( bufW, L"\\" LCROSSWORDS_DIR_NODBG );
    } else {
        wcscat( bufW, L"\\" LCROSSWORDS_DIR L"\\" );
    }
}

XP_U16
ceGetPath( CEAppGlobals* globals, CePathType typ, 
           void* bufOut, XP_U16 bufLen )
{
    XP_U16 len;
    wchar_t bufW[CE_MAX_PATH_LEN];
    XP_U16 cacheIndx = typ == PROGFILES_PATH ? PROGFILES_CACHE : MY_DOCS_CACHE;
    wchar_t* specialDir = globals->specialDirs[cacheIndx];
    XP_Bool asAscii = XP_FALSE;

    if ( !specialDir ) {
        wchar_t buf[128];
        XP_U16 len;
        lookupSpecialDir( buf, cacheIndx );
        len = 1 + wcslen( buf );
        specialDir = XP_MALLOC( globals->mpool, len * sizeof(specialDir[0]) );
        wcscpy( specialDir, buf );
        globals->specialDirs[cacheIndx] = specialDir;
    }

    wcscpy( bufW, specialDir );

    switch( typ ) {
    case PREFS_FILE_PATH_L:
        wcscat( bufW, L"xwprefs" );
        break;
    case DEFAULT_DIR_PATH_L:
        /* nothing to do */
        break;
    case DEFAULT_GAME_PATH:
        asAscii = XP_TRUE;
        wcscat( bufW, L"_newgame" );
        break;

    case PROGFILES_PATH:
        /* nothing to do */
        break;
    }

    len = wcslen( bufW );
    if ( asAscii ) {
        (void)WideCharToMultiByte( CP_ACP, 0, bufW, len + 1,
                                   (char*)bufOut, bufLen, NULL, NULL );
    } else {
        wcscpy( (wchar_t*)bufOut, bufW );
    }
    return len;
} /* ceGetPath */
