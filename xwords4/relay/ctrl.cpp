/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
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

#include <stdio.h>
#include <unistd.h>
#include <netdb.h>		/* gethostbyname */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <pthread.h>
#include <assert.h>
#include <sys/select.h>
#include <stdarg.h>
#include <sys/time.h>
#include <glib.h>

#include "ctrl.h"
#include "cref.h"
#include "crefmgr.h"
#include "mlock.h"
#include "xwrelay_priv.h"
#include "configs.h"
#include "lstnrmgr.h"
#include "tpool.h"
#include "devmgr.h"

/* this is *only* for testing.  Don't abuse!!!! */
extern pthread_rwlock_t gCookieMapRWLock;

/* Return of true means exit the ctrl thread */
typedef bool (*CmdPtr)( int socket, const char* cmd, int argc, gchar** args );

typedef struct FuncRec {
    const char* name;
    CmdPtr func;
} FuncRec;

vector<int> g_ctrlSocks;
pthread_mutex_t g_ctrlSocksMutex = PTHREAD_MUTEX_INITIALIZER;


static bool cmd_quit( int socket, const char* cmd, int argc, gchar** args );
static bool cmd_print( int socket, const char* cmd, int argc, gchar** args );
static bool cmd_devs( int socket, const char* cmd, int argc, gchar** args );
/* static bool cmd_lock( int socket, gchar** args ); */
static bool cmd_help( int socket, const char* cmd, int argc, gchar** args );
static bool cmd_start( int socket, const char* cmd, int argc, gchar** args );
static bool cmd_stop( int socket, const char* cmd, int argc, gchar** args );
/* static bool cmd_kill_eject( int socket, gchar** args ); */
static bool cmd_get( int socket, const char* cmd, int argc, gchar** args );
static bool cmd_set( int socket, const char* cmd, int argc, gchar** args );
static bool cmd_shutdown( int socket, const char* cmd, int argc, gchar** args );
static bool cmd_rev( int socket, const char* cmd, int argc, gchar** args );
static bool cmd_uptime( int socket, const char* cmd, int argc, gchar** args );
static bool cmd_crash( int socket, const char* cmd, int argc, gchar** args );

static void print_prompt( int socket );


static int
match( string* cmd, const char * const* first, int incr, int count )
{
    int cmdlen = cmd->length();
    int nFound = 0;
    const char* cmdFound = NULL;
    int which = -1;
    int ii;
    for ( ii = 0; (ii < count) && (nFound <= 1); ++ii ) {
        if ( 0 == strncmp( cmd->c_str(), *first, cmdlen ) ) {
            ++nFound;
            which = ii;
            cmdFound = *first;
        }
        first = (char* const*)(((char*)first) + incr);
    }

    if ( nFound == 1 ) {
        cmd->assign(cmdFound);
    } else {
        which = -1;
    }
    return which;
}

static void
print_to_sock( int sock, bool addCR, const char* what, ... )
{
    char buf[256];

    va_list ap;
    va_start( ap, what );
    vsnprintf( buf, sizeof(buf) - 1, what, ap );
    va_end(ap);

    if ( addCR ) {
        strncat( buf, "\n", sizeof(buf) );
    }
    send( sock, buf, strlen(buf), 0 );
}

static const FuncRec gFuncs[] = {
    { "?", cmd_help },
    { "crash", cmd_crash },
    /* { "eject", cmd_kill_eject }, */
    { "get", cmd_get },
    { "help", cmd_help },
    /* { "kill", cmd_kill_eject }, */
    /* { "lock", cmd_lock }, */
    { "print", cmd_print },
    { "devs", cmd_devs },
    { "quit", cmd_quit },
    { "rev", cmd_rev },
    { "set", cmd_set },
    { "shutdown", cmd_shutdown },
    { "start", cmd_start },
    { "stop", cmd_stop },
    { "uptime", cmd_uptime },
};

static bool
cmd_quit( int socket, const char* cmd, int argc, gchar** args )
{
    if ( 0 == strcmp( "help", args[1] ) ) {
        print_to_sock( socket, true, "* %s (disconnect from ctrl port)", 
                       args[0] );
        return false;
    } else {
        print_to_sock( socket, true, "bye bye" );
        return true;
    }
}

