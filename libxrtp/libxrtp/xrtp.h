/***************************************************************************
                          xrtp.h  -  main header
                             -------------------
    begin                : Tue Oct 28 2003
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

#ifndef XRTP_H
#define XRTP_H

typedef struct xrtp_set_s xrtp_set_t;

#include "session.h"

#define XRTP_VERSION_MAIN 0
#define XRTP_VERSION_MINUS 0
#define XRTP_VERSION_PATCH 1
#define XRTP_VERSION  000001

extern DECLSPEC 
xrtp_set_t*
xrtp_init(module_catalog_t* cata);

extern DECLSPEC
int 
xrtp_done(xrtp_set_t* set);

extern DECLSPEC 
module_catalog_t* 
xrtp_catalog(xrtp_set_t* set);

extern DECLSPEC 
session_sched_t* 
xrtp_scheduler(xrtp_set_t* set);

extern DECLSPEC 
int 
xrtp_add_session(xrtp_set_t* set, xrtp_session_t *session);

extern DECLSPEC 
int
xrtp_remove_session(xrtp_set_t* set, xrtp_session_t *session);

extern DECLSPEC 
xrtp_session_t* 
xrtp_find_session(xrtp_set_t* set, char *cname, int cnlen, char *ipaddr, uint16 rtp_portno, uint16 rtcp_portno, uint8 profile_no, char *profile_mime);

#endif

