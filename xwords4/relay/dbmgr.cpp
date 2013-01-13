/* -*- compile-command: "make -k -j3"; -*- */

/* 
 * Copyright 2010-2012 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dbmgr.h"
#include "mlock.h"
#include "configs.h"
#include "xwrelay_priv.h"

#define GAMES_TABLE "games"
#define MSGS_TABLE "msgs"
#define DEVICES_TABLE "devices"

#define ARRAYSUM "sum_array(nPerDevice)"

static DBMgr* s_instance = NULL;

#define DELIM "\1"

static void formatParams( char* paramValues[], int nParams, const char* fmt, 
                          char* buf, int bufLen, ... );
static int here_less_seed( const char* seeds, int perDeviceSum, 
                           unsigned short seed );
static void destr_function( void* conn );

/* static */ DBMgr*
DBMgr::Get() 
{
    if ( s_instance == NULL ) {
        s_instance = new DBMgr();
    }
    return s_instance;
} /* Get */

DBMgr::DBMgr()
{
    logf( XW_LOGINFO, "%s called", __func__ );

    pthread_key_create( &m_conn_key, destr_function );

    /* Now figure out what the largest cid currently is.  There must be a way
       to get postgres to do this for me.... */
    /* const char* query = "SELECT cid FROM games ORDER BY cid DESC LIMIT 1"; */
    /* PGresult* result = PQexec( m_pgconn, query ); */
    /* if ( 0 == PQntuples( result ) ) { */
    /*     m_nextCID = 1; */
    /* } else { */
    /*     char* value = PQgetvalue( result, 0, 0 ); */
    /*     m_nextCID = 1 + atoi( value ); */
    /* } */
    /* PQclear(result); */
    /* logf( XW_LOGINFO, "%s: m_nextCID=%d", __func__, m_nextCID ); */

    // I've seen rand returning the same series several times....
    srand( time( NULL ) );
}
 
DBMgr::~DBMgr()
{
    assert( s_instance == this );
    s_instance = NULL;

    int err = pthread_key_delete( m_conn_key );
    logf( XW_LOGINFO, "%s: pthread_key_delete=>%d", __func__, err );
}

void
DBMgr::AddNew( const char* cookie, const char* connName, CookieID cid, 
               int langCode, int nPlayersT, bool isPublic )
{         
    if ( !cookie ) cookie = "";
    if ( !connName ) connName = "";
 
    const char* command = "INSERT INTO " GAMES_TABLE
        " (cid, room, connName, nTotal, lang, pub)"
        " VALUES( $1, $2, $3, $4, $5, $6 )";
    int nParams = 6;
    char* paramValues[nParams];
    char buf[512];
    formatParams( paramValues, nParams,
                  "%d"DELIM"%s"DELIM"%s"DELIM"%d"DELIM"%d"DELIM"%s", 
                  buf, sizeof(buf), cid, cookie, connName, nPlayersT, 
                  langCode, isPublic?"TRUE":"FALSE" );

    PGresult* result = PQexecParams( getThreadConn(), command,
                                     nParams, NULL,
                                     paramValues, 
                                     NULL, NULL, 0 );
    if ( PGRES_COMMAND_OK != PQresultStatus(result) ) {
        logf( XW_LOGERROR, "PQexec=>%s;%s", PQresStatus(PQresultStatus(result)), 
              PQresultErrorMessage(result) );
    }
    PQclear( result );
}

