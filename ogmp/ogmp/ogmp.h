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
#include <timedia/catalog.h>
#include <timedia/config.h>

#include <stdlib.h>
#include <string.h>

#include "sipua.h"

#define VERSION 1

#define SEND_IP "127.0.0.1"

#define SEND_CNAME "rtp_server"
#define RECV_CNAME "rtp_client"

#define MAX_NCAP 16

#ifndef MODDIR
 #define MODDIR "."
#endif

#define FNAME "Dido_LifeForRent.ogg"

#if 0
typedef struct rtpcap_descript_s rtpcap_descript_t; 
struct rtpcap_descript_s
{
	struct capable_descript_s descript;

	char *profile_mime;
	uint8 profile_no;

	char *ipaddr;
	uint16 rtp_portno;
	uint16 rtcp_portno;
};
rtpcap_descript_t* rtp_capable_descript();
#endif

typedef struct media_transmit_s media_transmit_t;
struct media_transmit_s
{
   struct media_player_s player;

   int (*add_destinate)(media_transmit_t *trans, char *cname, int cnlen, char *ipaddr, uint16 rtp_port, uint16 rtcp_port);
   int (*delete_destinate)(media_transmit_t *trans, char *cname, int cnlen);

   int (*add_source)(media_transmit_t *trans, char *cname, int cnlen, char *ipaddr, uint16 rtp_port, uint16 rtcp_port);
   int (*delete_source)(media_transmit_t *trans, char *cname, int cnlen);
};

typedef struct rtp_setting_s rtp_setting_t;
typedef struct 
{
	char *profile_mime;

	uint8 profile_no;

	uint16 rtp_portno;
	uint16 rtcp_portno;

	int total_bw;
	int rtp_bw;

} rtp_profile_setting_t;

struct rtp_setting_s
{
	struct control_setting_s setting;

	int cnlen;
	char *cname;
	char *ipaddr;

	uint16 default_rtp_portno;
	uint16 default_rtcp_portno;

	int nprofile;
	rtp_profile_setting_t *profiles;
};

typedef struct ogmp_client_s ogmp_client_t;
typedef struct ogmp_server_s ogmp_server_t;

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

   sipua_t *sipua;

   int nplayer;
   media_player_t* players[MAX_NCAP];

   capable_descript_t* caps[MAX_NCAP];
   capable_descript_t* selected_caps[MAX_NCAP];
};

struct ogmp_client_s
{
   int finish;
   int valid;

   media_control_t *control;

   media_format_t *rtp_format;
   xlist_t *format_handlers;

   sipua_t *sipua;

   int ncap;
   capable_descript_t *caps[MAX_NCAP];

   xthr_lock_t *course_lock;
   xthr_cond_t *wait_course_finish;
};

ogmp_server_t* server_new(sipua_t *sipua, char *fname);
int server_setup(ogmp_server_t *ser, char *fname);
int server_start (ogmp_server_t *ser);
int server_stop (ogmp_server_t *ser);
int server_config_rtp(void *conf, control_setting_t *setting);

ogmp_client_t* client_new(sipua_t *sipua);
int client_setup(ogmp_client_t *client, char *src_cn, int src_cnlen, capable_descript_t *caps[], int ncap, char *mode);
int client_communicate(ogmp_client_t *client, char *cname, int cnlen);
int client_config_rtp(void *conf, control_setting_t *setting);
