/*
 * Copyright (C) 2000-2002 the timedia project team
 *
 * This file is part of libxrtp, a modulized rtp library.
 *
 * xrtp is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xrtp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: list.h,v 0.1 11/12/2002 15:47:39 heming$
 *
 */
 
#ifdef WIN32

 #include <winsock.h>
 #define socket_close closesocket

 #define SOCKET_INVALID INVALID_SOCKET
 #define SOCKET_FAIL	SOCKET_ERROR

#else

 #include <sys/socket.h>
 #include <sys/select.h>
 #include <unistd.h>
 #define socket_close close
 
 #define SOCKET_INVALID -1
 #define SOCKET_FAIL	-1
 
#endif

#include "os.h"

extern DECLSPEC
int
socket_init(void);
