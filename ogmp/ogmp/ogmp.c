/***************************************************************************
                          ogmplayer.c  -  Play Ogg Media
                             -------------------
    begin                : Wed Jan 28 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org, heming@bigfoot.com
 ***************************************************************************/
 
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ogmp.h"
#include <timedia/xstring.h>

#define OGMP_LOG

#ifdef OGMP_LOG
 #define ogmp_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define ogmp_log(fmtargs)
#endif

int main(int argc, char** argv){

   char * playmode = argv[1];
   
   /* sip useragent */
   /* define a player */
   xrtp_clock_t * clock_main = NULL;
   xrtp_lrtime_t remain = 0;
   
   sipua_t *sipua;
   ogmp_server_t *server;

   ogmp_log (("\nmain: modules in dir:%s\n", MODDIR));
   
   /* Initialise */
   sipua = sipua_new(5060, NULL, NULL);
   
   server = server_new(sipua, playmode);
   
   if(!strcmp(playmode, "playback"))
   {
	   server_setup(server, playmode);
	   server_start(server);
	   clock_main = time_start();
   
	   /* play 300 seconds audio */
	   time_msec_sleep (clock_main, 1000000, &remain);
   
	   server_stop (server);
   }

   if(!strcmp(playmode, "netcast"))
   {
	   ogmp_client_t* clie = client_new(sipua);
	   client_communicate(clie, SEND_CNAME, strlen(SEND_CNAME));
   }
   
   return 0;
}
