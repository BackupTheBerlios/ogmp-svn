/***************************************************************************
                          sipua.c  -  user agent
                             -------------------
    begin                : Wed Jul 14 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <timedia/xmalloc.h>
#include <timedia/ui.h>

#include "sipua.h"
#include <stdlib.h>
#include <string.h>

#include <xrtp/xrtp.h>
 
#define UA_LOG
#define UA_DEBUG

#ifdef UA_LOG
 #include <stdio.h>
 #define ua_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define ua_log(fmtargs)
#endif

#ifdef UA_DEBUG
 #include <stdio.h>
 #define ua_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define ua_debug(fmtargs)
#endif

#define MAX_REC 8
#define MAX_REC_CAP 8
#define MAX_CN_BYTES 256 /* max value in rtcp */

int sipua_book_pt(int *pt)
{
	int i;

	for(i=96; i<=127; i++)
	{
		if(pt[i] == 0)
		{
			pt[i] = 1;
			return i;
		}
	}
	

	return -1;
}

int sipua_release_pt(int *pool, int pt)
{
	pool[pt] = 0;

	return UA_OK;
}

char* sipua_userloc(sipua_t* sipua, char* uid)
{
	int nb, ub;
	char *nettype, *addrtype, *netaddr;

	char *loc, *p;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);

	ub = strlen(uid);
	nb = strlen(netaddr);

	p = loc = xmalloc(ub+nb+2);

	strcpy(p, uid);
	p += ub;

	*p = '@';
	p++;

	strcpy(p, netaddr);

	return loc;
}

int sipua_locate_user(sipua_t* sipua, user_t* user)
{
	int nb, ub;

	char *p;

	sipua->uas->address(sipua->uas, &user->nettype, &user->addrtype, &user->netaddr);

	ub = strlen(user->uid);
	nb = strlen(user->netaddr);

	if(user->userloc)
		xfree(user->userloc);

    user->userloc = p = xmalloc(ub+nb+2);

	strcpy(p, user->uid);
	p += ub;

	*p = '@';
	p++;

	strcpy(p, user->netaddr);

	return UA_OK;
}

/********************************************
 * Generate a new call, but yet to process.
 */
sipua_call_t* sipua_new_call(sipua_t *ua, user_profile_t* current_prof, char *from,
                              char *subject, int sbytes, char *info, int ibytes)
{
   sipua_call_t* call;

	call = xmalloc(sizeof(sipua_call_t));
	if(!call)
	{
		ua_log(("sipua_receive_call: No memory\n"));
		return NULL;
	}
	memset(call, 0, sizeof(sipua_call_t));

   /* call is not managed by uas */
   call->cid = -1;
   call->did = -1;

   /* *
    * May match to the right profile later, now map to current profile
    */
   call->user_prof = current_prof;

   call->subject = xstr_clone(subject);
   call->sbyte = sbytes;

   call->info = xstr_clone(info);
   call->ibyte = ibytes;

   /* If from is NULL, the call is outgoing call of the user profile. */
   call->from = from;

   return call;
}

