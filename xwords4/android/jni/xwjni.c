
/* -*- compile-command: "find-and-gradle.sh inXw4Deb"; -*- */
/*
 * Copyright © 2009 - 2018 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
#include <string.h>
#include <sys/time.h>
#include <pthread.h>

#include <jni.h>
#include <android/log.h>

#include "comtypes.h"
#include "game.h"
#include "board.h"
#include "mempool.h"
#include "strutils.h"
#include "dbgutil.h"
#include "dictnry.h"
#include "dictiter.h"
#include "dictmgr.h"
#include "nli.h"
#include "smsproto.h"

#include "utilwrapper.h"
#include "drawwrapper.h"
#include "xportwrapper.h"
#include "anddict.h"
#include "andutils.h"
#include "andglobals.h"
#include "jniutlswrapper.h"
#include "paths.h"

#define LOG_MAPPING
// #define LOG_MAPPING_ALL

typedef struct _EnvThreadEntry {
    JNIEnv* env;
    pthread_t owner;
#ifdef LOG_MAPPING
    const char* ownerFunc;
#endif
} EnvThreadEntry;

struct _EnvThreadInfo {
    pthread_mutex_t mtxThreads;
    int nEntries;
    EnvThreadEntry* entries;
    MPSLOT
};

/* Globals for the whole game */
typedef struct _JNIGlobalState {
    EnvThreadInfo ti;
    DictMgrCtxt* dictMgr;
    SMSProto* smsProto;
    VTableMgr* vtMgr;
    XW_DUtilCtxt* dutil;
    JNIUtilCtxt* jniutil;
    XP_Bool mpoolInUse;
    const char* mpoolUser;
    MPSLOT
} JNIGlobalState;

#ifdef MEM_DEBUG
static MemPoolCtx*
getMPoolImpl( JNIGlobalState* globalState, const char* user )
{
    if ( globalState->mpoolInUse ) {
        XP_LOGF( "%s(): mpoolInUse ALREADY SET!!!! (by %s)",
                 __func__, globalState->mpoolUser );
    }
    globalState->mpoolInUse = XP_TRUE;
    globalState->mpoolUser = user;
    return globalState->mpool;
}

#define GETMPOOL(gs) getMPoolImpl( (gs), __func__ )

static void
releaseMPool( JNIGlobalState* globalState )
{
    // XP_ASSERT( globalState->mpoolInUse ); /* fired again!!! */
    if ( !globalState->mpoolInUse ) {
        XP_LOGF( "%s() line %d; ERROR ERROR ERROR mpoolInUse not set",
                 __func__, __LINE__ );
    }
    globalState->mpoolInUse = XP_FALSE;
}
#else
# define releaseMPool(s)
#endif


#define GAMEPTR_IS_OBJECT
#ifdef GAMEPTR_IS_OBJECT
typedef jobject GamePtrType;
#else
typedef long GamePtrType;
#endif

#ifdef LOG_MAPPING
# ifdef DEBUG
static int 
countUsed(const EnvThreadInfo* ti)
{
    int count = 0;
    for ( int ii = 0; ii < ti->nEntries; ++ii ) {
        EnvThreadEntry* entry = &ti->entries[ii];
        if ( 0 != entry->owner ) {
#  ifdef LOG_MAPPING_ALL
            XP_LOGF( "%s(): ii=%d; owner: %x", __func__, ii, (unsigned int)entry->owner );
#  endif
            ++count;
        }
    }
    return count;
}
# endif
#endif

#define MAP_THREAD( ti, env ) map_thread_prv( (ti), (env), __func__ )

static void
map_thread_prv( EnvThreadInfo* ti, JNIEnv* env, const char* caller )
{
    pthread_t self = pthread_self();

    pthread_mutex_lock( &ti->mtxThreads );

    XP_Bool found = false;
    int nEntries = ti->nEntries;
    EnvThreadEntry* firstEmpty = NULL;
    for ( int ii = 0; !found && ii < nEntries; ++ii ) {
        EnvThreadEntry* entry = &ti->entries[ii];
        if ( 0 == entry->owner ) {
            if ( NULL == firstEmpty ) {
                firstEmpty = entry;
            }
        } else if ( self == entry->owner ) {
            found = true;
            if ( env != entry->env ) {
                /* this DOES happen!!! */
                XP_LOGF( "%s (ti=%p): replacing env %p with env %p for thread %x",
                         __func__, ti, entry->env, env, (int)self );
                entry->env = env;
            }
        }
    }

    if ( !found ) {
        if ( !firstEmpty ) {    /* out of slots */
            if ( 0 == nEntries ) { /* first time */
                nEntries = 2;
                XP_ASSERT( !ti->entries );
            } else {
                nEntries *= 2;
            }
            EnvThreadEntry* entries = XP_CALLOC( ti->mpool, nEntries * sizeof(*entries) );
            if ( !!ti->entries ) {
                XP_MEMCPY( entries, ti->entries, ti->nEntries * sizeof(*ti->entries) );
            }
            firstEmpty = &entries[ti->nEntries]; /* first new entry */
            ti->entries = entries;
            ti->nEntries = nEntries;
#ifdef LOG_MAPPING
            XP_LOGF( "%s: num env entries now %d", __func__, nEntries );
#endif
        }

        XP_ASSERT( !!firstEmpty );
        firstEmpty->owner = self;
        firstEmpty->env = env;
#ifdef LOG_MAPPING
        firstEmpty->ownerFunc = caller;
        XP_LOGF( "%s: entry %zu: mapped env %p to thread %x", __func__,
                 firstEmpty - ti->entries, env, (int)self );
        XP_LOGF( "%s: num entries USED now %d", __func__, countUsed(ti) );
#endif
    }

    pthread_mutex_unlock( &ti->mtxThreads );
} /* map_thread_prv */

static void
map_init( MPFORMAL EnvThreadInfo* ti, JNIEnv* env )
{
    pthread_mutex_init( &ti->mtxThreads, NULL );
    MPASSIGN( ti->mpool, mpool );
    MAP_THREAD( ti, env );
}

#define MAP_REMOVE( ti, env ) map_remove_prv((ti), (env), __func__)
static void
map_remove_prv( EnvThreadInfo* ti, JNIEnv* env, const char* func )
{
    XP_Bool found = false;

    pthread_mutex_lock( &ti->mtxThreads );
    for ( int ii = 0; !found && ii < ti->nEntries; ++ii ) {
        EnvThreadEntry* entry = &ti->entries[ii];
        found = env == entry->env;
        if ( found ) {
#ifdef LOG_MAPPING
            XP_LOGF( "%s: UNMAPPED env %p to thread %x (from %s; mapped by %s)", __func__,
                     entry->env, (int)entry->owner, func, entry->ownerFunc );
            XP_LOGF( "%s: %d entries left", __func__, countUsed( ti ) );
            entry->ownerFunc = NULL;
#endif
            entry->env = NULL;
            entry->owner = 0;
        }
    }
    pthread_mutex_unlock( &ti->mtxThreads );

    XP_ASSERT( found );
}

static void
map_destroy( EnvThreadInfo* ti )
{
    pthread_mutex_destroy( &ti->mtxThreads );
}

static JNIEnv*
prvEnvForMe( EnvThreadInfo* ti )
{
    JNIEnv* result = NULL;
    pthread_t self = pthread_self();
    pthread_mutex_lock( &ti->mtxThreads );
    for ( int ii = 0; !result && ii < ti->nEntries; ++ii ) {
        if ( self == ti->entries[ii].owner ) {
            result = ti->entries[ii].env;
        }
    }
    pthread_mutex_unlock( &ti->mtxThreads );
    return result;
}

JNIEnv*
envForMe( EnvThreadInfo* ti, const char* caller )
{
    JNIEnv* result = prvEnvForMe( ti );
#ifdef DEBUG
    if( !result ) {
        pthread_t self = pthread_self();
        XP_LOGF( "no env for %s (thread %x)", caller, (int)self );
        XP_ASSERT(0);
    }
#endif
    return result;
}

static void
tilesArrayToTileSet( JNIEnv* env, jintArray jtiles, TrayTileSet* tset )
{
    if ( jtiles != NULL ) {
        XP_ASSERT( !!jtiles );
        jsize nTiles = (*env)->GetArrayLength( env, jtiles );
        int tmp[MAX_TRAY_TILES];
        getIntsFromArray( env, tmp, jtiles, nTiles, XP_FALSE );

        tset->nTiles = nTiles;
        for ( int ii = 0; ii < nTiles; ++ii ) {
            tset->tiles[ii] = tmp[ii];
        }
    }
}

