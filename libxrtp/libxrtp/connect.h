/***************************************************************************
                          port.h  -  network port interface of a handler
                             -------------------
    begin                : Sat Oct 4 2003
    copyright            : (C) 2003 by Heming Ling
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef XRTP_CONNECT_H
#define XRTP_CONNECT_H

#define	DEFAULT_PORTNO	0

typedef struct xrtp_port_s xrtp_port_t;
/*
  typedef struct port_param_s port_param_t;
  struct port_param_s{

     uint32 local_portno;
     char * local_addr_str;
  };
*/
typedef struct session_connect_s session_connect_t;
  
typedef struct xrtp_teleport_s xrtp_teleport_t;

enum port_type_e { RTP_PORT, RTCP_PORT };
  
session_connect_t * connect_new(xrtp_port_t * port, xrtp_teleport_t * tport);

int connect_done(session_connect_t * conn);

int connect_io(session_connect_t * conn);
  
extern DECLSPEC
int 
connect_match(session_connect_t * conn1, session_connect_t * conn2);

extern DECLSPEC
int 
connect_from_teleport(session_connect_t * conn1, xrtp_teleport_t * tport);

int connect_receive(session_connect_t * conn, char **r_buff, int *header_bytes, rtime_t *ms, rtime_t *us, rtime_t *ns);

int connect_send(session_connect_t * conn, char *r_buff, int datalen);

/* These are for old static port protocol, which is rtcp is rtp + 1, It's NOT suggest to use, Simply for back compatability */
extern DECLSPEC
session_connect_t *
connect_rtp_to_rtcp(session_connect_t * rtp_conn);

extern DECLSPEC
session_connect_t *
connect_rtcp_to_rtp(session_connect_t * rtcp_conn);
  
extern DECLSPEC 
xrtp_port_t*
port_new(char * addr, uint16 portno, enum port_type_e type);

extern DECLSPEC 
int 
port_done(xrtp_port_t * port);

int port_set_session(xrtp_port_t * port, xrtp_session_t * session);

int port_io(xrtp_port_t * port);

int port_match_io(xrtp_port_t * port, int io);
  
int port_match(xrtp_port_t* port, char *ip, uint16 pno);

int port_portno(xrtp_port_t* port);

int port_is_multicast(xrtp_port_t * port);

int port_poll(xrtp_port_t * port, rtime_t timeout_usec);
  
int port_incoming(xrtp_port_t * port);
  
extern DECLSPEC xrtp_teleport_t*
teleport_new(char * addr, uint16 portno);

extern DECLSPEC int
teleport_done(xrtp_teleport_t * tport);

char * teleport_name(xrtp_teleport_t * tport);

uint32 teleport_portno(xrtp_teleport_t * tport);

#endif