/* Prepare the new call with initial sdp */
int sipua_make_call(sipua_t *sipua, sipua_call_t* call, char* id,
							char* mediatypes[], int rtp_ports[], int rtcp_ports[], int nmedia,
							media_control_t* control, int bw_budget,
							rtp_coding_t codings[], int ncoding, int pt_pool[])
{
	char *nettype, *addrtype, *netaddr;

	rtpcap_sdp_t sdp_info;

	int i;

	char tmp[16];
	char tmp2[16];

	module_catalog_t *cata = control->modules(control);

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);

	/* generate id and version */
	if(id == NULL)
	{
		sprintf(tmp, "%i", (int)time(NULL));
		call->setid.id = xstr_clone(tmp);
	}
	else
	{
		/* conference identification */
		call->setid.id = xstr_clone(id);
	}

   call->setid.username = call->user_prof->username;
   
   call->setid.nettype = nettype;
	call->setid.addrtype = addrtype;
	call->setid.netaddr = netaddr;

	sprintf(tmp, "%i", (int)time(NULL));
	call->version = xstr_clone(tmp);

	/*sipua->uas->clear_coding(sipua->uas);*/

   /* generate sdp message */
	sdp_info.sdp_media_pos = 0;
	sdp_message_init(&sdp_info.sdp_message);

	sdp_message_v_version_set(sdp_info.sdp_message, xstr_clone(SDP_VERSION));

	/* those fields MUST be set */
	sdp_message_o_origin_set (sdp_info.sdp_message, 
								xstr_clone(call->user_prof->username),
								xstr_clone(call->setid.id),
								xstr_clone(call->version),
								xstr_clone(nettype),
								xstr_clone(addrtype),
								xstr_clone(netaddr));

	sdp_message_s_name_set (sdp_info.sdp_message, xstr_clone(call->subject));

	sdp_message_i_info_set (sdp_info.sdp_message, -1, xstr_clone(call->info));

	sdp_message_c_connection_add (sdp_info.sdp_message, 
									-1, /* media_pos */
									xstr_clone(nettype), /* IN */
									xstr_clone(addrtype), /* IP4 */
									xstr_clone(netaddr),
									NULL, /* multicast_ttl */ 
									NULL /* multicast_int */);

	/* offer-answer draft says we must copy the "t=" line */
	sprintf (tmp, "%i", 0);
	sprintf (tmp2, "%i", 0);

	if (sdp_message_t_time_descr_add (sdp_info.sdp_message, tmp, tmp2) != 0)
	{
		sdp_message_free(sdp_info.sdp_message);

      sdp_message_free(sdp_info.sdp_message);

      return UA_FAIL;
	}

   for(i=0; i<ncoding; i++)
	{
		int media_bw;
		int pt;

		char mediatype[16];
		int rtp_portno = 0;
		int rtcp_portno = 0;
		int j;

		pt = sipua_book_pt(pt_pool);
		if(pt<0)
		{
			ua_log(("sipua_new_session: no payload type available\n"));
			break;
		}

		j = 0;
		do
		{
			mediatype[j] = codings[i].mime[j];
			j++;

		}while(codings[i].mime[j] != '/');

		mediatype[j] = '\0';
	
		for(j=0; j<nmedia; j++)
		{
			if(strcmp(mediatype, mediatypes[j]) == 0)
			{
				rtp_portno = rtp_ports[j];
				rtcp_portno = rtcp_ports[j];

				break;
			}
		}

		media_bw = session_new_sdp(cata, nettype, addrtype, netaddr, &rtp_portno, &rtcp_portno, pt, codings[i].mime, codings[i].clockrate, codings[i].param, bw_budget, control, &sdp_info);

		if(media_bw > 0 && bw_budget > call->bandwidth_need + media_bw)












		{
			call->bandwidth_need += media_bw;
		}
		else
			break;
	}

   sdp_message_to_str (sdp_info.sdp_message, &call->sdp_body);
   
   /* if free here, posix dore dump ??? win32 ok !!!
    sdp_message_free(sdp_info.sdp_message);
    */

	return UA_OK;
}