#ifdef GAMEPTR_IS_OBJECT
static JNIState*
getState( JNIEnv* env, GamePtrType gamePtr, const char* func )
{
#ifdef DEBUG
    if ( NULL == gamePtr ) {
        XP_LOGF( "ERROR: getState() called from %s() with null gamePtr",
                 func );
    }
#endif
    XP_ASSERT( NULL != gamePtr ); /* fired */
    jmethodID mid = getMethodID( env, gamePtr, "ptr", "()J" );
    XP_ASSERT( !!mid );
    return (JNIState*)(*env)->CallLongMethod( env, gamePtr, mid );
}
#else
# define getState( env, gamePtr, func ) ((JNIState*)(gamePtr))
#endif

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_initGlobals
( JNIEnv* env, jclass C, jobject jdutil, jobject jniu )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = mpool_make( NULL );
    XP_LOGF( "%s(): ptr size: %zu", __func__, sizeof(mpool) );
#endif
    JNIGlobalState* globalState = (JNIGlobalState*)XP_CALLOC( mpool,
                                                              sizeof(*globalState) );
    map_init( MPPARM(mpool) &globalState->ti, env );
    globalState->jniutil = makeJNIUtil( MPPARM(mpool) env, &globalState->ti, jniu );
    globalState->vtMgr = make_vtablemgr( MPPARM_NOCOMMA(mpool) );
    globalState->dutil = makeDUtil( MPPARM(mpool) &globalState->ti, jdutil,
                                    globalState->vtMgr, globalState->jniutil, NULL );
    globalState->dictMgr = dmgr_make( MPPARM_NOCOMMA( mpool ) );
    globalState->smsProto = smsproto_init( MPPARM( mpool ) globalState->dutil );
    MPASSIGN( globalState->mpool, mpool );
    // LOG_RETURNF( "%p", globalState );
    return (jlong)globalState;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_cleanGlobals
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    // LOG_FUNC();
    if ( 0 != jniGlobalPtr ) {
        JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
        MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
        XP_ASSERT( ENVFORME(&globalState->ti) == env );
        smsproto_free( globalState->smsProto );
        vtmgr_destroy( MPPARM(mpool) globalState->vtMgr );
        dmgr_destroy( globalState->dictMgr );
        destroyDUtil( &globalState->dutil );
        destroyJNIUtil( env, &globalState->jniutil );
        map_destroy( &globalState->ti );
        XP_FREE( mpool, globalState );
        mpool_destroy( mpool );
    }
}

static const SetInfo gi_ints[] = {
    ARR_MEMBER( CurGameInfo, nPlayers )
    ,ARR_MEMBER( CurGameInfo, gameSeconds )
    ,ARR_MEMBER( CurGameInfo, boardSize )
    ,ARR_MEMBER( CurGameInfo, gameID )
    ,ARR_MEMBER( CurGameInfo, dictLang )
    ,ARR_MEMBER( CurGameInfo, forceChannel )
};

static const SetInfo gi_bools[] = {
    ARR_MEMBER( CurGameInfo, hintsNotAllowed )
    ,ARR_MEMBER( CurGameInfo, timerEnabled )
    ,ARR_MEMBER( CurGameInfo, allowPickTiles )
    ,ARR_MEMBER( CurGameInfo, allowHintRect )
};

static const SetInfo pl_ints[] = {
    ARR_MEMBER( LocalPlayer, robotIQ )
    ,ARR_MEMBER( LocalPlayer, secondsUsed )
};

#define AANDS(a)                                \
    (a), VSIZE(a)

static CurGameInfo*
makeGI( MPFORMAL JNIEnv* env, jobject jgi )
{
    CurGameInfo* gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*gi) );
    XP_UCHAR buf[256];          /* in case needs whole path */

    getInts( env, (void*)gi, jgi, AANDS(gi_ints) );
    getBools( env, (void*)gi, jgi, AANDS(gi_bools) );

    /* Unlike on other platforms, gi is created without a call to
       game_makeNewGame, which sets gameID.  So check here if it's still unset
       and if necessary set it -- including back in the java world. */
    if ( 0 == gi->gameID ) {
        while ( 0 == gi->gameID ) {
            gi->gameID = getCurSeconds( env );
        }
        setInt( env, jgi, "gameID", gi->gameID );
    }

    gi->phoniesAction = 
        jenumFieldToInt( env, jgi, "phoniesAction",
                         PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );
    gi->serverRole = 
        jenumFieldToInt( env, jgi, "serverRole", 
                         PKG_PATH("jni/CurGameInfo$DeviceRole"));

    getString( env, jgi, "dictName", AANDS(buf) );
    gi->dictName = copyString( mpool, buf );

    XP_ASSERT( gi->nPlayers <= MAX_NUM_PLAYERS );

    jobject jplayers;
    if ( getObject( env, jgi, "players", "[L" PKG_PATH("jni/LocalPlayer") ";",
                    &jplayers ) ) {
        for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
            LocalPlayer* lp = &gi->players[ii];

            jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );
            XP_ASSERT( !!jlp );

            getInts( env, (void*)lp, jlp, AANDS(pl_ints) );

            lp->isLocal = getBool( env, jlp, "isLocal" );

            getString( env, jlp, "name", AANDS(buf) );
            lp->name = copyString( mpool, buf );
            getString( env, jlp, "password", AANDS(buf) );
            lp->password = copyString( mpool, buf );
            getString( env, jlp, "dictName", AANDS(buf) );
            lp->dictName = copyString( mpool, buf );

            deleteLocalRef( env, jlp );
        }
        deleteLocalRef( env, jplayers );
    } else {
        XP_ASSERT(0);
    }

    return gi;
} /* makeGI */

static const SetInfo nli_ints[] = {
    ARR_MEMBER( NetLaunchInfo, _conTypes ),
    ARR_MEMBER( NetLaunchInfo, lang ),
    ARR_MEMBER( NetLaunchInfo, forceChannel ),
    ARR_MEMBER( NetLaunchInfo, nPlayersT ),
    ARR_MEMBER( NetLaunchInfo, nPlayersH ),
    ARR_MEMBER( NetLaunchInfo, gameID ),
    ARR_MEMBER( NetLaunchInfo, osVers ),
};

static const SetInfo nli_bools[] = {
    ARR_MEMBER( NetLaunchInfo, isGSM ),
    ARR_MEMBER( NetLaunchInfo, remotesAreRobots ),
};

static const SetInfo nli_strs[] = {
    ARR_MEMBER( NetLaunchInfo, dict ),
    ARR_MEMBER( NetLaunchInfo, gameName ),
    ARR_MEMBER( NetLaunchInfo, room ),
    ARR_MEMBER( NetLaunchInfo, btName ),
    ARR_MEMBER( NetLaunchInfo, btAddress ),
    ARR_MEMBER( NetLaunchInfo, phone ),
    ARR_MEMBER( NetLaunchInfo, inviteID ),
};

static void
loadNLI( JNIEnv* env, NetLaunchInfo* nli, jobject jnli )
{
    getInts( env, (void*)nli, jnli, AANDS(nli_ints) );
    getBools( env, (void*)nli, jnli, AANDS(nli_bools) );
    getStrings( env, (void*)nli, jnli, AANDS(nli_strs) );
}

static void
setNLI( JNIEnv* env, jobject jnli, const NetLaunchInfo* nli )
{
    setInts( env, jnli, (void*)nli, AANDS(nli_ints) );
    setBools( env, jnli, (void*)nli, AANDS(nli_bools) );
    setStrings( env, jnli, (void*)nli, AANDS(nli_strs) );
}

static void
setJGI( JNIEnv* env, jobject jgi, const CurGameInfo* gi )
{
    // set fields

    setInts( env, jgi, (void*)gi, AANDS(gi_ints) );
    setBools( env, jgi, (void*)gi, AANDS(gi_bools) );

    setString( env, jgi, "dictName", gi->dictName );

    intToJenumField( env, jgi, gi->phoniesAction, "phoniesAction",
                     PKG_PATH("jni/CurGameInfo$XWPhoniesChoice") );
    intToJenumField( env, jgi, gi->serverRole, "serverRole",
                     PKG_PATH("jni/CurGameInfo$DeviceRole") );

    jobject jplayers;
    if ( getObject( env, jgi, "players", 
                    "[L" PKG_PATH("jni/LocalPlayer") ";",
                    &jplayers ) ) {
        for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
            const LocalPlayer* lp = &gi->players[ii];

            jobject jlp = (*env)->GetObjectArrayElement( env, jplayers, ii );
            XP_ASSERT( !!jlp );

            setInts( env, jlp, (void*)lp, AANDS(pl_ints) );
            
            setBool( env, jlp, "isLocal", lp->isLocal );
            setString( env, jlp, "name", lp->name );
            setString( env, jlp, "password", lp->password );
            setString( env, jlp, "dictName", lp->dictName );

            deleteLocalRef( env, jlp );
        }
        deleteLocalRef( env, jplayers );
    } else {
        XP_ASSERT(0);
    }
} /* setJGI */

