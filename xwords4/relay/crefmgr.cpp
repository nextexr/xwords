/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option.
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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "crefmgr.h"
#include "cref.h"
#include "mlock.h"
#include "configs.h"
#include "timermgr.h"

class SocketStuff {
 public:
    SocketStuff( CookieRef* cref )
        : m_cref(cref)
        {        
            pthread_mutex_init( &m_writeMutex, NULL );
        }
    ~SocketStuff() { pthread_mutex_destroy( &m_writeMutex ); }
    CookieRef* m_cref;
    pthread_mutex_t m_writeMutex; /* so only one thread writes at a time */
};

static CRefMgr* s_instance = NULL;

/* static */ CRefMgr*
CRefMgr::Get() 
{
    if ( s_instance == NULL ) {
        s_instance = new CRefMgr();
    }
    return s_instance;
} /* Get */

CRefMgr::CRefMgr()
    : m_nRoomsFilled(0)
    , m_startTime(time(NULL))
{
    /* should be using pthread_once() here */
    /* pthread_mutex_init( &m_SocketStuffMutex, NULL ); */
    pthread_mutex_init( &m_roomsFilledMutex, NULL );
    pthread_mutex_init( &m_freeList_mutex, NULL );
    pthread_rwlock_init( &m_cookieMapRWLock, NULL );
    m_db = DBMgr::Get();
    m_cidlock = CidLock::GetInstance();
}

CRefMgr::~CRefMgr()
{
    assert( this == s_instance );

    delete m_db;
    delete m_cidlock;

    pthread_mutex_destroy( &m_freeList_mutex );
    pthread_rwlock_destroy( &m_cookieMapRWLock );

    s_instance = NULL;
}

void
CRefMgr::CloseAll()
{
    /* Get every cref instance, shut it down */

    for ( ; ; ) {
        CookieRef* cref = NULL;
        {
            RWWriteLock rwl( &m_cookieMapRWLock );
            CookieMap::iterator iter = m_cookieMap.begin();
            if ( iter == m_cookieMap.end() ) {
                break;
            }
            cref = iter->second; 
            {
                SafeCref scr( cref->GetCid(), false ); /* cref */
                scr.Shutdown();
            }
        }
    }
} /* CloseAll */

void 
CRefMgr::IncrementFullCount( void )
{
    MutexLock ml( &m_roomsFilledMutex );
    ++m_nRoomsFilled;
}

int 
CRefMgr::GetNumRoomsFilled( void )
{
    MutexLock ml(&m_roomsFilledMutex);
    return m_nRoomsFilled;
}

int 
CRefMgr::GetSize( void )
{
    return m_cookieMap.size();
}

void
CRefMgr::GetStats( CrefMgrInfo& mgrInfo )
{
    mgrInfo.m_nRoomsFilled = GetNumRoomsFilled();
    mgrInfo.m_startTimeSpawn = m_startTime;

    if ( 0 == m_ports.length() ) {
        RelayConfigs* cfg = RelayConfigs::GetConfigs();
        vector<int> ints;
        if ( cfg->GetValueFor( "GAME_PORTS", ints ) ) {
            vector<int>::const_iterator iter;
            for ( iter = ints.begin(); ; ) {
                char buf[8];
                snprintf( buf, sizeof(buf), "%d", *iter );
                m_ports += buf;
                ++iter;
                if ( iter == ints.end() ) {
                    break;
                }
                m_ports += ",";
            }
        }
    }
    mgrInfo.m_ports = m_ports.c_str();

    RWReadLock rwl( &m_cookieMapRWLock );
    mgrInfo.m_nCrefsCurrent = m_cookieMap.size();

    CookieMap::iterator iter;
    for ( iter = m_cookieMap.begin(); iter != m_cookieMap.end(); ++iter ) {
        CookieRef* cref = iter->second;

        CrefInfo info;
        info.m_cookie = cref->Cookie();
        info.m_connName = cref->ConnName();
        info.m_cid = cref->GetCid();
        info.m_curState = cref->CurState();
        info.m_nPlayersSought = cref->GetPlayersSought();
        info.m_nPlayersHere = cref->GetPlayersHere();
        info.m_startTime = cref->GetStarttime();
        info.m_langCode = cref->GetLangCode();
        
        SafeCref sc(cref->GetCid(), false );
        sc.GetHostsConnected( &info.m_hostsIds, &info.m_hostSeeds, 
                              &info.m_hostIps );
        
        mgrInfo.m_crefInfo.push_back( info );
    }
}

