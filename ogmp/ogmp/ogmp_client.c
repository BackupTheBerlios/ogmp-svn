/***************************************************************************
                          ogmp_client.c
                             -------------------
    begin                : Tue Jul 20 2004
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

#include "format_rtp/rtp_format.h"

#include <timedia/xstring.h>
#include <timedia/xmalloc.h>

#define CLIE_LOG

#ifdef CLIE_LOG
 #define clie_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define clie_log(fmtargs)
#endif

int client_done_format_handler(void *gen)
{
   media_format_t * fmt = (media_format_t *)gen;

   fmt->done(fmt);

   return MP_OK;
}

/**
 * return capability number, if error, return ZERO
 */
int client_setup(ogmp_client_t *client, char *src_cn, int src_cnlen, capable_descript_t *caps[], int ncap, char *mode)
{
   /* define a player */
   //media_player_t *playa = NULL;
   config_t * conf;

   xlist_user_t $lu;
   int nmod;

   module_catalog_t * mod_cata = NULL;
   media_format_t *format;

   clie_log (("client_setup: modules in dir:%s\n", MODDIR));
   
   /* Initialise */
   conf = conf_new ( "ogmprc" );
   
   mod_cata = catalog_new( "mediaformat" );
   catalog_scan_modules ( mod_cata, VERSION, MODDIR );
   
   client->format_handlers = xlist_new();

   nmod = catalog_create_modules (mod_cata, "format", client->format_handlers);
   clie_log (("server_setup: %d format module found\n", nmod));

   /* set audio client */
   client->valid = 0;
   
   /* player controler */
   client->control = new_media_control();
   client->control->config(client->control, conf, mod_cata);
   client->control->add_device(client->control, "rtp", client_config_rtp, conf);
   
   /* create a media format, also get the capabilities of the file */
   format = (media_format_t *) xlist_first(client->format_handlers, &$lu);
   while (format)
   {
      /* open media source */
      client->ncap = format->open_capables(format, src_cn, src_cnlen, caps, ncap, client->control, conf, mode, client->caps);
	  if(client->ncap > 0)
	  {
		  client->rtp_format = format;
		  break;
	  }
      
      format = (media_format_t*) xlist_next(client->format_handlers, &$lu);
   }

   /* open media source */
   if (!client->rtp_format)
   {
      clie_log(("ogmp_client_setup: no format support\n"));

      return 0;
   }

   client->control->set_format (client->control, "rtp", format);

   client->valid = 1;

   return client->ncap;
}

/****************************************************************************************
 * SIP Callbacks
 */
int client_action_oncall(void *user, char *from_cn, int from_cnlen, capable_descript_t *from_caps[], int from_ncap, capable_descript_t* **selected_caps)
{
	ogmp_client_t *client = (ogmp_client_t*)user;

	clie_log(("\ncb_client_oncall: call from cn[%s]\n\n", from_cn));

	if(from_ncap>0)
	{
		rtpcap_descript_t *rtpcap = (rtpcap_descript_t*)from_caps[0];

		clie_log(("cb_client_callin: call from cn[%s]@[%s:%u/%u]\n", from_cn, rtpcap->ipaddr, rtpcap->rtp_portno, rtpcap->rtcp_portno));

		/* parameter setting stage should be here, eg: rtsp conversation */

		/* rtp session establish */
		client_setup(client, from_cn, from_cnlen, from_caps, from_ncap, "playback");

		*selected_caps = client->caps;

		clie_log(("\ncallback_client_oncall: cn[%s] support %d capables\n\n", RECV_CNAME, client->ncap));

		return client->ncap;
	}

	clie_log(("\ncallback_client_oncall: cn[%s] disabled\n\n", from_cn));

	return 0;
}

int client_action_onconnect(void *user, char *from_cn, int from_cnlen, capable_descript_t* from_caps[], int from_ncap, capable_descript_t* **selected_caps)
{
	ogmp_client_t *client = (ogmp_client_t*)user;

	clie_log(("\ncallback_client_onconnect: with cn[%s] requires %d capables\n\n", from_cn, from_ncap));

	if(from_ncap>0)
	{
		client_setup(client, from_cn, from_cnlen, from_caps, from_ncap, "playback");

		*selected_caps = client->caps;

		clie_log(("\ncallback_client_onconnect: cn[%s] support %d capables\n\n", RECV_CNAME, client->ncap));

		return client->ncap;
	}

	clie_log(("\ncallback_client_onconnect: cn[%s] disabled\n\n", from_cn));

	return 0;
}

/* change call status */
int client_action_onreset(void *user, char *from_cn, int from_cnlen, capable_descript_t *caps[], int ncap, capable_descript_t* **selected_caps)
{
	clie_log(("callback_client_onreset: reset from cn[%s]\n", from_cn));

	return ncap;
}

int client_action_onbye(void *user, char *from_cn, int cnlen)
{
	clie_log(("callback_client_onbye: bye from cn[%s]\n", from_cn));

	return UA_OK;
}
/****************************************************************************************/

int client_done(ogmp_client_t *client)
{
   client->rtp_format->close(client->rtp_format);
   
   xlist_done(client->format_handlers, client_done_format_handler);
   xthr_done_lock(client->course_lock);
   xthr_done_cond(client->wait_course_finish);
   
   xfree(client);

   return MP_OK;
}

ogmp_client_t* client_new(sipua_t *sipua)
{
	sipua_action_t *act=NULL;
	ogmp_client_t *client=NULL;

	client = xmalloc(sizeof(ogmp_client_t));
	memset(client, 0, sizeof(ogmp_client_t));

	client->course_lock = xthr_new_lock();
	client->wait_course_finish = xthr_new_cond(XTHREAD_NONEFLAGS);

	client->sipua = sipua;

	act = sipua_new_action( client, client_action_oncall,
									client_action_onconnect, 
									client_action_onreset,
									client_action_onbye);

	sipua->regist(sipua, NULL, RECV_CNAME, strlen(RECV_CNAME)+1, 3600, act);

	clie_log(("client_new: client sipua online\n"));
	return client;
}

int client_communicate(ogmp_client_t *clie, char *to_cn, int to_cnlen)
{
	xthr_lock(clie->course_lock);

	clie->sipua->connect(clie->sipua, 
						RECV_CNAME, strlen(RECV_CNAME), 
						to_cn, to_cnlen, 
						"ogmp_client_test", strlen("ogmp_client_test"),
						NULL, 0);

	xthr_cond_wait(clie->wait_course_finish, clie->course_lock);
	xthr_unlock(clie->course_lock);

	return MP_OK;
}