#if 0
/* backup */
/* Create a empty call with initial sdp */
sipua_call_t* sipua_make_call(sipua_t *sipua, user_profile_t* user_prof, char* id,
							   char* subject, int sbyte, char* info, int ibyte,
							   char* mediatypes[], int rtp_ports[], int rtcp_ports[], int nmedia,
							   media_control_t* control, int bw_budget,
							   rtp_coding_t codings[], int ncoding, int pt_pool[])
{
	sipua_call_t* call;

	char *nettype, *addrtype, *netaddr;

	rtpcap_sdp_t sdp_info;

	int i;

	char tmp[16];
	char tmp2[16];

	module_catalog_t *cata = control->modules(control);

	call = xmalloc(sizeof(sipua_call_t));
	if(!call)
	{
		ua_log(("sipua_new_set: No memory\n"));
		return NULL;
	}
	memset(call, 0, sizeof(sipua_call_t));

	call->user_prof = user_prof;

	call->subject = xstr_clone(subject);
	call->sbyte = sbyte;

	call->info = xstr_clone(info);
	call->ibyte = ibyte;

	ua_log(("sipua_make_call: \n"));
	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);

	/* generate id and version */
	if(id == NULL)
	{
		sprintf(tmp, "%i", (int)time(NULL));
		call->setid.id = xstr_clone(tmp);
	}
	else
	{
		/* conference identification */
		call->setid.id = xstr_clone(id);
		call->setid.username = user_prof->username;

		call->setid.nettype = nettype;
		call->setid.addrtype = addrtype;
		call->setid.netaddr = netaddr;
	}

	sprintf(tmp, "%i", (int)time(NULL));
	call->version = xstr_clone(tmp);

	/*sipua->uas->clear_coding(sipua->uas);*/

   /* generate sdp message */
	sdp_info.sdp_media_pos = 0;
	sdp_message_init(&sdp_info.sdp_message);

	sdp_message_v_version_set(sdp_info.sdp_message, xstr_clone(SDP_VERSION));

	/* those fields MUST be set */
	sdp_message_o_origin_set (sdp_info.sdp_message,
								xstr_clone(call->user_prof->username),
								xstr_clone(call->setid.id),
								xstr_clone(call->version),
								xstr_clone(nettype),
								xstr_clone(addrtype),
								xstr_clone(netaddr));

	sdp_message_s_name_set (sdp_info.sdp_message, xstr_clone(call->subject));

	sdp_message_i_info_set (sdp_info.sdp_message, -1, xstr_clone(call->info));

	sdp_message_c_connection_add (sdp_info.sdp_message,
									-1, /* media_pos */
									xstr_clone(nettype), /* IN */

									xstr_clone(addrtype), /* IP4 */
									xstr_clone(netaddr),
									NULL, /* multicast_ttl */
									NULL /* multicast_int */);

	/* offer-answer draft says we must copy the "t=" line */
	sprintf (tmp, "%i", 0);
	sprintf (tmp2, "%i", 0);

	if (sdp_message_t_time_descr_add (sdp_info.sdp_message, tmp, tmp2) != 0)
	{
		sdp_message_free(sdp_info.sdp_message);

		xfree(call);

		return NULL;
	}

   for(i=0; i<ncoding; i++)
	{
		int media_bw;
		int pt;

		char mediatype[16];

		int rtp_portno = 0;
		int rtcp_portno = 0;
		int j;

		pt = sipua_book_pt(pt_pool);
		if(pt<0)
		{
			ua_log(("sipua_new_session: no payload type available\n"));
			break;
		}

		j = 0;
		do
		{
			mediatype[j] = codings[i].mime[j];
			j++;

		}while(codings[i].mime[j] != '/');

		mediatype[j] = '\0';

		for(j=0; j<nmedia; j++)
		{
			if(strcmp(mediatype, mediatypes[j]) == 0)
			{
				rtp_portno = rtp_ports[j];
				rtcp_portno = rtcp_ports[j];

				break;
			}
		}

		media_bw = session_new_sdp(cata, nettype, addrtype, netaddr, &rtp_portno, &rtcp_portno, pt, codings[i].mime, codings[i].clockrate, codings[i].param, bw_budget, control, &sdp_info);

		if(media_bw > 0 && bw_budget > call->bandwidth_need + media_bw)
		{
			call->bandwidth_need += media_bw;
		}
		else
			break;
	}

   sdp_message_to_str (sdp_info.sdp_message, &call->sdp_body);

   /* if free here, posix dore dump ??? win32 ok !!!
    sdp_message_free(sdp_info.sdp_message);
    */

	return call;
}
#endif

