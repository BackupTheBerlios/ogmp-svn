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

#define MAX_CN_BYTES 256 /* max value in rtcp */
#define MAX_IP_LEN   64  /* may enough for ipv6 ? */

/* Contact Proxy
 * Sender register its session info to proxy
 * recvr contact sender by interact with proxy
 */
typedef struct sipua_s sipua_t;
typedef struct sipua_record_s sipua_record_t;

struct sipua_record_s
{
    char cname[MAX_CN_BYTES];
	int cnlen;
	char sip_ip[MAX_IP_LEN];
	uint16 sip_port;

	int (*done)(sipua_record_t *rec);
	int (*enable)(sipua_record_t *rec, capable_descript_t *cap);
	int (*disable)(sipua_record_t *rec, capable_descript_t *cap);
	int (*support)(sipua_record_t *rec, capable_descript_t *cap);

	void *oncall_user;
	int (*oncall)(void *user, char *cname, int cnlen, capable_descript_t* oppo_caps[], int oppo_ncap, capable_descript_t* **my_caps);

	void *onconnect_user;
	int (*onconnect)(void *user, char *cname, int cnlen, capable_descript_t* oppo_caps[], int oppo_ncap, capable_descript_t* **my_caps);

	void *onreset_user;
	int (*onreset)(void *user, char *cname, int cnlen, capable_descript_t* oppo_caps[], int oppo_ncap, capable_descript_t* **my_caps);

	void *onbye_user;
	int (*onbye)(void *user, char *cname, int cnlen);
};
sipua_record_t *sipua_new_record(void *cb_oncall_user, int(*cb_oncall)(void*,char*,int,capable_descript_t**,int,capable_descript_t***), 
								 void *cb_onconnect_user, int(*cb_onconnect)(void*,char*,int,capable_descript_t**,int,capable_descript_t***), 
								 void *cb_onreset_user, int(*cb_onreset)(void*,char*,int,capable_descript_t**,int,capable_descript_t***),
								 void *cb_onbye_user, int(*cb_onbye)(void*,char*,int));

struct sipua_s
{
	char proxy_ip[64];
	uint16 proxy_port;

	int (*done)(sipua_t *ua);

	int (*regist)(sipua_t *ua, char *cn, int cnlen, sipua_record_t *rec);
	int (*unregist)(sipua_t *ua, char *cn, int cnlen);

	int (*connect)(sipua_t *ua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen);
	int (*reconnect)(sipua_t *ua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen);
	int (*disconnect)(sipua_t *ua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen);
};
sipua_t *sipua_new(char *proxy_ip, uint16 proxy_port, void *config);

