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
#define MAX_CN_BYTES 256 /* max value in rtcp */

typedef struct action_impl_s action_impl_t;
struct action_impl_s
{
	struct sipua_action_s action;

	int ncap;
	capable_descript_t* caps[MAX_REC_CAP];
};

typedef struct sipua_impl_s sipua_impl_t;
typedef struct sipua_actionrec_s
{
	char cname[MAX_CN_BYTES];
	int cnlen;

	sipua_action_t *action;

}sipua_actionrec_t;

struct sipua_impl_s
{
	struct sipua_s sipua;

	/* participants registered */
	int nact;
	int lastrec;

	sipua_actionrec_t actions[MAX_REC];
};

int sipua_done_action(sipua_action_t *action)
{
	action_impl_t *act = (action_impl_t*)action;
	int i;

	for(i=0; i<act->ncap; i++)
	{
		act->caps[i]->done(act->caps[i]);
	}

	free(act);

	return UA_OK;
}

#if 0
int sipua_action_enable(sipua_action_t *action, capable_descript_t *cap)
{
	action_impl_t *act = (action_impl_t*)action;
	int i;

	if(act->ncap == MAX_REC_CAP)
		return UA_FAIL;

	for(i=0; i<act->ncap; i++)
	{
		if(act->caps[i]->match(act->caps[i], cap))
			return UA_OK;
	}

	act->caps[act->ncap++] = cap;

	return UA_OK;
}

int sipua_action_disable(sipua_action_t *action, capable_descript_t *cap)
{
	action_impl_t *act = (action_impl_t*)action;

	int i;
	for(i=0; i<act->ncap; i++)
	{
		if(act->caps[i]->match(act->caps[i], cap))
		{
			if(i == MAX_REC_CAP-1)
				act->caps[i] = NULL;
			else
				act->caps[i] = act->caps[i+1];

			act->ncap--;
		}

		if(act->caps[i] == NULL)
			break;
	}

	return UA_OK;
}

int sipua_action_support(sipua_action_t *action, capable_descript_t *cap)
{
	action_impl_t *act = (action_impl_t*)action;
	int i;
	for(i=0; i<act->ncap; i++)
	{
		if(act->caps[i] == cap || act->caps[i]->match(act->caps[i], cap))
			return 1;
	}

	return 0;
}
#endif

sipua_action_t *sipua_new_action(void *sip_user, int(*cb_oncall)(void*,char*,int,capable_descript_t**,int,capable_descript_t***), 
												 int(*cb_onconnect)(void*,char*,int,capable_descript_t**,int,capable_descript_t***), 
												 int(*cb_onreset)(void*,char*,int,capable_descript_t**,int,capable_descript_t***),
												 int(*cb_onbye)(void*,char*,int))
{
	sipua_action_t *act;
	act = malloc(sizeof(struct action_impl_s));
	if(!act)
	{
		ua_log(("action_new: No memory\n"));
		return NULL;
	}
	memset(act, 0, sizeof(struct action_impl_s));

	act->done = sipua_done_action;
//	act->enable = sipua_action_enable;
//	act->disable = sipua_action_disable;
//	act->support = sipua_action_support;

	act->sip_user = sip_user;
	act->oncall = cb_oncall;

	act->onconnect = cb_onconnect;

	act->onreset = cb_onreset;

	act->onbye = cb_onbye;

	return act;
}

int sipua_done(sipua_t *sipua)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;
	int i;
	for(i=0; i<ua->nact; i++)
	{
		ua->actions[i].action->done(ua->actions[i].action);
	}

	free(ua);

	return UA_OK;
}

int sipua_regist(sipua_t *sipua, char *registrar, char *id, int idbytes, int seconds, sipua_action_t *act)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;
	sipua_action_t *actrec = NULL;

	int nullrec = -1;
	int i;

	if(ua->nact == MAX_REC)
		return UA_FAIL;

	for(i=0; i<=ua->lastrec; i++)
	{
		if(ua->actions[i].action == NULL)
		{
			if(nullrec == -1)
				nullrec = i;

			continue;
		}
		
		if(0 == strncmp(ua->actions[i].cname, id, ua->actions[i].cnlen))
		{
			strncpy(ua->actions[i].cname,id,idbytes);
			ua->actions[i].cnlen = idbytes;

			ua->actions[i].action = act;
			return UA_OK;
		}
	}

	if(nullrec == -1)
	{
		nullrec = ua->lastrec+1;
		ua->lastrec = nullrec;
	}

	strncpy(ua->actions[nullrec].cname, id, idbytes);

	ua->actions[nullrec].cnlen = idbytes;

	ua->actions[nullrec].action = act;

	ua->nact++;

	ua_log(("sipua_regist: rec #%d: cn[%s] registered\n", nullrec, ua->actions[nullrec].cname));

	return UA_OK;
}

int sipua_unregist(sipua_t *sipua, char *registrar, char *cn, int cnlen)
{
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	int i;
	for(i=0; i<=ua->lastrec; i++)
	{
		if(!ua->actions[i].action)
			continue;

		if(0 == strncmp(ua->actions[i].cname, cn, cnlen))
		{
			ua->actions[i].action = NULL;
			ua->actions[i].cnlen = 0;

			ua->nact--;
			break;
		}
	}

	if(i == ua->lastrec)
	{
		for(i = ua->lastrec; i>=0; i--)
			if(ua->actions[i].action != NULL)
				break;
				
		ua->lastrec = i;
	}

	return UA_OK;
}

