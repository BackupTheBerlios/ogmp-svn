/***************************************************************************
                          socket_posix.c  -  description
                             -------------------
    begin                :Sat Jun 06
    copyright            : (C) 2004 by Heming
    email                : digitalme@sf.net; heming@bigfoot.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "os.h"
#include "socket.h"

/* The only reason of this api is for win32 compatibility,
 * actually no functionality on posix
 */
int socket_init(void) {return OS_OK;}