static void
print_cookies( int socket, CookieID theID )
{
    CRefMgr* cmgr = CRefMgr::Get();
    CookieMapIterator iter = cmgr->GetCookieIterator();
    CookieID id;
    for ( id = iter.Next(); id != 0; id = iter.Next() ) {
        if ( theID == 0 || theID == id ) {
            SafeCref scr( id );
            string s;
            scr.PrintCookieInfo( s );

            print_to_sock( socket, true, s.c_str() );
        }
    }
}

static bool
cmd_start( int socket, const char* cmd, int argc, gchar** args )
{
    print_to_sock( socket, true, "* %s (unimplemented)", args[0] );
    return false;
}

static bool
cmd_stop( int socket, const char* cmd, int argc, gchar** args )
{
    print_to_sock( socket, true, "* %s (unimplemented)", args[0] );
    return false;
}

#if 0
static bool
cmd_kill_eject( int socket, gchar** args )
{
    bool found = false;
    int isKill = 0 == strcmp( args[0], "kill" );

    if ( 0 == strcmp( args[1], "socket" ) ) {
        int victim = atoi( args[2] );
        if ( victim != 0 ) {
            XWThreadPool::GetTPool()-> EnqueueKill( victim, "ctrl command" );
            found = true;
        }
    } else if ( 0 == strcmp( args[1], "cref" ) ) {
        const char* idhow = args[2];
        for ( int indx = 3; ; ++indx ) {
            const char* id = args[indx];
            if ( idhow == NULL || id == NULL ) {
                break;
            }
            if ( 0 == strcmp( idhow, "name" ) ) {
                CRefMgr::Get()->Recycle( id );
                found = true;
            } else if ( 0 == strcmp( idhow, "id" ) ) {
                CRefMgr::Get()->Recycle( atoi( id ) );
                found = true;
            }
        }
    } else if ( 0 == strcmp( args[1], "relay" ) ) {
        print_to_sock( socket, true, "not yet unimplemented" );
    }

    const char* expl = isKill? 
        "silently remove from game"
        : "remove from game with error to device";
    if ( !found ) {
        const char* msg =
            "* %s socket <num>  -- %s\n"
            "  %s cref name <connName>+\n"
            "  %s cref id <id>+"
            ;
        print_to_sock( socket, true, msg, args[0], expl, args[0], args[0] );
    }
    return false;
} /* cmd_kill_eject */
#endif

static bool
cmd_get( int socket, const char* cmd, int argc, gchar** args )
{
    bool needsHelp = true;

    string attr(args[1]);
    const char* const attrs[] = { "help", "listeners", "loglevel" };
    int index = match( &attr, attrs, sizeof(attrs[0]), 
                       sizeof(attrs)/sizeof(attrs[0]));

    switch( index ) {
    case 0:
        break;
    case 1: {
        char buf[128];
        int len = 0;
        ListenersIter iter(&g_listeners, false);
        for ( ; ; ) {
            int listener = iter.next();
            if ( listener == -1 ) {
                break;
            }
            len += snprintf( &buf[len], sizeof(buf)-len, "%d,", listener );
        }
        print_to_sock( socket, true, "%s", buf );
        needsHelp = false;
    }
        break;
    case 2: {
        RelayConfigs* rc = RelayConfigs::GetConfigs();
        int level;
        if ( NULL != rc && rc->GetValueFor( "LOGLEVEL", &level ) ) {
            print_to_sock( socket, true, "loglevel=%d\n",  level );
            needsHelp = false;
        } else {
            logf( XW_LOGERROR, "RelayConfigs::GetConfigs() => NULL" );
        }
    }
        break;

    default:
        print_to_sock( socket, true, "unknown or ambiguous attribute: %s", attr.c_str() );
    }

    if ( needsHelp ) {
        /* includes help */
        print_to_sock( socket, false,
                       "* %s -- lists all attributes (unimplemented)\n"
                       "* %s listener\n"
                       "* %s loglevel\n"
                       , args[0], args[0], args[0] );
    }

    return false;
} /* cmd_get */

