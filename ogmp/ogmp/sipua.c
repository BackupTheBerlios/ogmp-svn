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

#include "sipua.h"
#include <stdlib.h>
#include <string.h>

#include <xrtp/xrtp.h>
 
#define UA_LOG

#ifdef UA_LOG
 #include <stdio.h>
 #define ua_log(fmtargs)  do{log_printf fmtargs;}while(0)
#else
 #define ua_log(fmtargs)
#endif

#define MAX_REC 8
#define MAX_REC_CAP 8
#define MAX_CN_BYTES 256 /* max value in rtcp */

int sipua_done_action(sipua_action_t *action)
{
	xfree(action);

	return UA_OK;
}

sipua_action_t *sipua_new_action(void *sip_user,
                                 int(*onregister)(void*,char*,int,void*), 
                                 int(*oncall)(void*,char*,void*,void*), 
                                 int(*onconnect)(void*,char*,void*), 
								 int(*onreset)(void*,char*,void*),
								 int(*onbye)(void*,char*))
{
	sipua_action_t *act;
   
	act = xmalloc(sizeof(sipua_action_t));
	if(!act)
	{
		ua_log(("action_new: No memory\n"));
		return NULL;
	}
   
	memset(act, 0, sizeof(sipua_action_t));

	act->done = sipua_done_action;
   
	act->sip_user = sip_user;
   
	act->onregister = onregister;
	act->oncall = oncall;
	act->onconnect = onconnect;
	act->onreset = onreset;
	act->onbye = onbye;

	return act;
}

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

sipua_set_t* sipua_new_conference(sipua_t *sipua, user_profile_t* user_prof, char* id, 
							   char* subject, int sbyte, char* info, int ibyte,
							   int default_rtp_portno, int default_rtcp_portno, int bandwidth, int *bw_used,
							   media_control_t* control, rtp_coding_t codings[], int ncoding, int pt_pool[])
{
	xrtp_session_t* rtp_session;
	dev_rtp_t* rtpdev;
	sipua_set_t* set;

	char tmp[16];

	char *nettype, *addrtype, *netaddr;

	int i;
	int bw_budget = bandwidth;

	module_catalog_t *cata = control->modules(control);

	set = malloc(sizeof(sipua_set_t));
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
		sprintf(tmp, "%i", time(NULL));
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

	sprintf(tmp, "%i", time(NULL));
	set->version = xstr_clone(tmp);

	set->mediaset = xlist_new();

	/* create default rtp session */
	rtpdev = (dev_rtp_t*)control->find_device(control, "rtp");

	sipua->uas->clear_coding(sipua->uas);

	for(i=0; i<ncoding; i++)
	{
		xrtp_media_t* rtp_media;

		int pt = sipua_book_pt(pt_pool);
		int rtp_portno, rtcp_portno;

		if(pt<0)
		{
			ua_log(("sipua_new_session: no payload type available\n"));
			continue;
		}

		rtp_session = rtpdev->rtp_session(rtpdev, cata, control,
										  user_prof->cname, strlen(user_prof->cname)+1,
										  netaddr, (uint16)default_rtp_portno, (uint16)default_rtcp_portno,
										  (uint8)pt, codings[i].mime, codings[i].clockrate, codings[i].param,
										  bw_budget);

		if(!rtp_session)
		{
			sipua_release_pt(pt_pool, pt);
			ua_log(("sipua_new_session: fail to create rtp session[%s]\n", codings[i].mime));
			continue;
		}

		if(session_bandwidth(rtp_session) > bw_budget)
		{
			session_done(rtp_session);
			break;
		}

		bw_budget -= session_bandwidth(rtp_session);

		session_portno(rtp_session, &rtp_portno, &rtcp_portno);

		rtp_media = session_media(rtp_session);
		/*
		rtp_media->set_coding(rtp_media, codings[i].clockrate, codings[i].param);
		sipua->uas->add_coding(sipua->uas, pt, rtp_portno, rtcp_portno, codings[i].mime, codings[i].clockrate, codings[i].param);
		*/
		xlist_addto_first(set->mediaset, rtp_media);
	}

	*bw_used = bandwidth - bw_budget;

	ua_log(("sipua_new_conference: conference[%s] created\n", subject));

	return set;
}

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

int sipua_invite(sipua_t *sipua, sipua_set_t* set, char *regname) 
{
	char *sdp_body = NULL;
	int ret;

	ua_log(("sipua_connect: Call from [%s] to [%s]\n", set->user_prof->regname, regname));

	sipua_session_sdp(sipua, set, &sdp_body);

	ua_log(("sipua_connect: Call-ID#%u initial sdp\n", set->setid.id));
	ua_log(("--------------------------\n"));
	ua_log(("%s", sdp_body));
	ua_log(("--------------------------\n"));

	ret = sipua->uas->call(sipua->uas, regname, set, sdp_body, strlen(sdp_body)+1);
	if(ret < UA_OK)
		return ret;

	free(sdp_body);

	return UA_OK;
}

int sipua_bye(sipua_t *sipua, sipua_set_t* set)
{
	ua_log(("sipua_disconnect: FIXME - yet to implement\n"));

	return UA_OK;
}

int sipua_reinvite(sipua_t *sipua, sipua_set_t* set)
{
	ua_log(("sipua_reconnect: FIXME - yet to implement\n"));

	return UA_OK;
}
