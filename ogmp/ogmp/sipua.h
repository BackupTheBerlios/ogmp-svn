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

#include <timedia/os.h>
#include "media_format.h"

#define UA_OK		0
#define UA_FAIL		-1
#define UA_REJECT	-2
#define UA_IGNORE	-3

#define MAX_CN_BYTES 256 /* max value in rtcp */
#define MAX_IP_LEN   64  /* may enough for ipv6 ? */

/* Contact Proxy
 * Sender register its session info to proxy
 * recvr contact sender by interact with proxy
 */
typedef struct sipua_s sipua_t;
typedef struct sipua_action_s sipua_action_t;

struct sipua_action_s
{
	int (*done)(sipua_action_t *rec);

	void *sip_user;
	int (*oncall)(void *user, char *cname, int cnlen, capable_descript_t* oppo_caps[], int oppo_ncap, capable_descript_t* **my_caps);
	int (*onconnect)(void *user, char *cname, int cnlen, capable_descript_t* oppo_caps[], int oppo_ncap, capable_descript_t* **my_caps);
	int (*onreset)(void *user, char *cname, int cnlen, capable_descript_t* oppo_caps[], int oppo_ncap, capable_descript_t* **my_caps);
	int (*onbye)(void *user, char *cname, int cnlen);
};

sipua_action_t *sipua_new_action(void *sip_user, int(*cb_oncall)(void*,char*,int,capable_descript_t**,int,capable_descript_t***), 
												 int(*cb_onconnect)(void*,char*,int,capable_descript_t**,int,capable_descript_t***), 
												 int(*cb_onreset)(void*,char*,int,capable_descript_t**,int,capable_descript_t***),
												 int(*cb_onbye)(void*,char*,int));

struct sipua_s
{
	int (*done)(sipua_t *ua);

	int (*regist)(sipua_t *sipua, char *registrar, char *id, int idbytes, int seconds, sipua_action_t *act);
	int (*unregist)(sipua_t *sipua, char *registrar, char *id, int idbytes);

	int (*connect)(sipua_t *ua,
					char *from, int from_bytes,
					char *to, int to_bytes, 
					char *subject, int subject_bytes,
					char *route, int route_bytes);
	
	int (*disconnect)(sipua_t *ua,
					char *from, int from_bytes,
					char *to, int to_bytes, 
					char *subject, int subject_bytes);
	
	int (*reconnect)(sipua_t *ua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen);
};

sipua_t *sipua_new(uint16 proxy_port, char *firewall, void *config);

