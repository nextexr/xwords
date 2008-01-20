/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2000-2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CEMAIN_H_
#define _CEMAIN_H_

#ifdef _WIN32_WCE
# include <aygshell.h>
#endif
#include "draw.h"
#include "game.h"
#include "util.h"
#include "mempool.h"
#include "cesockwr.h"

enum { CE_BONUS1_COLOR,
       CE_BONUS2_COLOR,
       CE_BONUS3_COLOR,
       CE_BONUS4_COLOR,
       
       CE_BKG_COLOR, 
       CE_TILEBACK_COLOR,

       CE_FOCUS_COLOR,

       CE_USER_COLOR1,
       CE_USER_COLOR2,
       CE_USER_COLOR3,
       CE_USER_COLOR4,

       CE_BLACK_COLOR,     /* not editable by users */
       CE_WHITE_COLOR,

       CE_NUM_COLORS		/* last */
};

#define CUR_CE_PREFS_FLAGS 0x0003
typedef struct CEAppPrefs {
    XP_U16 versionFlags;
    CommonPrefs cp;
    COLORREF colors[CE_NUM_COLORS];
    XP_Bool showColors;
} CEAppPrefs;

#define NUM_BUTTONS 4

typedef struct CEAppGlobals {
    HINSTANCE hInst;
    HDC hdc;			/* to pass drawing ctxt to draw code */
    HWND hWnd;
#if defined TARGET_OS_WINCE
    HWND hwndCB;
#endif

    HWND buttons[NUM_BUTTONS];

#ifdef _WIN32_WCE
    SHACTIVATEINFO sai;
    XP_Bool fullScreen;         /* this probably wants to be a preference,
                                   i.e. preserved */
#endif

    DrawCtx* draw;
    XWGame game;
    CurGameInfo gameInfo;
    XP_UCHAR* curGameName;      /* path to storage for current game */
    XW_UtilCtxt util;
    VTableMgr* vtMgr;
    XP_U16* bonusInfo;
    wchar_t* lastDefaultDir;

    XP_U32 timerIDs[NUM_TIMERS_PLUS_ONE];
    XWTimerProc timerProcs[NUM_TIMERS_PLUS_ONE];
    void* timerClosures[NUM_TIMERS_PLUS_ONE];

    XP_U16 flags;               /* bits defined below */

#ifdef CEFEATURE_CANSCROLL
    XP_U16 nHiddenRows;
    HWND scrollHandle;
#endif

    CeSocketWrapper* socketWrap;

    CEAppPrefs appPrefs;

    XP_Bool isNewGame;
    XP_Bool penDown;
    XP_Bool hintPending;
    XP_Bool doGlobalPrefs;

#if defined DEBUG && !defined _WIN32_WCE
    int dbWidth, dbHeight;
#endif

#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool askTrayLimits;
#endif
    MPSLOT

} CEAppGlobals;

#ifdef DICTS_MOVED_ALERT
# define FLAGS_BIT_SHOWN_NEWDICTLOC 0x0001
#endif

#define GAME_IN_PROGRESS(g) ((g)->gameInfo.dictName != 0)

enum {
    XWWM_TIME_RQST = WM_APP
    ,XWWM_PACKET_ARRIVED

};

#define CE_NUM_EDITABLE_COLORS CE_BLACK_COLOR

typedef struct CEDrawCtx {
    DrawCtxVTable* vtable;
    
    HWND mainWin;
    CEAppGlobals* globals;

    COLORREF prevBkColor;

    HBRUSH brushes[CE_NUM_COLORS];

    HFONT trayFont;
    HFONT selPlayerFont;
    HFONT playerFont;

    HBITMAP rightArrow;
    HBITMAP downArrow;
    HBITMAP origin;

    XP_U16 trayOwner;
    XP_U16 miniLineHt;
    XP_Bool scoreIsVertical;

    MPSLOT
} CEDrawCtx;

DrawCtx* ce_drawctxt_make( MPFORMAL HWND mainWin, CEAppGlobals* globals );
void ce_drawctxt_update( DrawCtx* dctx );


#ifdef DEBUG
void logLastError( const char* comment );
void messageToBuf( UINT message, char* buf, int bufSize );
#else
# define logLastError(c)
#endif


#endif /* _CEMAIN_H_ */