static bool
cmd_set( int socket, const char* cmd, int argc, gchar** args )
{
    const char* val = args[2];
    const char* const attrs[] = { "help", "listeners", "loglevel" };
    string attr(args[1]);
    int index = match( &attr, attrs, sizeof(attrs[0]), 
                       sizeof(attrs)/sizeof(attrs[0]));

    bool needsHelp = true;
    switch( index ) {
    case 1:
        if ( NULL != val && val[0] != '\0' ) {
            istringstream str( val );
            vector<int> sv;
            while ( !str.eof() ) {
                int sock;
                char comma;
                str >> sock >> comma;
                logf( XW_LOGERROR, "%s: read %d", __func__, sock );
                sv.push_back( sock );
            }
            g_listeners.SetAll( &sv );
            needsHelp = false;
        }
        break;
    case 2:
        if ( NULL != val && val[0] != '\0' ) {
            RelayConfigs* rc = RelayConfigs::GetConfigs();
            if ( rc != NULL ) {
                rc->SetValueFor( "LOGLEVEL", val );
                needsHelp = false;
            }
        }
        break;
    default:
        break;
    }

    if ( needsHelp ) {
        print_to_sock( socket, true, 
                       "* %s listeners <n>,[<n>,..<n>,]\n"
                       "* %s loglevel <n>"
                       ,args[0], args[0] );
    }
    return false;
}

void
format_rev( char* buf, int len )
{
    snprintf( buf, len, "git rev: %s", SVN_REV );
}

static bool
cmd_rev( int socket, const char* cmd, int argc, gchar** args )
{
    if ( 0 == strcmp( args[1], "help" ) ) {
        print_to_sock( socket, true,
                       "* %s -- prints svn rev number of build",
                       args[0] );
    } else {
        char buf[128];
        format_rev( buf, sizeof(buf) );
        print_to_sock( socket, true, "%s", buf );
    }
    return false;
}

void
format_uptime( time_t seconds, char* buf, int len )
{
    int days = seconds / (24*60*60);
    seconds %= (24*60*60);

    int hours = seconds / (60*60);
    seconds %= (60*60);

    int minutes = seconds / 60;
    seconds %= 60;

    if ( days > 0 ) {
        snprintf( buf, len, "%d D %.2d:%.2d:%.2ld", days, hours,
                  minutes, seconds );
    } else if ( hours > 0 ) {
        snprintf( buf, len, "%.2d:%.2d:%.2ld", hours,
                  minutes, seconds );
    } else if ( minutes > 0 ) {
        snprintf( buf, len, "%.2d:%.2ld", minutes, seconds );
    } else {
        snprintf( buf, len, "%.2ld s", seconds );
    }
}

static bool
cmd_uptime( int socket, const char* cmd, int argc, gchar** args )
{
    if ( 0 == strcmp( args[1], "help" ) ) {
        print_to_sock( socket, true,
                       "* %s -- prints how long the relay's been running",
                       args[0] );
    } else {
        char buf[128];
        format_uptime( uptime(), buf, sizeof(buf) );
        print_to_sock( socket, true, "uptime: %s", buf );
    }
    return false;
}

static bool
cmd_crash( int socket, const char* cmd, int argc, gchar** args )
{
    if ( 0 == strcmp( args[1], "help" ) ) {
        print_to_sock( socket, true,
                       "* %s -- fires an assert (debug case) or divides-by-zero",
                       args[0] );
    } else {
        logf( XW_LOGERROR, "crashing..." );
        assert(0);
        int ii = 1;
        while ( ii > 0 ) --ii;
        return 6/ii > 0;
    }
    return false;
}

static bool
cmd_shutdown( int socket, const char* cmd, int argc, gchar** args )
{
    print_to_sock( socket, true,
                   "* %s  -- shuts down relay (exiting main) (unimplemented)",
                   args[0] );
    return false;
}

static void
print_cookies( int socket, const char* cookie, const char* connName )
{
    CookieMapIterator iter = CRefMgr::Get()->GetCookieIterator();
    CookieID id;

    for ( id = iter.Next(); id != 0; id = iter.Next() ) {
        SafeCref scr( id );
        if ( cookie != NULL && 0 == strcasecmp( scr.Cookie(), cookie ) ) {
            /* print this one */
        } else if ( connName != NULL && 
                    0 == strcmp( scr.ConnName(), connName ) ) {
            /* print this one */
        } else {
            continue;
        }
        string s;
        scr.PrintCookieInfo( s );

        print_to_sock( socket, true, s.c_str() );
    }
}

#if 0
static void
print_socket_info( int out, int which )
{
    string s;
    CRefMgr::Get()->PrintSocketInfo( which, s );
    print_to_sock( out, 1, s.c_str() );
}
#endif