/* Create a new call from incoming sdp, and generate new sdp */
int sipua_negotiate_call(sipua_t *sipua, sipua_call_t *call,
								rtpcap_set_t *rtpcapset, char *mediatypes[], 
								int rtp_ports[], int rtcp_ports[], int nmedia, 
								media_control_t *control, int call_max_bw)
{
	rtpcap_descript_t* rtpcap;
	xlist_user_t u;

	rtpcap_sdp_t sdp_info;

	char *nettype, *addrtype, *netaddr;

	char tmp[16];
	char tmp2[16];

	int bw_budget;
	int call_bw;

	module_catalog_t *cata = control->modules(control);

	bw_budget = control->book_bandwidth(control, call_max_bw);
	if(bw_budget == 0)
	{
		/* No enough bandwidth to make call */
		ua_log(("sipua_new_set: No bandwidth\n"));
		return UA_FAIL;
	}

	call->bandwidth_need = bw_budget;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);
   
	call->version = xstr_clone(rtpcapset->version);

   /* call owner identification */
	call->setid.id = xstr_clone(rtpcapset->callid);
	call->setid.username = rtpcapset->username;
	call->setid.nettype = xstr_clone(rtpcapset->nettype);
	call->setid.addrtype = xstr_clone(rtpcapset->addrtype);
	call->setid.netaddr = xstr_clone(rtpcapset->netaddr);

	sipua->uas->clear_coding(sipua->uas);

	/* generate sdp message */
	/* sdp_info = xmalloc(sizeof(rtpcap_sdp_t)); */

	sdp_info.sdp_media_pos = 0;
	sdp_message_init(&sdp_info.sdp_message);

	sdp_message_v_version_set(sdp_info.sdp_message, xstr_clone(SDP_VERSION));

	/* Not used in negotiation, but can identify call */
	sdp_message_o_origin_set (sdp_info.sdp_message, 
								xstr_clone(rtpcapset->username),
								xstr_clone(rtpcapset->callid),
								xstr_clone(rtpcapset->version),
								xstr_clone(rtpcapset->nettype),
								xstr_clone(rtpcapset->addrtype),
								xstr_clone(rtpcapset->netaddr));

	sdp_message_s_name_set (sdp_info.sdp_message, xstr_clone(rtpcapset->subject));

	/* My net connection */
	sdp_message_c_connection_add (sdp_info.sdp_message, 
									-1 /* media_pos */,
									xstr_clone(nettype) /* IN */,
									xstr_clone(addrtype) /* IP4 */,
									xstr_clone(netaddr),
									NULL /* multicast_ttl */, 
									NULL /* multicast_int */);

	/* offer-answer draft says we must copy the "t=" line */
	sprintf (tmp, "%i", 0);
	sprintf (tmp2, "%i", 0);

	if (sdp_message_t_time_descr_add (sdp_info.sdp_message, tmp, tmp2) != 0)
	{
		sdp_message_free(sdp_info.sdp_message);

		return UA_FAIL;
	}
	
	rtpcap = xlist_first(rtpcapset->rtpcaps, &u);

	call_bw = 0;

   while(rtpcap)
	{
		int media_bw;

		char mediatype[16];

      int j = 0;
	
		do
		{
			mediatype[j] = rtpcap->profile_mime[j];
			j++;

		}while(rtpcap->profile_mime[j] != '/');

		mediatype[j] = '\0';
	
		rtpcap->local_rtp_portno = 0;
		rtpcap->local_rtcp_portno = 0;

		for(j=0; j<nmedia; j++)
		{
			if(strcmp(mediatype, mediatypes[j]) == 0)
			{
				rtpcap->local_rtp_portno = rtp_ports[j];
				rtpcap->local_rtcp_portno = rtcp_ports[j];

				break;
			}
		}

		if(j < nmedia && !rtpcap->enable)
		{
			media_bw = session_new_sdp(cata, nettype, addrtype, netaddr, 
							&rtpcap->local_rtp_portno, &rtpcap->local_rtcp_portno, 
							rtpcap->profile_no, rtpcap->profile_mime, 
							rtpcap->clockrate, rtpcap->coding_param, 
							bw_budget, control, &sdp_info);
			
			printf("sipua_new_set: media_bw[%d]\n", media_bw);

			if(media_bw == 0)
            {

                /* Not support this rtpcap */
                rtpcap = xlist_next(rtpcapset->rtpcaps, &u);

                continue;
            }
                
			if(media_bw < 0 || bw_budget < call_bw + media_bw)
				break;

			rtpcap->enable = 1;

			call_bw += media_bw;
		}
		
		rtpcap = xlist_next(rtpcapset->rtpcaps, &u);
	}

   call->bandwidth_need = call_bw;

	sdp_message_to_str(sdp_info.sdp_message, &call->reply_body);

    /* FIXME: If free here, lead to segment fault on linux(POSIX)
     * But why?
	sdp_message_free(sdp_info.sdp_message);
    */
    
    /*
	printf("sipua_negotiate_call:\n");
	printf("-------Reply SDP----------\n");
	printf("CallId[%s]\n", call->setid.id);
	printf("--------------------------\n");
	printf("%s\n", call->reply_body);
	printf("--------------------------\n");
	getchar();
	*/
    
	return UA_OK;
}

