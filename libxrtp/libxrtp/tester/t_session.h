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

#include "../xrtp.h"
#include "spu_text.h"
#include "sipua.h"

#include <string.h>                          
#include <stdlib.h>

#define PT 96
#define PROFILE_TYPE "text/rtp-test" 

typedef struct rtp_capable_s rtp_capable_t;
struct rtp_capable_s
{
	struct capable_s capable;

	uint8 profile_no;
	char profile_type[32];
	char ip[64];
	uint16 rtp_port;
	uint16 rtcp_port;
};
capable_t* rtp_new_capable(uint8 pt, char *type, char *ip, uint16 rtp_port, uint16 rtcp_port);

typedef struct subt_sender_s subt_sender_t;
typedef struct subt_recvr_s subt_recvr_t;

subt_sender_t* sender_new(sipua_t *sipua);
int sender_shutdown(subt_sender_t *send);

subt_recvr_t* recvr_new(sipua_t *sipua);
int recvr_shutdown(subt_recvr_t *recv);
int recvr_start(subt_recvr_t *recvr, char *to_cn, int to_cnlen);
int recvr_end(subt_recvr_t *recvr, char *to_cn, int to_cnlen);
