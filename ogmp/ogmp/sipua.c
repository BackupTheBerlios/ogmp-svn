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

#ifdef UA_LOG
 #include <stdio.h>
 #define ua_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define ua_log(fmtargs)
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
	rtpcap_sdp_t* sdp_info;

	char *nettype, *addrtype, *netaddr;

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

	sipua->uas->clear_coding(sipua->uas);

	/* generate sdp message */
	sdp_info = xmalloc(sizeof(rtpcap_sdp_t));
	sdp_info->sdp_media_pos = 0;
	sdp_message_init(&sdp_info->sdp_message);

	sdp_message_v_version_set(sdp_info->sdp_message, xstr_clone(SDP_VERSION));

	/* those fields MUST be set */
	sdp_message_o_origin_set (sdp_info->sdp_message, 
								xstr_clone(set->user_prof->username),
								xstr_clone(set->setid.id),
								xstr_clone(set->version),
								xstr_clone(nettype),
								xstr_clone(addrtype),
								xstr_clone(netaddr));

	sdp_message_s_name_set (sdp_info->sdp_message, xstr_clone(set->subject));

	sdp_message_c_connection_add (sdp_info->sdp_message, 
									-1, /* media_pos */
									xstr_clone(nettype), /* IN */
									xstr_clone(addrtype), /* IP4 */
									xstr_clone(netaddr),
									NULL, /* multicast_ttl */ 
									NULL /* multicast_int */);

	/* offer-answer draft says we must copy the "t=" line */
	sprintf (tmp, "%i", 0);
	sprintf (tmp2, "%i", 0);

	if (sdp_message_t_time_descr_add (sdp_info->sdp_message, tmp, tmp2) != 0)
	{
		sdp_message_free(sdp_info->sdp_message);
		
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

		media_bw = session_new_sdp(cata, nettype, addrtype, netaddr, &rtp_portno, &rtcp_portno, pt, codings[i].mime, codings[i].clockrate, codings[i].param, bw_budget, control, sdp_info);

		if(media_bw > 0)
			bw_budget -= media_bw;
		else
			break;
	}

	sdp_message_to_str(sdp_info->sdp_message, &set->sdp_body);

	xfree(sdp_info);

	return set;
}

/* Create a empty call from incoming sdp, and generate new sdp */
sipua_set_t* sipua_negotiate_call(sipua_t *sipua, user_profile_t* user_prof, 

								rtpcap_set_t* rtpcapset,
								char* mediatypes[], int rtp_ports[], int rtcp_ports[], 
								int nmedia, media_control_t* control)
{
	sipua_set_t* set;
	rtpcap_sdp_t* sdp_info;
	rtpcap_descript_t* rtpcap;
	xlist_user_t u;

	char *nettype, *addrtype, *netaddr;

	char tmp[16];
	char tmp2[16];

	int medias[MAX_SIPUA_MEDIA];
	int bw_budget;

	module_catalog_t *cata = control->modules(control);

	set = xmalloc(sizeof(sipua_set_t));
	if(!set)
	{
		ua_log(("sipua_new_set: No memory\n"));
		return NULL;
	}
	memset(set, 0, sizeof(sipua_set_t));

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
	sdp_info = xmalloc(sizeof(rtpcap_sdp_t));
	sdp_info->sdp_media_pos = 0;
	sdp_message_init(&sdp_info->sdp_message);

	sdp_message_v_version_set(sdp_info->sdp_message, xstr_clone(SDP_VERSION));

	/* Not used in negotiation, but can identify call */
	sdp_message_o_origin_set (sdp_info->sdp_message, 
								xstr_clone(rtpcapset->username),
								xstr_clone(rtpcapset->callid),
								xstr_clone(rtpcapset->version),
								xstr_clone(rtpcapset->nettype),
								xstr_clone(rtpcapset->addrtype),
								xstr_clone(rtpcapset->netaddr));

	sdp_message_s_name_set (sdp_info->sdp_message, xstr_clone(rtpcapset->subject));

	/* My net connection */
	sdp_message_c_connection_add (sdp_info->sdp_message, 
									-1 /* media_pos */,
									xstr_clone(nettype) /* IN */,
									xstr_clone(addrtype) /* IP4 */,
									xstr_clone(netaddr),
									NULL /* multicast_ttl */, 
									NULL /* multicast_int */);

	/* offer-answer draft says we must copy the "t=" line */
	sprintf (tmp, "%i", 0);
	sprintf (tmp2, "%i", 0);

	if (sdp_message_t_time_descr_add (sdp_info->sdp_message, tmp, tmp2) != 0)
	{
		sdp_message_free(sdp_info->sdp_message);
		
		xfree(set);

		return NULL;
	}
	
	memset(medias, 0, sizeof(medias));

	bw_budget = control->book_bandwidth(control, 0);

	rtpcap = xlist_first(rtpcapset->rtpcaps, &u);
	while(rtpcap)
	{
		int media_bw;

		char mediatype[16];
		int j;

		j = 0;
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

		if(medias[j] == 0)
		{
			media_bw = session_new_sdp(cata, nettype, addrtype, netaddr, 
							&rtpcap->local_rtp_portno, &rtpcap->local_rtcp_portno, 
							rtpcap->profile_no, rtpcap->profile_mime, 
							rtpcap->clockrate, rtpcap->coding_param, 
							bw_budget, control, sdp_info);
			
			if(media_bw > 0)
			{
				medias[j]++;
				bw_budget -= media_bw;
				rtpcap->enable = 1;
			}
			else
				break;
		}
		
		rtpcap = xlist_next(rtpcapset->rtpcaps, &u);
	}

	sdp_message_to_str(sdp_info->sdp_message, &set->reply_body);

	xfree(sdp_info);

	return set;
}

