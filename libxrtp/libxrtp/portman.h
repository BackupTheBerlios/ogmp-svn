/***************************************************************************
                          portman.h  -  port manager
                             -------------------
    begin                : Wed Dec 3 2003
    copyright            : (C) 2003 by Heming Ling
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef XRTP_PORTMAN_H
 
#define XRTP_PORTMAN_H

typedef struct portman_s portman_t;

#include "session.h"

portman_t * portman_new();

int portman_done(portman_t * man);

int portman_add_port(portman_t * man, xrtp_port_t * port);

int portman_remove_port(portman_t * man, xrtp_port_t * port);

int portman_clear_ports(portman_t * man);

/* How many port can be handle up to */
int portman_maximum(portman_t * man);

int portman_poll(portman_t * man);

#endif