int sipua_connect(sipua_t *sipua,
					char *from_cn, int from_cnlen,
					char *to_cn, int to_cnlen, 
					char *subject, int subject_bytes,
					char *route, int route_bytes)
{
	action_impl_t *from_act = NULL, *to_act = NULL;
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	capable_descript_t **to_caps, **selected_caps, **estab_caps;
	int to_ncap, selected_ncap, estab_ncap;

	int i, from_ok=0, to_ok=0;

	ua_log(("sipua_connect: Call from [%s] to [%s]\n", from_cn, to_cn));

	for(i=0; i<=ua->lastrec; i++)
	{
		if(!ua->actions[i].action)
			continue;
		
		ua_log(("sipua_connect: rec #%d: cn[%s]\n", i, ua->actions[i].cname));

		if(strncmp(from_cn, ua->actions[i].cname, from_cnlen) == 0)
		{
			from_act =  (action_impl_t*)ua->actions[i].action;

			from_ok = 1;
			if(to_ok)
				break;
		}

		if(strncmp(to_cn, ua->actions[i].cname, to_cnlen) == 0)
		{
			to_act =  (action_impl_t*)ua->actions[i].action;

			to_ok = 1;
			if(from_ok)
				break;
		}
	}

	if(!from_act)
	{
		ua_log(("sipua_connect: [%s] is not present\n", from_cn));
		return UA_FAIL;
	}

	if(!to_act)
	{
		ua_log(("sipua_connect: [%s] is not present\n", to_cn));
		return UA_FAIL;
	}

	/* Three way handshake */
	to_ncap = to_act->action.oncall(to_act->action.sip_user, from_cn, from_cnlen, from_act->caps, from_act->ncap, &to_caps);
	
	selected_ncap = from_act->action.onconnect(from_act->action.sip_user, to_cn, to_cnlen, to_caps, to_ncap, &selected_caps);
	
	estab_ncap = to_act->action.onconnect(to_act->action.sip_user, from_cn, from_cnlen, selected_caps, selected_ncap, &estab_caps);

	/* Established */
	ua_log(("sipua_connect: sip session established ======================================\n"));

	return UA_OK;
}

int sipua_reconnect(sipua_t *sipua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen)
{
	action_impl_t *from_act = NULL, *to_act = NULL;
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	int i, from_ok=0, to_ok=0;

	capable_descript_t **selected_caps, **estab_caps;
	int selected_ncap, estab_ncap;

	for(i=0; i<=ua->lastrec; i++)
	{
		if(!ua->actions[i].action)
			continue;
		
		if(strncmp(from_cn, ua->actions[i].cname, from_cnlen) == 0)
		{
			from_act =  (action_impl_t*)ua->actions[i].action;
			from_ok = 1;

			if(to_ok)
				break;
		}

		if(strncmp(to_cn, ua->actions[i].cname, to_cnlen) == 0)
		{
			to_act =  (action_impl_t*)ua->actions[i].action;
			to_ok = 1;

			if(from_ok)
				break;
		}
	}

	if(!to_act || !from_act)
		return UA_FAIL;

	/* Three way handshake */
	selected_ncap = to_act->action.onreset(to_act->action.sip_user, from_cn, from_cnlen, from_act->caps, from_act->ncap, &selected_caps);

	estab_ncap = from_act->action.onreset(from_act->action.sip_user, to_cn, to_cnlen, selected_caps, selected_ncap, &estab_caps);

	ua_log(("sipua_reconnect: session modified ======================================\n"));

	return UA_OK;
}

int sipua_disconnect(sipua_t *sipua, char *from_cn, int from_cnlen, char *to_cn, int to_cnlen)
{
	action_impl_t *from_act = NULL, *to_act = NULL;
	sipua_impl_t *ua = (sipua_impl_t*)sipua;

	int i, from_ok=0, to_ok=0;
	for(i=0; i<=ua->lastrec; i++)
	{
		if(!ua->actions[i].action)
			continue;
		
		if(strncmp(from_cn, ua->actions[i].cname, from_cnlen) == 0)
		{
			from_act =  (action_impl_t*)ua->actions[i].action;
			from_ok = 1;

			if(to_ok)
				break;
		}

		if(strncmp(to_cn, ua->actions[i].cname, to_cnlen) == 0)
		{
			to_act =  (action_impl_t*)ua->actions[i].action;
			to_ok = 1;

			if(from_ok)
				break;
		}
	}

	if(!to_act || !from_act)
		return UA_FAIL;

	to_act->action.onbye(to_act->action.sip_user, from_cn, from_cnlen);
	from_act->action.onbye(from_act->action.sip_user, to_cn, to_cnlen);

	ua_log(("sipua_reconnect: session finished ======================================\n"));

	return UA_OK;
}

sipua_t *sipua_new(uint16 proxy_port, char *firewall, void *config)
{
	sipua_t *sipua = malloc(sizeof(struct sipua_impl_s));
	if(!sipua)
	{
		ua_log(("sipua_new: No memory\n"));
		return NULL;
	}

	memset(sipua, 0, sizeof(struct sipua_impl_s));

	sipua->regist = sipua_regist;
	sipua->unregist = sipua_unregist;

	sipua->connect = sipua_connect;

	sipua->done = sipua_done;

	return sipua;
}
