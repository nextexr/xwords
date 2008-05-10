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

#include "ceutil.h"
#include "cedefines.h"
#include "cedebug.h"

#define BUF_SIZE 128
#define VPADDING 4
#define HPADDING_L 2
#define HPADDING_R 3

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
ce_selectAndShow( HWND hDlg, XP_U16 resID, XP_U16 index )
{
    SendDlgItemMessage( hDlg, resID, LB_SETCURSEL, index, 0 );
    SendDlgItemMessage( hDlg, resID, LB_SETANCHORINDEX, index, 0 );
} /* ce_selectAndShow */

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

XP_Bool
ceIsLandscape( CEAppGlobals* globals )
{
    XP_U16 width, height;
    XP_Bool landscape;

    XP_ASSERT( !!globals );
    XP_ASSERT( !!globals->hWnd );

    if ( 0 ) {
#if defined DEBUG && !defined _WIN32_WCE
    } else if ( globals->dbWidth != 0 ) {
        width = globals->dbWidth;
        height = globals->dbHeight;
#endif
    } else {
        RECT rect;
        GetClientRect( globals->hWnd, &rect );
        width = (XP_U16)(rect.right - rect.left);
        height = (XP_U16)(rect.bottom - rect.top);
    }

    landscape = (height - CE_SCORE_HEIGHT) 
        < (width - CE_MIN_SCORE_WIDTH);
    return landscape;
} /* ceIsLandscape */

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

void
ceSizeIfFullscreen( CEAppGlobals* globals, HWND hWnd )
{
    if ( globals->appPrefs.fullScreen != ceIsFullScreen(globals, hWnd) ) {
        RECT rect;
        XP_U16 cbHeight = 0;
        if ( !!globals->hwndCB && hWnd == globals->hWnd ) {
            GetWindowRect( globals->hwndCB, &rect );
            cbHeight = rect.bottom - rect.top;
        }

        /* I'm leaving the SIP/cmdbar in place until I can figure out how to
           get menu events with it hidden -- and also the UI for making sure
           users don't get stuck in fullscreen mode not knowing how to reach
           menus to get out.  Later, add SHFS_SHOWSIPBUTTON and
           SHFS_HIDESIPBUTTON to the sets shown and hidden below.*/
        if ( globals->appPrefs.fullScreen ) {
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
    }
} /* ceSizeIfFullscreen */

static XP_Bool
mkFullscreenWithSoftkeys( CEAppGlobals* globals, HWND hDlg )
{
    XP_Bool success = XP_FALSE;
    XP_Bool fullScreen = XP_TRUE; /* probably want this TRUE for
                                     small-screened smartphones only. */

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
    } else if ( fullScreen ) {
        ceSizeIfFullscreen( globals, hDlg );
        success = XP_TRUE;
    }

    if ( success ) {
        SHMENUBARINFO mbi;
        XP_MEMSET( &mbi, 0, sizeof(mbi) );
        mbi.cbSize = sizeof(mbi);
        mbi.hwndParent = hDlg;
        mbi.nToolBarId = IDM_OKCANCEL_MENUBAR;
        mbi.hInstRes = globals->hInst;
        success = SHCreateMenuBar( &mbi );
        if ( !success ) {
            XP_LOGF( "SHCreateMenuBar failed: %ld", GetLastError() );
        }
    }

    return success;
} /* mkFullscreenWithSoftkeys */
#endif

#define TITLE_HT 20            /* Need to get this from the OS */
void
ceDlgSetup( CEAppGlobals* globals, HWND hDlg )
{
    XP_ASSERT( !!globals );
    RECT rect;
    XP_U16 vHeight;

    GetClientRect( hDlg, &rect );
    XP_ASSERT( rect.top == 0 );
    vHeight = rect.bottom;         /* This is before we've resized it */

#ifdef _WIN32_WCE
    (void)mkFullscreenWithSoftkeys( globals, hDlg );
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
     
        XP_LOGF( "%s: vHeight: %d; r.bottom: %ld", __func__, vHeight, 
                 rect.bottom );

        XP_MEMSET( &sinfo, 0, sizeof(sinfo) );
        sinfo.cbSize = sizeof(sinfo);

        sinfo.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
        sinfo.nPos = 0;
        sinfo.nMin = 0;
        sinfo.nMax = vHeight - rect.bottom;
        if ( sinfo.nMax < 0 ) {
            sinfo.nMax = 0;     /* disables the thing! */
        }
        XP_LOGF( "%s: set max to %d", __func__, sinfo.nMax );
        sinfo.nPage = 10;
        
        (void)SetScrollInfo( hDlg, SB_VERT, &sinfo, FALSE );
    }

} /* ceDlgSetup */

static void
setScrollPos( HWND hDlg, XP_S16 newPos )
{
    SCROLLINFO sinfo;
    XP_S16 vertChange;

    XP_LOGF( "%s(%d)", __func__, newPos );

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
    LOG_RETURN_VOID();
} /* setScrollPos */

