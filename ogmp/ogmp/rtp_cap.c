/***************************************************************************
                          rtp_cap.c  -  capablility description for rtp stream
                             -------------------
    begin                : Mon Sep 6 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org; heming@bigfoot.com
 ***************************************************************************/
 
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include <timedia/xstring.h>
#include <timedia/xmalloc.h>

#include "rtp_cap.h"
#include "log.h"

#define RTPCAP_LOG
#define RTPCAP_DEBUG

#ifdef RTPCAP_LOG
 #define rtpcap_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define rtpcap_log(fmtargs)
#endif

#ifdef RTPCAP_DEBUG
 #define rtpcap_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define rtpcap_debug(fmtargs)
#endif

/**************************************************************
 * rtp capable descript module
 */
int rtp_descript_done(capable_descript_t *cap)
{
	rtpcap_descript_t *rtpcap = (rtpcap_descript_t*)cap;

	xstr_done_string(rtpcap->profile_mime);

	xstr_done_string(rtpcap->nettype);
	xstr_done_string(rtpcap->addrtype);
	xstr_done_string(rtpcap->netaddr);

	xfree(rtpcap);

	return MP_OK;
}

int rtp_descript_match_value(capable_descript_t *cap, char *type, char *value, int vbytes)
{
   rtpcap_descript_t *rtpcap = (rtpcap_descript_t*)cap;

   rtpcap_log(("rtp_descript_match_value: %s:%s\n", type, value));

   if(0 != strncmp("mime", type, 4))
      return 0;

	return (0 == strncmp(rtpcap->profile_mime, value, vbytes));
}

int rtp_descript_match(capable_descript_t *me, capable_descript_t *oth)
{
	rtpcap_descript_t *rtpoth;
	rtpcap_descript_t *rtpme = (rtpcap_descript_t*)me;

	rtpcap_log(("rtp_descript_match: %s\n", rtpme->profile_mime));

	rtpoth = (rtpcap_descript_t*)oth;

	return oth->match_value(oth, "mime", rtpme->profile_mime, strlen(rtpme->profile_mime));
}

rtpcap_descript_t* rtp_capable_descript(int payload_no, 
										char* nettype, char* addrtype, char *netaddr, 
										uint media_port, uint control_port, char *mime, 
										int clockrate, int coding_param, 
										void *sdp_message)
{
	rtpcap_descript_t *rtpcap;

	rtpcap = xmalloc(sizeof(rtpcap_descript_t));
	if(!rtpcap)
	{
		rtpcap_log(("rtp_capable_descript: No memory\n"));

		return NULL;
	}
	memset(rtpcap, 0, sizeof(rtpcap_descript_t));

	rtpcap->descript.done = rtp_descript_done;
	rtpcap->descript.match = rtp_descript_match;
	rtpcap->descript.match_value = rtp_descript_match_value;

	rtpcap->profile_mime = xstr_clone(mime);
	rtpcap->profile_no = payload_no;

	rtpcap->nettype = xstr_clone(nettype);
	rtpcap->addrtype = xstr_clone(addrtype);
	rtpcap->netaddr = xstr_clone(netaddr);

	rtpcap->rtp_portno = media_port;
	rtpcap->rtcp_portno = control_port;

	rtpcap->clockrate = clockrate;
	rtpcap->coding_param = coding_param;

	rtpcap->sdp_message = sdp_message;

	return rtpcap;
}

/**
 * Suppose in "a=rtpmap:96 G.729a/8000/1", rtpmap string would be "96 G.729a/8000/1"
 * parse it into rtpmapno=96; coding_type="G.729a"; clockrate=8000; coding_param=1
 */
int sdp_parse_rtpmap(char *rtpmap, int *rtpmapno, char *coding_type, int *clockrate, int *coding_param)
{
	char *end;
	char *token = rtpmap;

	*rtpmapno = (int)strtol(token, NULL, 10);

	while(*token != ' ' && *token != '\t')
		token++;
	while(*token == ' ' || *token == '\t')
		token++;

	end = strchr(token, '/');

	strncpy(coding_type, token, end-token);
	coding_type[end-token] = '\0';

	token = ++end;

	*clockrate = (int)strtol(token, NULL, 10);

	end = strchr(token, '/');
	token = ++end;

	*coding_param = (int)strtol(token, NULL, 10);
	
	return MP_OK;
}

/**
 * Parse IPv4 string "a.b.c.d/n" into "a.b.c.d" and n (maskbits)
 */
int sdp_parse_ipv4(char *addr, char *ip, int ipbytes, int *ttl)
{
	char *end = strchr(addr, '/');

	if(end != NULL)
	{
		strncpy(ip, addr, end - addr);
		ip[end-addr] = '\0';

		*ttl = atoi(++end);
	
		return MP_OK;
	}

	*ttl = 0;
	strcpy(ip, addr);

	return MP_OK;
}

/**
 * Parse sdp rtcp attr (a=rtcp:)
 * port: "53020"
 * ipv4: "53020 IN IP4 126.16.64.4"
 * ipv6: "53020 IN IP6 2001:2345:6789:ABCD:EF01:2345:6789:ABCD"
 */
