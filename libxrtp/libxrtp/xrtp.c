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

struct xrtp_set_s
{
	xrtp_list_t* sessions;
	xthr_lock_t* sessions_lock;

	module_catalog_t * module_catalog;

	session_sched_t * main_scheduler;
	portman_t * xrtp_portman;

	int initialized;
};

char * plugins_dir = ".";

xrtp_set_t* xrtp_init(module_catalog_t *cata)
{
   xrtp_set_t *set;
	
   if(socket_init()!=OS_OK)
   {
	  xrtp_debug(("xrtp_init: network down\n"));
	  return NULL;
   }

   set = malloc(sizeof(xrtp_set_t));
   if(!set)
	   return NULL;

   memset(set, 0, sizeof(xrtp_set_t));
   
   set->sessions_lock = xthr_new_lock();

   set->module_catalog = cata;
   
   set->sessions = xrtp_list_new();
   if(!set->sessions)
   {
      xrtp_log(("xrtp_init: Can't create session list !\n"));

      catalog_done(set->module_catalog);
      free(set);

      return NULL;
   }
    
   /* NOTE: use which scheduler is compile-time determined */
   set->main_scheduler = sched_new();
   if(!set->main_scheduler)
   {
      xrtp_log(("xrtp_init: Can't create session secheduler !\n"));

      catalog_done(set->module_catalog);

      xlist_done(set->sessions, NULL);

      return NULL;
   }

   set->initialized = 1;
        
   xrtp_log(("xrtp_init: libxrtp Initialized ...\n"));
    
   return set;
}

int xrtp_done_session(void * gen)
{
	return session_done((xrtp_session_t *)gen);
}

int xrtp_done(xrtp_set_t *set)
{
	if(set->initialized)
	{
		set->main_scheduler->done(set->main_scheduler);

		xthr_done_lock(set->sessions_lock);

		xrtp_list_free(set->sessions, xrtp_done_session);

		catalog_done(set->module_catalog);
	}

	free(set);

	return XRTP_OK;
}

module_catalog_t * xrtp_catalog(xrtp_set_t *set)
{
	return set->module_catalog;
}

session_sched_t* xrtp_scheduler(xrtp_set_t *set)
{
	xrtp_log(("xrtp_scheduler: sched[%x]\n", (int)set->main_scheduler));
    
	return set->main_scheduler;
}

int 
xrtp_add_session(xrtp_set_t* set, xrtp_session_t *session)
{
	xthr_lock(set->sessions_lock);

	xlist_addto_first(set->sessions, session);

	xthr_unlock(set->sessions_lock);

	return XRTP_OK;
}

int
xrtp_remove_session(xrtp_set_t* set, xrtp_session_t *session)
{
	xthr_lock(set->sessions_lock);

	xlist_remove_item(set->sessions, session);

	xthr_unlock(set->sessions_lock);

	return XRTP_OK;
}

xrtp_session_t* xrtp_find_session(xrtp_set_t *set, char *cn, int cnlen, char *ip, uint16 rtp_pno, uint16 rtcp_pno, uint8 profno, char *mime)
{
	xlist_user_t lu;
	xrtp_session_t *ses;

	xthr_lock(set->sessions_lock);

	ses = xlist_first(set->sessions, &lu);
	while(ses)
	{
		if(session_match(ses, cn, cnlen, ip, rtp_pno, rtcp_pno, profno, mime))
			break;

		ses = xlist_next(set->sessions, &lu);
	}

	xthr_unlock(set->sessions_lock);

	if(!ses) 
		xrtp_log(("xrtp_session: No session match\n"));

	return ses;
}