static void
print_sockets( int out, int sought )
{
    /* SocketsIterator iter = CRefMgr::Get()->MakeSocketsIterator(); */
    /* int sock; */
    /* while ( (sock = iter.Next()) != 0 ) { */
    /*     if ( sought == 0 || sought == sock ) { */
    /*         print_socket_info( out, sock ); */
    /*     } */
    /* } */
}

static bool
cmd_print( int socket, const char* cmd, int argc, gchar** args )
{
    bool found = false;
    if ( 0 == strcmp( "cref", args[1] ) ) {
        if ( 0 == strcmp( "all", args[2] ) ) {
            print_cookies( socket, (CookieID)0 );
            found = true;
        } else if ( 0 == strcmp( "cookie", args[2] ) ) {
            print_cookies( socket, args[3], NULL );
            found = true;
        } else if ( 0 == strcmp( "connName", args[2] ) ) {
            print_cookies( socket, NULL, args[3] );
            found = true;
        } else if ( 0 == strcmp( "id", args[2] ) ) {
            print_cookies( socket, atoi(args[3]) );
            found = true;
        }
    } else if ( 0 == strcmp( "socket", args[1] ) ) {
        if ( 0 == strcmp( "all", args[2] ) ) {
            print_sockets( socket, 0 );
            found = true;
        } else if ( 0 == strcmp( "id", args[2] ) ) {
            print_sockets( socket, atoi(args[3]) );
            found = true;
        }
    }

    if ( !found ) {
        const char* str =
            "* %s cref all\n"
            "  %s cref name <name>\n"
            "  %s cref connName <name>\n"
            "  %s cref id <id>\n"
            "  %s dev all -- list all known devices (by how recently connected)\n"
            "  %s socket all\n"
            "  %s socket <num>  -- print info about crefs and sockets";
        print_to_sock( socket, true, str, args[0], args[0], args[0], 
                       args[0], args[0], args[0], args[0] );
    }
    return false;
} /* cmd_print */

/* NOTE: this will probably crash if socket is closed before the ack comes
   back or times out */
static void 
onAckProc( bool acked, DevIDRelay devid, uint32_t packetID, void* data )
{
    int socket = (int)data;
    if ( acked ) {
        print_to_sock( socket, true, "got ack for packet %d from dev %d", 
                       packetID, devid );
    } else {
        print_to_sock( socket, true, "NO ACK for packetID %d from dev %d", 
                       packetID, devid );
    }
    print_prompt( socket );
}

static bool
cmd_devs( int socket, const char* cmd, int argc, gchar** args )
{
    bool found = false;
    string result;
    if ( 1 >= argc ) {
        /* missing param; let help print */
    } else if ( 0 == strcmp( "print", args[1] ) ) {
        DevIDRelay devid = 0;
        if ( 2 < argc ) {
            devid = (DevIDRelay)strtoul( args[2], NULL, 10 );
        }
        DevMgr::Get()->printDevices( result, devid );
        found = true;
    } else if ( 0 == strcmp( "ping", args[1] ) ) {
    } else if ( 0 == strcmp( "msg", args[1] ) && 3 < argc ) {
        DevIDRelay devid = (DevIDRelay)strtoul( args[2], NULL, 10 );
        const char* msg = args[3];
        if ( post_message( devid, msg, onAckProc, (void*)socket ) ) {
            string_printf( result, "posted message: %s\n", msg );
        } else {
            string_printf( result, "unable to post; does dev %d exist\n", 
                           devid );
        }
        found = true;
    } else if ( 0 == strcmp( "rm", args[1] ) && 2 < argc  ) {
        DevIDRelay devid = (DevIDRelay)strtoul( args[2], NULL, 10 );
        if ( DevMgr::Get()->forgetDevice( devid ) ) {
            string_printf( result, "dev %d removed\n", devid );
        } else {
            string_printf( result, "dev %d unknown\n", devid );
        }
        found = true;
    }

    if ( found ) {
        if ( 0 < result.size() ) {
            send( socket, result.c_str(), result.size(), 0 );
        }
    } else {
        const char* strs[] = {
            "* %s print [<id>]\n",
            "  %s ping\n",
            "  %s msg <devid> <msg_text>\n",
            "  %s rm <devid>\n",
        };
        string help;
        for ( size_t ii = 0; ii < VSIZE(strs); ++ii ) {
            string_printf( help, strs[ii], args[0] );
        }
        send( socket, help.c_str(), help.size(), 0 );
    }
    return false;
}