#ifdef COMMON_LAYOUT
static const SetInfo bd_ints[] = {
    ARR_MEMBER( BoardDims, left )
    ,ARR_MEMBER( BoardDims, top )
    ,ARR_MEMBER( BoardDims, width )
    ,ARR_MEMBER( BoardDims, height )
    ,ARR_MEMBER( BoardDims, scoreLeft )
    ,ARR_MEMBER( BoardDims, scoreHt )
    ,ARR_MEMBER( BoardDims, scoreWidth )
    ,ARR_MEMBER( BoardDims, boardWidth )
    ,ARR_MEMBER( BoardDims, boardHt )
    ,ARR_MEMBER( BoardDims, trayLeft )
    ,ARR_MEMBER( BoardDims, trayTop )
    ,ARR_MEMBER( BoardDims, trayWidth )
    ,ARR_MEMBER( BoardDims, trayHt )
    ,ARR_MEMBER( BoardDims, cellSize )
    ,ARR_MEMBER( BoardDims, maxCellSize )
    ,ARR_MEMBER( BoardDims, timerWidth )
};

static void
dimsJToC( JNIEnv* env, BoardDims* out, jobject jdims )
{
    getInts( env, (void*)out, jdims, AANDS(bd_ints) );
}

static void
dimsCtoJ( JNIEnv* env, jobject jdims, const BoardDims* in )
{
    setInts( env, jdims, (void*)in, AANDS(bd_ints) );
}
#endif

static void
destroyGI( MPFORMAL CurGameInfo** gip )
{
    CurGameInfo* gi = *gip;
    if ( !!gi ) {
        gi_disposePlayerInfo( MPPARM(mpool) gi );
        XP_FREE( mpool, gi );
        *gip = NULL;
    }
}

static void
loadCommonPrefs( JNIEnv* env, CommonPrefs* cp, jobject j_cp )
{
    XP_ASSERT( !!j_cp );
    cp->showBoardArrow = getBool( env, j_cp, "showBoardArrow" );
    cp->showRobotScores = getBool( env, j_cp, "showRobotScores" );
    cp->hideTileValues = getBool( env, j_cp, "hideTileValues" );
    cp->skipCommitConfirm = getBool( env, j_cp, "skipCommitConfirm" );
    cp->showColors = getBool( env, j_cp, "showColors" );
    cp->sortNewTiles = getBool( env, j_cp, "sortNewTiles" );
    cp->allowPeek = getBool( env, j_cp, "allowPeek" );
#ifdef XWFEATURE_CROSSHAIRS
    cp->hideCrosshairs = getBool( env, j_cp, "hideCrosshairs" );
#endif
}

static XWStreamCtxt*
streamFromJStream( MPFORMAL JNIEnv* env, VTableMgr* vtMgr, jbyteArray jstream )
{
    XP_ASSERT( !!jstream );
    int len = (*env)->GetArrayLength( env, jstream );
    XWStreamCtxt* stream = mem_stream_make_sized( MPPARM(mpool) vtMgr,
                                                  len, NULL, 0, NULL );
    jbyte* jelems = (*env)->GetByteArrayElements( env, jstream, NULL );
    stream_putBytes( stream, jelems, len );
    (*env)->ReleaseByteArrayElements( env, jstream, jelems, 0 );
    return stream;
} /* streamFromJStream */

/****************************************************
 * These three methods are stateless: no gamePtr
 ****************************************************/
JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_gi_1to_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jgi )
{
    jbyteArray result;
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, jgi );
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globalState->vtMgr,
                                            NULL, 0, NULL );

    game_saveToStream( NULL, gi, stream, 0 );
    destroyGI( MPPARM(mpool) &gi );

    result = streamToBArray( env, stream );
    stream_destroy( stream );
    releaseMPool( globalState );
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_gi_1from_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jgi, jbyteArray jstream )
{
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env,
                                              globalState->vtMgr, jstream );

    CurGameInfo gi;
    XP_MEMSET( &gi, 0, sizeof(gi) );
    if ( game_makeFromStream( MPPARM(mpool) stream, NULL,
                              &gi, NULL, NULL, NULL, NULL, NULL, NULL ) ) {
        setJGI( env, jgi, &gi );
    } else {
        XP_LOGF( "%s: game_makeFromStream failed", __func__ );
    }

    gi_disposePlayerInfo( MPPARM(mpool) &gi );

    stream_destroy( stream );
    releaseMPool( globalState );
}

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_nli_1to_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject njli )
{
    LOG_FUNC();
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif

    jbyteArray result;
    NetLaunchInfo nli = {0};
    loadNLI( env, &nli, njli );
    /* CurGameInfo* gi = makeGI( MPPARM(mpool) env, jgi ); */
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globalState->vtMgr,
                                            NULL, 0, NULL );

    nli_saveToStream( &nli, stream );

    result = streamToBArray( env, stream );
    stream_destroy( stream );
    releaseMPool( globalState );
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_nli_1from_1stream
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jnli, jbyteArray jstream )
{
    LOG_FUNC();
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif
    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env,
                                              globalState->vtMgr, jstream );

    NetLaunchInfo nli = {0};
    if ( nli_makeFromStream( &nli, stream ) ) {
        setNLI( env, jnli, &nli );
    } else {
        XP_LOGF( "%s: game_makeFromStream failed", __func__ );
    }

    stream_destroy( stream );
    releaseMPool( globalState );
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getInitialAddr
( JNIEnv* env, jclass C, jobject jaddr, jstring jname, jint port )
{
    CommsAddrRec addr;

    const char* chars = (*env)->GetStringUTFChars( env, jname, NULL );
    comms_getInitialAddr( &addr, chars, port );
    (*env)->ReleaseStringUTFChars( env, jname, chars );
    setJAddrRec( env, jaddr, &addr );
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getUUID
( JNIEnv* env, jclass C )
{
    jstring jstr = 
#ifdef XWFEATURE_BLUETOOTH
        (*env)->NewStringUTF( env,
# if defined VARIANT_xw4NoSMS || defined VARIANT_xw4fdroid || defined VARIANT_xw4SMS
        XW_BT_UUID
# elif defined VARIANT_xw4d || defined VARIANT_xw4dNoSMS
        XW_BT_UUID_DBG
# endif
                              )
#else
        NULL
#endif
        ;
    return jstr;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1ref
( JNIEnv* env, jclass C, jlong dictPtr )
{
    if ( 0 != dictPtr ) {
        DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
        dict_ref( dict );
    }
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1unref
( JNIEnv* env, jclass C, jlong dictPtr )
{
    if ( 0 != dictPtr ) {
        DictionaryCtxt* dict = (DictionaryCtxt*)dictPtr;
        dict_unref( dict );
    }
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getInfo
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jDictBytes,
  jstring jname, jstring jpath, jboolean check, jobject jinfo )
{
    jboolean result = false;
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
    MAP_THREAD( &globalState->ti, env );

#ifdef MEM_DEBUG
    MemPoolCtx* mpool = GETMPOOL( globalState );
#endif

    DictionaryCtxt* dict = makeDict( MPPARM(mpool) env, globalState->dictMgr,
                                     globalState->jniutil, jname, jDictBytes, jpath,
                                     NULL, check );
    if ( NULL != dict ) {
        if ( NULL != jinfo ) {
            XP_LangCode code = dict_getLangCode( dict );
            XP_ASSERT( 0 < code );
            setInt( env, jinfo, "langCode", code );
            setInt( env, jinfo, "wordCount", dict_getWordCount( dict ) );
            setString( env, jinfo, "md5Sum", dict_getMd5Sum( dict ) );
        }
        dict_unref( dict );
        result = true;
    }

    releaseMPool( globalState );
    return result;
}

/* Dictionary methods: don't use gamePtr */
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1tilesAreSame
( JNIEnv* env, jclass C, jlong dictPtr1, jlong dictPtr2 )
{
    jboolean result;
    const DictionaryCtxt* dict1 = (DictionaryCtxt*)dictPtr1;
    XP_ASSERT( !!dict1 );
    const DictionaryCtxt* dict2 = (DictionaryCtxt*)dictPtr2;
    XP_ASSERT( !!dict2 );
    result = dict_tilesAreSame( dict1, dict2 );
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getChars
( JNIEnv* env, jclass C, jlong dictPtr )
{
    jobject result = NULL;
    result = and_dictionary_getChars( env, (DictionaryCtxt*)dictPtr );
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1getTileValue
( JNIEnv* env, jclass C, jlong dictPtr, jint tile )
{
    return dict_getTileValue( (DictionaryCtxt*)dictPtr, tile );
}

static jobjectArray
msgArrayToByteArrays( JNIEnv* env, const SMSMsgArray* arr )
{
    XP_ASSERT( arr->format == FORMAT_NET );

    jobjectArray result = makeByteArrayArray( env, arr->nMsgs );
    for ( int ii = 0; ii < arr->nMsgs; ++ii ) {
        SMSMsgNet* msg = &arr->u.msgsNet[ii];
        jbyteArray arr = makeByteArray( env, msg->len, (const jbyte*)msg->data );
        (*env)->SetObjectArrayElement( env, result, ii, arr );
        deleteLocalRef( env, arr );
    }
    return result;
}

static jobjectArray
msgArrayToJMsgArray( JNIEnv* env, const SMSMsgArray* arr )
{
    XP_ASSERT( arr->format == FORMAT_LOC );
    jclass clas = (*env)->FindClass( env, PKG_PATH("jni/XwJNI$SMSProtoMsg") );
    jobjectArray result = (*env)->NewObjectArray( env, arr->nMsgs, clas, NULL );

    jmethodID initId = (*env)->GetMethodID( env, clas, "<init>", "()V" );
    for ( int ii = 0; ii < arr->nMsgs; ++ii ) {
        jobject jmsg = (*env)->NewObject( env, clas, initId );

        const SMSMsgLoc* msgsLoc = &arr->u.msgsLoc[ii];
        intToJenumField( env, jmsg, msgsLoc->cmd, "cmd", PKG_PATH("jni/XwJNI$SMS_CMD") );
        setInt( env, jmsg, "gameID", msgsLoc->gameID );

        jbyteArray arr = makeByteArray( env, msgsLoc->len,
                                        (const jbyte*)msgsLoc->data );
        setObject( env, jmsg, "data", "[B", arr );
        deleteLocalRef( env, arr );
        
        (*env)->SetObjectArrayElement( env, result, ii, jmsg );
        deleteLocalRef( env, jmsg );
    }
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_smsproto_1prepOutbound
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jobject jCmd, jint jGameID,
  jbyteArray jData, jstring jToPhone, jint jPort, jintArray jWaitSecsArr )
{
    jobjectArray result = NULL;
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
    MAP_THREAD( &globalState->ti, env );

    SMS_CMD cmd = jEnumToInt( env, jCmd );
    jbyte* data = NULL;
    int len = 0;
    if ( NULL != jData ) {
        len = (*env)->GetArrayLength( env, jData );
        data = (*env)->GetByteArrayElements( env, jData, NULL );
    }
    const char* toPhone = (*env)->GetStringUTFChars( env, jToPhone, NULL );

    XP_U16 waitSecs;
    SMSMsgArray* arr = smsproto_prepOutbound( globalState->smsProto, cmd,
                                              jGameID, (const XP_U8*)data, len,
                                              toPhone, jPort, XP_FALSE,
                                              &waitSecs );
    if ( !!arr ) {
        result = msgArrayToByteArrays( env, arr );
        smsproto_freeMsgArray( globalState->smsProto, arr );
    }

    setIntInArray( env, jWaitSecsArr, 0, waitSecs );

    (*env)->ReleaseStringUTFChars( env, jToPhone, toPhone );
    if ( NULL != jData ) {
        (*env)->ReleaseByteArrayElements( env, jData, data, 0 );
    }

    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_smsproto_1prepInbound
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jData,
  jstring jFromPhone, jint jWantPort )
{
    jobjectArray result = NULL;

    if ( !!jData ) {
        JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
        MAP_THREAD( &globalState->ti, env );

        int len = (*env)->GetArrayLength( env, jData );
        jbyte* data = (*env)->GetByteArrayElements( env, jData, NULL );
        const char* fromPhone = (*env)->GetStringUTFChars( env, jFromPhone, NULL );

        SMSMsgArray* arr = smsproto_prepInbound( globalState->smsProto, fromPhone,
                                                 jWantPort, (XP_U8*)data, len );
        if ( !!arr ) {
            result = msgArrayToJMsgArray( env, arr );
            smsproto_freeMsgArray( globalState->smsProto, arr );
        }

        (*env)->ReleaseStringUTFChars( env, jFromPhone, fromPhone );
        (*env)->ReleaseByteArrayElements( env, jData, data, 0 );
    } else {
        XP_LOGF( "%s() => null (null input)", __func__ );
    }
    return result;
}

struct _JNIState {
    XWGame game;
    JNIGlobalState* globalJNI;
    AndGameGlobals globals;
    // pthread_mutex_t msgMutex;
    XP_U16 curSaveCount;
    XP_U16 lastSavedSize;
#ifdef DEBUG
    const char* envSetterFunc;
#endif
    MPSLOT
};

#define XWJNI_START() {                                     \
    JNIState* state = getState( env, gamePtr, __func__ );   \
    MPSLOT;                                                 \
    MPASSIGN( mpool, state->mpool);                         \
    XP_ASSERT( !!state->globalJNI );                        \
    MAP_THREAD( &state->globalJNI->ti, env );               \

#define XWJNI_START_GLOBALS()                           \
    XWJNI_START()                                       \
    AndGameGlobals* globals = &state->globals;          \

#define XWJNI_END()                                   \
    }                                                 \

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_initGameJNI
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jint seed )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = ((JNIGlobalState*)jniGlobalPtr)->mpool;
#endif
    JNIState* state = (JNIState*)XP_CALLOC( mpool, sizeof(*state) );
    state->globalJNI = (JNIGlobalState*)jniGlobalPtr;
    MAP_THREAD( &state->globalJNI->ti, env );
    AndGameGlobals* globals = &state->globals;
    globals->dutil = state->globalJNI->dutil;
    globals->state = (JNIState*)state;
    MPASSIGN( state->mpool, mpool );
    globals->vtMgr = make_vtablemgr(MPPARM_NOCOMMA(mpool));

    /* XP_LOGF( "%s: initing srand with %d", __func__, seed ); */
    srandom( seed );

    /* LOG_RETURNF( "%p", state ); */
    return (jlong) state;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_envDone
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    JNIGlobalState* globalJNI = (JNIGlobalState*)jniGlobalPtr;
    MAP_REMOVE( &globalJNI->ti, env );
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeNewGame
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject j_gi, 
  jobjectArray j_names, jobjectArray j_dicts, jobjectArray j_paths,
  jstring j_lang, jobject j_util, jobject j_draw, jobject j_cp,
  jobject j_procs )
{
    XWJNI_START_GLOBALS();
    EnvThreadInfo* ti = &state->globalJNI->ti;
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, j_gi );
    globals->gi = gi;
    globals->util = makeUtil( MPPARM(mpool) ti, j_util, gi, 
                              globals );
    globals->jniutil = state->globalJNI->jniutil;
    DrawCtx* dctx = NULL;
    if ( !!j_draw ) {
        dctx = makeDraw( MPPARM(mpool) ti, j_draw );
    }
    globals->dctx = dctx;
    globals->xportProcs = makeXportProcs( MPPARM(mpool) ti, j_procs );
    CommonPrefs cp;
    loadCommonPrefs( env, &cp, j_cp );

    game_makeNewGame( MPPARM(mpool) &state->game, gi, globals->util, dctx, &cp,
                      globals->xportProcs );

    DictionaryCtxt* dict;
    PlayerDicts dicts;

    makeDicts( MPPARM(state->globalJNI->mpool) env, state->globalJNI->dictMgr, 
               globals->jniutil, &dict, &dicts, j_names, j_dicts, 
               j_paths, j_lang );
#ifdef STUBBED_DICT
    if ( !dict ) {
        XP_LOGF( "falling back to stubbed dict" );
        dict = make_stubbed_dict( MPPARM_NOCOMMA(mpool) );
    }
#endif
    model_setDictionary( state->game.model, dict );
    dict_unref( dict );         /* game owns it now */
    model_setPlayerDicts( state->game.model, &dicts );
    dict_unref_all( &dicts );
    XWJNI_END();
} /* makeNewGame */

JNIEXPORT void JNICALL Java_org_eehouse_android_xw4_jni_XwJNI_game_1dispose
( JNIEnv* env, jclass claz, GamePtrType gamePtr )
{
    JNIState* state = getState( env, gamePtr, __func__ );

#ifdef MEM_DEBUG
    MemPoolCtx* mpool = state->mpool;
#endif
    AndGameGlobals* globals = &state->globals;

    destroyGI( MPPARM(mpool) &globals->gi );

    game_dispose( &state->game );

    destroyDraw( &globals->dctx );
    destroyXportProcs( &globals->xportProcs );
    destroyUtil( &globals->util );
    vtmgr_destroy( MPPARM(mpool) globals->vtMgr );

    MAP_REMOVE( &state->globalJNI->ti, env );
    /* pthread_mutex_destroy( &state->msgMutex ); */

    XP_FREE( mpool, state );
} /* game_dispose */

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1makeFromStream
( JNIEnv* env, jclass C, GamePtrType gamePtr, jbyteArray jstream, jobject /*out*/jgi,
  jobjectArray jdictNames, jobjectArray jdicts, jobjectArray jpaths,
  jstring jlang, jobject jutil, jobject jdraw, jobject jcp, jobject jprocs )
{
    jboolean result;
    DictionaryCtxt* dict;
    PlayerDicts dicts;
    XWJNI_START_GLOBALS();
    EnvThreadInfo* ti = &state->globalJNI->ti;

    globals->gi = (CurGameInfo*)XP_CALLOC( mpool, sizeof(*globals->gi) );
    globals->util = makeUtil( MPPARM(mpool) ti, jutil, globals->gi, globals);
    globals->jniutil = state->globalJNI->jniutil;
    makeDicts( MPPARM(state->globalJNI->mpool) env, state->globalJNI->dictMgr, 
               globals->jniutil, &dict, &dicts, jdictNames, jdicts, jpaths, 
               jlang );
    if ( !!jdraw ) {
        globals->dctx = makeDraw( MPPARM(mpool) ti, jdraw );
    }
    globals->xportProcs = makeXportProcs( MPPARM(mpool) ti, jprocs );

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, 
                                              globals->vtMgr, jstream );

    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );
    result = game_makeFromStream( MPPARM(mpool) stream, &state->game, 
                                  globals->gi, dict, &dicts,
                                  globals->util, globals->dctx, &cp,
                                  globals->xportProcs );
    stream_destroy( stream );
    dict_unref( dict );         /* game owns it now */
    dict_unref_all( &dicts );

    /* If game_makeFromStream() fails, the platform-side caller still needs to
       call game_dispose. That requirement's better than having cleanup code
       in two places. */
    if ( result ) {
        XP_ASSERT( 0 != globals->gi->gameID );
        if ( !!jgi ) {
            setJGI( env, jgi, globals->gi );
        }
    }

    XWJNI_END();
    return result;
} /* makeFromStream */

