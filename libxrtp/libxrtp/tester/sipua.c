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

#include "sipua.h"
#include <stdlib.h>
#include <string.h>
 
#define UA_LOG

#ifdef UA_LOG
 #include <stdio.h>
 #define ua_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define ua_log(fmtargs)
#endif

#define MAX_REC 16
#define MAX_REC_CAP 8

typedef struct record_impl_s record_impl_t;
struct record_impl_s
{
	struct sipua_record_s record;

	int ncap;
	capable_t* caps[MAX_REC_CAP];
};

typedef struct sipua_impl_s sipua_impl_t;
struct sipua_impl_s
{
	struct sipua_s sipua;

	/* participants registered */
	int nrec;
	sipua_record_t* records[MAX_REC];
};

int sipua_done_record(sipua_record_t *record)
{
	record_impl_t *rec = (record_impl_t*)record;
	int i;

	for(i=0; i<rec->ncap; i++)
	{
		rec->caps[i]->done(rec->caps[i]);
	}

	free(rec);

	return UA_OK;
}

int sipua_record_enable(sipua_record_t *record, capable_t *cap)
{
	record_impl_t *rec = (record_impl_t*)record;
	int i;

	if(rec->ncap == MAX_REC_CAP)
		return UA_FAIL;

	for(i=0; i<rec->ncap; i++)
	{
		if(rec->caps[i]->match(rec->caps[i], cap))
			return UA_OK;
	}

	rec->caps[rec->ncap++] = cap;

	return UA_OK;
}

int sipua_record_disable(sipua_record_t *record, capable_t *cap)
{
	record_impl_t *rec = (record_impl_t*)record;

	int i;
	for(i=0; i<rec->ncap; i++)
	{
		if(rec->caps[i]->match(rec->caps[i], cap))
		{
			if(i == MAX_REC_CAP-1)
				rec->caps[i] = NULL;
			else
				rec->caps[i] = rec->caps[i+1];

			rec->ncap--;
		}

		if(rec->caps[i] == NULL)
			break;
	}

	return UA_OK;
}

int sipua_record_support(sipua_record_t *record, capable_t *cap)
{
	record_impl_t *rec = (record_impl_t*)record;
	int i;
	for(i=0; i<rec->ncap; i++)
	{
		if(rec->caps[i] == cap || rec->caps[i]->match(rec->caps[i], cap))
			return 1;
	}

	return 0;
}

sipua_record_t *sipua_new_record(char cname[], int cnlen, 
						   void *cb_connect_user, int(*cb_connect)(void*,char*,int,capable_t**,int), 
						   void *cb_reconnect_user, int(*cb_reconnect)(void*,char*,int,capable_t**,int),
						   void *cb_disconnect_user, int(*cb_disconnect)(void*,char*,int))
{
	sipua_record_t *rec;
	rec = malloc(sizeof(struct record_impl_s));
	if(!rec)
	{
		ua_log(("record_new: No memory\n"));
		return NULL;
	}
	memset(rec, 0, sizeof(struct record_impl_s));

	rec->done = sipua_done_record;
	rec->enable = sipua_record_enable;
	rec->disable = sipua_record_disable;
	rec->support = sipua_record_support;

	strncpy(rec->cname,cname,cnlen);
	rec->cnlen = cnlen;

	rec->connect_notify_user = cb_connect_user;
	rec->connect_notify = cb_connect;
	rec->reconnect_notify_user = cb_reconnect_user;
	rec->reconnect_notify = cb_reconnect;
	rec->disconnect_notify_user = cb_disconnect_user;
	rec->disconnect_notify = cb_disconnect;

	return rec;
}

int sipua_done(sipua_t *sipua)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;
	int i;
	for(i=0; i<ua->nrec; i++)
	{
		ua->records[i]->done(ua->records[i]);
	}

	free(ua);

	return UA_OK;
}

int sipua_regist(sipua_t *sipua, char *cn, int cnlen, sipua_record_t *rec)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;
	int i;

	if(ua->nrec == MAX_REC)
		return UA_FAIL;

	for(i=0; i<ua->nrec; i++)
	{
		if(strncmp(rec->cname, ua->records[i]->cname, cnlen) == 0)
		{
			ua->records[i] = rec;
			return UA_OK;
		}
	}

	ua->records[ua->nrec++] = rec;

	ua_log(("sipua_regist: cn[%s] registered, %d records now\n", cn, ua->nrec));

	return UA_OK;
}