CookieID
CRefMgr::cookieIDForConnName( const char* connName )
{
    CookieID cid = 0;
    /* for now, just walk the existing data structure and see if the thing's
       in use.  If it isn't, return a new id. */

    RWReadLock rwl( &m_cookieMapRWLock );

    CookieMap::iterator iter = m_cookieMap.begin();
    while ( iter != m_cookieMap.end() ) {
        CookieRef* cref = iter->second;
        if ( 0 == strcmp( cref->ConnName(), connName ) ) {
            cid = iter->first;
            break;
        }
        ++iter;
    }

    return cid;
} /* cookieIDForConnName */

void
CRefMgr::addToFreeList( CookieRef* cref )
{
    MutexLock ml( &m_freeList_mutex );
    m_freeList.push_back( cref );
}

CookieRef*
CRefMgr::getFromFreeList( void )
{
    CookieRef* cref = NULL;
    MutexLock ml( &m_freeList_mutex );
    if ( m_freeList.size() > 0 ) {
        cref = m_freeList.front();
        m_freeList.pop_front();
    }
    return cref;
}

/* connect case */
CidInfo*
CRefMgr::getMakeCookieRef( const char* cookie, int nPlayersH, int nPlayersT, 
                           int langCode, int seed, int clientIndx, 
                           bool wantsPublic, bool makePublic, bool* seenSeed )
{
    CidInfo* cinfo = NULL;

    /* We have a cookie from a new connection or from a reconnect.  This may
       be the first time it's been seen, or there may be a game currently in
       the XW_ST_CONNECTING state, or it may be a dupe of a connect packet on
       the same or a different socket.  If there's a game, cool.  Otherwise add
       a new one.  Pass the connName which will be used if set, but if not set
       we'll be generating another later when the game is complete.
    */
    for ( int ii = 0; ; ++ii ) {
        /* What's this for loop thing?  It's to fix a race condition.  One
           thread has "claim" on cid <N>, which is in the DB.  Another comes
           into this function and looks it up in the DB, retrieving <N>, but
           progress is blocked inside getCookieRef_impl which calls Claim().
           The first thread winds up removing <N> from the DB and deleting its
           cref before calling Relinquish so that when Claim() returns there's
           no cref.  So we test for that case and retry. */

        /* I'm now seeing an infinte loop here. Until it's tracked down, let's
           assert out. Note that I've seen it here, not at any of the other
           places where I'm replacing FOREVER loops with this test*/
        if ( ii > 5 ) {
            assert(0);
            break;
        }

        CookieID cid;
        char connNameBuf[MAX_CONNNAME_LEN+1] = {0};
        int alreadyHere = 0;

        *seenSeed = m_db->SeenSeed( cookie, seed, langCode, nPlayersT, 
                                    wantsPublic, connNameBuf, 
                                    sizeof(connNameBuf), &alreadyHere, &cid );
        if ( !*seenSeed ) {
            cid = m_db->FindOpen( cookie, langCode, nPlayersT, nPlayersH, 
                                  wantsPublic, connNameBuf, sizeof(connNameBuf), 
                                  &alreadyHere );
        }

        if ( cid > 0 ) {
            cinfo = m_cidlock->Claim( cid );
            if ( NULL == cinfo->GetRef() ) {
                m_cidlock->Relinquish( cinfo, true );
                continue;
            } else if ( *seenSeed ) { /* this my home? */
                logf( XW_LOGINFO, "%s: *seenSeed case", __func__ );
                break;
            } else if ( !cinfo->GetRef()->HaveRoom( nPlayersH ) ) {
                m_cidlock->Relinquish( cinfo, false );
                continue;
            }
        } else {
            cinfo = m_cidlock->Claim();
            cid = cinfo->GetCid();
            CookieRef* cref = AddNew( cookie, connNameBuf, cid, langCode, 
                                      nPlayersT, alreadyHere );
            cinfo->SetRef( cref );
            if ( !connNameBuf[0] ) { /* didn't exist in DB */
                m_db->AddNew( cookie, cref->ConnName(), cid, langCode, nPlayersT, 
                              wantsPublic || makePublic );
            } else {
                if ( !m_db->AddCID( connNameBuf, cid ) ) {
                    m_cidlock->Relinquish( cinfo, true );
                    continue;
                }
            }
        }
        break;
    }
    assert( cinfo->GetRef() );
    return cinfo;
} /* getMakeCookieRef */

