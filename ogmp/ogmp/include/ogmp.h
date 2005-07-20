/***************************************************************************
                          ogmp_source.c  -  description
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

#ifndef OGMP_H
#define OGMP_H

#ifdef __cplusplus
extern "C"{
#endif

#include "media_format.h"

#include <timedia/timer.h>
#include <timedia/xthread.h>
#include <timedia/xmalloc.h>
#include <timedia/catalog.h>
#include <timedia/config.h>
#include <timedia/ui.h>

#include <stdlib.h>
#include <string.h>

#include "sipua.h"
#include "rtp_cap.h"

#define SEND_IP "127.0.0.1"
#define SIP_SESSION_ID "20040922"
#define SIP_SESSION_NAME "ogmp voip"

#define RECV_REGNAME "rtp_client@localhost"
#define RECV_FNAME "voip rtp client"

#define SESSION_SUBJECT "ogmp voip test"
#define SESSION_DESCRIPT "ogmp source and client connectivity"

#define MAX_NCAP 16

#define OGMP_VERSION  1

#define OGMP_VERSION_STRING  "0.1.0"

#ifndef MED_DIR
 #define MED_DIR "samples"
#endif

#define COMMAND_TYPE_GOON		0
#define COMMAND_TYPE_EXIT		1
#define COMMAND_TYPE_REGISTER		2
#define COMMAND_TYPE_UNREGISTER		3

typedef struct ogmp_command_s ogmp_command_t;
typedef struct ogmp_client_s ogmp_client_t;
typedef struct ogmp_source_s ogmp_source_t;

typedef struct ogmp_ui_s ogmp_ui_t;
struct ogmp_ui_s
{
	struct ui_s ui;
    
    int (*set_sipua)(ogmp_ui_t *ogui, sipua_t* sipua);

	ogmp_command_t*(*wait_command)(ogmp_ui_t *ui);
};

struct ogmp_command_s
{
    int type;
    void* instruction;
};

/**
 * OGMP Client
 */
enum sipua_callback_type
{
    SIPUA_CALLBACK_ON_REGISTER=1,
    SIPUA_CALLBACK_ON_NEWCALL,
    SIPUA_CALLBACK_ON_CONVERSATION_START,
    SIPUA_CALLBACK_ON_CONVERSATION_END,
    SIPUA_CALLBACK_ON_BYE,
};

struct ogmp_client_s
{
	sipua_t sipua;

	int finish;
	int valid;

	media_control_t *control;

	media_format_t *rtp_format;

	xlist_t *format_handlers;

	media_source_t *background_source;
	char *background_source_subject;
	char *background_source_info;

	config_t * conf;

	char *sdp_body;

	int registered;

	int ncap;
	capable_descript_t *caps[MAX_NCAP];

	ogmp_ui_t* ogui;

	xthread_t* main_thread;

	xthr_cond_t* wait_unregistered;

	/* below is sipua related */
	user_profile_t* reg_profile;
   user_t *user;

	int expire_sec; /* registration expired seconds, time() */

	uint16 default_rtp_portno;
	uint16 default_rtcp_portno;

	int pt[MAX_PAYLOAD_TYPE];

   /* line #0 reserved for current call */
	sipua_call_t* lines[MAX_SIPUA_LINES+1];   
	xthr_lock_t* lines_lock;

	void* lisener;
	int (*notify_event)(void* listener, sipua_event_t* e);

	int nmedia;
	char* mediatypes[MAX_SIPUA_MEDIA];
	int default_rtp_ports[MAX_SIPUA_MEDIA];
	int default_rtcp_ports[MAX_SIPUA_MEDIA];

	xthread_t *thread_ringing;

	int nring;
	xthr_lock_t *nring_lock;
    
	/* callbacks */
	int (*on_register)(void *user_on_register, int result, char *reason, int isreg);
   void *user_on_register;

	int (*on_progress)(void *user_on_progress, sipua_call_t *call, int statuscode);
   void *user_on_progress;

	int (*on_newcall)(void *user_on_newcall, int lineno, sipua_call_t *call);
   void *user_on_newcall;

	int (*on_terminate)(void *user_on_terminate, sipua_call_t *call, int statuscodes);
   void *user_on_terminate;

   int (*on_conversation_start)(void *user_on_conversation_start, int lineno, sipua_call_t *call);
   void *user_on_conversation_start;

   int (*on_conversation_end)(void *user_on_conversation_end, int lineno, sipua_call_t *call);
   void *user_on_conversation_end;

	int (*on_bye)(void *user_on_bye, int lineno, char *caller, char *reason);
   void *user_on_bye;

	int (*on_authenticate)(void *user_on_authenticate, char *realm, user_profile_t* profile, char** user_id, char** user_password);
   void *user_on_authenticate;
};

struct ogmp_source_s
{
   struct transmit_source_s tsource;

   int finish;

   media_control_t *control;

   xrtp_thread_t *demuxer;
   int demuxing;

   xthr_lock_t *lock;
   xthr_cond_t *wait_request;

   int nstream;
   media_player_t *players[MAX_NCAP];
   media_format_t *format;
};

typedef struct netcast_parameter_s netcast_parameter_t;
struct netcast_parameter_s
{
	char *subject;
	char *info;
	user_profile_t *user_profile;
};

extern DECLSPEC
ui_t* 
client_new_ui(ogmp_client_t *client, module_catalog_t* mod_cata, char* type);

extern DECLSPEC
int
client_init_ui(ogmp_client_t *client, ui_t *ui);

extern DECLSPEC
int 
client_config_rtp(void *conf, control_setting_t *setting);

extern DECLSPEC
sipua_setting_t* client_setting(sipua_t* sipua);

extern DECLSPEC
media_source_t* 
source_open(char* name, media_control_t* control, char* mode, void* mode_param);

sipua_setting_t*
source_setting(media_control_t *control);

int
source_associate(media_source_t* msrc, media_format_t* rtp_fmt, char *cname);

int
source_associate_guests(media_source_t* msrc, media_format_t* rtp_fmt);

/****************************************************************************************/
extern ogmp_ui_t* global_ui;

extern DECLSPEC
sipua_t*
client_new(char *uitype, sipua_uas_t* uas, char* proxy_realm, module_catalog_t* mod_cata, int bandwidth);

extern DECLSPEC
int
client_start(sipua_t* sipua);

extern DECLSPEC
sipua_uas_t*
client_new_uas(module_catalog_t* mod_cata, char* type);


/******************
 * Ogmp Configure *
 ******************/
extern DECLSPEC
config_t* client_load_config(sipua_t *sipua, char* confile);

#ifdef __cplusplus
}
#endif

#endif
