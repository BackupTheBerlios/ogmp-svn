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

#include "session.h"

#define XRTP_VERSION_MAIN 0
#define XRTP_VERSION_MINUS 0
#define XRTP_VERSION_PATCH 1
#define XRTP_VERSION  000001

int xrtp_init();

int xrtp_done();

module_catalog_t* xrtp_catalog();

session_sched_t* xrtp_scheduler();

xrtp_session_t* xrtp_session(char * cname);

#endif