/* reconnect case */
CidInfo*
CRefMgr::getMakeCookieRef( const char* connName, const char* cookie,
                           HostID hid, int nPlayersH, int nPlayersS, int seed, 
                           int langCode, bool isPublic, bool* isDead )
{
    CookieRef* cref = NULL;
    CidInfo* cinfo = NULL;

    for ( int ii = 0; ; ++ii ) {     /* for: see comment above */

        if ( ii > 5 ) {
            assert(0);
            break;
        }

        /* fetch these from DB */
        char curCookie[MAX_INVITE_LEN+1];
        int curLangCode;
        int nAlreadyHere = nPlayersH;

        CookieID cid;
        if ( !m_db->FindGameFor( connName, curCookie, sizeof(curCookie),
                                 seed, hid, nPlayersH, nPlayersS,
                                 &curLangCode, isDead, &cid ) ) {
            break;
        }

        if ( 0 != cid ) {           /* already open */
            cinfo = m_cidlock->Claim( cid );
            if ( NULL == cinfo->GetRef() ) {
                m_cidlock->Relinquish( cinfo, true );
                continue;
            }
        } else {
            /* The entry may not even be in the DB, e.g. if it got deleted.
               Deal with that possibility by taking the caller's word for it. */
            cinfo = m_cidlock->Claim();
            cid = cinfo->GetCid();

            if ( !m_db->AddCID( connName, cid ) ) {
                m_cidlock->Relinquish( cinfo, true );
                continue;
            }
            cookie = curCookie;

            cref = AddNew( cookie, connName, cid, curLangCode, nPlayersS, 
                           nAlreadyHere );
            cinfo->SetRef( cref );
        }
        break;
    } /* for */
    assert( NULL == cinfo || cinfo->GetRef() );
    return cinfo;
} /* getMakeCookieRef */

CidInfo*
CRefMgr::getMakeCookieRef( const char* const connName, HostID hid, bool* isDead )
{
    CookieRef* cref = NULL;
    CidInfo* cinfo = NULL;
    char curCookie[MAX_INVITE_LEN+1];
    int curLangCode;
    int nPlayersT = 0;
    int nAlreadyHere = 0;

    for ( int ii = 0; ; ++ii ) {     /* for: see comment above */
        if ( ii > 5 ) {
            assert(0);
            break;
        }

        CookieID cid = m_db->FindGame( connName, hid, curCookie, sizeof(curCookie),
                                       &curLangCode, &nPlayersT, &nAlreadyHere,
                                       isDead );
        if ( 0 != cid ) {           /* already open */
            cinfo = m_cidlock->Claim( cid );
            if ( NULL == cinfo->GetRef() ) {
                m_cidlock->Relinquish( cinfo, true );
                continue;
            }
        } else {
            if ( nPlayersT == 0 ) { /* wasn't in the DB */
                /* do nothing; insufficient info to fake it */
            } else {
                cinfo = m_cidlock->Claim();
                if ( !m_db->AddCID( connName, cinfo->GetCid() ) ) {
                    m_cidlock->Relinquish( cinfo, true );
                    continue;
                }
                cref = AddNew( curCookie, connName, cinfo->GetCid(), curLangCode, 
                               nPlayersT, nAlreadyHere );
                cinfo->SetRef( cref );
            }
        }
        break;
    }
    return cinfo;
}

