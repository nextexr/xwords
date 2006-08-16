/* -*-mode: C; fill-column: 76; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#ifndef _PALMMAIN_H_
#define _PALMMAIN_H_

#define AppType APPID
#define PrefID 0
#define VERSION_NUM_405 1
#define VERSION_NUM     2           /* 1 to 2 moving to ARM */

#include <PalmTypes.h>
#include <DataMgr.h>
#include <Window.h>
#include <List.h>
#include <Form.h>
#include <IrLib.h>
#ifdef BEYOND_IR
# include <NetMgr.h>
#endif

#include "game.h"
#include "util.h"
#include "mempool.h"
#include "nwgamest.h"

/* #include "prefsdlg.h" */
#include "xwcolors.h"

#include "xwords4defines.h"

#ifdef MEM_DEBUG
# define MEMPOOL globals->mpool,
#else
# define MEMPOOL 
#endif


enum { ONEBIT, /*  GREYSCALE,  */COLOR };
typedef unsigned char GraphicsAbility; /* don't let above be 4 bytes */
typedef struct PalmAppGlobals PalmAppGlobals;

typedef XP_UCHAR* (*GetResStringFunc)( PalmAppGlobals* globals, 
                                       XP_U16 strID );

typedef struct {
    XP_S16 topOffset;           /* how many pixels from the top of the
                                   drawing area is the first pixel set in
                                   the glyph */
    XP_U16 height;              /* How many rows tall is the image? */
} PalmFontHtInfo;

typedef struct PalmDrawCtx {
    DrawCtxVTable* vtable;
    PalmAppGlobals* globals;

    void (*drawBitmapFunc)( DrawCtx* dc, Int16 resID, Int16 x, Int16 y );
    GetResStringFunc getResStrFunc;

    DrawingPrefs* drawingPrefs;

    RectangleType oldScoreClip;
    RectangleType oldTrayClip;

    XP_S16 trayOwner;
    XP_U16 fntHeight;

    GraphicsAbility able;

    UInt16 oldCoord;
    XP_Bool doHiRes;
    XP_Bool oneDotFiveAvail;

    XP_LangCode fontLangCode;
    PalmFontHtInfo* fontHtInfo;

    union {
        struct {
            XP_U8 reserved;     /* make CW compiler happy */
        } clr;
        struct {
            CustomPatternType valuePatterns[4];
        } bnw;
    } u;
    MPSLOT
} PalmDrawCtx;

#define draw_drawBitmapAt(dc,id,x,y) \
     (*((((PalmDrawCtx*)dc))->drawBitmapFunc))((dc),(id),(x),(y))

typedef struct ListData {
    unsigned char** strings;
    unsigned char* storage;
    XP_U16 nItems;
    XP_U16 storageLen;
    XP_U16 nextIndex;
    XP_S16 selIndex;
#ifdef DEBUG
    XP_Bool choicesSet;    /* ARM hack: don't use strings after PACE
                              swaps.... */
#endif
} ListData;

typedef struct XWords4PreferenceType {
    Int16 versionNum;

    Int16 curGameIndex;		/* which game is currently open */

    /* these are true global preferences */
    Boolean showProgress;
    Boolean showGrid;
    Boolean showColors;
#ifdef DEBUG
    Boolean showDebugstrs;
    Boolean logToMemo;
    Boolean reserved1;
    Boolean reserved2;
#else
    Boolean reserved1;
#endif
    /* New for 0x0405 */
    CommonPrefs cp;
    
} XWords4PreferenceType;

typedef struct MyIrConnect {
    IrConnect irCon;
    PalmAppGlobals* globals;
} MyIrConnect;

typedef XP_U8 IR_STATE;		/* enums are in palmir.h */

#define IR_BUF_SIZE 256

typedef struct MyIrPacket MyIrPacket;

typedef struct ProgressCtxt {
    RectangleType boundsRect;
    XP_S16 curLine;
} ProgressCtxt;

/* I *hate* having to define these globally... */
typedef struct SavedGamesState {
    struct PalmAppGlobals* globals;
    FormPtr form;
    ListPtr gamesList;
    FieldPtr nameField;
    char** stringPtrs;
    Int16 nStrings;
    Int16 displayGameIndex;
} SavedGamesState;

typedef struct PrefsDlgState {
    ListPtr playerBdSizeList;
    ListPtr phoniesList;

    CommonPrefs cp;

    XP_U16 gameSeconds;
    XP_Bool stateTypeIsGlobal;

    XP_U8   phoniesAction;
    XP_U8   curBdSize;
    XP_Bool showColors;
    XP_Bool smartRobot;
    XP_Bool showProgress;
    XP_Bool showGrid;
    XP_Bool hintsNotAllowed;
    XP_Bool timerEnabled;
    XP_Bool allowPickTiles;
    XP_Bool allowHintRect;
} PrefsDlgState;

typedef struct DictState {
    ListPtr dictList;
    ListData sLd;
    XP_U16 nDicts;
} DictState;

typedef struct PalmNewGameState {
    FormPtr form;
    ListPtr playerNumList;
    NewGameCtx* ngc;
    XP_UCHAR passwds[MAX_PASSWORD_LENGTH+1][MAX_NUM_PLAYERS];
    XP_UCHAR* dictName;
    XP_UCHAR shortDictName[32]; /* as long as a dict name can be */
    XP_Bool forwardChange;
    Connectedness curServerHilite;
#ifdef BEYOND_IR
    CommsAddrRec addr;
    XP_Bool connsSettingChanged;
#endif
} PalmNewGameState;

typedef struct PalmDictList PalmDictList;

