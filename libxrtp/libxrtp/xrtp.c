/***************************************************************************
                          xrtp.c  -  Main body
                             -------------------
    begin                : Sun Nov 16 2003
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

#include "xrtp.h"
#include <stdio.h>

#include "portman.h"

#ifndef MODDIR
#define MODDIR "module"
#endif

#define XRTP_LOG
#define XRTP_DEBUG
 
#ifdef XRTP_LOG
 #define xrtp_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define xrtp_log(fmtargs)
#endif

#ifdef XRTP_DBUG
 #define xrtp_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define xrtp_debug(fmtargs)
#endif

module_catalog_t * module_catalog = NULL;

xrtp_list_t * sessions = NULL;
xthr_lock_t * sessions_lock = NULL;

session_sched_t * main_scheduler = NULL;

portman_t * xrtp_portman;

int initialized = 0;

char * plugins_dir = ".";

int xrtp_init(module_catalog_t *cata)
{
   if(socket_init()!=OS_OK)
   {
	  xrtp_debug(("xrtp_init: network down\n"));
	  return XRTP_FAIL;
   }

   sessions_lock = xthr_new_lock();

   module_catalog = cata;
   
   /*
   int nplug = catalog_scan_modules(cata, XRTP_VERSION, plugins_dir);
   if(nplug < 0)
   {
      xrtp_log(("xrtp_init: No modules found !\n"));
       
      catalog_done(module_catalog);
      module_catalog = NULL;

      return XRTP_FAIL;
   }
   xrtp_log(("xrtp_init: Found %d module(s).\n", nplug));
   */

   sessions = xrtp_list_new();
   if(!sessions)
   {
      xrtp_log(("xrtp_init: Can't create session list !\n"));

      catalog_done(module_catalog);
      module_catalog = NULL;

      return XRTP_FAIL;
   }
    
   /* NOTE: use which scheduler is compile-time determined */
   main_scheduler = sched_new();
   if(!main_scheduler)
   {
      xrtp_log(("xrtp_init: Can't create session secheduler !\n"));

      catalog_done(module_catalog);
      module_catalog = NULL;

      xrtp_list_free(sessions, NULL);

      return XRTP_FAIL;
   }

   initialized = 1;
        
   xrtp_log(("xrtp_init: libxrtp Initialized ...\n"));
    
   return XRTP_OK;
}

int xrtp_done_session(void * gen)
{
   return session_done((xrtp_session_t *)gen);
}

int xrtp_done()
{
	if(!initialized) return XRTP_OK;

   main_scheduler->done(main_scheduler);
   main_scheduler = NULL;

   xthr_done_lock(sessions_lock);

   xrtp_list_free(sessions, xrtp_done_session);
   catalog_done(module_catalog);

   return XRTP_OK;
}

module_catalog_t * xrtp_catalog()
{
   return module_catalog;
}

session_sched_t* xrtp_scheduler()
{
   return main_scheduler;
}

xrtp_session_t* xrtp_session(int id)
{
	xlist_user_t lu;
	xrtp_session_t *ses;

	xthr_lock(sessions_lock);
	ses = xlist_first(sessions, &lu);
	while(ses)
	{
		if(id == ses->id) break;

		ses = xlist_next(sessions, &lu);
	}
	xthr_lock(sessions_lock);

	if(!ses) xrtp_log(("xrtp_session: can not find session #%d\n", id));

	return ses;
}

xrtp_session_t* xrtp_find_session(char *cn, int cnlen, char *ip, uint16 rtp_pno, uint16 rtcp_pno, uint8 profno, char *mime)
{
	xlist_user_t lu;
	xrtp_session_t *ses;

	xthr_lock(sessions_lock);
	ses = xlist_first(sessions, &lu);
	while(ses)
	{
		if(session_match(ses, cn, cnlen, ip, rtp_pno, rtcp_pno, profno, mime))
			break;

		ses = xlist_next(sessions, &lu);
	}
	xthr_lock(sessions_lock);

	if(!ses) 
		xrtp_log(("xrtp_session: No session match\n"));

	return ses;
}
