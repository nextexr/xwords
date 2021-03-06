/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#ifndef _UDPQUEUE_H_
#define _UDPQUEUE_H_

#include <pthread.h>
#include <deque>
#include <map>

#include "xwrelay_priv.h"
#include "addrinfo.h"

using namespace std;

class PacketThreadClosure;

typedef void (*QueueCallback)( PacketThreadClosure* closure );

class PacketThreadClosure {
public:
    PacketThreadClosure( const AddrInfo* addr, const uint8_t* buf, 
                      int len, QueueCallback cb )
        : m_buf(new uint8_t[len])
        , m_len(len)
        , m_addr(*addr)
        , m_cb(cb)
        , m_created(time( NULL ))
        { 
            memcpy( m_buf, buf, len );
            m_addr.ref();
        }

    ~PacketThreadClosure() {
        m_addr.unref();
        delete[] m_buf;
    }

    const uint8_t* buf() const { return m_buf; } 
    int len() const { return m_len; }
    const AddrInfo::AddrUnion* saddr() const { return m_addr.saddr(); }
    const AddrInfo* addr() const { return &m_addr; }
    void noteDequeued() { m_dequed = time( NULL ); }
    void logStats();
    time_t ageInSeconds() { return time( NULL ) - m_created; }
    const QueueCallback cb() const { return m_cb; }
    void setID( int id ) { m_id = id; }
    int getID( void ) { return m_id; }

 private:
    uint8_t* m_buf;
    int m_len;
    AddrInfo m_addr;
    QueueCallback m_cb;
    time_t m_created;
    time_t m_dequed;
    int m_id;
};

class PartialPacket {
 public:
    PartialPacket(int sock)
        :m_len(0)
        ,m_sock(sock)
        ,m_errno(0)
        {}
    bool stillGood() const ;
    bool readAtMost( int len );
    size_t readSoFar() const { return m_buf.size(); }
    const uint8_t* data() const { return m_buf.data(); }

    unsigned short m_len;       /* decoded via ntohs from the first 2 bytes */
 private:

    vector<uint8_t> m_buf;
    int m_sock;
    int m_errno;
};

class UdpQueue {
 public:
    static UdpQueue* get();
    UdpQueue();
    ~UdpQueue();
    bool handle( const AddrInfo* addr, QueueCallback cb );
    void handle( const AddrInfo* addr, const uint8_t* buf, int len,
                 QueueCallback cb );
    void newSocket( int sock );
    void newSocket( const AddrInfo* addr );

 private:
    void newSocket_locked( int sock );
    static void* thread_main_static( void* closure );
    void* thread_main();

    pthread_mutex_t m_partialsMutex;
    pthread_mutex_t m_queueMutex;
    pthread_cond_t m_queueCondVar;
    deque<PacketThreadClosure*> m_queue;
    // map<int, vector<PacketThreadClosure*> > m_bySocket;
    int m_nextID;
    map<int, PartialPacket*> m_partialPackets;
};

#endif
