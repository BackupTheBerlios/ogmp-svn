/***************************************************************************
                          port_udp.c  -  UDP port
                             -------------------
    begin                : Fri Mar 28 2003
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
 
#include "session.h"
#include "const.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <timedia/xmalloc.h>
#include <timedia/socket.h> 
#include <timedia/inet.h> 
/*
#define UDP_LOG
#define UDP_DEBUG
*/
#ifdef UDP_LOG
	#define udp_log(fmtargs)  do{printf fmtargs;}while(0)
#else
	#define udp_log(fmtargs)  
#endif

#ifdef UDP_DEBUG
	#define udp_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
	#define udp_debug(fmtargs)  
#endif

#define LOOP_RTP_PORT  5000
#define LOOP_RTCP_PORT  5001

#define UDP_FLAGS 0  /* Refer to man(sendto) */

#define UDP_MAX_LEN   5000

#define IPV4_BYTES	16 /* 'xxx.xxx.xxx.xxx\0' */

/* bytes for ipv4 udp+ip header */
#define IPV4_UDPIP_HEADER_BYTES 28

 struct session_connect_s
 {
    xrtp_port_t * port;
    
    rtime_t usec_arrival;
    rtime_t msec_arrival;
	rtime_t nsec_arrival;

    struct sockaddr_in remote_addr;
    int addrlen;

    char *data_in;
    int datalen_in;
 };

struct xrtp_port_s
{
	enum port_type_e type;

    int socket;

    xrtp_session_t *session;

    xthr_lock_t *lock;

	char ip[IPV4_BYTES];
	uint16 portno;
};

struct xrtp_teleport_s
{
    uint16 portno;
    uint32 addr;
};

session_connect_t * connect_new(xrtp_port_t * port, xrtp_teleport_t * tport)
{
     session_connect_t * udp = (session_connect_t *)xmalloc(sizeof(struct session_connect_s));
     if(!udp)
	 {
		udp_debug(("connect_new: No memory\n"));
        return NULL;
     }   
     memset(udp, 0, sizeof(struct session_connect_s));
    
     udp->port = port;

     udp->remote_addr.sin_family = AF_INET;
     udp->remote_addr.sin_port = tport->portno;       /* Already in Network byteorder, see teleport_new() */
     udp->remote_addr.sin_addr.s_addr = tport->addr;  /* Already in Network byteorder, see inet_addr() */

     udp_log(("connect_new: new connect to [%s:%u]\n", 
		 inet_ntoa(udp->remote_addr.sin_addr), ntohs(udp->remote_addr.sin_port)));

     return udp;   
}

int connect_done(session_connect_t * conn)
{
     udp_log(("connect_done: free connect[#%d]\n", connect_io(conn)));

     if(conn->data_in) xfree(conn->data_in);
     xfree(conn);

     return XRTP_OK;
}

int connect_io(session_connect_t * conn)
{
     return conn->port->socket;
}

int connect_match(session_connect_t * conn1, session_connect_t * conn2)
{
     return conn1->remote_addr.sin_family == conn2->remote_addr.sin_family &&
            conn1->remote_addr.sin_addr.s_addr == conn2->remote_addr.sin_addr.s_addr &&
            conn1->remote_addr.sin_port == conn2->remote_addr.sin_port;
}

int connect_from_teleport(session_connect_t * conn, xrtp_teleport_t * tport)
{
      return conn->remote_addr.sin_family == AF_INET &&
             conn->remote_addr.sin_addr.s_addr == tport->addr &&
             conn->remote_addr.sin_port == tport->portno;
}

int connect_receive(session_connect_t * conn, char **r_buff, int *header_bytes, rtime_t *ms, rtime_t *us, rtime_t *ns)
{
      int datalen;
      
      datalen = conn->datalen_in;
      *r_buff = conn->data_in;
      
      conn->data_in = NULL;
      conn->datalen_in = 0;

	  *header_bytes = IPV4_UDPIP_HEADER_BYTES;

      *ms = conn->msec_arrival;
      *us = conn->usec_arrival;
	  *ns = conn->nsec_arrival;
      
      return datalen;
}

int connect_send(session_connect_t * conn, char *data, int datalen)
{
      int n;

	  /* test */                                                                               
      n = sendto(conn->port->socket, data, datalen, 0, (struct sockaddr *)&(conn->remote_addr), sizeof(struct sockaddr_in));
	  if(n<0)
	  {
		udp_debug(("connect_send: sending fail\n"));
	  }

      udp_log(("connect_send: send to [%s:%d]\n", inet_ntoa(conn->remote_addr.sin_addr), ntohs(conn->remote_addr.sin_port)));
      return n;
}

