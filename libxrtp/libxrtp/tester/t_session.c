/***************************************************************************
                          t_session.c  -  session tester
                             -------------------
    begin                : Tue Nov 11 2003
    copyright            : (C) 2003 by Heming Ling
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

#include "t_session.h"
#include <timedia/xstring.h>

#define TEST_LOG

#ifdef TEST_LOG
 #define test_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define test_log(fmtargs)
#endif

struct rtp_params_s
{
    int8 profile_no; /* dynamic assignment */
    char profile_type[256];

	char ip[64];
	uint16 rtp_port;
	uint16 rtcp_port;

} rtp_params = {96, "text/rtp-test", "127.0.0.1", 3000, 3001};

int rtp_done_capable(capable_t *cap)
{
	free(cap);
	return UA_OK;
}

int rtp_match_capable(capable_t *cap1, capable_t *cap2)
{
	rtp_capable_t *rtp1 = (rtp_capable_t*)cap1;
	rtp_capable_t *rtp2 = (rtp_capable_t*)cap2;

	if(strcmp(rtp1->profile_type, rtp2->profile_type))
		return 1;
	
	return 0;
}

capable_t* rtp_new_capable(uint8 pt, char *type, char *ip, uint16 rtp_port, uint16 rtcp_port)
{
	rtp_capable_t *cap = malloc(sizeof(rtp_capable_t));

	cap->profile_no = pt;
	strcpy(cap->profile_type, type);
	strcpy(cap->ip, ip);
	cap->rtp_port = rtp_port;
	cap->rtcp_port = rtcp_port;

	cap->capable.done = rtp_done_capable;
	cap->capable.match = rtp_match_capable;

	return (capable_t*)cap;
}

int main(int argc, char** argv)
{
	subt_sender_t *sender = NULL;
	subt_recvr_t *recvr = NULL;
	sipua_t *sipua;

	char *proxy_ip = "127.0.0.1";
	uint16 proxy_port = 3500;

	char *sender_cn = "spu_sender";

	xclock_t *clock = time_start();
	rtime_t ms_remain;

    /* Init xrtp */
    if(xrtp_init() < XRTP_OK)
        return -1;

	sipua = sipua_new(proxy_ip, proxy_port, NULL);

    /* setup end point */
	sender = sender_new(sipua);
	recvr = recvr_new(sipua);

	recvr_start(recvr, sender_cn, strlen(sender_cn));
	test_log(("main: recvr started\n"));

	time_msec_sleep(clock, 600000, &ms_remain);
	test_log(("main: wake up\n"));
	
	recvr_end(recvr, sender_cn, strlen(sender_cn));
	time_msec_sleep(clock, 3000, &ms_remain);
	 
	sender_shutdown(sender);
	recvr_shutdown(recvr);
    
    /* Fin xrtp */
    xrtp_done();

	time_end(clock);
        
    return 0;
}
