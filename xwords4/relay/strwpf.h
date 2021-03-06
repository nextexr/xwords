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

#ifndef _STRWPF_H_
#define _STRWPF_H_

#include <string>
#include <stdarg.h>

class StrWPF : public std::string {
 public:
    StrWPF() : m_addsiz(100){}

    void catf( const char* fmt, ... );
    /* Don't overload catf: some compilers use the wrong one, maybe on
       32-bit? */
    bool catfap( const char* fmt, va_list ap );
 private:
    int m_addsiz;
};

#endif