int sdp_parse_rtcp(char *rtcp, char *ip, int buflen, uint *port)
{
	char *token;
	*port = strtol(rtcp, NULL, 10);

	token = strstr(rtcp, "IP");

	while(*token != ' ' || *token != '\t')
		token++;
	while(*token == ' ' || *token == '\t')
		token++;

	strcpy(ip, token);

	return MP_OK;
}

int rtp_capable_to_sdp (rtpcap_descript_t* rtpcap, sdp_message_t *sdp, int pos)
{
	char media_type[16];
	char coding_type[16];
	char media_port[8];
	char rtpmapno[4];

	char rtpmap[64];

	int attrlen;
	char *p;
	
	/* media_type */
	p = strchr(rtpcap->profile_mime, '/');
	attrlen = p - rtpcap->profile_mime;
	strncpy(media_type, rtpcap->profile_mime, attrlen);
	media_type[attrlen] = '\0';
	
	/* coding_type */
	p++;
	strcpy(coding_type, p);

	/* media_port */
	itoa(rtpcap->rtp_portno, media_port, 10);
	/* rtpmap no */
	itoa(rtpcap->profile_no, rtpmapno, 10);

	sdp_message_m_media_add (sdp, xstr_clone(media_type), xstr_clone(media_port), xstr_clone("2"), xstr_clone("RTP/AVP"));

	/* rtpmap */
	p = rtpmap;
	strcpy(p, rtpmapno);

	while(*p)
		p++;
	*p = ' ';
	p++;
	strcpy(p, coding_type);
	
	while(*p)
		p++;
	*p = '/';
	p++;
	itoa(rtpcap->clockrate, p, 10);
	
	while(*p)
		p++;
	*p = '/';
	p++;
	itoa(rtpcap->coding_param, p, 10);

	sdp_message_a_attribute_add (sdp, pos, xstr_clone("rtpmap"), xstr_clone(rtpmap));

	return MP_OK;
}

