/***************************************************************************
                          ogmp_server.c  -  description
                             -------------------
    begin                : Wed May 19 2004
    copyright            : (C) 2004 by Heming
    email                : heming@bigfoot.com; heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "media_format.h"

#include <timedia/timer.h>
#include <timedia/xthread.h>
#include <timedia/xmalloc.h>
#include <timedia/catalog.h>
#include <timedia/config.h>

#include <stdlib.h>
#include <string.h>

#include "sipua.h"
#include "rtp_cap.h"

#define VERSION 1

#define SEND_IP "127.0.0.1"
#define SIP_SESSION_ID "20040922"
#define SIP_SESSION_NAME "ogmp voip"

#define SEND_REGNAME "rtp_server@localhost"
#define SEND_FNAME "voip rtp server"

#define RECV_REGNAME "rtp_client@localhost"
#define RECV_FNAME "voip rtp client"

#define SESSION_SUBJECT "ogmp voip test"
#define SESSION_DESCRIPT "ogmp server and client connectivity"

#define MAX_NCAP 16

#ifndef MODDIR
 #define MODDIR "."
#endif

#ifndef MEDDIR
 #define MEDDIR "samples"
#endif

//#define FNAME "Dido_LifeForRent.ogg"
#define FNAME "cairo1.spx"

#define COMMAND_TYPE_GOON			0
#define COMMAND_TYPE_EXIT			1
#define COMMAND_TYPE_REGISTER		2
#define COMMAND_TYPE_UNREGISTER		3

typedef struct ogmp_command_s ogmp_command_t;
typedef struct ogmp_client_s ogmp_client_t;
typedef struct ogmp_server_s ogmp_server_t;

typedef struct ogmp_ui_s ogmp_ui_t;
struct ogmp_ui_s
{
	int (*show)(ogmp_ui_t *ui);
	ogmp_command_t*(*wait_command)(ogmp_ui_t *ui);
};

extern DECLSPEC
ogmp_ui_t* 
ogmp_new_ui(sipua_t* sipua);

struct ogmp_command_s
{
	int type;
	void* instruction;
};

struct ogmp_server_s
{
   int finish;
   int valid;

   media_control_t *control;

   xrtp_thread_t *demuxer;

   int demuxing;

   xthr_lock_t *lock;
   xthr_cond_t *wait_request;

   media_format_t *format;
   xlist_t *format_handlers;

   char *fname;

   user_profile_t* user_profile;

   sipua_t *sipua;

   sipua_set_t* call;

   char *sdp_body;

   int registered;

   int nplayer;
   media_player_t* players[MAX_NCAP];

   capable_descript_t* caps[MAX_NCAP];
   capable_descript_t* selected_caps[MAX_NCAP];

   ogmp_ui_t* ui;

   xthread_t* main_thread;
   /*
   xthr_lock_t* command_lock;
   xthr_cond_t* wait_command;
   ogmp_command_t *command;
   */
   xthr_cond_t* wait_unregistered;
};

typedef struct ogmp_setting_s ogmp_setting_t;
struct ogmp_setting_s
{
	char nettype[8];
	char addrtype[8];

	int default_rtp_portno;
	int default_rtcp_portno;

	int ncoding;
	rtp_coding_t codings[MAX_NPAYLOAD_PRESET];
};

ogmp_server_t* server_new(sipua_uas_t* uas, ogmp_ui_t* ui, user_profile_t* user, int bw);
int server_config_rtp(void *conf, control_setting_t *setting);
ogmp_setting_t* server_setting(media_control_t *control);

int server_command(ogmp_server_t *server, ogmp_command_t* cmd);
int server_setup(ogmp_server_t *server, char *mode, void* extra);
int server_start (ogmp_server_t *ser);
int server_stop (ogmp_server_t *ser);

ogmp_client_t* client_new(user_profile_t* user, int portno, char* nettype, char* addrtype, char* firewall, char* proxy, int bandwidth);
int client_config_rtp(void *conf, control_setting_t *setting);
ogmp_setting_t* client_setting(media_control_t *control);

int client_setup(ogmp_client_t *client, char *mode, rtpcap_set_t *rtpcapset);
int client_call(ogmp_client_t *client, char *regname);
