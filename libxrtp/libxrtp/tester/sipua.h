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

typedef struct capable_s capable_t;

struct sipua_record_s
{
    char cname[MAX_CN_BYTES];
	int cnlen;
	char sip_ip[MAX_IP_LEN];
	uint16 sip_port;

	int (*done)(sipua_record_t *rec);
	int (*enable)(sipua_record_t *rec, capable_t *cap);
	int (*disable)(sipua_record_t *rec, capable_t *cap);
	int (*support)(sipua_record_t *rec, capable_t *cap);

	void *connect_notify_user;
	int (*connect_notify)(void *user, char *cname, int cnlen, capable_t *caps[], int ncap);

	void *reconnect_notify_user;
	int (*reconnect_notify)(void *user, char *cname, int cnlen, capable_t *caps[], int ncap);

	void *disconnect_notify_user;
	int (*disconnect_notify)(void *user, char *cname, int cnlen);
};
sipua_record_t *sipua_new_record(char *cname, int cnlen, 
						   void *cb_connect_user, int(*cb_connect)(void*,char*,int,capable_t**,int), 
						   void *cb_reconnect_user, int(*cb_reconnect)(void*,char*,int,capable_t**,int),
						   void *cb_disconnect_user, int(*cb_disconnect)(void*,char*,int));

struct capable_s
{
	int (*done)(capable_t *cap);
	int (*match)(capable_t *cap1, capable_t *cap2);
};

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

