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

/* Create a empty call with initial sdp */
sipua_set_t* sipua_new_call(sipua_t *sipua, user_profile_t* user_prof, char* id, 
							   char* subject, int sbyte, char* info, int ibyte,
							   char* mediatypes[], int rtp_ports[], int rtcp_ports[], int nmedia,
							   media_control_t* control, int bw_budget,
							   rtp_coding_t codings[], int ncoding, int pt_pool[])
{
	sipua_set_t* set;

	char *nettype, *addrtype, *netaddr;

	rtpcap_sdp_t sdp_info;

	int i;

	char tmp[16];
	char tmp2[16];

	module_catalog_t *cata = control->modules(control);

	set = xmalloc(sizeof(sipua_set_t));
	if(!set)
	{
		ua_log(("sipua_new_set: No memory\n"));
		return NULL;
	}
	memset(set, 0, sizeof(sipua_set_t));

	set->user_prof = user_prof;

	set->subject = xstr_clone(subject);
	set->sbyte = sbyte;

	set->info = xstr_clone(info);
	set->ibyte = ibyte;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);
	
	/* generate id and version */
	if(id == NULL)
	{
		sprintf(tmp, "%i", (int)time(NULL));
		set->setid.id = xstr_clone(tmp);
	}
	else
	{
		/* conference identification */
		set->setid.id = xstr_clone(id);
		set->setid.username = user_prof->username;

		set->setid.nettype = xstr_clone(nettype);
		set->setid.addrtype = xstr_clone(addrtype);
		set->setid.netaddr = xstr_clone(netaddr);
	}

	sprintf(tmp, "%i", (int)time(NULL));
	set->version = xstr_clone(tmp);

	/*sipua->uas->clear_coding(sipua->uas);*/

	/* generate sdp message */
	sdp_info.sdp_media_pos = 0;
	sdp_message_init(&sdp_info.sdp_message);

	sdp_message_v_version_set(sdp_info.sdp_message, xstr_clone(SDP_VERSION));

	/* those fields MUST be set */
	sdp_message_o_origin_set (sdp_info.sdp_message, 
								xstr_clone(set->user_prof->username),
								xstr_clone(set->setid.id),
								xstr_clone(set->version),
								xstr_clone(nettype),
								xstr_clone(addrtype),
								xstr_clone(netaddr));

	sdp_message_s_name_set (sdp_info.sdp_message, xstr_clone(set->subject));

	sdp_message_i_info_set (sdp_info.sdp_message, -1, xstr_clone(set->info));

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
		
		xfree(set);

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

		if(media_bw > 0 && bw_budget > set->bandwidth_need + media_bw)
		{
			set->bandwidth_need += media_bw;
		}
		else
			break;
	}

	sdp_message_to_str (sdp_info.sdp_message, &set->sdp_body);

	/* if free here, posix dore dump ??? win32 ok !!!
    sdp_message_free(sdp_info.sdp_message);
    */

	return set;
}

/* Create a new call from incoming sdp, and generate new sdp */
sipua_set_t* sipua_negotiate_call(sipua_t *sipua, user_profile_t* user_prof, 
								rtpcap_set_t* rtpcapset, char* mediatypes[], 
								int rtp_ports[], int rtcp_ports[], int nmedia, 
								media_control_t* control, int call_max_bw)
{
	sipua_set_t* set;

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
		return NULL;
	}

	set = xmalloc(sizeof(sipua_set_t));
	if(!set)
	{
		ua_log(("sipua_new_set: No memory\n"));
		return NULL;
	}
	memset(set, 0, sizeof(sipua_set_t));

	set->bandwidth_need = bw_budget;

	set->user_prof = user_prof;

	set->subject = xstr_clone(rtpcapset->subject);

	set->sbyte = rtpcapset->sbytes;

	set->info = xstr_clone(rtpcapset->info);
	set->ibyte = rtpcapset->ibytes;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);
	
	set->version = xstr_clone(rtpcapset->version);

	/* call owner identification */
	set->setid.id = xstr_clone(rtpcapset->callid);
	set->setid.username = rtpcapset->username;

	set->setid.nettype = xstr_clone(rtpcapset->nettype);
	set->setid.addrtype = xstr_clone(rtpcapset->addrtype);
	set->setid.netaddr = xstr_clone(rtpcapset->netaddr);

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







      xfree(set);

		return NULL;
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

	set->bandwidth_need = call_bw;

	sdp_message_to_str(sdp_info.sdp_message, &set->reply_body);

    /* FIXME: If free here, lead to segment fault on linux(POSIX)
     * But why?
	sdp_message_free(sdp_info.sdp_message);
    */
    
    /*
	printf("sipua_negotiate_call:\n");
	printf("-------Reply SDP----------\n");
	printf("CallId[%s]\n", set->setid.id);
	printf("--------------------------\n");
	printf("%s\n", set->reply_body);
	printf("--------------------------\n");
	getchar();
	*/
    
	return set;
}

/* *
 * Open an input source from given device name
 */