int sipua_establish_call(sipua_t* sipua, sipua_set_t* call, char *mode, rtpcap_set_t* rtpcapset,
						xlist_t* format_handlers, media_control_t* control, int pt_pool[])
{
	dev_rtp_t* rtpdev;

	xlist_user_t u;

	char *nettype, *addrtype, *netaddr;

	int bw = 0;
	int bw_budget;
	module_catalog_t *cata;

	/* define a player */
	media_format_t *format;

	bw_budget = control->book_bandwidth(control, call->bandwidth);
	if(bw_budget < 0)
		return UA_FAIL;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);
	
	cata = control->modules(control);

	/* create default rtp session */
	rtpdev = (dev_rtp_t*)control->find_device(control, "rtp");

	/* create a media format, also get the capabilities of the file */
	format = (media_format_t *)xlist_first(format_handlers, &u);
	while (format)
	{
		/* open media source */
		if(format->support_type(format, "mime", "application/sdp"))
		{
			rtp_format_t* rtpfmt = (rtp_format_t*)format;

			bw = rtpfmt->open_capables(format, rtpcapset, control, mode, call->bandwidth);
			if(bw > 0)
			{
				call->rtp_format = format;

				break;
			}
		}
      
		format = (media_format_t*) xlist_next(format_handlers, &u);
	}

	if(call->rtp_format == NULL)
	{
		ua_log(("sipua_establish_call: no format support\n"));

		return 0;
	}
	/*
	rtp_media = session_media(rtp_session);
	xlist_addto_first(call->mediaset, rtp_media);
	*/
	if(call->bandwidth - bw > 0)
		control->release_bandwidth(control, call->bandwidth - bw);

	call->bandwidth = bw;

	ua_log(("sipua_create_call: call[%s] created\n", rtpcapset->subject));

	return bw;
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
	/*
	if (config->fcn_set_info != NULL)
		config->fcn_set_info (con, *sdp);
	if (config->fcn_set_uri != NULL)
		config->fcn_set_uri (con, *sdp);
	if (config->fcn_set_emails != NULL)
		config->fcn_set_emails (con, *sdp);
	if (config->fcn_set_phones != NULL)
		config->fcn_set_phones (con, *sdp);
	*/
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
	char* siploc, *p;
	
	p = siploc = xmalloc(strlen(userloc)+5);
	if(!siploc)
		return UA_FAIL;

	strcpy(p, "sip:");
	p += 4;
	strcpy(p, userloc);

	ret = sipua->uas->regist(sipua->uas, siploc, user->registrar, user->regname, user->seconds);

	if(ret < UA_OK)
		user->reg_status = SIPUA_STATUS_REG_FAIL;
	else
	{
		user->cname = userloc;

		user->reg_status = SIPUA_STATUS_REG_DOING;
	}

	xfree(siploc);

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

int sipua_call(sipua_t *sipua, sipua_set_t* set, char *regname) 
{
	/*char *sdp_body = NULL;*/
	int ret;

	ua_log(("sipua_connect: Call from [%s] to [%s]\n", set->user_prof->regname, regname));

	/*sipua_session_sdp(sipua, set, &sdp_body);*/

	ua_log(("sipua_connect: Call-ID#%u initial sdp\n", set->setid.id));
	ua_log(("--------------------------\n"));
	ua_log(("%s", set->sdp_body));
	ua_log(("--------------------------\n"));

	ret = sipua->uas->invite(sipua->uas, regname, set, set->sdp_body, strlen(set->sdp_body)+1);
	if(ret < UA_OK)
		return ret;

	/*free(sdp_body);*/

	return UA_OK;
}

int sipua_answer(sipua_t *ua, sipua_set_t* call, int reply)
{
	return ua->uas->answer(ua->uas, call, reply, "application/sdp", call->reply_body);
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