CidInfo*
CRefMgr::getMakeCookieRef( const AddrInfo::ClientToken clientToken, HostID srcID )
{
    CookieRef* cref = NULL;
    CidInfo* cinfo = NULL;
    char curCookie[MAX_INVITE_LEN+1];
    int curLangCode;
    int nPlayersT = 0;
    int nAlreadyHere = 0;

    for ( int ii = 0; ; ++ii ) {     /* for: see comment above */
        if ( ii > 5 ) {
            assert(0);
            break;
        }

        char connName[MAX_CONNNAME_LEN+1] = {0};
        CookieID cid = m_db->FindGame( clientToken, srcID,
                                       connName, sizeof(connName),
                                       curCookie, sizeof(curCookie),
                                       &curLangCode, &nPlayersT, &nAlreadyHere );
            // &seed );
        if ( 0 != cid ) {           /* already open */
            cinfo = m_cidlock->Claim( cid );
            if ( NULL == cinfo->GetRef() ) {
                m_cidlock->Relinquish( cinfo, true );
                continue;
            }
        } else if ( nPlayersT == 0 ) { /* wasn't in the DB */
                /* do nothing; insufficient info to fake it */
        } else {
            cinfo = m_cidlock->Claim();
            if ( !m_db->AddCID( connName, cinfo->GetCid() ) ) {
                m_cidlock->Relinquish( cinfo, true );
                continue;
            }
            logf( XW_LOGINFO, "%s(): added cid???", __func__ );
            cref = AddNew( curCookie, connName, cinfo->GetCid(), curLangCode,
                           nPlayersT, nAlreadyHere );
            cinfo->SetRef( cref );
        }
        break;
    }
    logf( XW_LOGINFO, "%s() => %p", __func__, cinfo );
    return cinfo;
}

void 
CRefMgr::RemoveSocketRefs( const AddrInfo* addr )
{
    {    
        SafeCref scr( addr );
        scr.Remove( addr );
    }
}

void
CRefMgr::PrintSocketInfo( int sock, string& out )
{
    SafeCref scr( sock );
    const char* name = scr.Cookie();
    if ( name != NULL && name[0] != '\0' ) {
        char buf[64];

        snprintf( buf, sizeof(buf), "* socket: %d\n", sock );
        out += buf;

        snprintf( buf, sizeof(buf), "  in cookie: %s\n", name );
        out += buf;
    }
}

CidInfo*
CRefMgr::getCookieRef( CookieID cid, bool failOk /* = false */ )
{
    CidInfo* cinfo = NULL;
    for ( int count = 0; ; ++count ) {
        cinfo = m_cidlock->Claim( cid );
        if ( NULL != cinfo->GetRef() ) { /* What's it mean to get a cinfo back but have it be empty??? */
            break;
        } else if ( failOk || count > 20 ) {
            break;
        }
        m_cidlock->Relinquish( cinfo, true );
        logf( XW_LOGINFO, "%s: (count=%d) sleeping after "
              "failing to get cinfo", __func__, count );
        usleep(200000);         /* 2/10 second */
        cinfo = NULL;
    }
    return cinfo;
} /* getCookieRef */