int sipua_unregist(sipua_t *sipua, char *cn, int cnlen)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	int i;
	for(i=0; i<ua->nrec; i++)
	{
		sipua_record_t *rec = ua->records[i];

		if(strncmp(rec->cname, cn, cnlen) == 0)
		{
			if(i == MAX_REC-1)
				ua->records[i] = NULL;
			else
				ua->records[i] = ua->records[i+1];

			ua->nrec--;
		}

		if(ua->records[i] == NULL)
			break;
	}

	return UA_OK;
}

int sipua_connect(sipua_t *sipua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen)
{
	record_impl_t *from_rec = NULL, *to_rec = NULL;
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	int i, from_ok=0, to_ok=0;
	for(i=0; i<ua->nrec; i++)
	{
		if(strncmp(from_cn, ua->records[i]->cname, from_cnlen) == 0)
		{
			from_rec =  (record_impl_t*)ua->records[i];
			from_ok = 1;

			if(to_ok)
				break;
		}

		if(strncmp(to_cn, ua->records[i]->cname, to_cnlen) == 0)
		{
			to_rec =  (record_impl_t*)ua->records[i];
			to_ok = 1;

			if(from_ok)
				break;
		}
	}

	if(!to_rec || !from_rec)
		return UA_FAIL;

	/* Three way handshake */
	if(to_rec->record.connect_notify(to_rec->record.connect_notify_user, from_cn, from_cnlen, from_rec->caps, from_rec->ncap) < UA_OK)
		return UA_FAIL;

	/* if connect fail, ncap of to_cn is ZERO */
	from_rec->record.connect_notify(from_rec->record.connect_notify_user, to_cn, to_cnlen, to_rec->caps, to_rec->ncap);

	ua_log(("sipua_connect: sip session established ======================================\n"));

	return UA_OK;
}

int sipua_reconnect(sipua_t *sipua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen)
{
	record_impl_t *from_rec = NULL, *to_rec = NULL;
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	int i, from_ok=0, to_ok=0;
	for(i=0; i<ua->nrec; i++)
	{
		if(strncmp(from_cn, ua->records[i]->cname, from_cnlen) == 0)
		{
			from_rec =  (record_impl_t*)ua->records[i];
			from_ok = 1;

			if(to_ok)
				break;
		}

		if(strncmp(to_cn, ua->records[i]->cname, to_cnlen) == 0)
		{
			to_rec =  (record_impl_t*)ua->records[i];
			to_ok = 1;

			if(from_ok)
				break;
		}
	}

	if(!to_rec || !from_rec)
		return UA_FAIL;

	/* Three way handshake */
	if(to_rec->record.reconnect_notify(to_rec->record.reconnect_notify_user, from_cn, from_cnlen, from_rec->caps, from_rec->ncap) < UA_OK)
		return UA_FAIL;

	/* if reconnect fail, ncap of to_cn is ZERO */
	from_rec->record.reconnect_notify(from_rec->record.reconnect_notify_user, to_cn, to_cnlen, to_rec->caps, to_rec->ncap);

	ua_log(("sipua_reconnect: session modified ======================================\n"));

	return UA_OK;
}

int sipua_disconnect(sipua_t *sipua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen)
{
	record_impl_t *from_rec = NULL, *to_rec = NULL;
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	int i, from_ok=0, to_ok=0;
	for(i=0; i<ua->nrec; i++)
	{
		if(strncmp(from_cn, ua->records[i]->cname, from_cnlen) == 0)
		{
			from_rec =  (record_impl_t*)ua->records[i];
			from_ok = 1;

			if(to_ok)
				break;
		}

		if(strncmp(to_cn, ua->records[i]->cname, to_cnlen) == 0)
		{
			to_rec =  (record_impl_t*)ua->records[i];
			to_ok = 1;

			if(from_ok)
				break;
		}
	}

	if(!to_rec || !from_rec)
		return UA_FAIL;

	/* Three way handshake */
	to_rec->record.disconnect_notify(to_rec->record.disconnect_notify_user, from_cn, from_cnlen);

	from_rec->record.disconnect_notify(from_rec->record.disconnect_notify_user, to_cn, to_cnlen);

	ua_log(("sipua_reconnect: session finished ======================================\n"));

	return UA_OK;
}

sipua_t *sipua_new(char *proxy_ip, uint16 proxy_port, void *config)
{
	sipua_t *sipua = malloc(sizeof(struct sipua_impl_s));
	if(!sipua)
	{
		ua_log(("sipua_new: No memory\n"));
		return NULL;
	}

	memset(sipua, 0, sizeof(struct sipua_impl_s));

	strcpy(sipua->proxy_ip, proxy_ip);
	sipua->proxy_port = proxy_port;

	sipua->regist = sipua_regist;
	sipua->unregist = sipua_unregist;

	sipua->connect = sipua_connect;
	sipua->reconnect = sipua_reconnect;

	sipua->done = sipua_done;

	return sipua;
}
