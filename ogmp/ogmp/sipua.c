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
 #define ua_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define ua_log(fmtargs)
#endif

#define MAX_REC 8
#define MAX_REC_CAP 8
#define MAX_CN_BYTES 256 /* max value in rtcp */

typedef struct sipua_impl_s sipua_impl_t;
struct sipua_impl_s
{
	struct sipua_s sipua;

	user_profile_t* user;
	media_control_t* control;

	int in_registration;
	int expire_sec; /* registration expired seconds, time() */

	uint16 default_rtp_portno;
	uint16 default_rtcp_portno;

	int bandwidth;

	sipua_action_t* action;

	int pt[MAX_PAYLOAD_TYPE];
	
	xlist_t* sip_sessions;

	void* listener;
	int (*notify_event)(void* listener, sipua_event_t* e);
};

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

int sipua_book_pt(sipua_t* sipua)
{
	int i;
	sipua_impl_t* ua = (sipua_impl_t*)sipua;

	for(i=96; i<=127; i++)
	{
		if(ua->pt[i] == 0)
		{
			ua->pt[i] = 1;
			return i;
		}
	}
	
	return -1;
}

int sipua_release_pt(sipua_t* sipua, int pt)
{
	sipua_impl_t* ua = (sipua_impl_t*)sipua;

	ua->pt[pt] = 0;

	return UA_OK;
}

int sipua_cname(sipua_t* sipua, char *cn, int bytes)
{
	int nb, ub;
	char *nettype, *addrtype, *netaddr;

	sipua_impl_t* ua = (sipua_impl_t*)sipua;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);

	nb = strlen(netaddr);
	ub = strlen(ua->user->username);

	if(bytes < ub+nb+2)
	{
		strcpy(cn, netaddr);

		return nb+1;
	}

	strcpy(cn, ua->user->username);
	cn += ub;

	*cn = '@';
	cn++;

	strcpy(cn, netaddr);

	return ub+nb+2;
}

sipua_set_t* sipua_new_conference(sipua_t *sipua, char* id, 
							   char* subject, int sbyte, char* info, int ibyte, 
							   rtp_coding_t codings[], int ncoding)
{
	sipua_impl_t* ua = (sipua_impl_t*)sipua;
	xrtp_session_t* rtp_session;
	dev_rtp_t* rtpdev;
	sipua_set_t* set;

	char tmp[16];
	char cname[256];
	int cnbyte;

	char *nettype, *addrtype, *netaddr;
	media_control_t* control = ua->control;

	int i;

	module_catalog_t *cata = control->modules(control);

	set = malloc(sizeof(sipua_set_t));
	if(!set)
	{
		ua_log(("sipua_new_set: No memory\n"));
		return NULL;
	}
	memset(set, 0, sizeof(sipua_set_t));

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
		set->setid.username = xstr_clone(ua->user->username);

		set->setid.nettype = xstr_clone(nettype);
		set->setid.addrtype = xstr_clone(addrtype);
		set->setid.netaddr = xstr_clone(netaddr);
	}

	sprintf(tmp, "%i", time(NULL));
	set->version = xstr_clone(tmp);

	set->mediaset = xlist_new();

	/* create default rtp session */
	rtpdev = (dev_rtp_t*)control->find_device(control, "rtp");

	cnbyte = sipua_cname(sipua, cname, 256);

	sipua->uas->clear_coding(sipua->uas);

	for(i=0; i<ncoding; i++)
	{
		xrtp_media_t* rtp_media;

		int pt = sipua_book_pt(sipua);
		int rtp_portno, rtcp_portno;

		if(pt<0)
		{
			ua_log(("sipua_new_session: no payload type available\n"));
			continue;
		}

		rtp_session = rtpdev->rtp_session(rtpdev, cata, control,
										  cname, cnbyte,
										  netaddr, ua->default_rtp_portno, ua->default_rtcp_portno,
										  (uint8)pt, codings[i].mime,
										  ua->bandwidth, 0);

		if(!rtp_session)
		{
			ua_log(("sipua_new_session: fail to create rtp session[%s]\n", codings[i].mime));
			continue;
		}

		ua->bandwidth -= session_bandwidth(rtp_session);

		session_portno(rtp_session, &rtp_portno, &rtcp_portno);

		rtp_media = session_media(rtp_session);

		rtp_media->set_coding(rtp_media, codings[i].clockrate, codings[i].param);
		
		sipua->uas->add_coding(sipua->uas, pt, rtp_portno, rtcp_portno, codings[i].mime, codings[i].clockrate, codings[i].param);

		xlist_addto_first(set->mediaset, rtp_media);
	}

	ua_log(("sipua_new_conference: conference[%s] created\n", subject));

	return set;
}

int sipua_done_rtp_media(void* gen)
{
	xrtp_media_t* m = (xrtp_media_t*)gen;

	session_done(m->session);

	return UA_OK;
}