CidInfo*
CRefMgr::getCookieRef( const AddrInfo* addr )
{
    CidInfo* cinfo = m_cidlock->ClaimSocket( addr );

    assert( NULL == cinfo || NULL != cinfo->GetRef() );
    return cinfo;
} /* getCookieRef */

#ifdef RELAY_HEARTBEAT
/* static */ void
CRefMgr::heartbeatProc( void* closure )
{
    CRefMgr* self = (CRefMgr*)closure;
    self->checkHeartbeats( ::uptime() );
} /* heartbeatProc */
#endif

CookieRef*
CRefMgr::AddNew( const char* cookie, const char* connName, CookieID cid,
                 int langCode, int nPlayers, int nAlreadyHere )
{
    if ( 0 == connName[0] ) {
        connName = NULL;
    }
    /* PENDING: should this return a locked cref? */
    logf( XW_LOGINFO, "%s( cookie=%s, connName=%s, cid=%d)", __func__,
          cookie, connName, cid );

    CookieRef* ref = getFromFreeList();

    RWWriteLock rwl( &m_cookieMapRWLock );
    logf( XW_LOGINFO, "making new cref: %d", cid );
    
    if ( !!ref ) {
        ref->ReInit( cookie, connName, cid, langCode, nPlayers, nAlreadyHere );
    } else {
        ref = new CookieRef( cookie, connName, cid, langCode, nPlayers, 
                             nAlreadyHere );
    }

    ref->assignConnName();

    pair<CookieMap::iterator,bool> result =
        m_cookieMap.insert( pair<CookieID, CookieRef*>(ref->GetCid(), ref ) );
    assert( result.second );
    logf( XW_LOGINFO, "%s: paired cookie %s/connName %s with cid %d", __func__, 
          (cookie?cookie:"NULL"), connName, ref->GetCid() );

#ifdef RELAY_HEARTBEAT
    if ( m_cookieMap.size() == 1 ) {
        RelayConfigs* cfg = RelayConfigs::GetConfigs();
        int heartbeat;
        if ( cfg->GetValueFor( "HEARTBEAT", &heartbeat ) ) {
            TimerMgr::GetTimerMgr()->SetTimer( heartbeat, heartbeatProc, this,
                                               heartbeat );
        }
    }
#endif

    logf( XW_LOGINFO, "%s=>%p", __func__, ref );
    return ref;
} /* AddNew */

void
CRefMgr::Recycle_locked( CookieRef* cref )
{
    logf( XW_LOGINFO, "%s(cref=%p,cookie=%s)", __func__, cref, cref->Cookie() );
    CookieID cid = cref->GetCid();
    DBMgr::Get()->ClearCID( cref->ConnName() );
    cref->Clear();
    addToFreeList( cref );

    cref->Unlock();

    /* don't grab this lock until after releasing cref's lock; otherwise
       deadlock happens. */
    RWWriteLock rwl( &m_cookieMapRWLock );

    CookieMap::iterator iter = m_cookieMap.begin();
    while ( iter != m_cookieMap.end() ) {
        CookieRef* ref = iter->second;
        if ( ref == cref ) {
            logf( XW_LOGINFO, "%s: erasing cref cid %d", __func__, cid );
            m_cookieMap.erase( iter );
            break;
        }
        ++iter;
    }
    assert( iter != m_cookieMap.end() ); /* we found something */

#ifdef RELAY_HEARTBEAT
    if ( m_cookieMap.size() == 0 ) {
        TimerMgr::GetTimerMgr()->ClearTimer( heartbeatProc, this );
    }
#endif
} /* CRefMgr::Recycle */

void
CRefMgr::Recycle( CookieID cid )
{
    CidInfo* cinfo = getCookieRef( cid );
    if ( cinfo != NULL ) {
        CookieRef* cref = cinfo->GetRef();
        cref->Lock();
        Recycle_locked( cref );
    }
} /* Delete */

void
CRefMgr::Recycle( const char* connName )
{
    Recycle( cookieIDForConnName( connName ) );
} /* Delete */