int sipua_done_maker(void *gen)
{
   media_maker_t* maker = (media_maker_t*)gen;
   return maker->done(maker);
}

/* *
 * Open an output for a media stream.
 */
int sipua_open_stream_output(media_control_t* control, sipua_call_t* call, media_stream_t* outstream, char* mediatype, char* playmode)
{                 
   int ret;
   
   media_info_t *minfo;
   media_player_t* outplayer;

   minfo = outstream->media_info;
   outplayer = control->find_player(control, playmode, minfo->mime, minfo->fourcc);

   if(!outplayer)
	   return MP_FAIL;

   ret = outplayer->link_stream(outplayer, outstream, control, call->rtpcapset);
   if(ret < MP_OK)
   {
      outplayer->done(outplayer);
      return ret;
   }

   return MP_OK;
}

/* *
 * Open an input for a media stream.
 */
int sipua_open_stream_input(media_control_t* control, media_stream_t* outstream, char* mediatype, char* iomode)
{
    media_maker_t* maker = NULL;

	 xlist_t* maker_mods;

	 int num;

	 module_catalog_t * mod_cata = NULL;

    control_setting_t *setting = NULL;
    media_device_t *dev = NULL;

	 xlist_user_t $lu;

    media_info_t *minfo = outstream->media_info;

    ua_log(("source_open_device: need media device\n"));
    dev = control->find_device(control, mediatype, iomode);

   if(!dev)
   {
      ua_debug(("source_open_device: No %s found\n\n", mediatype));
	   return UA_FAIL;
   }

   setting = control->fetch_setting(control, mediatype, dev);
   if(!setting)
   {
      ua_log(("source_open_device: use default setting for '%s' device\n", mediatype));
   }
   else
   {
      dev->setting(dev, setting, mod_cata);
   }

	mod_cata = control->modules(control);

   maker_mods = xlist_new();

	num = catalog_create_modules (mod_cata, "create", maker_mods);

	maker = (media_maker_t *) xlist_first (maker_mods, &$lu);
	while (maker)
	{
		if(maker->receiver.match_type(&maker->receiver, minfo->mime, minfo->fourcc))
		{
			xlist_remove_item(maker_mods, maker);
         maker->link_stream(maker, outstream, control, dev, minfo);
         
         break;
		}

		maker = (media_maker_t *)xlist_next(maker_mods, &$lu);
	}

   xlist_done(maker_mods, sipua_done_maker);

   return UA_OK;
}

int sipua_link_medium(sipua_t* sipua, sipua_call_t* call, media_control_t* control)
{
   media_info_t* minfo = NULL;

   media_format_t* rtpfmt = NULL;
   
   media_stream_t* instream = NULL;
   media_stream_t* outstream = NULL;

   int i, n_input = 0;
   char mediatype[33];
   
   rtpfmt = call->rtp_format;   
   instream = rtpfmt->first;
   
   while(instream)
   {
      outstream = instream->peer;

      minfo = (media_info_t*)session_mediainfo(((rtp_stream_t*)instream)->session);

      /* Detremine the media type */
      i=0;

      while(minfo->mime[i] != '/')
      {
         mediatype[i] = minfo->mime[i];
         i++;
      }

      mediatype[i] = '\0';

      if(sipua_open_stream_output (control, call, outstream, mediatype, "netcast") < MP_OK)
      {
         instream = instream->next;
         continue;
      }
      
      sipua_open_stream_input (control, outstream, mediatype, "input");
      
      outstream->start(outstream);

      n_input++;
	   instream = instream->next;
   }

   return n_input;
}

