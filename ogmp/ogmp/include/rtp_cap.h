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

#ifndef RTP_CAP_H
#define RTP_CAP_H

#include "media_format.h"
#include "phonebook.h"

#include <osip2/osip_negotiation.h>

#define MAX_MIME_BYTES 64
#define MAX_CNAME_BYTES 256
#define MAX_IP_BYTES  64
#define MAX_NPAYLOAD_PRESET 8

#define DEFAULT_PORTNO 0
#define SDP_VERSION "0"
#define PAYLOADTYPE_DYNA -1

#define RTPCAP_NONE_CAPABLES		0x0
#define RTPCAP_ALL_CAPABLES			0xF
#define RTPCAP_SELECTED_CAPABLES	0xA

typedef struct rtp_coding_s rtp_coding_t;
struct rtp_coding_s
{
	char mime[MAX_MIME_BYTES];

	int clockrate;    
	int param;
    
	int pt;  /* <0 means dyna */
};

typedef struct rtp_profile_setting_s rtp_profile_setting_t;
struct rtp_profile_setting_s
{
	int rtp_portno;
	int rtcp_portno;
	int media_bps;
	/*
	int rtp_bandwidth;
	*/
};

typedef struct rtp_profile_set_s rtp_profile_set_t;
struct rtp_profile_set_s
{
	int npayload;
	rtp_coding_t coding[MAX_NPAYLOAD_PRESET];
};

typedef struct rtp_setting_s rtp_setting_t;
struct rtp_setting_s
{
	struct control_setting_s setting;
	/*
	int cnlen;
	char *cname;
	char *ipaddr;
	*/
	uint16 default_rtp_portno;
	uint16 default_rtcp_portno;

	rtp_profile_set_t default_profiles;
};

typedef struct rtpcap_descript_s rtpcap_descript_t; 
struct rtpcap_descript_s
{
	struct capable_descript_s descript;

	char *profile_mime;
	uint8 profile_no;

	char *nettype;
	char *addrtype;
	char *netaddr;

	uint16 rtp_portno;
	uint16 rtcp_portno;

	int local_rtp_portno;
	int local_rtcp_portno;

	int clockrate;
	int coding_param;

	int enable;

	sdp_message_t *sdp_message;
};


typedef struct rtpcap_set_s rtpcap_set_t;
struct rtpcap_set_s
{
	char *callid;
	char *username;
	char *version;

	char *nettype;
	char *addrtype;
	char *netaddr;	/* participant's default media address */

	char cname[MAX_CNAME_BYTES];

	int sbytes;
	char *subject;

	int ibytes;
	char *info;

	xlist_t *rtpcaps;

	sdp_message_t *sdp_message;

	user_profile_t *user_profile;

	int rtpcap_selection;
};

typedef struct rtpcap_sdp_s rtpcap_sdp_t;
struct rtpcap_sdp_s
{
	sdp_message_t *sdp_message;
	int sdp_media_pos;

};

typedef struct transmit_source_s transmit_source_t;
typedef struct media_transmit_s media_transmit_t;

struct transmit_source_s
{
   struct media_source_s source;

   int (*add_destinate)(transmit_source_t *tsrc, char *mime, char *cname, char *nettype, char *addrtype, char *netaddr, int rtp_port, int rtcp_port);
   int (*remove_destinate)(transmit_source_t *tsrc, char *mime, char *cname, char *nettype, char *addrtype, char *netaddr, int rtp_port, int rtcp_port);
};

struct media_transmit_s
{
   struct media_player_s player;

   int (*add_destinate)(media_transmit_t *trans, char *cname, char *nettype, char *addrtype, char *netaddr, int rtp_port, int rtcp_port);
   int (*remove_destinate)(media_transmit_t *trans, char *cname, char *nettype, char *addrtype, char *netaddr, int rtp_port, int rtcp_port);

   int (*add_source)(media_transmit_t *trans, char *cname, int cnlen, char *netaddr, uint16 rtp_port, uint16 rtcp_port);
   int (*remove_source)(media_transmit_t *trans, char *cname);
};

DECLSPEC
rtpcap_descript_t* 
rtp_capable_descript(int payload_no, 
						char* nettype, char* addrtype, char *netaddr, 
						uint media_port, uint control_port, char *mime, 
						int clockrate, int coding_param, 
						void *sdp_message);

/**
 * Suppose in "a=rtpmap:96 G.729a/8000/1", rtpmap string would be "96 G.729a/8000/1"
 * parse it into rtpmapno=96; coding_type="G.729a"; clockrate=8000; coding_param=1
 */
DECLSPEC
int 
sdp_parse_rtpmap(char *rtpmap, int *rtpmapno, char *coding_type, int *clockrate, int *coding_param);

/**
 * convert rtp_capable_descript to sdp
 * return pos_media of sdp_message, see osip api for detail 
 */
DECLSPEC
int 
rtp_capable_to_sdp(rtpcap_descript_t* rtpcap, sdp_message_t *sdp, int pos);

/**
 * convert sdp to rtp_capable_descript
 * return pos_media of sdp_message, see osip api for detail 
int rtp_capable_from_sdp(rtpcap_descript_t* rtpcaps[], int max_cap, sdp_message_t *sdp);
 */
DECLSPEC
rtpcap_set_t* 
rtp_capable_from_sdp(sdp_message_t *sdp);

int rtp_capable_cname(rtpcap_set_t* set, char *cn, int bytes);

DECLSPEC
rtpcap_descript_t* 
rtp_get_capable(rtpcap_set_t* set, char* mime);

DECLSPEC
int 
rtp_capable_done_set(rtpcap_set_t* rtpcap_set);

#endif
