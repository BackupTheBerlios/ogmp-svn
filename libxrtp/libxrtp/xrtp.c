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
 
#ifdef XRTP_LOG
 #define xrtp_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define xrtp_log(fmtargs)
#endif

module_catalog_t * module_catalog = NULL;

xrtp_list_t * sessions = NULL;

session_sched_t * main_scheduler = NULL;

portman_t * xrtp_portman;

char * plugins_dir = ".";

int xrtp_init(){

   int nplug;

   module_catalog = catalog_new("xrtp_handler");
   nplug = catalog_scan_modules(module_catalog, XRTP_VERSION, plugins_dir);

   if(nplug < 0){
      
      xrtp_log(("xrtp_init: No modules found !\n"));
       
      catalog_done(module_catalog);
      module_catalog = NULL;

      return XRTP_FAIL;
   }

   xrtp_log(("xrtp_init: Found %d module(s).\n", nplug));

   sessions = xrtp_list_new();
   if(!sessions){

      xrtp_log(("xrtp_init: Can't create session list !\n"));

      catalog_done(module_catalog);
      module_catalog = NULL;

      return XRTP_FAIL;
   }
    
   /* NOTE: use which scheduler is compile-time determined */
   main_scheduler = sched_new();
   if(!main_scheduler){

      xrtp_log(("xrtp_init: Can't create session secheduler !\n"));

      catalog_done(module_catalog);
      module_catalog = NULL;

      xrtp_list_free(sessions, NULL);

      return XRTP_FAIL;
   }
        
   xrtp_log(("xrtp_init: libxrtp Initialized ...\n"));
    
   return XRTP_OK;
}

int xrtp_free_session(void * gen){

   return session_done((xrtp_session_t *)gen);
}

int xrtp_done(){

   main_scheduler->done(main_scheduler);
   main_scheduler = NULL;

   xrtp_list_free(sessions, xrtp_free_session);
   catalog_done(module_catalog);

   return XRTP_OK;
}

module_catalog_t * xrtp_catalog() {

   return module_catalog;
}

session_sched_t* xrtp_scheduler() {

   return main_scheduler;
}

xrtp_session_t * xrtp_session(char * cname);