int sipua_establish_call(sipua_t* sipua, sipua_call_t* call, char *mode, rtpcap_set_t* rtpcapset,
                        xlist_t* format_handlers, media_control_t* control, int pt_pool[])
{
	dev_rtp_t* rtpdev;

	xlist_user_t u;

	char *nettype, *addrtype, *netaddr;

	int bw = 0;
	module_catalog_t *cata;

	/* define a player */
	media_format_t *format;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);
	
	cata = control->modules(control);

	/* create default rtp session */
	rtpdev = (dev_rtp_t*)control->find_device(control, "rtp", "inout");

	format = (media_format_t *)xlist_first(format_handlers, &u);
	while (format)
	{
		/* open media source */
		if(format->support_type(format, "mime", "application/sdp"))
		{
			rtp_format_t* rtpfmt = (rtp_format_t*)format;

			printf("sipua_establish_call: call needs bandwidth[%d]\n", call->bandwidth_need);

			bw = rtpfmt->open_capables(format, rtpcapset, control, mode, call->bandwidth_need);
			if(bw > 0)
			{
				call->rtp_format = format;
			}
			else
			{
				printf("sipua_establish_call: no enough bandwidth\n");
				return 0;
			}

			break;
		}
      
		format = (media_format_t*) xlist_next(format_handlers, &u);
	}

	if(call->rtp_format == NULL)
	{
		ua_log(("sipua_establish_call: no format support\n"));
		return 0;
	}
     
	/**
	 * rtp_media = session_media(rtp_session);
	 * xlist_addto_first(call->mediaset, rtp_media);
	 */
	if(call->bandwidth_need - bw > 0)
		control->release_bandwidth(control, call->bandwidth_need - bw);

	call->bandwidth_need = bw;
   call->rtpcapset = rtpcapset;

	/* create a input source */
	if(call->status == SIP_STATUS_CODE_OK)
	   sipua_link_medium(sipua, call, control);    
   
	ua_log(("sipua_establish_call: call[%s] established\n", rtpcapset->subject));

	return bw;
}

int sipua_receive(sipua_t *ua, sipua_call_event_t *call_e, char **from, char **subject, int *sbytes, char **info, int *ibytes)
{
   *subject = call_e->subject;
   *sbytes = strlen(call_e->subject)+1;
   
   *info = call_e->textinfo;
   *ibytes = strlen(call_e->textinfo)+1;

   *from = call_e->remote_uri;

   return UA_OK;
}