#ifdef RELAY_HEARTBEAT
void
CRefMgr::checkHeartbeats( time_t now )
{
    vector<CookieRef*> crefs;

    {
        RWReadLock rwl( &m_cookieMapRWLock );
        CookieMap::iterator iter = m_cookieMap.begin();
        while ( iter != m_cookieMap.end() ) {
            crefs.push_back(iter->second);
            ++iter;
        }
    }

    unsigned int ii;
    for ( ii = 0; ii < crefs.size(); ++ii ) {
        SafeCref scr( crefs[ii] );
        scr.CheckHeartbeats( now );
    }
} /* checkHeartbeats */
#endif

time_t
CRefMgr::uptime( void )
{
    return time(NULL) - m_startTime;
}

/* static */ CookieMapIterator
CRefMgr::GetCookieIterator()
{
    CookieMapIterator iter(&m_cookieMapRWLock);
    return iter;
}


CookieMapIterator::CookieMapIterator(pthread_rwlock_t* rwlock)
    : m_rwl( rwlock )
    ,_iter( CRefMgr::Get()->m_cookieMap.begin() )
{
}

CookieID
CookieMapIterator::Next()
{
    CookieID cid = 0;
    if ( _iter != CRefMgr::Get()->m_cookieMap.end() ) {
        CookieRef* cref = _iter->second;
        cid = cref->GetCid();
        ++_iter;
    }
    return cid;
}

//////////////////////////////////////////////////////////////////////////////
// SafeCref
//////////////////////////////////////////////////////////////////////////////