#ifdef BEYOND_IR
typedef struct NetLibStuff {
    UInt16 netLibRef;
    NetSocketRef socket;
    XP_Bool ipAddrInval;
} NetLibStuff;
#define socketIsOpen(g) ((g)->nlStuff.socket != -1)
#endif

#define MAX_DLG_PARAMS 2

struct PalmAppGlobals {
    FormPtr mainForm;
    PrefsDlgState* prefsDlgState;
#if defined OWNER_HASH || defined NO_REG_REQUIRED
    SavedGamesState* savedGamesState;
#endif
    XWGame game;
    DrawCtx* draw;
    XW_UtilCtxt util;

    XP_U32 dlgParams[MAX_DLG_PARAMS];

    VTableMgr* vtMgr;

    XWords4PreferenceType gState;

    DrawingPrefs drawingPrefs;

    PalmDictList* dictList;

    DmOpenRef boardDBP;
    LocalID boardDBID;

    DmOpenRef gamesDBP;
    LocalID gamesDBID;

#ifdef BEYOND_IR
    UInt16 exgLibraryRef;    /* what library did user choose for sending? */
#endif

    XP_UCHAR* stringsResPtr;
    XP_U8* bonusResPtr[NUM_BOARD_SIZES];
    Boolean penDown;
    Boolean isNewGame;
    Boolean stateTypeIsGlobal;
    Boolean timeRequested;
    Boolean hintPending;
    Boolean isLefty;
    Boolean dictuiForBeaming;
    Boolean postponeDraw;
    Boolean needsScrollbar;
    Boolean msgReceivedDraw;
    Boolean isFirstLaunch;
    Boolean menuIsDown;
    XP_Bool newGameIsNew;
    XP_Bool runningOnPOSE;    /* Needed for NetLibSelect */

    GraphicsAbility able;
    XP_U16 prevScroll;		/* for scrolling in 'ask' dialog */
    UInt16 romVersion;

    XP_U8 scrollValue;		/* 0..2: scrolled position of board */

#ifdef SHOW_PROGRESS
    ProgressCtxt progress;
#endif

    XP_U16 width, height;
    XP_U16 sonyLibRef;
    XP_Bool doVSK;
    XP_Bool hasHiRes;
    XP_Bool oneDotFiveAvail;
    XP_Bool useHiRes;

#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool askTrayLimits;
#endif

    CurGameInfo gameInfo;	/* for the currently open, or new, game */

    /* dialog/forms state */
    PalmNewGameState newGameState;

    DictState dictState;

    struct ConnsDlgState* connState;

    TimerProc timerProcs[NUM_TIMERS_PLUS_ONE];
    void* timerClosures[NUM_TIMERS_PLUS_ONE];
    XP_U32 timerFireAt[NUM_TIMERS_PLUS_ONE];

#ifdef BEYOND_IR
    NetLibStuff nlStuff;
    XP_U32 heartTimerFireAt;
#endif

#ifdef DEBUG
    UInt8 save_rLsap;
    IR_STATE ir_state_prev;
    XP_U16 yCount;
/*     Boolean resetGame; */
#endif
    MPSLOT
}; /* PalmAppGlobals */

/* custom events */
enum { dictSelectedEvent = firstUserEvent /* 0x6000 */
       ,newGameOkEvent
       ,newGameCancelEvent
       ,loadGameEvent
       ,prefsChangedEvent
       ,openSavedGameEvent
#ifdef BEYOND_IR
       ,connsSettingChgEvent
#endif
#ifdef FEATURE_SILK
       ,doResizeWinEvent
#endif
};

enum {
    PNOLET_STORE_FEATURE = 1    /* where FtrPtr to pnolet code lives */
    , GLOBALS_FEATURE           /* for passing globals to form handlers */
#ifdef FEATURE_DUALCHOOSE
    , FEATURE_WANTS_68K         /* support for (pre-ship) ability to choose
                                   armlet or 68K */
#endif
#ifdef XWFEATURE_COMBINEDAWG
    , DAWG_STORE_FEATURE
#endif
};
enum { WANTS_68K, WANTS_ARM };


/* If we're calling the old PilotMain (in palmmain.c) from from the one in
   enter68k.c it needs a different name.  But if this is the 68K-only app
   then that is the entry point. */
#ifdef FEATURE_PNOAND68K
# define PM2(pm) pm2_ ## pm
UInt32 PM2(PilotMain)(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags);
#else
# define PM2(pm) pm
#endif

DrawCtx* palm_drawctxt_make( MPFORMAL GraphicsAbility able,
                             PalmAppGlobals* globals,
                             GetResStringFunc getRSF,
                             DrawingPrefs* drawprefs );
void palm_drawctxt_destroy( DrawCtx* dctx );

void palm_warnf( char* format, ... );

Boolean askPassword( const XP_UCHAR* name, Boolean isNew, XP_UCHAR* retbuf, 
                     XP_U16* len );
Boolean palmaskFromStrId( PalmAppGlobals* globals, XP_U16 strId, 
                          XP_S16 titleID );
void freeSavedGamesData( MPFORMAL SavedGamesState* state );

void writeNameToGameRecord( PalmAppGlobals* globals, XP_S16 index, 
                            char* newName, XP_U16 len );

XP_UCHAR* getResString( PalmAppGlobals* globals, XP_U16 strID );
Boolean palmask( PalmAppGlobals* globals, XP_UCHAR* str, XP_UCHAR* altButton, 
                 XP_S16 titleID );
void checkAndDeliver( PalmAppGlobals* globals, XWStreamCtxt* instream );

#ifdef XW_TARGET_PNO
# define READ_UNALIGNED16(n) read_unaligned16((unsigned char*)(n))
#else
# define READ_UNALIGNED16(n) *(n)
#endif

#endif /* _PALMMAIN_H_ */