/* Create a SDP with new call of media info */
char* sipua_call_sdp(sipua_t *sipua, sipua_call_t* call, int bw_budget, media_control_t* control,
                char* mediatypes[], int rtp_ports[], int rtcp_ports[], int nmedia,
                media_source_t* source, int pt_pool[])
{       
	char *sdp_body = NULL;
	char *nettype, *addrtype, *netaddr;
 
	rtpcap_sdp_t sdp_info;

    int pt;   
   
	char tmp[16];          
	char tmp2[16];

	module_catalog_t *cata = control->modules(control);
    
    media_format_t *src_fmt = source->format;
    media_stream_t *src_strm;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);
 
	/*sipua->uas->clear_coding(sipua->uas);*/

	/* generate sdp message */
	sdp_info.sdp_media_pos = 0;
	sdp_message_init(&sdp_info.sdp_message);

	sdp_message_v_version_set(sdp_info.sdp_message, xstr_clone(SDP_VERSION));

	/* those fields MUST be set */
	sdp_message_o_origin_set (sdp_info.sdp_message,
								xstr_clone(call->user_prof->username),
								xstr_clone(call->setid.id),
								xstr_clone(call->version),
								xstr_clone(nettype),
								xstr_clone(addrtype),
								xstr_clone(netaddr));

	sdp_message_s_name_set (sdp_info.sdp_message, xstr_clone(call->subject));

	sdp_message_i_info_set (sdp_info.sdp_message, -1, xstr_clone(call->info));

	sdp_message_c_connection_add (sdp_info.sdp_message,
									-1, /* media_pos */
									xstr_clone(nettype), /* IN */
									xstr_clone(addrtype), /* IP4 */
									xstr_clone(netaddr),
									NULL, /* multicast_ttl */
									NULL /* multicast_int */);
         
	/* offer-answer draft says we must copy the "t=" line */
	sprintf (tmp, "%i", 0);
	sprintf (tmp2, "%i", 0);
   
   if (sdp_message_t_time_descr_add (sdp_info.sdp_message, tmp, tmp2) != 0)
	{
		sdp_message_free(sdp_info.sdp_message);

		return NULL;
	}   

   src_strm = src_fmt->first;
   while(src_strm)
	{
		int media_bw;

		char mediatype[16];
		int rtp_portno = 0;
		int rtcp_portno = 0;
      
      media_stream_t *rtp_strm = NULL;

      if(call->rtp_format)
         rtp_strm = call->rtp_format->find_mime(call->rtp_format, src_strm->media_info->mime);

      if(!rtp_strm)
      {
         int j;
            
         pt = sipua_book_pt(pt_pool);
         if(pt<0)
         {
            ua_log(("sipua_new_session: no payload type available\n"));
            break;
         }

         j = 0;
         do
         {
            mediatype[j] = src_strm->media_info->mime[j];
            j++;

         }while(src_strm->media_info->mime[j] != '/');

         mediatype[j] = '\0';
         
         for(j=0; j<nmedia; j++)
         {
            if(strcmp(mediatype, mediatypes[j]) == 0)
            {

               rtp_portno = rtp_ports[j];
               rtcp_portno = rtcp_ports[j];

               break;
            }
         }

         printf("sipua_call_sdp: rtp[%d] rtcp[%d]\n", rtp_portno, rtcp_portno);
         
         /* Cause to ceate a new session instance */
         media_bw = session_new_sdp(cata, nettype, addrtype, netaddr, &rtp_portno, &rtcp_portno, pt, src_strm->media_info->mime, src_strm->media_info->sample_rate, src_strm->media_info->coding_parameter, (bw_budget - call->bandwidth_hold), control, &sdp_info);

         printf("sipua_call_sdp: sample_rate[%d]\n", src_strm->media_info->sample_rate);
         printf("sipua_call_sdp: coding_parameter[%d]\n", src_strm->media_info->coding_parameter);
         
		   printf("sipua_call_sdp: media_bw[%d]\n", media_bw);
      }
      else
		{
			/* The existed session */
			rtp_stream_t* rtpstream = (rtp_stream_t*)rtp_strm;
			media_bw = session_mediainfo_sdp(rtpstream->session, nettype, addrtype, netaddr, &rtp_portno, &rtcp_portno, rtp_strm->media_info->mime, (bw_budget - call->bandwidth_need), control, &sdp_info, rtp_strm->media_info);
		}
        
		printf("sipua_call_sdp: mime[%s]\n", src_strm->media_info->mime);
		printf("sipua_call_sdp: pt[%d]\n", pt);

		if(media_bw > 0 && bw_budget > call->bandwidth_need + media_bw)
		{
			call->bandwidth_need += media_bw;
		}
		else
		{
			break;
		}

		src_strm = src_strm->next;
	}

	sdp_message_to_str (sdp_info.sdp_message, &sdp_body);

	/* if free here, posix dore dump ??? win32 ok !!!
    sdp_message_free(sdp_info.sdp_message);
    */
   return sdp_body;
}

#if 0
int sipua_add(sipua_t *sipua, sipua_call_t* call, xrtp_media_t* rtp_media, int bandwidth)
{
	int bw = bandwidth - session_bandwidth(rtp_media->session);
	if(bw >= 0)
		xlist_addto_first(call->mediaset, rtp_media);

	return bw;
}

int sipua_remove(sipua_t *sipua, sipua_call_t* call, xrtp_media_t* rtp_media)
{
	ua_log(("sipua_remove: FIXME - yet to implement\n"));

	return UA_OK;
}
#endif

#if 0