media_maker_t* sipua_open_device(char* devname, char* devmode, media_control_t* control, media_receiver_t* receiver, media_info_t *minfo)
{
   media_maker_t* maker = NULL;

   media_stream_t* stream = NULL;

	xlist_t* maker_mods;

	int num;

	module_catalog_t * mod_cata = NULL;

   control_setting_t *setting = NULL;
   media_device_t *dev = NULL;

	xlist_user_t $lu;

   ua_log(("source_open_device: need audio device\n"));
   dev = control->find_device(control, devname, "input");

   if(!dev)
   {
      ua_debug(("source_open_device: No %s found\n\n", devname));

	   return NULL;
   }

   setting = control->fetch_setting(control, devname, dev);
   if(!setting)
   {
      ua_log(("source_open_device: use default setting for '%s' device\n", devname));
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

         stream = maker->new_media_stream(maker, control, dev, receiver, minfo);
         if(stream)
            return maker;
		}

		maker = (media_maker_t *)xlist_next(maker_mods, &$lu);
	}

   //xlist_done(maker_mods);
   ua_debug(("source_open_device: no maker found\n\n"));

   return NULL;
}

int sipua_new_inputs(sipua_t* sipua, sipua_set_t* call, rtpcap_set_t* rtpcapset, media_control_t* control)
{
   media_maker_t* maker = NULL;
   media_info_t* minfo = NULL;
   
   media_format_t* rtpfmt = NULL;
   media_stream_t* strm = NULL;

   int i, n_input = 0;
   char mediatype[33];
   
   rtpfmt = call->rtp_format;   
   strm = rtpfmt->first;
   
   while(strm)
   {
      minfo = (media_info_t*)session_mediainfo(((rtp_stream_t*)strm)->session);

      i=0;
      while(minfo->mime[i] != '/')
      {
         mediatype[i] = minfo->mime[i];
         i++;
      }
      mediatype[i] = '\0';

      strm->player = control->find_player(control, "netcast", minfo->mime, minfo->fourcc, call->rtpcapset);
      if(strm->player->link_device(strm->player, control, strm, rtpcapset) < MP_OK)
      {
	      strm = strm->next;
         continue;
      }
      
      maker = sipua_open_device(mediatype, "input", control, &strm->player->receiver, minfo);
      if(maker)
      {
         maker->start(maker);
         n_input++;
      }
      
	   strm = strm->next;
   }

   return n_input;
}

int sipua_establish_call(sipua_t* sipua, sipua_set_t* call, char *mode, rtpcap_set_t* rtpcapset,
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

   /* create a input source */
   if(call->status == SIP_STATUS_CODE_OK)
      sipua_new_inputs(sipua, call, rtpcapset, control);
   
	ua_log(("sipua_create_call: call[%s] established\n", rtpcapset->subject));

	return bw;
}

/* Create a SDP with new set of media info */
char* sipua_call_sdp(sipua_t *sipua, sipua_set_t* call, int bw_budget, media_control_t* control,
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
int sipua_add(sipua_t *sipua, sipua_set_t* set, xrtp_media_t* rtp_media, int bandwidth)
{
	int bw = bandwidth - session_bandwidth(rtp_media->session);
	if(bw >= 0)
		xlist_addto_first(set->mediaset, rtp_media);

	return bw;
}

int sipua_remove(sipua_t *sipua, sipua_set_t* set, xrtp_media_t* rtp_media)
{
	ua_log(("sipua_remove: FIXME - yet to implement\n"));

	return UA_OK;
}
#endif

#if 0
int sipua_session_sdp(sipua_t *sipua, sipua_set_t* set, char** sdp_body)
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
								xstr_clone(set->user_prof->username),
								xstr_clone(set->setid.id),
								xstr_clone(set->version),
								xstr_clone(nettype),
								xstr_clone(addrtype),
								xstr_clone(netaddr));

	sdp_message_s_name_set (sdp.sdp_message, xstr_clone(set->subject));

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

	rtp_media = xlist_first(set->mediaset, &u);
	while(rtp_media)
	{
		rtp_media->sdp(rtp_media, &sdp);

		rtp_media = xlist_next(set->mediaset, &u);
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

int sipua_call(sipua_t *sipua, sipua_set_t* call, char *callee, char *sdp_body) 
{
	int ret;

	if(sipua->incall)
    {
        ua_log(("sipua_connect: You are in call, can not make a new call\n"));
        return UA_BUSY;
    }
    
	ua_log(("sipua_call: Call from [%s] to [%s]\n", call->user_prof->regname, callee));

	ret = sipua->uas->invite(sipua->uas, callee, call, sdp_body, strlen(sdp_body)+1);

   if(ret < UA_OK)
   {
	   ua_debug(("sipua_call: Call from [%s] to [%s] fail\n", call->user_prof->regname, callee));
		return ret;
   }
   

   sipua->incall = call;


	return UA_OK;
}

int sipua_answer(sipua_t *ua, sipua_set_t* call, int reply, char *sdp_body)
{
	return ua->uas->answer(ua->uas, call, reply, "application/sdp", sdp_body);
}

int sipua_options_call(sipua_t *ua, sipua_set_t* call)
{
	ua_log(("sipua_options_call: FIXME - yet to implement\n"));

	return UA_OK;
}
int sipua_info_call(sipua_t *ua, sipua_set_t* call, char *type, char *info)
{
	ua_log(("sipua_info_call: FIXME - yet to implement\n"));

	return UA_OK;

}

int sipua_bye(sipua_t *sipua, sipua_set_t* set)
{
	ua_log(("sipua_disconnect: FIXME - yet to implement\n"));

	return UA_OK;
}

int sipua_recall(sipua_t *sipua, sipua_set_t* set)
{
	ua_log(("sipua_reconnect: FIXME - yet to implement\n"));

	return UA_OK;
}