rtpcap_set_t* rtp_capable_from_sdp(sdp_message_t *sdp)
{
	/* Retrieve capable from the sdp message */
	int ncap = 0;

	char *media_type; /* "audio"; "video"; etc */

	int payload_no = 0;

	char *protocol; /* "RTP/AVP" */

	char *rtpmap = NULL;

	int rtpmapno = 0;

	int clockrate;
	int coding_param;

	char mime[MAX_MIME_BYTES] = "";
	char media_ip[MAX_IP_BYTES] = "";
	char control_ip[MAX_IP_BYTES] = "";
	char coding_type[MAX_MIME_BYTES] = "";

	int pos_media = 0;

	char *addr;
	int ttl;

	rtpcap_set_t* rtpcapset = xmalloc(sizeof(rtpcap_set_t));
	if(!rtpcapset)
	{
		return NULL;
	}
	memset(rtpcapset, 0, sizeof(rtpcap_set_t));

	rtpcapset->rtpcaps = xlist_new();
	if(!rtpcapset->rtpcaps)
	{
		xfree(rtpcapset);
		return NULL;
	}

	rtpcapset->sdp_message = sdp;

	rtpcapset->nettype = xstr_clone(sdp_message_c_nettype_get (sdp, -1, 0));
	rtpcapset->addrtype = xstr_clone(sdp_message_c_addrtype_get (sdp, -1, 0));

	addr = sdp_message_c_addr_get (sdp, -1, 0);

	if(0==strcmp(rtpcapset->nettype, "IN") && 0==strcmp(rtpcapset->addrtype, "IP4"))
		sdp_parse_ipv4(addr, media_ip, MAX_IP_BYTES, &ttl);

#ifdef IPV6
	if(0==strcmp(rtpcapset->nettype, "IN") && 0==strcmp(rtpcapset->addrtype, "IP6"))
		jua_parse_ipv6(addr, media_ip, ...);
#endif

	rtpcap_log(("rtp_capable_to_sdp: media_ip[%s] ttl[%d]\n", media_ip, ttl));

	rtpcapset->netaddr = xstr_clone(media_ip);

	{
		char *p = rtpcapset->cname;
		char *username;

		username = sdp_message_o_username_get(sdp);
		if(username)
		{
			rtpcapset->username = xstr_clone(username);
			rtpcapset->callid = xstr_clone(sdp_message_o_sess_id_get(sdp));
			rtpcapset->version = xstr_clone(sdp_message_o_sess_version_get(sdp));

			strcpy(rtpcapset->cname, rtpcapset->username);

			while(*p) p++;
			*p = '@'; p++;
			strcpy(p, rtpcapset->netaddr);
		}
	}

	while (!sdp_message_endof_media (sdp, pos_media))
	{
		uint media_port = 0;
		uint control_port = 0;
		int nport = 0;

		char *attr = NULL;

		int pos_attr = 0;

		sdp_media_t *sdp_media;
		rtpcap_descript_t *rtpcap;

		int m_ttl;
		int rtpmap_ok, rtcp_ok;
	
		char *m_nettype=NULL, *m_addrtype=NULL, *m_addr=NULL;

		char m_media_ip[MAX_IP_BYTES] = "";

		media_type = sdp_message_m_media_get (sdp, pos_media);
		media_port = (uint)strtol(sdp_message_m_port_get (sdp, pos_media), NULL, 10);

		nport = atoi(sdp_message_m_number_of_port_get (sdp, pos_media));
		protocol = sdp_message_m_proto_get (sdp, pos_media);

		rtpcap_log(("rtp_capable_to_sdp: media[%s] port[%d]\n", media_type, media_port));

		sdp_media = osip_list_get(sdp->m_medias, pos_media);

		m_nettype = sdp_message_c_nettype_get (sdp, pos_media, 0);
		m_addrtype = sdp_message_c_addrtype_get (sdp, pos_media, 0);
		m_addr = sdp_message_c_addr_get (sdp, pos_media, 0);

		if(m_addr && 0==strcmp(m_nettype, "IN") && 0==strcmp(m_addrtype, "IP4"))
			sdp_parse_ipv4(m_addr, m_media_ip, MAX_IP_BYTES, &m_ttl);

#ifdef IPV6
		if(m_addr && 0==strcmp(m_nettype, "IN") && 0==strcmp(m_addrtype, "IP6"))
			jua_parse_ipv6(addr, media_ip, ...);
#endif

		rtpcap_log(("rtp_capable_to_sdp: media_ip[%s] ttl[%d]\n", media_ip, ttl));

		/* find rtpmap attribute */
		rtpmap_ok = rtcp_ok = 0;
		attr = sdp_message_a_att_field_get (sdp, pos_media, pos_attr);
		while(attr)
		{
			if(0 == strcmp(attr, "rtpmap"))
			{
				rtpmap = sdp_message_a_att_value_get(sdp, pos_media, pos_attr);
				sdp_parse_rtpmap(rtpmap, &rtpmapno, coding_type, &clockrate, &coding_param);
					
				rtpcap_log(("rtp_capable_to_sdp: rtpmap[%d]; coding[%s]; clockrate[%d]; param[%d]\n", rtpmapno, coding_type, clockrate, coding_param));

				rtpmap_ok = 1;
				if(rtcp_ok)
					break;
			}
					
			if(0 == strcmp(attr, "rtcp"))
			{
				char *rtcp;
				
				rtcp = sdp_message_a_att_value_get(sdp, pos_media, pos_attr);
				sdp_parse_rtcp(rtcp, control_ip, MAX_IP_BYTES, &control_port);
					
				rtcp_ok = 1;
				if(rtpmap_ok)
					break;
			}
					
			attr = sdp_message_a_att_field_get (sdp, pos_media, ++pos_attr);
		}

		snprintf(mime, MAX_MIME_BYTES, "%s/%s", media_type, coding_type);

		if(control_port == 0 && (nport == 2 || nport == 0))
			control_port = media_port + 1;

		/* open issue, rtcp and rtp flow may use different ip */
		if(m_addr)
			rtpcap = rtp_capable_descript(rtpmapno, m_nettype, m_addrtype, m_media_ip, 
										media_port, control_port, 
										mime, clockrate, coding_param, sdp);
		else
			rtpcap = rtp_capable_descript(rtpmapno, rtpcapset->nettype, 
										rtpcapset->addrtype, rtpcapset->netaddr, 
										media_port, control_port, 
										mime, clockrate, coding_param, sdp);

		xlist_addto_first(rtpcapset->rtpcaps, rtpcap);

		pos_media++;
	}

	return rtpcapset;
}

int rtpcap_match_mime(void* tar, void* pat)
{
	rtpcap_descript_t* rtpcap = (rtpcap_descript_t*)tar;
	char* mime = (char*)pat;

	return strcmp(rtpcap->profile_mime, mime);
}

rtpcap_descript_t* rtp_get_capable(rtpcap_set_t* set, char* mime)
{
	xlist_user_t u;
	return xlist_find(set->rtpcaps, mime, rtpcap_match_mime, &u);
}

int rtp_capable_cname(rtpcap_set_t* set, char *cn, int bytes)
{
	int nb = strlen(set->netaddr);
	int ub = strlen(set->username);

	if(bytes < ub+nb+2)
	{
		strcpy(cn, set->netaddr);

		return nb+1;
	}

	strcpy(cn, set->username);
	cn += ub;

	*cn = '@';
	cn++;

	strcpy(cn, set->netaddr);

	return ub+nb+2;
}

int rtpcap_done(void* gen)
{
	capable_descript_t *cap = (capable_descript_t*)gen;
	cap->done(cap);

	return OS_OK;
}

int rtp_capable_done_set(rtpcap_set_t* rtpcapset)
{
	xfree(rtpcapset->nettype);
	xfree(rtpcapset->addrtype);
	xfree(rtpcapset->netaddr);

	if(rtpcapset->rtpcaps)
		xlist_done(rtpcapset->rtpcaps, rtpcap_done);

	xfree(rtpcapset);

	return MP_OK;
}