#if 0
static bool
cmd_lock( int socket, gchar** args )
{
    CRefMgr* mgr = CRefMgr::Get();
    if ( 0 == strcmp( "on", args[1] ) ) {
        mgr->LockAll();
    } else if ( 0 == strcmp( "off", args[1] ) ) {
        mgr->UnlockAll();
    } else {
        print_to_sock( socket, true, "* %s [on|off]  -- lock/unlock access mutex", 
                       args[0] );
    }
    
    return 0;
} /* cmd_lock */
#endif

static bool
cmd_help( int socket, const char* cmd, int argc, gchar** argv )
{
    if ( 1 < argc && NULL != argv[1] && 0 == strcmp( "help", argv[1] ) ) {
        print_to_sock( socket, true, "* %s  -- prints this", argv[0] );
    } else {

        gchar* help[] = { NULL, (gchar*)"help" };
        const FuncRec* fp = gFuncs;
        const FuncRec* last = fp + (sizeof(gFuncs) / sizeof(gFuncs[0]));
        while ( fp < last ) {
            help[0] = (gchar*)fp->name;
            (*fp->func)( socket, (gchar*)fp->name, VSIZE(help), help );
            ++fp;
        }
    }
    return 0;
}

static void
print_prompt( int socket )
{
    print_to_sock( socket, false, "=> " );
}

static void*
ctrl_thread_main( void* arg )
{
    blockSignals();
    int sock = (int)arg;

    {
        MutexLock ml( &g_ctrlSocksMutex );
        g_ctrlSocks.push_back( sock );
    }

    gint argc = 0;
    gchar** args = NULL;
    int index = -1;
    string cmd;

    for ( ; ; ) {

        print_prompt( sock );

        char buf[512];
        ssize_t nGot = recv( sock, buf, sizeof(buf)-1, 0 );
        if ( 0 >= nGot ) {
            break;
        } else if ( 1 == nGot ) {
            assert( 0 );        /* not happening, as getting \r\n terminator */
        } else if ( 2 == nGot ) {
            /* user hit return; repeat prev command */
        } else {
            buf[nGot-2] = '\0';
            if ( NULL != args ) {
                g_strfreev( args );
            }

            if ( !g_shell_parse_argv( buf, &argc, &args, NULL ) ) {
                assert( 0 );
            } else {
                cmd = args[0];
                index = match( &cmd, (char*const*)&gFuncs[0].name, 
                                   sizeof(gFuncs[0]), 
                                   sizeof(gFuncs)/sizeof(gFuncs[0]) );
            }
        }
        if ( index == -1 ) {
            print_to_sock( sock, 1, "unknown or ambiguous command: \"%s\"", 
                           cmd.c_str() );
            gchar* args[] = { (gchar*)cmd.c_str() };
            (void)cmd_help( sock, "help", 1, args );
        } else if ( (*gFuncs[index].func)( sock, cmd.c_str(), 
                                           argc, args ) ) {
            break;
        }
    }

    close ( sock );

    MutexLock ml( &g_ctrlSocksMutex );
    vector<int>::iterator iter = g_ctrlSocks.begin();
    while ( iter != g_ctrlSocks.end() ) {
        if ( *iter == sock ) {
            g_ctrlSocks.erase(iter);
            break;
        }
    }
    return NULL;
} /* ctrl_thread_main */

void
run_ctrl_thread( int ctrl_sock )
{
    logf( XW_LOGINFO, "calling accept on socket %d", ctrl_sock );

    sockaddr newaddr;
    socklen_t siz = sizeof(newaddr);
    int newSock = accept( ctrl_sock, &newaddr, &siz );
    logf( XW_LOGINFO, "got one for ctrl: %d", newSock );

    pthread_t thread;
    int result = pthread_create( &thread, NULL, 
                                 ctrl_thread_main, (void*)newSock );
    pthread_detach( thread );

    assert( result == 0 );
}

void
stop_ctrl_threads()
{
    MutexLock ml( &g_ctrlSocksMutex );
    vector<int>::iterator iter = g_ctrlSocks.begin();
    while ( iter != g_ctrlSocks.end() ) {
        int sock = *iter++;
        print_to_sock( sock, 1, "relay going down..." );
        close( sock );
    }
}