int sipua_done_conference(sipua_t* sipua, sipua_set_t* set)
{
	sipua_impl_t* ua = (sipua_impl_t*)sipua;

	xlist_remove_item(ua->sip_sessions, set);

	if(set->setid.username) xfree(set->setid.username);
	if(set->setid.nettype) xfree(set->setid.nettype);
	if(set->setid.addrtype) xfree(set->setid.addrtype);
	if(set->setid.netaddr) xfree(set->setid.netaddr);

	xfree(set->setid.id);
	xfree(set->version);
	xfree(set->subject);
	xfree(set->info);

	xlist_done(set->mediaset, sipua_done_rtp_media);

	xfree(set);
		
	return UA_OK;
}

int sipua_add(sipua_t *sipua, sipua_set_t* set, xrtp_media_t* rtp_media)
{
	sipua_impl_t* ua = (sipua_impl_t*)sipua;

	ua->bandwidth -= session_bandwidth(rtp_media->session);

	xlist_addto_first(set->mediaset, rtp_media);

	return UA_OK;
}

int sipua_remove(sipua_t *sipua, sipua_set_t* set, xrtp_media_t* rtp_media)
{
	sipua_impl_t* ua = (sipua_impl_t*)sipua;

	ua_log(("sipua_remove: FIXME - yet to implement\n"));

	return UA_OK;
}

int sipua_session_sdp(sipua_t *sipua, sipua_set_t* set, char** sdp_body)
{
	rtpcap_sdp_t sdp;
	xrtp_media_t* rtp_media;

	xlist_user_t u;

	char cname[256];
	int cnbyte;

	char tmp[16];
	char tmp2[16];

	char *nettype, *addrtype, *netaddr;

	sipua_impl_t* ua = (sipua_impl_t*)sipua;

	sipua->uas->address(sipua->uas, &nettype, &addrtype, &netaddr);

	sdp.sdp_media_pos = 0;
	sdp_message_init(&sdp.sdp_message);

	cnbyte = sipua_cname(sipua, cname, 256);

	sdp_message_v_version_set(sdp.sdp_message, xstr_clone(SDP_VERSION));

	/* those fields MUST be set */
	sdp_message_o_origin_set(	sdp.sdp_message, 
								xstr_clone(ua->user->username),
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

/* NOTE! if match, return ZERO, otherwise -1 */
int sipua_match_conference(void* tar, void* pat)
{
	sipua_set_t *set = (sipua_set_t*)tar;
	sipua_setid_t *id = (sipua_setid_t*)pat;

	if(	!strcmp(set->setid.id, id->id) && 
		!strcmp(set->setid.username, id->username) && 
		!strcmp(set->setid.nettype, id->nettype) && 
		!strcmp(set->setid.addrtype, id->addrtype) && 
		!strcmp(set->setid.netaddr, id->netaddr))
			return 0;

	return -1;
}

sipua_set_t* sipua_find_session(sipua_t* sipua, char* id, char* username, char* nettype, char* addrtype, char* netaddr)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	sipua_setid_t setid;
	xlist_user_t u;

	setid.id = id;
	setid.username = username;
	setid.nettype = nettype;
	setid.addrtype = addrtype;
	setid.netaddr = netaddr;

	return (sipua_set_t*)xlist_find(ua->sip_sessions, &setid, sipua_match_conference, &u);
}

int sipua_done_sip_session(void* gen)
{
	sipua_set_t *set = (sipua_set_t*)gen;
	
	if(set->setid.username) xfree(set->setid.username);
	if(set->setid.nettype) xfree(set->setid.nettype);
	if(set->setid.addrtype) xfree(set->setid.addrtype);
	if(set->setid.netaddr) xfree(set->setid.netaddr);

	xfree(set->setid.id);
	xfree(set->version);
	xfree(set->subject);
	xfree(set->info);

	xlist_done(set->mediaset, sipua_done_rtp_media);

	xfree(set);
	
	return UA_OK;
}

int sipua_done(sipua_t *sipua)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	sipua->uas->shutdown(sipua->uas);

	xfree(ua->action);

	xlist_done(ua->sip_sessions, sipua_done_sip_session);
	xfree(ua);

	return UA_OK;
}

int sipua_regist(sipua_t *sipua, sipua_action_t *act)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	if(ua->action && ua->action != act)
	{
		ua->action->done(ua->action);
		ua->action = act;
	}

	ua->in_registration = 1;

	return sipua->uas->regist(sipua->uas, ua->user->registrar, ua->user->regname, ua->user->seconds);
}

int sipua_unregist(sipua_t *sipua)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	/* stop regist thread */
	
	ua->action->done(ua->action);
	ua->action = NULL;
   
	ua->in_registration = 0;

	ua_log(("sipua_unregist: unregistering [%s] from [%s] ...\n", ua->user->regname, ua->user->registrar));

	return sipua->uas->unregist(sipua->uas, ua->user->registrar, ua->user->regname);
}

int sipua_invite(sipua_t *sipua, sipua_set_t* set, char *regname) 
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;
	char *sdp_body = NULL;
	int ret;

	ua_log(("sipua_connect: Call from [%s] to [%s]\n", ua->user->regname, regname));

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
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	ua_log(("sipua_disconnect: FIXME - yet to implement\n"));

	return UA_OK;
}