CookieID
DBMgr::FindGame( const char* connName, char* cookieBuf, int bufLen,
                 int* langP, int* nPlayersTP, int* nPlayersHP, bool* isDead )
{
    CookieID cid = 0;

    const char* fmt = "SELECT cid, room, lang, nTotal, nPerDevice, dead FROM " 
        GAMES_TABLE " WHERE connName = '%s'"
        " LIMIT 1";
    string query;
    string_printf( query, fmt, connName );
    logf( XW_LOGINFO, "query: %s", query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    if ( 1 == PQntuples( result ) ) {
        cid = atoi( PQgetvalue( result, 0, 0 ) );
        snprintf( cookieBuf, bufLen, "%s", PQgetvalue( result, 0, 1 ) );
        *langP = atoi( PQgetvalue( result, 0, 2 ) );
        *nPlayersTP = atoi( PQgetvalue( result, 0, 3 ) );
        *nPlayersHP = atoi( PQgetvalue( result, 0, 4 ) );
        *isDead = 't' == PQgetvalue( result, 0, 5 )[0];
    }
    PQclear( result );

    logf( XW_LOGINFO, "%s(%s)=>%d", __func__, connName, cid );
    return cid;
} /* FindGame */

bool
DBMgr::SeenSeed( const char* cookie, unsigned short seed,
                 int langCode, int nPlayersT, bool wantsPublic, 
                 char* connNameBuf, int bufLen, int* nPlayersHP, 
                 CookieID* cid )
{
    int nParams = 5;
    char* paramValues[nParams];
    char buf[512];
    formatParams( paramValues, nParams,
                  "%s"DELIM"%d"DELIM"%d"DELIM"%d"DELIM"%s", buf, sizeof(buf),
                  cookie, langCode, nPlayersT, seed, 
                  wantsPublic?"TRUE":"FALSE" );

    const char* cmd = "SELECT cid, connName, seeds, sum_array(nPerDevice) FROM "
        GAMES_TABLE
        " WHERE NOT dead"
        " AND room ILIKE $1"
        " AND lang = $2"
        " AND nTotal = $3"
        " AND $4 = ANY(seeds)"
        " AND $5 = pub"
        " ORDER BY ctime DESC"
        " LIMIT 1";

    PGresult* result = PQexecParams( getThreadConn(), cmd,
                                     nParams, NULL,
                                     paramValues, 
                                     NULL, NULL, 0 );
    bool found = 1 == PQntuples( result );
    if ( found ) {
        *cid = atoi( PQgetvalue( result, 0, 0 ) );
        *nPlayersHP = here_less_seed( PQgetvalue( result, 0, 2 ),
                                      atoi( PQgetvalue( result, 0, 3 ) ),
                                      seed );
        snprintf( connNameBuf, bufLen, "%s", PQgetvalue( result, 0, 1 ) );
    }
    PQclear( result );
    logf( XW_LOGINFO, "%s(%4X)=>%s", __func__, seed, found?"true":"false" );
    return found;
}

CookieID
DBMgr::FindOpen( const char* cookie, int lang, int nPlayersT, int nPlayersH,
                 bool wantsPublic, char* connNameBuf, int bufLen,
                 int* nPlayersHP )
{
    CookieID cid = 0;

    int nParams = 5;
    char* paramValues[nParams];
    char buf[512];
    formatParams( paramValues, nParams,
                  "%s"DELIM"%d"DELIM"%d"DELIM"%d"DELIM"%s", buf, sizeof(buf),
                  cookie, lang, nPlayersT, nPlayersH, wantsPublic?"TRUE":"FALSE" );

    /* NOTE: ILIKE, for case-insensitive comparison, is a postgres extension
       to SQL. */
    const char* cmd = "SELECT cid, connName, sum_array(nPerDevice) FROM "
        GAMES_TABLE
        " WHERE NOT dead"
        " AND room ILIKE $1"
        " AND lang = $2"
        " AND nTotal = $3"
        " AND $4 <= nTotal-sum_array(nPerDevice)"
        " AND $5 = pub"
        " LIMIT 1";

    PGresult* result = PQexecParams( getThreadConn(), cmd,
                                     nParams, NULL,
                                     paramValues, 
                                     NULL, NULL, 0 );
    if ( 1 == PQntuples( result ) ) {
        cid = atoi( PQgetvalue( result, 0, 0 ) );
        snprintf( connNameBuf, bufLen, "%s", PQgetvalue( result, 0, 1 ) );
        *nPlayersHP = atoi( PQgetvalue( result, 0, 2 ) );
        /* cid may be 0, but should use game anyway  */
    }
    PQclear( result );
    logf( XW_LOGINFO, "%s=>%d", __func__, cid );
    return cid;
} /* FindOpen */

bool
DBMgr::AllDevsAckd( const char* const connName )
{
    const char* cmd = "SELECT ntotal=sum_array(nperdevice) AND 'A'=ALL(ack) from " GAMES_TABLE
        " WHERE connName='%s'";
    string query;
    string_printf( query, cmd, connName );
    logf( XW_LOGINFO, "query: %s", query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    int nTuples = PQntuples( result );
    assert( nTuples <= 1 );
    bool full = nTuples == 1 && 't' == PQgetvalue( result, 0, 0 )[0];
    PQclear( result );
    logf( XW_LOGINFO, "%s=>%d", __func__, full );
    return full;
}

// Return DevIDRelay for device, adding it to devices table IFF it's not
// already there.
DBMgr::DevIDRelay
DBMgr::RegisterDevice( const DevID* host )
{
    DBMgr::DevIDRelay devID;
    assert( host->m_devIDType != ID_TYPE_NONE );
    int ii;
    bool success;

    // if it's already present, just return
    devID = getDevID( host );

    // If it's not present *and* of type ID_TYPE_RELAY, we can do nothing.
    // Fail.
    if ( DEVID_NONE == devID && ID_TYPE_RELAY < host->m_devIDType ) {
        // loop until we're successful inserting the unique key.  Ship with this
        // coming from random, but test with increasing values initially to make
        // sure duplicates are detected.
        for ( success = false, ii = 0; !success; ++ii ) {
            assert( 10 > ii );  // better to check that we're looping BECAUSE
                                // of uniqueness problem.
            devID = (DBMgr::DevIDRelay)random();
            if ( DEVID_NONE == devID ) {
                continue;
            }
            const char* command = "INSERT INTO " DEVICES_TABLE
                " (id, devType, devid)"
                " VALUES( $1, $2, $3 )";
            int nParams = 3;
            char* paramValues[nParams];
            char buf[512];
            formatParams( paramValues, nParams,
                          "%d"DELIM"%d"DELIM"%s", 
                          buf, sizeof(buf), devID, host->m_devIDType, 
                          host->m_devIDString.c_str() );

            PGresult* result = PQexecParams( getThreadConn(), command,
                                             nParams, NULL,
                                             paramValues, 
                                             NULL, NULL, 0 );
            success = PGRES_COMMAND_OK == PQresultStatus(result);
            if ( !success ) {
                logf( XW_LOGERROR, "PQexec=>%s;%s", PQresStatus(PQresultStatus(result)), 
                      PQresultErrorMessage(result) );
            }
            PQclear( result );
        }
    }
    return devID;
}

HostID
DBMgr::AddDevice( const char* connName, HostID curID, int clientVersion, 
                  int nToAdd, unsigned short seed, const AddrInfo* addr,
                  DevIDRelay devID, bool ackd )
{
    HostID newID = curID;

    if ( newID == HOST_ID_NONE ) {
        int arr[4] = {0};
        readArray( connName, arr );
        for ( newID = HOST_ID_SERVER; newID <= 4; ++newID ) {
            if ( arr[newID-1] == 0 ) {
                break;
            }
        }
    }
    assert( newID <= 4 );

    string devIDBuf;
    if ( DEVID_NONE != devID ) {
        string_printf( devIDBuf, "devids[%d] = %d, ", newID, devID );
    } else {
        assert( 0 == strlen(devIDBuf.c_str()) );
    }

    const char* fmt = "UPDATE " GAMES_TABLE " SET nPerDevice[%d] = %d,"
        " clntVers[%d] = %d,"
        " seeds[%d] = %d, addrs[%d] = \'%s\', %s"
        " mtimes[%d]='now', ack[%d]=\'%c\'"
        " WHERE connName = '%s'";
    string query;
    char* ntoa = inet_ntoa( ((sockaddr_in*)addr)->sin_addr );
    string_printf( query, fmt, newID, nToAdd, newID, clientVersion,
                   newID, seed, newID, ntoa, devIDBuf.c_str(), newID, 
                   newID, ackd?'A':'a', connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    execSql( query );

    return newID;
} /* AddDevice */

void
DBMgr::NoteAckd( const char* const connName, HostID id )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET ack[%d]='A'"
        " WHERE connName = '%s'";
    string query;
    string_printf( query, fmt, id, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    execSql( query );
}

bool
DBMgr::RmDeviceByHid( const char* connName, HostID hid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET nPerDevice[%d] = 0, "
        "seeds[%d] = 0, ack[%d]='-', mtimes[%d]='now' WHERE connName = '%s'";
    string query;
    string_printf( query, fmt, hid, hid, hid, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    return execSql( query );
}

HostID
DBMgr::HIDForSeed( const char* const connName, unsigned short seed )
{
    HostID hid = HOST_ID_NONE;
    char seeds[128] = {0};
    const char* fmt = "SELECT seeds FROM " GAMES_TABLE
        " WHERE connName = '%s'"
        " AND %d = ANY(seeds)";
    string query;
    string_printf( query, fmt, connName, seed );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    if ( 1 == PQntuples( result ) ) {
        snprintf( seeds, sizeof(seeds), "%s", PQgetvalue( result, 0, 0 ) );
    }
    PQclear( result );

    if ( 0 != seeds[0] ) {
        char *saveptr = NULL;
        int ii;
        char* str;
        for ( str = seeds, ii = 0; ; str = NULL, ++ii ) {
            char* tok = strtok_r( str, "{},", &saveptr );
            if ( NULL == tok ) {
                break;
            } else {
                int asint = atoi( tok );
                if ( asint == seed ) {
                    hid = ii + 1;
                    break;
                }
            }
        }
    } else {
        assert(0);              /* but don't ship with this!!!! */
    }

    return hid;
}

void
DBMgr::RmDeviceBySeed( const char* const connName, unsigned short seed )
{
    HostID hid = HIDForSeed( connName, seed );
    if ( hid != HOST_ID_NONE ) {
        RmDeviceByHid( connName, hid );
    }
} /* RmDeviceSeed */

bool
DBMgr::HaveDevice( const char* connName, HostID hid, int seed )
{
    bool found = false;
    const char* fmt = "SELECT * from " GAMES_TABLE 
        " WHERE connName = '%s' AND seeds[%d] = %d";
    string query;
    string_printf( query, fmt, connName, hid, seed );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    found = 1 == PQntuples( result );
    PQclear( result );
    return found;
}

bool
DBMgr::AddCID( const char* const connName, CookieID cid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET cid = %d "
        " WHERE connName = '%s' AND cid IS NULL";
    string query;
    string_printf( query, fmt, cid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    bool result = execSql( query );
    logf( XW_LOGINFO, "%s(cid=%d)=>%d", __func__, cid, result );
    return result;
}

void
DBMgr::ClearCID( const char* connName )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET cid = null "
        "WHERE connName = '%s'";
    string query;
    string_printf( query, fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    execSql( query );
}

void
DBMgr::RecordSent( const char* const connName, HostID hid, int nBytes )
{
    assert( hid >= 0 && hid <= 4 );
    const char* fmt = "UPDATE " GAMES_TABLE " SET"
        " nsent = nsent + %d, mtimes[%d] = 'now'"
        " WHERE connName = '%s'";
    string query;
    string_printf( query, fmt, nBytes, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    execSql( query );
}

void
DBMgr::RecordSent( const int* msgIDs, int nMsgIDs )
{
    if ( nMsgIDs > 0 ) {
        string query( "SELECT connname,hid,sum(msglen)"
                      " FROM " MSGS_TABLE " WHERE id IN (" );
        for ( int ii = 0; ; ) {
            string_printf( query, "%d", msgIDs[ii] );
            if ( ++ii == nMsgIDs ) {
                break;
            } else {
                query.append( "," );
            }
        }
        query.append( ") GROUP BY connname,hid" );

        PGresult* result = PQexec( getThreadConn(), query.c_str() );
        if ( PGRES_TUPLES_OK == PQresultStatus( result ) ) {
            int ntuples = PQntuples( result );
            for ( int ii = 0; ii < ntuples; ++ii ) {
                RecordSent( PQgetvalue( result, ii, 0 ),
                            atoi( PQgetvalue( result, ii, 1 ) ),
                            atoi( PQgetvalue( result, ii, 2 ) ) );
            }
        }
        PQclear( result );
    }
}

void
DBMgr::RecordAddress( const char* const connName, HostID hid, 
                      const AddrInfo* addr )
{
    assert( hid >= 0 && hid <= 4 );
    const char* fmt = "UPDATE " GAMES_TABLE " SET addrs[%d] = \'%s\'"
        " WHERE connName = '%s'";
    string query;
    char* ntoa = inet_ntoa( ((sockaddr_in*)addr)->sin_addr );
    string_printf( query, fmt, hid, ntoa, connName );
    logf( XW_LOGVERBOSE0, "%s: query: %s", __func__, query.c_str() );

    execSql( query );
}

void
DBMgr::GetPlayerCounts( const char* const connName, int* nTotal, int* nHere )
{
    const char* fmt = "SELECT ntotal, sum_array(nperdevice) FROM " GAMES_TABLE
        " WHERE connName = '%s'";
    string query;
    string_printf( query, fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    assert( 1 == PQntuples( result ) );
    *nTotal = atoi( PQgetvalue( result, 0, 0 ) );
    *nHere = atoi( PQgetvalue( result, 0, 1 ) );
    PQclear( result );
}

void
DBMgr::KillGame( const char* const connName, int hid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET dead = TRUE,"
        " nperdevice[%d] = - nperdevice[%d]"
        " WHERE connName = '%s'";
    string query;
    string_printf( query, fmt, hid, hid, connName );
    execSql( query );
}

void
DBMgr::ClearCIDs( void )
{
    execSql( "UPDATE " GAMES_TABLE " set cid = null" );
}

void
DBMgr::PublicRooms( int lang, int nPlayers, int* nNames, string& names )
{
    const char* fmt = "SELECT room, nTotal-sum_array(nPerDevice),"
        " round( extract( epoch from age('now', ctime)))"
        " FROM " GAMES_TABLE
        " WHERE NOT dead"
        " AND pub = TRUE"
        " AND lang = %d"
        " AND nTotal>sum_array(nPerDevice)"
        " AND nTotal = %d";

    string query;
    string_printf( query, fmt, lang, nPlayers );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    int nTuples = PQntuples( result );
    for ( int ii = 0; ii < nTuples; ++ii ) {
        names.append( PQgetvalue( result, ii, 0 ) );
        names.append( "/" );
        names.append( PQgetvalue( result, ii, 1 ) );
        names.append( "/" );
        names.append( PQgetvalue( result, ii, 2 ) );
        names.append( "\n" );
    }
    PQclear( result );
    *nNames = nTuples;
}

int
DBMgr::PendingMsgCount( const char* connName, int hid )
{
    int count = 0;
    const char* fmt = "SELECT COUNT(*) FROM " MSGS_TABLE
        " WHERE connName = '%s' AND hid = %d "
#ifdef HAVE_STIME
        "AND stime IS NULL"
#endif
        ;
    string query;
    string_printf( query, fmt, connName, hid );
    logf( XW_LOGVERBOSE0, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    if ( 1 == PQntuples( result ) ) {
        count = atoi( PQgetvalue( result, 0, 0 ) );
    }
    PQclear( result );
    return count;
}

bool
DBMgr::execSql( const string& query )
{
    return execSql( query.c_str() );
}

bool
DBMgr::execSql( const char* const query )
{
    PGresult* result = PQexec( getThreadConn(), query );
    bool ok = PGRES_COMMAND_OK == PQresultStatus(result);
    if ( !ok ) {
        logf( XW_LOGERROR, "PQexec=>%s;%s", PQresStatus(PQresultStatus(result)), PQresultErrorMessage(result) );
    }
    PQclear( result );
    return ok;
}

void
DBMgr::readArray( const char* const connName, int arr[]  ) /* len 4 */
{
    const char* fmt = "SELECT nPerDevice FROM " GAMES_TABLE " WHERE connName='%s'";

    string query;
    string_printf( query, fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    assert( 1 == PQntuples( result ) );
    const char* arrStr = PQgetvalue( result, 0, 0 );
    sscanf( arrStr, "{%d,%d,%d,%d}", &arr[0], &arr[1], &arr[2], &arr[3] );
    PQclear( result );
}

DBMgr::DevIDRelay 
DBMgr::getDevID( const char* connName, int hid )
{
    DBMgr::DevIDRelay devID;
    const char* fmt = "SELECT devids[%d] FROM " GAMES_TABLE " WHERE connName='%s'";
    string query;
    string_printf( query, fmt, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    assert( 1 == PQntuples( result ) );
    devID = (DBMgr::DevIDRelay)strtoul( PQgetvalue( result, 0, 0 ), NULL, 10 );
    PQclear( result );
    return devID;
}

DBMgr::DevIDRelay 
DBMgr::getDevID( const DevID* devID )
{
    DBMgr::DevIDRelay rDevID = DEVID_NONE;
    DevIDType devIDType = devID->m_devIDType;
    string query;
    assert( ID_TYPE_NONE < devIDType );
    const char* asStr = devID->m_devIDString.c_str();
    if ( ID_TYPE_RELAY == devIDType ) {
        // confirm it's there
        DBMgr::DevIDRelay cur = strtoul( asStr, NULL, 16 );
        if ( DEVID_NONE != cur ) {
            const char* fmt = "SELECT id FROM " DEVICES_TABLE " WHERE id=%d";
            string_printf( query, fmt, cur );
        }
    } else {
        const char* fmt = "SELECT id FROM " DEVICES_TABLE " WHERE devtype=%d and devid = '%s'";
        string_printf( query, fmt, devIDType, asStr );
    }

    if ( 0 < query.size() ) {
        logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
        PGresult* result = PQexec( getThreadConn(), query.c_str() );
        assert( 1 >= PQntuples( result ) );
        if ( 1 == PQntuples( result ) ) {
            rDevID = (DBMgr::DevIDRelay)strtoul( PQgetvalue( result, 0, 0 ), NULL, 10 );
        }
        PQclear( result );
    }
    return rDevID;
}

/*
 id | connname  | hid |   msg   
----+-----------+-----+---------
  1 | abcd:1234 |   2 | xyzzx
  2 | abcd:1234 |   2 | xyzzxxx
  3 | abcd:1234 |   3 | xyzzxxx
*/

int
DBMgr::CountStoredMessages( const char* const connName, int hid )
{
    const char* fmt = "SELECT count(*) FROM " MSGS_TABLE 
        " WHERE connname = '%s' "
#ifdef HAVE_STIME
        "AND stime IS NULL"
#endif
        ;

    string query;
    string_printf( query, fmt, connName );

    if ( hid != -1 ) {
        string_printf( query, "AND hid = %d", hid );
    }

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    assert( 1 == PQntuples( result ) );
    int count = atoi( PQgetvalue( result, 0, 0 ) );
    PQclear( result );
    return count;
}

int
DBMgr::CountStoredMessages( const char* const connName )
{
    return CountStoredMessages( connName, -1 );
} /* CountStoredMessages */

void
DBMgr::StoreMessage( const char* const connName, int hid, 
                     const unsigned char* buf, int len )
{
    DevIDRelay devID = getDevID( connName, hid );

    size_t newLen;
    const char* fmt = "INSERT INTO " MSGS_TABLE 
        " (connname, hid, devid, msg, msglen)"
        " VALUES( '%s', %d, %d, E'%s', %d)";

    unsigned char* bytes = PQescapeByteaConn( getThreadConn(), buf, 
                                              len, &newLen );
    assert( NULL != bytes );
    
    string query;
    string_printf( query, fmt, connName, hid, devID, bytes, len );

    PQfreemem( bytes );

    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    execSql( query );
}

bool
DBMgr::GetNthStoredMessage( const char* const connName, int hid, int nn, 
                            unsigned char* buf, size_t* buflen, int* msgID )
{
    const char* fmt = "SELECT id, msg, msglen FROM " MSGS_TABLE
        " WHERE connName = '%s' AND hid = %d "
#ifdef HAVE_STIME
        "AND stime IS NULL "
#endif
        "ORDER BY id LIMIT 1 OFFSET %d";
    string query;
    string_printf( query, fmt, connName, hid, nn );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    int nTuples = PQntuples( result );
    assert( nTuples <= 1 );

    bool found = nTuples == 1;
    if ( found ) {
        if ( NULL != msgID ) {
            *msgID = atoi( PQgetvalue( result, 0, 0 ) );
        }
        size_t msglen = atoi( PQgetvalue( result, 0, 2 ) );

        /* int len = PQgetlength( result, 0, 1 ); */
        const unsigned char* from =
            (const unsigned char* )PQgetvalue( result, 0, 1 );
        size_t to_length;
        unsigned char* bytes = PQunescapeBytea( from, &to_length );
        assert( to_length <= *buflen );
        memcpy( buf, bytes, to_length );
        PQfreemem( bytes );
        *buflen = to_length;
        assert( 0 == msglen || to_length == msglen );
    }
    PQclear( result );
    return found;
}

bool
DBMgr::GetStoredMessage( const char* const connName, int hid,
                         unsigned char* buf, size_t* buflen, int* msgID )
{
    return GetNthStoredMessage( connName, hid, 0, buf, buflen, msgID );
}

void
DBMgr::RemoveStoredMessages( const int* msgIDs, int nMsgIDs )
{
    if ( nMsgIDs > 0 ) {
        string ids;
        size_t len = 0;
        int ii;
        for ( ii = 0; ; ) {
            string_printf( ids, "%d", msgIDs[ii] );
            assert( len < sizeof(ids) );
            if ( ++ii == nMsgIDs ) {
                break;
            } else {
                ids.append( "," );
            }
        }

        const char* fmt = 
#ifdef HAVE_STIME
        "UPDATE " MSGS_TABLE " SET stime='now' "
#else
        "DELETE FROM " MSGS_TABLE 
#endif
            " WHERE id IN (%s)";
        string query;
        string_printf( query, fmt, ids.c_str() );
        logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
        execSql( query );
    }
}

static void
formatParams( char* paramValues[], int nParams, const char* fmt, char* buf, 
              int bufLen, ... )
{
    va_list ap;
    va_start( ap, bufLen );

    int len = vsnprintf( buf, bufLen, fmt, ap );
    assert( buf[len] == '\0' );

    int pnum;
    char* ptr = buf;
    for ( pnum = 0; pnum < nParams; ++pnum ) {
        paramValues[pnum] = ptr;
        for ( ; *ptr != '\0' && *ptr != DELIM[0]; ++ptr ) {
            // do nothing
            assert( ptr < &buf[bufLen] );
        }
        // we've found an end
        *ptr = '\0';
        ++ptr;
    }
    va_end(ap);
}

static int
here_less_seed( const char* seeds, int sumPerDevice, unsigned short seed )
{
    logf( XW_LOGINFO, "%s: find %x(%d) in \"%s\", sub from \"%d\"", __func__, 
          seed, seed, seeds, sumPerDevice );
    return sumPerDevice - 1;    /* FIXME */
}

static void
destr_function( void* conn )
{
    logf( XW_LOGINFO, "%s()", __func__ );
    PGconn* pgconn = (PGconn*)conn;
    PQfinish( pgconn );
}

PGconn* 
DBMgr::getThreadConn( void )
{
    PGconn* conn = (PGconn*)pthread_getspecific( m_conn_key );

    if ( NULL == conn ) {
        char buf[128];
        int len = snprintf( buf, sizeof(buf), "dbname = " );
        if ( !RelayConfigs::GetConfigs()->
             GetValueFor( "DB_NAME", &buf[len], sizeof(buf)-len ) ) {
            assert( 0 );
        }
        conn = PQconnectdb( buf );
        pthread_setspecific( m_conn_key, conn );
    }
    return conn;
}