/* These are for old static port protocol, which is rtcp is rtp + 1, It's NOT suggest to use, Simply for back compatability */
session_connect_t * connect_rtp_to_rtcp(session_connect_t * rtp_conn)
{
      xrtp_port_t *rtp_port, *rtcp_port;
      xrtp_session_t * ses = NULL;

      uint16 pno;      
      
      session_connect_t * rtcp_conn = (session_connect_t *)xmalloc(sizeof(struct session_connect_s));
      if(!rtcp_conn)
        return NULL;

      memset(rtcp_conn, 0, sizeof(struct session_connect_s));

      ses = rtp_conn->port->session;
      session_ports(ses, &rtp_port, &rtcp_port);
      
      rtcp_conn->port = rtcp_port;

      rtcp_conn->remote_addr.sin_family = AF_INET;

      pno = ntohs(rtp_conn->remote_addr.sin_port);      
      rtcp_conn->remote_addr.sin_port = htons(++pno);       /* rtcp port = rtp port + 1 */

      rtcp_conn->remote_addr.sin_addr.s_addr = rtp_conn->remote_addr.sin_addr.s_addr;  /* Already in Network byteorder, see inet_addr() */

      udp_log(("connect_rtp_to_rtcp: rtp connect is ip[%s:%u] at socket[%d]\n", inet_ntoa(rtp_conn->remote_addr.sin_addr), ntohs(rtp_conn->remote_addr.sin_port), rtp_conn->port->socket));
      udp_log(("connect_rtp_to_rtcp: rtcp connect is ip[%s:%u] at socket[%d]\n", inet_ntoa(rtcp_conn->remote_addr.sin_addr), ntohs(rtcp_conn->remote_addr.sin_port), rtcp_conn->port->socket));

      return rtcp_conn;
}
  
session_connect_t * connect_rtcp_to_rtp(session_connect_t * rtcp_conn)
{
      xrtp_port_t *rtp_port, *rtcp_port;
	  xrtp_session_t * ses = NULL;

      uint16 pno = 0;

      session_connect_t * rtp_conn = (session_connect_t *)xmalloc(sizeof(struct session_connect_s));
      if(!rtp_conn)
	  {
        udp_log(("connect_rtcp_to_rtp: Fail to allocate memery for rtp connect\n"));
        return NULL;
      }
      
      memset(rtp_conn, 0, sizeof(struct session_connect_s));

      ses = rtcp_conn->port->session;
      session_ports(ses, &rtp_port, &rtcp_port);

      
      rtp_conn->port = rtp_port;

      rtp_conn->remote_addr.sin_family = AF_INET;

      pno = ntohs(rtcp_conn->remote_addr.sin_port);
      rtp_conn->remote_addr.sin_port = htons(--pno);       /* rtp port = rtcp port - 1 */

      rtp_conn->remote_addr.sin_addr.s_addr = rtcp_conn->remote_addr.sin_addr.s_addr;  /* Already in Network byteorder, see inet_addr() */

      udp_log(("connect_rtcp_to_rtp: rtcp[%s:%u]\n", inet_ntoa(rtcp_conn->remote_addr.sin_addr), ntohs(rtcp_conn->remote_addr.sin_port)));
      udp_log(("connect_rtcp_to_rtp: rtp[%s:%u]\n", inet_ntoa(rtp_conn->remote_addr.sin_addr), ntohs(rtp_conn->remote_addr.sin_port)));

      return rtp_conn;
}