int sipua_reinvite(sipua_t *sipua, sipua_set_t* set)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	ua_log(("sipua_reconnect: FIXME - yet to implement\n"));

	return UA_OK;
}

int sipua_event(void* lisener, sipua_event_t* e)
{
	sipua_t *sipua = (sipua_t*)lisener;
	sipua_impl_t *ua = (sipua_impl_t*)lisener;

	sipua_set_t *call_info = e->call_info;

	switch(e->type)
	{
		case(SIPUA_EVENT_REGISTRATION_SUCCEEDED):
		{
			void* seconds = e->content;

			if(ua->in_registration)
			{
				sipua->uas->start(sipua->uas);
				ua->action->onregister(ua->action->sip_user, e->from, SIPUA_EVENT_REGISTRATION_SUCCEEDED, e->content);
			}
			else
			{
				ua->action->onregister(ua->action->sip_user, e->from, SIPUA_EVENT_UNREGISTRATION_SUCCEEDED, NULL);
			}

			break;
		}

		case(SIPUA_EVENT_REGISTRATION_FAILURE):
		{
			if(ua->in_registration)
				ua->action->onregister(ua->action->sip_user, e->from, SIPUA_EVENT_REGISTRATION_FAILURE, NULL);
			else
				ua->action->onregister(ua->action->sip_user, e->from, SIPUA_EVENT_UNREGISTRATION_FAILURE, NULL);

			break;
		}

		case(SIPUA_EVENT_NEW_CALL):
		{
			rtp_coding_t *codings;
			int ncoding=0;

			int lineno = e->lineno;

			rtpcap_set_t* rtpcapset;
			rtpcap_descript_t* rtpcap;
			sipua_set_t* call_info;

			xlist_user_t u;

			sdp_message_t *sdp_message;
			char* sdp_body = (char*)e->content;

			sdp_message_init(&sdp_message);

			if (sdp_message_parse(sdp_message, sdp_body) != 0)
			{
				ua_log(("\nsipua_event: fail to parse sdp\n"));
				sdp_message_free(sdp_message);
				
				return UA_FAIL;
			}

			rtpcapset = rtp_capable_from_sdp(sdp_message);

			codings = xmalloc(sizeof(rtp_coding_t) * xlist_size(rtpcapset->rtpcaps));

			rtpcap = xlist_first(rtpcapset->rtpcaps, &u);
			while(rtpcap)
			{
				strncpy(codings[ncoding].mime, rtpcap->profile_mime, MAX_MIME_BYTES);
				codings[ncoding].clockrate = rtpcap->clockrate;
				codings[ncoding].param = rtpcap->coding_param;
			
				rtpcap = xlist_next(rtpcapset->rtpcaps, &u);
				ncoding++;
			}

			call_info = ua->sipua.new_conference(&ua->sipua, rtpcapset->callid, 
										rtpcapset->subject, rtpcapset->sbytes, 
										rtpcapset->info, rtpcapset->ibytes, 
										codings, ncoding);

			sipua->uas->set_call(sipua->uas, lineno, call_info);

			sdp_message_free(sdp_message);
			xfree(codings);

			break;
		}

		case(SIPUA_EVENT_PROCEEDING):
			break;

		case(SIPUA_EVENT_RINGING):
			break;

		case(SIPUA_EVENT_ANSWERED):
		{
			ua->action->onconnect(ua->action->sip_user, e->from, e->content);
			break;
		}

	}

	return UA_OK;
}

int sipua_set_user(sipua_t *sipua, user_profile_t *user)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	if(!user)
	{
		ua->user = NULL;
		return UA_OK;
	}

	if(ua->user && 0==strcmp(ua->user->username, user->username))
		return UA_OK;
		
	ua->user = user;

	return UA_OK;
}

user_profile_t* sipua_user(sipua_t *sipua)
{
	return ((sipua_impl_t*)sipua)->user;
}

sipua_t* sipua_new(sipua_uas_t* uas, int bandwidth, media_control_t* control)
{
	sipua_impl_t *impl;

	sipua_t *sipua;

	sipua = xmalloc(sizeof(struct sipua_impl_s));
	if(!sipua)
	{
		ua_log(("sipua_new: No memory\n"));
		return NULL;
	}

	memset(sipua, 0, sizeof(struct sipua_impl_s));

	sipua->uas = uas;

	sipua->done = sipua_done;

	sipua->set_user = sipua_set_user;
	sipua->user = sipua_user;

	sipua->regist = sipua_regist;
	sipua->unregist = sipua_unregist;

	sipua->new_conference = sipua_new_conference;
	sipua->done_conference = sipua_done_conference;

	sipua->add = sipua_add;
	sipua->remove = sipua_remove;

	sipua->invite = sipua_invite;
	sipua->bye = sipua_bye;
	sipua->reinvite = sipua_reinvite;

	impl = (sipua_impl_t*)sipua;

	impl->bandwidth = bandwidth;

	impl->control = control;

	uas->set_listener(uas, sipua, sipua_event);
	/*	
	uas->regist(uas, user->registrar, user->regname, user->seconds); 
	*/
	return sipua;
}