static void
adjustScrollPos( HWND hDlg, XP_S16 vertChange )
{
    XP_LOGF( "%s(%d)", __func__, vertChange );
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

XP_Bool
ceDoDlgScroll( CEAppGlobals* globals, HWND hDlg, WPARAM wParam )
{
    XP_Bool handled = !IS_SMARTPHONE(globals);
    if ( handled ) {
        XP_S16 vertChange = 0;
    
        switch ( LOWORD(wParam) ) {

        case SB_LINEUP: // Scrolls one line up 
            vertChange = -1;
            break;
        case SB_PAGEUP: // 
            vertChange = -10;
            break;

        case SB_LINEDOWN: // Scrolls one line down 
            vertChange = 1;
            break;
        case SB_PAGEDOWN: // Scrolls one page down 
            vertChange = 10;
            break;

        case SB_THUMBTRACK:     /* still dragging; don't redraw */
        case SB_THUMBPOSITION:
            setScrollPos( hDlg, HIWORD(wParam) );
            break;
        }

        if ( 0 != vertChange ) {
            adjustScrollPos( hDlg, vertChange );
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

void
ceDoDlgFocusScroll( CEAppGlobals* globals, HWND hDlg, WPARAM wParam, LPARAM lParam )
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

    if ( !IS_SMARTPHONE(globals) ) {
        HWND nextCtrl;
        if ( LOWORD(lParam) ) {
            nextCtrl = (HWND)wParam;
        } else {
            BOOL previous = wParam != 0;
            nextCtrl = GetNextDlgTabItem( hDlg, GetFocus(), previous );
        }

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

            XP_LOGF( "%p: ctrlPos is %d; height is %d", 
                     nextCtrl, ctrlPos, ctrlHeight );

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
ceFindMenu( HMENU menu, XP_U16 id, HMENU* foundMenu, XP_U16* foundPos,
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
            found = ceFindMenu( minfo.hSubMenu, id, foundMenu, foundPos,
                                foundBuf, bufLen );
        } else if ( MFT_SEPARATOR == minfo.fType ) {
            continue;
        } else if ( minfo.wID == id ) {
            found = XP_TRUE;
            *foundPos = pos;
            *foundMenu = menu;
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

void
ceSetLeftSoftkey( CEAppGlobals* globals, XP_U16 newId )
{
    if ( newId != globals->softkey.oldId ) {
        HMENU menu;
        HMENU prevMenu;
        XP_U16 prevPos;
        XP_U16 oldId = globals->softkey.oldId;
        if ( 0 == oldId ) {
            oldId = ID_INITIAL_SOFTID;
        }

#ifdef _WIN32_WCE
        TBBUTTONINFO info;
        XP_MEMSET( &info, 0, sizeof(info) );
        info.cbSize = sizeof(info);
#endif

#ifdef _WIN32_WCE
        info.dwMask = TBIF_LPARAM;
        SendMessage( globals->hwndCB, TB_GETBUTTONINFO, IDM_MENU, 
                     (LPARAM)&info );
        menu = (HMENU)info.lParam;  /* Use to remove item being installed in
                                       left button */
#else
        menu = GetMenu( globals->hWnd );
#endif

        /* First put any existing menu item back in the main menu! */
        if ( globals->softkey.oldMenu != 0 ) {
            if ( ! InsertMenu( globals->softkey.oldMenu, 
                               globals->softkey.oldPos, MF_BYPOSITION, 
                               globals->softkey.oldId,
                               globals->softkey.oldName ) ) {
                XP_LOGF( "%s: InsertMenu failed", __func__ );
            }
        }

        /* Then find, remember and remove the new */
        if ( ceFindMenu( menu, newId, &prevMenu, &prevPos,
                         globals->softkey.oldName,
                         VSIZE(globals->softkey.oldName) ) ) {
            if ( !DeleteMenu( prevMenu, prevPos, MF_BYPOSITION ) ) {
                XP_LOGF( "%s: DeleteMenu failed", __func__ );
            }
            globals->softkey.oldMenu = prevMenu;
            globals->softkey.oldPos = prevPos;
            globals->softkey.oldId = newId;
        } else {
            XP_LOGF( "%s: ceFindMenu failed", __func__ );
        }

        /* Make it the button */
#ifdef _WIN32_WCE
        info.dwMask = TBIF_TEXT | TBIF_COMMAND;
        info.idCommand = newId;
        info.pszText = globals->softkey.oldName;
        SendMessage( globals->hwndCB, TB_SETBUTTONINFO, oldId, (LPARAM)&info );
#else
        setW32DummyMenu( globals, menu, newId, globals->softkey.oldName );
#endif
    }
} /* ceSetLeftSoftkey */
