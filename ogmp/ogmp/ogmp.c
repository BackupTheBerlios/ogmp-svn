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

int main(int argc, char** argv)
{
	char * playmode = argv[1];
   
	/* define a player */
	xrtp_clock_t * clock_main = NULL;
	xrtp_lrtime_t remain = 0;

	/* sip useragent */
	sipua_uas_t *uas_serv;
	ogmp_server_t *server;

	user_profile_t prof_serv = {"ogmps", "ogmp server", 12, "ogmps@timedia", "12345", "ogmps@timedia", 14400, "", "voip.timedia.org"};

	ogmp_log (("\nmain: modules in dir:%s\n", MODDIR));

	/* Initialise server */
	uas_serv = sipua_uas(5060, "IN", "IP4", NULL, "127.0.0.1");
		
	server = server_new(uas_serv, &prof_serv, 1024*1024);
   
	if(!strcmp(playmode, "playback"))
	{
		server_setup(server, playmode, NULL);
		server_start(server);

		clock_main = time_start();
   
		/* play 7200 seconds audio */
		time_msec_sleep(clock_main, 7200000, &remain);
   
		server_stop(server);
	}

	if(!strcmp(playmode, "netcast"))
	{
		ogmp_client_t* client;
		ogmp_command_t cmd;

		/* Initialise client */
		user_profile_t prof_clie = {"ogmpc@timedia", "ogmpc", "ogmp client", 12, "ogmpc@timedia", "54321", 3600, "myname.net", "voip.timedia.org"};

		client = client_new(&prof_clie, 5070, "IN", "IP4", NULL, "127.0.0.1");

		cmd.type = COMMAND_TYPE_REGISTER;
		cmd.instruction = NULL;

		client_command(client, &cmd);

		client_call(client, "ogmps@timedia");
	}
   
	return 0;
}
