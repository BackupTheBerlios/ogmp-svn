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
	capable_descript_t* caps[MAX_REC_CAP];
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

int sipua_record_enable(sipua_record_t *record, capable_descript_t *cap)
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

int sipua_record_disable(sipua_record_t *record, capable_descript_t *cap)
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

int sipua_record_support(sipua_record_t *record, capable_descript_t *cap)
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

sipua_record_t *sipua_new_record(void *cb_oncall_user, int(*cb_oncall)(void*,char*,int,capable_descript_t**,int,capable_descript_t***), 
								 void *cb_onconnect_user, int(*cb_onconnect)(void*,char*,int,capable_descript_t**,int,capable_descript_t***), 
								 void *cb_onreset_user, int(*cb_onreset)(void*,char*,int,capable_descript_t**,int,capable_descript_t***),
								 void *cb_onbye_user, int(*cb_onbye)(void*,char*,int))
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

	rec->oncall_user = cb_oncall_user;
	rec->oncall = cb_oncall;

	rec->onconnect_user = cb_onconnect_user;
	rec->onconnect = cb_onconnect;

	rec->onreset_user = cb_onreset_user;
	rec->onreset = cb_onreset;

	rec->onbye_user = cb_onbye_user;
	rec->onbye = cb_onbye;

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

int sipua_regist(sipua_t *sipua, char *cname, int cnlen, sipua_record_t *rec)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;
	int i;

	if(ua->nrec == MAX_REC)
		return UA_FAIL;

	for(i=0; i<ua->nrec; i++)
	{
		if(strncmp(ua->records[i]->cname, cname, ua->records[i]->cnlen) == 0)
		{
			strncpy(rec->cname,cname,cnlen);
			rec->cnlen = cnlen;

			ua->records[i] = rec;
			return UA_OK;
		}
	}

	strncpy(rec->cname,cname,cnlen);
	rec->cnlen = cnlen;

	ua->records[ua->nrec++] = rec;

	ua_log(("sipua_regist: cn[%s] registered, %d records now\n", cname, ua->nrec));

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

	capable_descript_t **to_caps, **selected_caps, **estab_caps;
	int to_ncap, selected_ncap, estab_ncap;

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
	to_ncap = to_rec->record.oncall(to_rec->record.oncall_user, from_cn, from_cnlen, from_rec->caps, from_rec->ncap, &to_caps);
	
	selected_ncap = from_rec->record.onconnect(from_rec->record.onconnect_user, to_cn, to_cnlen, to_caps, to_ncap, &selected_caps);
	estab_ncap = to_rec->record.onconnect(to_rec->record.onconnect_user, from_cn, from_cnlen, selected_caps, selected_ncap, &estab_caps);

	ua_log(("sipua_connect: sip session established ======================================\n"));

	return UA_OK;
}

int sipua_reconnect(sipua_t *sipua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen)
{
	record_impl_t *from_rec = NULL, *to_rec = NULL;
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	int i, from_ok=0, to_ok=0;

	capable_descript_t **selected_caps, **estab_caps;
	int selected_ncap, estab_ncap;

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
	selected_ncap = to_rec->record.onreset(to_rec->record.onreset_user, from_cn, from_cnlen, from_rec->caps, from_rec->ncap, &selected_caps);

	estab_ncap = from_rec->record.onreset(from_rec->record.onreset_user, to_cn, to_cnlen, selected_caps, selected_ncap, &estab_caps);

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

	to_rec->record.onbye(to_rec->record.onbye_user, from_cn, from_cnlen);
	from_rec->record.onbye(from_rec->record.onbye_user, to_cn, to_cnlen);

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