int sipua_session_sdp(sipua_t *sipua, sipua_call_t* call, char** sdp_body)
{

	rtpcap_sdp_t sdp;
	xrtp_media_t* rtp_media;

	xlist_user_t u;

	char tmp[16];
	char tmp2[16];

	char *nettype, *addrtype, *netaddr;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);

	sdp.sdp_media_pos = 0;
	sdp_message_init(&sdp.sdp_message);

	sdp_message_v_version_set(sdp.sdp_message, xstr_clone(SDP_VERSION));

	/* those fields MUST be set */
	sdp_message_o_origin_set(	sdp.sdp_message, 
								xstr_clone(call->user_prof->username),
								xstr_clone(call->setid.id),
								xstr_clone(call->version),
								xstr_clone(nettype),
								xstr_clone(addrtype),
								xstr_clone(netaddr));

	sdp_message_s_name_set (sdp.sdp_message, xstr_clone(call->subject));

	sdp_message_c_connection_add (sdp.sdp_message, 
									-1 /* media_pos */,
									xstr_clone(nettype) /* IN */,
									xstr_clone(addrtype) /* IP4 */,


									xstr_clone(netaddr),
									NULL /* multicast_ttl */, 
									NULL /* multicast_int */);

	/* offer-answer draft says we must copy the "t=" line */
	sprintf (tmp, "%i", 0);
	sprintf (tmp2, "%i", 0);

	if (sdp_message_t_time_descr_add (sdp.sdp_message, tmp, tmp2) != 0)
	{
		sdp_message_free(sdp.sdp_message);
		return 0;
	}

	rtp_media = xlist_first(call->mediaset, &u);
	while(rtp_media)
	{
		rtp_media->sdp(rtp_media, &sdp);

		rtp_media = xlist_next(call->mediaset, &u);
	}

	sdp_message_to_str(sdp.sdp_message, sdp_body);

	sdp_message_free(sdp.sdp_message);


	return UA_OK;

}
#endif    


int sipua_regist(sipua_t *sipua, user_profile_t *user, char *userloc)
{
   int ret;

   ret = sipua->uas->regist(sipua->uas, &user->regno, userloc, user->registrar, user->regname, user->seconds);

	if(ret < UA_OK)
		user->reg_status = SIPUA_STATUS_REG_FAIL;
	else
	{
		user->cname = userloc;
		user->reg_status = SIPUA_STATUS_REG_DOING;
	}

	return ret;
}

int sipua_unregist(sipua_t *sipua, user_profile_t *user)
{
	int ret;
	char* siploc, *p;
	
	p = siploc = xmalloc(strlen(user->cname)+5);
	if(!siploc)
		return UA_FAIL;

	strcpy(p, "sip:");
	p += 4;
	strcpy(p, user->cname);

	ret =  sipua->uas->unregist(sipua->uas, siploc, user->registrar, user->regname);

	if(ret < UA_OK)
		user->reg_status = SIPUA_STATUS_REG_FAIL;
	else
	{
		user->cname = NULL;

		user->reg_status = SIPUA_STATUS_UNREG_DOING;
	}

	xfree(siploc);

	return ret;
}

int sipua_retry_call(sipua_t *ua, sipua_call_t* call)
{
   return ua->uas->retry(ua->uas, call);
}

int sipua_call(sipua_t *sipua, sipua_call_t* call, const char *callee, char *sdp_body) 
{
	int ret;

	ua_log(("sipua_call: Call from [%s] to [%s]\n", call->user_prof->regname, callee));

   /* NOTE: size of SDP body MUST NOT count in string terminal */
   ret = sipua->uas->invite(sipua->uas, callee, call, sdp_body, strlen(sdp_body));

   if(ret < UA_OK)
   {
	   ua_debug(("sipua_call: Call from [%s] to [%s] fail\n", call->user_prof->regname, callee));
		return ret;
   }

	return UA_OK;
}

int sipua_answer(sipua_t *ua, sipua_call_t* call, int reply, char *sdp_body)
{
	return ua->uas->answer(ua->uas, call, reply, "application/sdp", sdp_body);
}

int sipua_options_call(sipua_t *ua, sipua_call_t* call)
{
	ua_log(("sipua_options_call: FIXME - yet to implement\n"));
	return UA_OK;
}
int sipua_info_call(sipua_t *ua, sipua_call_t* call, char *type, char *info)
{
	ua_log(("sipua_info_call: FIXME - yet to implement\n"));

	return UA_OK;
}

int sipua_bye(sipua_t *sipua, sipua_call_t* call)
{
	ua_log(("sipua_disconnect: FIXME - yet to implement\n"));

	return UA_OK;
}

int sipua_recall(sipua_t *sipua, sipua_call_t* call)
{
	ua_log(("sipua_reconnect: FIXME - yet to implement\n"));

	return UA_OK;
}