xrtp_port_t * port_new(char *local_addr,  uint16 local_portno, enum port_type_e type)
{
     xrtp_port_t * port = NULL;

     struct sockaddr_in sin;

     if(!local_addr) return NULL;

	 udp_log(("port_new: %s:%d\n", local_addr, local_portno));

     port = (xrtp_port_t *)xmalloc(sizeof(struct xrtp_port_s));

     if(!port) return NULL;

     port->session = NULL;
     port->type = type;

     port->socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if(port->socket == SOCKET_INVALID){
	 
		 udp_debug(("port_new: fail to create  socket\n"));
		 return NULL;
	 }

	
     /*
     setsockopt(port->socket, SOL_IP, IP_PKTINFO, (void *)1, sizeof(int));
     */
              
     sin.sin_family = AF_INET;
     sin.sin_port = htons(local_portno);
     if(inet_aton(local_addr, &(sin.sin_addr)) == 0) /* string to int addr */
	 {
        udp_debug(("port_new: Illegal ip address\n"));
        socket_close(port->socket);
        xfree(port);
        
        return NULL;
     }
     
     if(bind(port->socket, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == SOCKET_FAIL)
	 {
        udp_debug(("port_new: Fail to name socket '%s:%d'\n", local_addr, local_portno));
        socket_close(port->socket);
        port->socket = 0;
        xfree(port);
           
        return NULL;
     }
        
     port->lock = xthr_new_lock();

	 strcpy(port->ip, local_addr);
	 port->portno = local_portno;

     udp_log(("port_new: socket[%d] opened as local ip[%u]:%u\n", port->socket, sin.sin_addr.s_addr, ntohs(sin.sin_port)));

     return port;
  }

  int port_set_session(xrtp_port_t * port, xrtp_session_t * ses){

     port->session = ses;

     return XRTP_OK;
  }

  int port_done(xrtp_port_t * port){

     socket_close(port->socket);
     xthr_done_lock(port->lock);
     xfree(port);

     return XRTP_OK;
  }

  int port_io(xrtp_port_t * port){

     return port->socket;
  }

  int port_match_io(xrtp_port_t * port, int io){

     return port->socket == io;
  }

/* Not implemented multicast yet */
int port_is_multicast(xrtp_port_t * port)
{
     return XRTP_NO;
}

int port_match(xrtp_port_t *port, char *ip, uint16 pno)
{
     if(strcmp(port->ip, ip) != 0)
		 return 0;

     if(port->portno != pno)
		 return 0;

	 return 1;
}

int port_portno(xrtp_port_t* port)
{
	return port->portno;
}

  int port_poll(xrtp_port_t * port, rtime_t timeout_usec)
  {
     fd_set io_set;

	 int n;
     
     struct timeval tout;
     tout.tv_sec = timeout_usec / 1000000;     
     tout.tv_usec = timeout_usec % 1000000;

     FD_ZERO(&io_set);
     FD_SET(port->socket, &io_set);

     udp_log(("port_poll: check incoming and timeout is set to %d seconds %d microseconds\n", (int)(tout.tv_sec), (int)(tout.tv_usec)));

     n = select(port->socket+1, &io_set, NULL, NULL, &tout);

     if(n == 1)
        port_incoming(port);

     return n;
  } 

  int port_incoming(xrtp_port_t * port)
  {
     char data[UDP_MAX_LEN];
     struct sockaddr_in remote_addr;

     int addrlen, datalen;
     session_connect_t * conn = NULL;

     /* Determine the incoming packet address from recvfrom return */
	 addrlen = sizeof(remote_addr);
     datalen = recvfrom(port->socket, data, UDP_MAX_LEN, UDP_FLAGS, (struct sockaddr *)&remote_addr, &addrlen);

     if(!port->session)
	 {
        udp_log(("port_incoming: discard data from [%s:%d] as no session bound to the port\n", inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port)));
        return XRTP_EREFUSE;
     }

     /* From Address 0:0 is unacceptable */
     if(remote_addr.sin_addr.s_addr == 0 && remote_addr.sin_port == 0)
	 {     
        udp_log(("port_incoming: discard data from [%s:%d] which is unacceptable !\n", inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port)));
        return XRTP_EREFUSE;
     }
        
     conn = (session_connect_t *)xmalloc(sizeof(struct session_connect_s));
     if(!conn)
	 {
        udp_debug(("port_incoming: fail to allocate connect of incoming\n"));
        return XRTP_EMEM;
     }     
     memset(conn, 0, sizeof(struct session_connect_s));

     conn->data_in = (char *)xmalloc(datalen);
     if(!conn->data_in)
	 {
        udp_log(("port_incoming: fail to allocate data space of incoming connect\n"));
        
        xfree(conn);
        return XRTP_EMEM;
     }
     memset(conn->data_in, 0, datalen);
     
     conn->addrlen = sizeof(struct sockaddr_in);
     conn->datalen_in = datalen;
     conn->remote_addr = remote_addr;
     conn->port = port;
     
     if(conn->datalen_in > 0)
        memcpy(conn->data_in, data, datalen);

     time_rightnow(session_clock(port->session), &(conn->msec_arrival), &(conn->usec_arrival), &(conn->nsec_arrival));
     
     udp_log(("port_incoming: received %d bytes from [%s:%d] on @%dms/%dus/%dns\n", datalen, inet_ntoa(conn->remote_addr.sin_addr), ntohs(conn->remote_addr.sin_port), conn->msec_arrival, conn->usec_arrival, conn->nsec_arrival));

     if(port->type == RTP_PORT)
	 {
        session_rtp_arrived(port->session, conn); /* High speed schedle */
     }
	 else
	 {
        session_rtcp_arrival_notified(port->session, conn, conn->msec_arrival); /* Low speed schedule */
     }
        
     return XRTP_OK;
  }

  xrtp_teleport_t * teleport_new(char * addr_str, uint16 pno){

     xrtp_teleport_t * tp = xmalloc(sizeof(struct xrtp_teleport_s));
     if(tp){

        tp->portno = htons(pno);
        tp->addr = inet_addr(addr_str);
        if(inet_aton(addr_str, (struct in_addr *)&(tp->addr)) == 0){ /* string to int addr */

            udp_log(("teleport_new: Illegal ip address\n"));
            xfree(tp);

            return NULL;
        }
     }
     
     udp_log(("teleport_new: new remote port is ip[%u]:%u\n", tp->addr, ntohs(tp->portno)));

     return tp;
  }

  int teleport_done(xrtp_teleport_t * tport){

     xfree(tport);
     return XRTP_OK;
  }

  char * teleport_name(xrtp_teleport_t * tport){

     struct in_addr addr;
     addr.s_addr = tport->addr;
     

     return inet_ntoa(addr);
  }

  uint32 teleport_portno(xrtp_teleport_t * tport){

     return ntohs(tport->portno);
  }