/* connect case */
SafeCref::SafeCref( const char* cookie, const AddrInfo* addr, int clientVers,
                    DevID* devID, int nPlayersH, int nPlayersS, 
                    unsigned short gameSeed, int clientIndx, int langCode,
                    bool wantsPublic, bool makePublic )
    : m_cinfo( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_addr( *addr )
    , m_clientVersion( clientVers )
    , m_devID( devID )
    , m_isValid( false )
    , m_seenSeed( false )
{
    if ( playerCountsOk( nPlayersH, nPlayersS ) ) {
        CidInfo* cinfo;
        cinfo = m_mgr->getMakeCookieRef( cookie, nPlayersH, nPlayersS, 
                                         langCode, gameSeed, clientIndx, 
                                         wantsPublic, makePublic, &m_seenSeed );
        if ( cinfo != NULL ) {
            CookieRef* cref = cinfo->GetRef();
            m_locked = cref->Lock();
            m_cinfo = cinfo;
            m_isValid = true;
        }
    }
}

/* Reconnect case
 *
 * Device thinks it's connected, but we may disagree, e.g. if it sent an ACK
 * we didn't receive in time.  So we may actually wind up creating a new row,
 * with a new connname, in the games DB in response to this!
 *
 */
SafeCref::SafeCref( const char* connName, const char* cookie, HostID hid, 
                    const AddrInfo* addr, int clientVers, DevID* devID, 
                    int nPlayersH, int nPlayersS, unsigned short gameSeed, 
                    int clientIndx, int langCode, bool wantsPublic,
                    bool makePublic )
    : m_cinfo( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_addr( *addr )
    , m_clientVersion( clientVers )
    , m_devID( devID )
    , m_hid( hid )
    , m_isValid( false )
{
    if ( playerCountsOk( nPlayersH, nPlayersS ) && hid <= 4 ) {
        CidInfo* cinfo;

        bool isDead = false;
        cinfo = m_mgr->getMakeCookieRef( connName, cookie, hid, nPlayersH, 
                                         nPlayersS, gameSeed, langCode,
                                         wantsPublic || makePublic, &isDead );

        /* If the reconnect doesn't check out, treat it as a connect. But
           preserve the existing hid. If the DB was deleted it's important
           that devices keep their places (hids) */
        if ( NULL == cinfo ) {
            logf( XW_LOGINFO, "%s: taking a second crack; (cur hid: %d)",
                  __func__, hid );
            assert( m_hid == hid );
            // m_hid = HOST_ID_NONE; /* wrong; but why was I doing it? */
            cinfo = m_mgr->getMakeCookieRef( cookie, nPlayersH, nPlayersS, 
                                             langCode, gameSeed, clientIndx,
                                             wantsPublic, makePublic, &m_seenSeed );
        }
        if ( cinfo != NULL ) {
            assert( cinfo->GetCid() == cinfo->GetRef()->GetCid() );
            m_locked = cinfo->GetRef()->Lock();
            m_cinfo = cinfo;
            m_isValid = true;
            m_dead = isDead;
        }
    }
}

/* ConnName case -- must exist (unless DB record's been removed */
SafeCref::SafeCref( const char* const connName, HostID hid )
    : m_cinfo( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_isValid( false )
{
    bool isDead = false;
    CidInfo* cinfo = m_mgr->getMakeCookieRef( connName, hid, &isDead );
    if ( NULL != cinfo && NULL != cinfo->GetRef() ) {
        assert( cinfo->GetCid() == cinfo->GetRef()->GetCid() );
        m_locked = cinfo->GetRef()->Lock();
        m_cinfo = cinfo;
        m_isValid = true;
        m_dead = isDead;
    }
}

SafeCref::SafeCref( CookieID cid, bool failOk /* = false */ )
    : m_cinfo( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_isValid( false )
    , m_locked( false )
{
    CidInfo* cinfo = m_mgr->getCookieRef( cid, failOk );
    if ( cinfo != NULL ) {       /* known cookie? */
        CookieRef* cref = cinfo->GetRef();
        if ( NULL != cref ) {
            assert( cinfo->GetCid() == cref->GetCid() );
            m_locked = cref->Lock();
            m_isValid = m_locked && cid == cref->GetCid();
        }
        m_cinfo = cinfo;
    }
}

SafeCref::SafeCref( const AddrInfo* addr )
    : m_cinfo( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_addr( *addr )
    , m_isValid( false )
{
    CidInfo* cinfo = m_mgr->getCookieRef( addr );
    if ( cinfo != NULL ) {       /* known socket? */
        CookieRef* cref = cinfo->GetRef();
        assert( cinfo->GetCid() == cref->GetCid() );
        m_locked = cref->Lock();
        m_isValid = m_locked && cref->HasSocket_locked( addr );
        m_cinfo = cinfo;
    }
}

SafeCref::SafeCref( const AddrInfo::ClientToken clientToken, HostID srcID )
    : m_cinfo( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_isValid( false )
{
    CidInfo* cinfo = m_mgr->getMakeCookieRef( clientToken, srcID );
    if ( NULL != cinfo && NULL != cinfo->GetRef() ) {
        m_locked = cinfo->GetRef()->Lock();
        m_cinfo = cinfo;
        m_isValid = true;
    }
}

SafeCref::~SafeCref()
{
    if ( m_cinfo != NULL ) {
        bool recycle = true;
        if ( m_locked ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( m_cinfo->GetCid() == cref->GetCid() );
            recycle = cref->ShouldDie();
            if ( recycle ) {
                m_mgr->Recycle_locked( cref );
            } else {
                cref->Unlock();
            }
        }
        m_mgr->m_cidlock->Relinquish( m_cinfo, recycle );
    }
}

bool
SafeCref::playerCountsOk( int nPlayersH, int nPlayersT )
{
    bool result = ( 0 < nPlayersH && 4 >= nPlayersH
                    && 0 < nPlayersT && 4 >= nPlayersT
                    && nPlayersH < nPlayersT );
    if ( !result ) {
        logf( XW_LOGERROR, "%s: dropping with bad player counts: here: "
              "%d; total: %d", __func__, nPlayersH, nPlayersT );
    }
    return result;
}