JNIEXPORT jbyteArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1saveToStream
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgi )
{
    jbyteArray result;
    XWJNI_START_GLOBALS();

    /* Use our copy of gi if none's provided.  That's because only the caller
       knows if its gi should win -- user has changed game config -- or if
       ours should -- changes like remote players being added. */
    CurGameInfo* gi = 
        (NULL == jgi) ? globals->gi : makeGI( MPPARM(mpool) env, jgi );
    XWStreamCtxt* stream = mem_stream_make_sized( MPPARM(mpool) globals->vtMgr,
                                                  state->lastSavedSize,
                                                  NULL, 0, NULL );

    game_saveToStream( &state->game, gi, stream, ++state->curSaveCount );

    if ( NULL != jgi ) {
        destroyGI( MPPARM(mpool) &gi );
    }

    state->lastSavedSize = stream_getSize( stream );
    result = streamToBArray( env, stream );
    stream_destroy( stream );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1saveSucceeded
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    game_saveSucceeded( &state->game, state->curSaveCount );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setDraw
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jdraw )
{
    XWJNI_START_GLOBALS();

    DrawCtx* newDraw = makeDraw( MPPARM(mpool) &state->globalJNI->ti, jdraw );
    board_setDraw( state->game.board, newDraw );

    destroyDraw( &globals->dctx );
    globals->dctx = newDraw;

    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1invalAll
( JNIEnv *env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    board_invalAll( state->game.board );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1draw
( JNIEnv *env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_draw( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1drawSnapshot
( JNIEnv *env, jclass C, GamePtrType gamePtr, jobject jdraw, jint width,
  jint height )
{
    XWJNI_START();
    DrawCtx* newDraw = makeDraw( MPPARM(mpool) &state->globalJNI->ti, jdraw );
    board_drawSnapshot( state->game.board, newDraw, width, height );
    destroyDraw( &newDraw );
    XWJNI_END();
}

#ifdef COMMON_LAYOUT
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1figureLayout
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgi, jint left, jint top, 
  jint width, jint height, jint scorePct, jint trayPct, jint scoreWidth,
  jint fontWidth, jint fontHt, jboolean squareTiles, jobject jdims )
{
    XWJNI_START();
    CurGameInfo* gi = makeGI( MPPARM(mpool) env, jgi );

    BoardDims dims;
    board_figureLayout( state->game.board, gi, left, top, width, height, 
                        115, scorePct, trayPct, scoreWidth,
                        fontWidth, fontHt, squareTiles,
                        ((!!jdims) ? &dims : NULL) );

    destroyGI( MPPARM(mpool) &gi );

    if ( !!jdims ) {
        dimsCtoJ( env, jdims, &dims );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1applyLayout
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jdims )
{
    XWJNI_START();
    BoardDims dims;
    dimsJToC( env, &dims, jdims );
    board_applyLayout( state->game.board, &dims );
    XWJNI_END();
}

#else

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setPos
(JNIEnv *env, jclass C, GamePtrType gamePtr, jint left, jint top, jint width, 
 jint height, jint maxCellSize, jboolean lefty )
{
    XWJNI_START();
    board_setPos( state->game.board, left, top, width, height, maxCellSize, 
                  lefty );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setScoreboardLoc
( JNIEnv *env, jclass C, GamePtrType gamePtr, jint left, jint top, 
  jint width, jint height, jboolean divideHorizontally )
{
    XWJNI_START();
    board_setScoreboardLoc( state->game.board, left, top, width, 
                            height, divideHorizontally );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setTimerLoc
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint timerLeft, jint timerTop,
  jint timerWidth, jint timerHeight )
{
    XWJNI_START();
    XP_LOGF( "%s(%d,%d,%d,%d)", __func__, timerLeft, timerTop,
             timerWidth, timerHeight );
    board_setTimerLoc( state->game.board, timerLeft, timerTop, 
                       timerWidth, timerHeight );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setTrayLoc
( JNIEnv *env, jclass C, GamePtrType gamePtr, jint left, jint top, 
  jint width, jint height, jint minDividerWidth )
{
    XWJNI_START();
    board_setTrayLoc( state->game.board, left, top, width, height, 
                      minDividerWidth );
    XWJNI_END();
}
#endif

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1zoom
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint zoomBy, jbooleanArray jCanZoom )
{
    jboolean result;
    XWJNI_START();
    XP_Bool canInOut[2];
    result = board_zoom( state->game.board, zoomBy, canInOut );
    jboolean canZoom[2] = { canInOut[0], canInOut[1] };
    setBoolArray( env, jCanZoom, VSIZE(canZoom), canZoom );
    XWJNI_END();
    return result;
}

#ifdef XWFEATURE_ACTIVERECT
JNIEXPORT jboolean JNICALL 
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getActiveRect
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jrect, jintArray dims )
{
    jboolean result;
    XWJNI_START();
    XP_Rect rect;
    XP_U16 nCols, nRows;
    result = board_getActiveRect( state->game.board, &rect, &nCols, &nRows );
    if ( result ) {
        setInt( env, jrect, "left", rect.left );
        setInt( env, jrect, "top", rect.top );
        setInt( env, jrect, "right", rect.left + rect.width );
        setInt( env, jrect, "bottom", rect.top + rect.height );
        if ( !!dims ) {
            setIntInArray( env, dims, 0, nCols );
            setIntInArray( env, dims, 1, nRows );
        }
    }
    XWJNI_END();
    return result;
}
#endif

#ifdef POINTER_SUPPORT
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenDown
(JNIEnv *env, jclass C, GamePtrType gamePtr, jint xx, jint yy, jbooleanArray barray )
{
    jboolean result;
    XWJNI_START();
    XP_Bool bb;                 /* drop this for now */
    result = board_handlePenDown( state->game.board, xx, yy, &bb );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenMove
( JNIEnv *env, jclass C, GamePtrType gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START();
    result = board_handlePenMove( state->game.board, xx, yy );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handlePenUp
( JNIEnv *env, jclass C, GamePtrType gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START();
    result = board_handlePenUp( state->game.board, xx, yy );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1containsPt
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint xx, jint yy )
{
    jboolean result;
    XWJNI_START();
    result = board_containsPt( state->game.board, xx, yy );
    XWJNI_END();
    return result;
}
#endif

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1juggleTray
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_juggleTray( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getTrayVisState
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_getTrayVisState( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getSelPlayer
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jint result;
    XWJNI_START();
    result = board_getSelPlayer( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1passwordProvided
(JNIEnv* env, jclass C, GamePtrType gamePtr, jint player, jstring jpasswd )
{
    jboolean result;
    XWJNI_START();
    const char* passwd = (*env)->GetStringUTFChars( env, jpasswd, NULL );
    result = board_passwordProvided( state->game.board, player, passwd );
    (*env)->ReleaseStringUTFChars( env, jpasswd, passwd );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1hideTray
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_hideTray( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1showTray
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_showTray( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1beginTrade
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_beginTrade( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1endTrade
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_endTrade( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1setBlankValue
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint player,
  jint col, jint row, jint tile )
{
    jboolean result;
    XWJNI_START();
    result = board_setBlankValue( state->game.board, player, col, row, tile );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1toggle_1showValues
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_toggle_showValues( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1commitTurn
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean phoniesConfirmed,
  jboolean turnConfirmed, jintArray jNewTiles )
{
    jboolean result;
    XWJNI_START();
    TrayTileSet* newTilesP = NULL;
    TrayTileSet newTiles;

    if ( jNewTiles != NULL ) {
        tilesArrayToTileSet( env, jNewTiles, &newTiles );
        newTilesP = &newTiles;
    }

    result = board_commitTurn( state->game.board, phoniesConfirmed,
                               turnConfirmed, newTilesP );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1flip
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_flip( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1replaceTiles
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    jboolean result;
    XWJNI_START();
    result = board_replaceTiles( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL 
Java_org_eehouse_android_xw4_jni_XwJNI_board_1redoReplacedTiles
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = board_redoReplacedTiles( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1reset
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    server_reset( state->game.server, state->game.comms );
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1handleUndo
(JNIEnv* env, jclass C, GamePtrType gamePtr)
{
    XWJNI_START();
    server_handleUndo( state->game.server, 0 );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1do
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    XP_ASSERT( !!state->game.server );
    result = server_do( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1tilesPicked
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint player, jintArray jNewTiles )
{
    XWJNI_START();
    TrayTileSet newTiles;
    tilesArrayToTileSet( env, jNewTiles, &newTiles );
    server_tilesPicked( state->game.server, player, &newTiles );
    XWJNI_END();
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1countTilesInPool
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jint result;
    XWJNI_START();
    result = server_countTilesInPool( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1resetEngine
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    board_resetEngine( state->game.board );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1requestHint
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean useLimits, 
  jboolean goBack, jbooleanArray workRemains )
{
    jboolean result;
    XWJNI_START();
    XP_Bool tmpbool;
    result = board_requestHint( state->game.board, 
#ifdef XWFEATURE_SEARCHLIMIT
                                useLimits, 
#endif
                                goBack, &tmpbool );
    /* If passed need to do workRemains[0] = tmpbool */
    if ( workRemains ) {
        jboolean jbool = tmpbool;
        setBoolArray( env, workRemains, 1, &jbool );
    }
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_timerFired
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint why, jint when, jint handle )
{
    jboolean result;
    XWJNI_START_GLOBALS();
    XW_UtilCtxt* util = globals->util;
    result = utilTimerFired( util, why, handle );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1formatRemainingTiles
(JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                            NULL, 0, NULL );
    board_formatRemainingTiles( state->game.board, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );

    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1formatDictCounts
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint nCols )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    server_formatDictCounts( state->game.server, stream, nCols );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1getGameIsOver
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = server_getGameIsOver( state->game.server );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1writeGameHistory
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean gameOver )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    model_writeGameHistory( state->game.model, stream, state->game.server,
                            gameOver );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    XWJNI_END();
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1getNMoves
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jint result;
    XWJNI_START();
    XP_ASSERT( !!state->game.model );
    result = model_getNMoves( state->game.model );
    XWJNI_END();
    return result;
}


JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1getNumTilesInTray
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint player )
{
    jint result;
    XWJNI_START();
    XP_ASSERT( !!state->game.model );
    result = model_getNumTilesInTray( state->game.model, player );
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_model_1getPlayersLastScore
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint player, jobject jlmi )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.model );
    LastMoveInfo lmi;
    XP_Bool valid = model_getPlayersLastScore( state->game.model, 
                                               player, &lmi );
    setBool( env, jlmi, "isValid", valid );
    if ( valid ) {
        setInt( env, jlmi, "score", lmi.score );
        setInt( env, jlmi, "nTiles", lmi.nTiles );
        setInt( env, jlmi, "moveType", lmi.moveType );
        setString( env, jlmi, "name", lmi.name );
        setString( env, jlmi, "word", lmi.word );
    }
    XWJNI_END();
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1writeFinalScores
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jstring result;
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    server_writeFinalScores( state->game.server, stream );
    result = streamToJString( env, stream );
    stream_destroy( stream );
    XWJNI_END();
    return result;
}

void
and_send_on_close( XWStreamCtxt* stream, void* closure )
{
    AndGameGlobals* globals = (AndGameGlobals*)closure;
    JNIState* state = (JNIState*)globals->state;

    XP_ASSERT( !!state->game.comms );
    comms_send( state->game.comms, stream );
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1initClientConnection
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    LOG_FUNC();
    XWJNI_START_GLOBALS();
    XWStreamCtxt* stream = and_empty_stream( MPPARM(mpool) globals );
    stream_setOnCloseProc( stream, and_send_on_close );
    result = server_initClientConnection( state->game.server, stream );
    XWJNI_END();
    LOG_RETURNF( "%s", boolToStr(result) );
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1start
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    CommsCtxt* comms = state->game.comms;
    if ( !!comms ) {
        comms_start( comms );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1stop
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    CommsCtxt* comms = state->game.comms;
    if ( !!comms ) {
        comms_stop( comms );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1resetSame
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    if ( !!state->game.comms ) {
        comms_resetSame( state->game.comms );
    }
    XWJNI_END();
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getAddr
(JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jaddr )
{
    XWJNI_START();
    XP_ASSERT( state->game.comms );
    CommsAddrRec addr;
    comms_getAddr( state->game.comms, &addr );
    setJAddrRec( env, jaddr, &addr );
    XWJNI_END();
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getAddrs
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jobjectArray result = NULL;
    XWJNI_START();
    XP_ASSERT( state->game.comms );
    if ( !!state->game.comms ) {
        CommsAddrRec addrs[MAX_NUM_PLAYERS];
        XP_U16 count = VSIZE(addrs);
        comms_getAddrs( state->game.comms, addrs, &count );

        jclass clas = (*env)->FindClass( env, PKG_PATH("jni/CommsAddrRec") );
        result = (*env)->NewObjectArray( env, count, clas, NULL );

        jmethodID initId = (*env)->GetMethodID( env, clas, "<init>", "()V" );
        for ( int ii = 0; ii < count; ++ii ) {
            jobject jaddr = (*env)->NewObject( env, clas, initId );
            setJAddrRec( env, jaddr, &addrs[ii] );
            (*env)->SetObjectArrayElement( env, result, ii, jaddr );
            deleteLocalRef( env, jaddr );
        }
        deleteLocalRef( env, clas );
    }
    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1setAddr
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jaddr )
{
    XWJNI_START();
    if ( state->game.comms ) {
        CommsAddrRec addr = {0};
        getJAddrRec( env, &addr, jaddr );
        comms_setAddr( state->game.comms, &addr );
    } else {
        XP_LOGF( "%s: no comms this game", __func__ );
    }
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1receiveMessage
( JNIEnv* env, jclass C, GamePtrType gamePtr, jbyteArray jstream, jobject jaddr )
{
    jboolean result;
    XWJNI_START_GLOBALS();

    XWStreamCtxt* stream = streamFromJStream( MPPARM(mpool) env, globals->vtMgr,
                                              jstream );
    CommsAddrRec* addrp = NULL;
    CommsAddrRec addr = {0};
    XP_ASSERT( !!jaddr );
    if ( NULL != jaddr ) {
        getJAddrRec( env, &addr, jaddr );
        addrp = &addr;
    }

    result = game_receiveMessage( &state->game, stream, addrp );

    stream_destroy( stream );

    XWJNI_END();
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1summarize
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jsummary )
{
    XWJNI_START();
    ModelCtxt* model = state->game.model;
    XP_S16 nMoves = model_getNMoves( model );
    setInt( env, jsummary, "nMoves", nMoves );
    XP_Bool gameOver = server_getGameIsOver( state->game.server );
    setBool( env, jsummary, "gameOver", gameOver );
    XP_Bool isLocal = XP_FALSE;
    setInt( env, jsummary, "turn", 
            server_getCurrentTurn( state->game.server, &isLocal ) );
    setBool( env, jsummary, "turnIsLocal", isLocal );
    setInt( env, jsummary, "lastMoveTime", 
            server_getLastMoveTime(state->game.server) );
    
    if ( !!state->game.comms ) {
        CommsAddrRec addr;
        CommsCtxt* comms = state->game.comms;
        comms_getAddr( comms, &addr );
        setInt( env, jsummary, "seed", comms_getChannelSeed( comms ) );
        setInt( env, jsummary, "missingPlayers", 
                server_getMissingPlayers( state->game.server ) );
        setInt( env, jsummary, "nPacketsPending", 
                comms_countPendingPackets( state->game.comms ) );

        setTypeSetFieldIn( env, &addr, jsummary, "conTypes" );

        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( &addr, &typ, &st ); ) {
            switch( typ ) {
            case COMMS_CONN_RELAY: {
                XP_UCHAR buf[128];
                XP_U16 len = VSIZE(buf);
                if ( comms_getRelayID( comms, buf, &len ) ) {
                    XP_ASSERT( '\0' == buf[len-1] ); /* failed! */
                    setString( env, jsummary, "relayID", buf );
                }
                setString( env, jsummary, "roomName", addr.u.ip_relay.invite );
            }
                break;
            case COMMS_CONN_NFC:
                break;
#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_SMS || defined XWFEATURE_P2P
            case COMMS_CONN_BT:
            case COMMS_CONN_P2P:
            case COMMS_CONN_SMS: {
                CommsAddrRec addrs[MAX_NUM_PLAYERS];
                XP_U16 count = VSIZE(addrs);
                comms_getAddrs( comms, addrs, &count );
            
                const XP_UCHAR* addrps[count];
                for ( int ii = 0; ii < count; ++ii ) {
                    switch ( typ ) {
                    case COMMS_CONN_BT: addrps[ii] = (XP_UCHAR*)&addrs[ii].u.bt.btAddr; break;
                    case COMMS_CONN_P2P: addrps[ii] = (XP_UCHAR*)&addrs[ii].u.p2p.mac_addr; break;
                    case COMMS_CONN_SMS: addrps[ii] = (XP_UCHAR*)&addrs[ii].u.sms.phone; break;
                    default: XP_ASSERT(0); break;
                    }
                    XP_LOGF( "%s: adding btaddr/phone/mac %s", __func__, addrps[ii] );
                }
                jobjectArray jaddrs = makeStringArray( env, count, addrps );
                setObject( env, jsummary, "remoteDevs", "[Ljava/lang/String;", 
                           jaddrs );
                deleteLocalRef( env, jaddrs );
            }
                break;
#endif
            default:
                XP_ASSERT(0);
            }
        }
    }

    XP_U16 nPlayers = model_getNPlayers( model );
    jint jvals[nPlayers];
    if ( gameOver ) {
        ScoresArray scores;
        model_figureFinalScores( model, &scores, NULL );
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            jvals[ii] = scores.arr[ii];
        }
    } else {
        for ( int ii = 0; ii < nPlayers; ++ii ) {
            jvals[ii] = model_getPlayerScore( model, ii );
        }
    }
    jintArray jarr = makeIntArray( env, nPlayers, jvals, sizeof(jvals[0]) );
    setObject( env, jsummary, "scores", "[I", jarr );
    deleteLocalRef( env, jarr );

    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1server_1prefsChanged
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jcp )
{
    jboolean result;
    XWJNI_START();

    CommonPrefs cp;
    loadCommonPrefs( env, &cp, jcp );

    result = board_prefsChanged( state->game.board, &cp );
    server_prefsChanged( state->game.server, &cp );

    XWJNI_END();
    return result;
}

#ifdef KEYBOARD_NAV
JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1getFocusOwner
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jint result;
    XWJNI_START();
    result = board_getFocusOwner( state->game.board );
    XWJNI_END();
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1focusChanged
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint typ )
{
    jboolean result;
    XWJNI_START();
    result = board_focusChanged( state->game.board, typ, XP_TRUE );
    XWJNI_END();
    return result;
}
#endif

#ifdef KEYBOARD_NAV
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1handleKey
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jkey, jboolean jup, 
  jbooleanArray jhandled )
{
    jboolean result;
    XWJNI_START();

    XP_Bool tmpbool;
    XP_Key key = jEnumToInt( env, jkey );
    if ( jup ) {
        result = board_handleKeyUp( state->game.board, key, &tmpbool );
    } else {
        result = board_handleKeyDown( state->game.board, key, &tmpbool );
    }
    jboolean jbool = tmpbool;
    setBoolArray( env, jhandled, 1, &jbool );
    XWJNI_END();
    return result;
}
#endif

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1getGi
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgi )
{
    XWJNI_START_GLOBALS();
    setJGI( env, jgi, globals->gi );
    XWJNI_END();
}

static const SetInfo gsi_ints[] = {
    ARR_MEMBER( GameStateInfo, visTileCount ),
    ARR_MEMBER( GameStateInfo, nPendingMessages ),
    ARR_MEMBER( GameStateInfo, trayVisState ),
};
static const SetInfo gsi_bools[] = {
    ARR_MEMBER( GameStateInfo,canHint ),
    ARR_MEMBER( GameStateInfo, canUndo ),
    ARR_MEMBER( GameStateInfo, canRedo ),
    ARR_MEMBER( GameStateInfo, inTrade ),
    ARR_MEMBER( GameStateInfo, tradeTilesSelected ),
    ARR_MEMBER( GameStateInfo, canChat ),
    ARR_MEMBER( GameStateInfo, canShuffle ),
    ARR_MEMBER( GameStateInfo, curTurnSelected ),
    ARR_MEMBER( GameStateInfo, canHideRack ),
    ARR_MEMBER( GameStateInfo, canTrade ),
};

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1getState
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgsi )
{
    XWJNI_START();
    GameStateInfo info;
    game_getState( &state->game, &info );

    setInts( env, jgsi, (void*)&info, AANDS(gsi_ints) );
    setBools( env, jgsi, (void*)&info, AANDS(gsi_bools) );

    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1hasComms
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = NULL != state->game.comms;
    XWJNI_END();
    return result;
}

#ifdef XWFEATURE_CHANGEDICT
JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_game_1changeDict
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jgi, jstring jname, 
  jbyteArray jDictBytes, jstring jpath )
{
    XWJNI_START_GLOBALS();
    DictionaryCtxt* dict = makeDict( MPPARM(state->globalJNI->mpool) env, 
                                     state->globalJNI->dictMgr, 
                                     globals->jniutil, jname, jDictBytes, 
                                     jpath, NULL, false );
    game_changeDict( MPPARM(mpool) &state->game, globals->gi, dict );
    dict_unref( dict );
    setJGI( env, jgi, globals->gi );
    XWJNI_END();
    return XP_FALSE;            /* no need to redraw */
}
#endif

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1resendAll
( JNIEnv* env, jclass C, GamePtrType gamePtr, jboolean force, jobject jFilter,
  jboolean thenAck )
{
    jint result;
    XWJNI_START();
    CommsCtxt* comms = state->game.comms;
    XP_ASSERT( !!comms );
    CommsConnType filter =
        NULL == jFilter ? COMMS_CONN_NONE : jEnumToInt( env, jFilter );
    result = comms_resendAll( comms, filter, force );
    if ( thenAck ) {
#ifdef XWFEATURE_COMMSACK
        comms_ackAny( comms );
#endif
    }
    XWJNI_END();
    return result;
}

typedef struct _GotOneClosure {
    JNIEnv* env;
    jbyteArray msgs[16];
    int count;
} GotOneClosure;

static void
onGotOne( void* closure, XP_U8* msg, XP_U16 len, MsgID XP_UNUSED(msgID) )
{
    GotOneClosure* goc = (GotOneClosure*)closure;
    if ( goc->count < VSIZE(goc->msgs) ) {
        jbyteArray arr = makeByteArray( goc->env, len, (const jbyte*)msg );
        goc->msgs[goc->count++] = arr;
    } else {
        XP_ASSERT( 0 );
    }
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getPending
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jobjectArray result = NULL;
    XWJNI_START();
    GotOneClosure goc = { .env = env, .count = 0 };
    XP_ASSERT( !!state->game.comms );
    comms_getPending( state->game.comms, onGotOne, &goc );

    result = makeByteArrayArray( env, goc.count );
    for ( int ii = 0; ii < goc.count; ++ii ) {
        (*env)->SetObjectArrayElement( env, result, ii, goc.msgs[ii] );
        deleteLocalRef( env, goc.msgs[ii] );
    }

    XWJNI_END();
    return result;
}

#ifdef XWFEATURE_COMMSACK
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1ackAny
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.comms );
    (void)comms_ackAny( state->game.comms );
    XWJNI_END();
}
#endif

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1transportFailed
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject failedTyp )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.comms );

    CommsConnType typ = jEnumToInt( env, failedTyp );
    (void)comms_transportFailed( state->game.comms, typ );
    XWJNI_END();
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1isConnected
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jboolean result;
    XWJNI_START();
    result = NULL != state->game.comms && comms_isConnected( state->game.comms );
    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1formatRelayID
( JNIEnv* env, jclass C, GamePtrType gamePtr, jint indx )
{
    jstring result = NULL;
    XWJNI_START();

    XP_UCHAR buf[64];
    XP_U16 len = sizeof(buf);
    if ( comms_formatRelayID( state->game.comms, indx, buf, &len ) ) {
        XP_ASSERT( len < sizeof(buf) );
        LOG_RETURNF( "%s", buf );
        result = (*env)->NewStringUTF( env, buf );
    }

    XWJNI_END();
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getStats
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    jstring result = NULL;
#ifdef DEBUG
    XWJNI_START_GLOBALS();
    if ( NULL != state->game.comms ) {
        XWStreamCtxt* stream = mem_stream_make( MPPARM(mpool) globals->vtMgr,
                                                NULL, 0, NULL );
        comms_getStats( state->game.comms, stream );
        result = streamToJString( env, stream );
        stream_destroy( stream );
    }
    XWJNI_END();
#endif
    return result;
}

#ifdef DEBUG
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1setAddrDisabled
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jConnTyp,
  jboolean forSend, jboolean val )
{
    XWJNI_START();
    if ( NULL != state->game.comms ) {
        CommsConnType connType = jEnumToInt( env, jConnTyp );
        comms_setAddrDisabled( state->game.comms, connType, forSend, val );
    }
    XWJNI_END();
}
#endif

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_comms_1getAddrDisabled
( JNIEnv* env, jclass C, GamePtrType gamePtr, jobject jConnTyp,
  jboolean forSend )
{
    jboolean result = XP_FALSE;
#ifdef DEBUG
    XWJNI_START();
    if ( NULL != state->game.comms ) {
        CommsConnType connType = jEnumToInt( env, jConnTyp );
        result = comms_getAddrDisabled( state->game.comms, connType, forSend );
    }
    XWJNI_END();
#endif
    return result;
}

JNIEXPORT jboolean JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_haveEnv
( JNIEnv* env, jclass C, jlong jniGlobalPtr )
{
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
    jboolean result = NULL != prvEnvForMe(&globalState->ti);
    return result;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_server_1endGame
( JNIEnv* env, jclass C, GamePtrType gamePtr )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.server );
    server_endGame( state->game.server );
    XWJNI_END();
}

#ifdef XWFEATURE_CHAT
JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_board_1sendChat
( JNIEnv* env, jclass C, GamePtrType gamePtr, jstring jmsg )
{
    XWJNI_START();
    XP_ASSERT( !!state->game.server );
    const char* msg = (*env)->GetStringUTFChars( env, jmsg, NULL );
    board_sendChat( state->game.board, msg );
    (*env)->ReleaseStringUTFChars( env, jmsg, msg );
    XWJNI_END();
}
#endif

#ifdef XWFEATURE_WALKDICT
////////////////////////////////////////////////////////////
// Dict iterator
////////////////////////////////////////////////////////////

typedef struct _DictIterData {
    JNIEnv* env;
    JNIGlobalState* globalState;
    JNIUtilCtxt* jniutil;
    VTableMgr* vtMgr;
    DictionaryCtxt* dict;
    DictIter iter;
    IndexData idata;
    XP_U16 depth;
#ifdef MEM_DEBUG
    MemPoolCtx* mpool;
#endif
} DictIterData;

static void makeIndex( DictIterData* data );
static void freeIndices( DictIterData* data );

JNIEXPORT jlong JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1init
( JNIEnv* env, jclass C, jlong jniGlobalPtr, jbyteArray jDictBytes,
  jstring jname, jstring jpath )
{
    jlong closure = 0;
    JNIGlobalState* globalState = (JNIGlobalState*)jniGlobalPtr;
    MAP_THREAD( &globalState->ti, env );

    DictionaryCtxt* dict = makeDict( MPPARM(globalState->mpool) env,
                                     globalState->dictMgr, globalState->jniutil,
                                     jname, jDictBytes, jpath, NULL, false );
    if ( !!dict ) {
        DictIterData* data = XP_CALLOC( globalState->mpool, sizeof(*data) );
        data->env = env;
        data->globalState = globalState;
        data->vtMgr = make_vtablemgr( MPPARM_NOCOMMA(globalState->mpool) );
        data->jniutil = globalState->jniutil;
        data->dict = dict;
        data->depth = 2;
#ifdef MEM_DEBUG
        data->mpool = globalState->mpool;
#endif
        closure = (jlong)data;
    }
    return closure;
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1setMinMax
( JNIEnv* env, jclass C, jlong closure, jint min, jint max )
{
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        dict_initIter( &data->iter, data->dict, min, max );
        makeIndex( data );
        (void)dict_firstWord( &data->iter );
    }
}

JNIEXPORT void JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1destroy
( JNIEnv* env, jclass C, jlong closure )
{
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
#ifdef MEM_DEBUG
        MemPoolCtx* mpool = data->mpool;
#endif

        dict_unref( data->dict );
        freeIndices( data );

        MAP_REMOVE( &data->globalState->ti, env );

        vtmgr_destroy( MPPARM(mpool) data->vtMgr );
        XP_FREE( mpool, data );
    }
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1wordCount
(JNIEnv* env, jclass C, jlong closure )
{
    jint result = 0;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        result = data->iter.nWords;
    }
    return result;
}

JNIEXPORT jintArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getCounts
(JNIEnv* env, jclass C, jlong closure )
{
    jintArray result = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        DictIter iter;
        dict_initIter( &iter, data->dict, 0, MAX_COLS_DICT );

        LengthsArray lens;
        if ( 0 < dict_countWords( &iter, &lens ) ) {
            XP_ASSERT( sizeof(jint) == sizeof(lens.lens[0]) );
            result = makeIntArray( env, VSIZE(lens.lens), (jint*)&lens.lens,
                                   sizeof(lens.lens[0]) );
        }
    }
    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getPrefixes
( JNIEnv* env, jclass C, jlong closure )
{
    jobjectArray result = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data && NULL != data->idata.prefixes ) {
        result = makeStringArray( env, data->idata.count, NULL );

        XP_U16 depth = data->depth;
        for ( int ii = 0; ii < data->idata.count; ++ii ) {
            XP_UCHAR buf[16];
            (void)dict_tilesToString( data->dict, 
                                      &data->idata.prefixes[depth*ii], 
                                      depth, buf, VSIZE(buf) );
            jstring jstr = (*env)->NewStringUTF( env, buf );
            (*env)->SetObjectArrayElement( env, result, ii, jstr );
            deleteLocalRef( env, jstr );
        }
    }
    return result;
}

JNIEXPORT jintArray JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getIndices
( JNIEnv* env, jclass C, jlong closure )
{
    jintArray jindices = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        XP_ASSERT( !!data->idata.indices );
        XP_ASSERT( sizeof(jint) == sizeof(data->idata.indices[0]) );
        jindices = makeIntArray( env, data->idata.count, 
                                 (jint*)data->idata.indices,
                                 sizeof(data->idata.indices[0]) );
    }
    return jindices;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1nthWord
( JNIEnv* env, jclass C, jlong closure, jint nn)
{
    jstring result = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        if ( dict_getNthWord( &data->iter, nn, data->depth, &data->idata ) ) {
            XP_UCHAR buf[64];
            dict_wordToString( &data->iter, buf, VSIZE(buf) );
            result = (*env)->NewStringUTF( env, buf );
        }
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getStartsWith
( JNIEnv* env, jclass C, jlong closure, jstring jprefix )
{
    jint result = -1;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        const char* prefix = (*env)->GetStringUTFChars( env, jprefix, NULL );
        if ( 0 <= dict_findStartsWith( &data->iter, prefix ) ) {
            result = dict_getPosition( &data->iter );
        }
        (*env)->ReleaseStringUTFChars( env, jprefix, prefix );
    }
    return result;
}

JNIEXPORT jstring JNICALL
Java_org_eehouse_android_xw4_jni_XwJNI_dict_1iter_1getDesc
( JNIEnv* env, jclass C, jlong closure )
{
    jstring result = NULL;
    DictIterData* data = (DictIterData*)closure;
    if ( NULL != data ) {
        const XP_UCHAR* disc = dict_getDesc( data->dict );
        if ( NULL != disc && '\0' != disc[0] ) {
            result = (*env)->NewStringUTF( env, disc );
        }
    }
    return result;
}

static void
freeIndices( DictIterData* data )
{
    IndexData* idata = &data->idata;
    if ( !!idata->prefixes ) {
        XP_FREE( data->mpool, idata->prefixes );
        idata->prefixes = NULL;
    }
    if( !!idata->indices ) {
        XP_FREE( data->mpool, idata->indices );
        idata->indices = NULL;
    }
}

static void
makeIndex( DictIterData* data )
{
    XP_U16 nFaces = dict_numTileFaces( data->dict );
    XP_U16 ii;
    XP_U16 count;
    for ( count = 1, ii = 0; ii < data->depth; ++ii ) {
        count *= nFaces;
    }

    freeIndices( data );

    IndexData* idata = &data->idata;
    idata->prefixes = XP_MALLOC( data->mpool, count * data->depth
                                 * sizeof(*idata->prefixes) );
    idata->indices = XP_MALLOC( data->mpool,
                                count * sizeof(*idata->indices) );
    idata->count = count;

    dict_makeIndex( &data->iter, data->depth, idata );
    if ( 0 < idata->count ) {
        idata->prefixes = XP_REALLOC( data->mpool, idata->prefixes,
                                      idata->count * data->depth *
                                      sizeof(*idata->prefixes) );
        idata->indices = XP_REALLOC( data->mpool, idata->indices,
                                     idata->count * sizeof(*idata->indices) );
    } else {
        freeIndices( data );
    }
} /* makeIndex */

#endif  /* XWFEATURE_BOARDWORDS */
