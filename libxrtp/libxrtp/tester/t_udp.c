/***************************************************************************
                          t_udp.c  -  udp tester
                             -------------------
    begin                : Tue Dec 2 2003
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

 #include "xrtp.h"
 #include <string.h>
 #include <stdio.h>
 #include <unistd.h>

 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>

  int test_sendto(int s, char * msg, int len){

    struct sockaddr_in sin;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(8000); // htons for network byte order
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    int n = sendto(s, msg, len, 0, (struct sockaddr *)&sin, sizeof(sin));

    printf("test_socket: %d bytes are sent to ip[%u]:%u by socket[%d]\n", n, sin.sin_addr.s_addr, ntohs(sin.sin_port), s);

    return n;
  }

  void test_socket(){

    int s = socket(PF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in sin;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(9000); // htons for network byte order
    if(inet_aton("127.0.0.1", &(sin.sin_addr)) == 0){ /* string to int addr */

       printf("test_socket: Illegal ip address\n");

       close(s);
       return;
    }

    printf("test_socket: socket[%d] is bound to local ip[%u]:%u\n", s, sin.sin_addr.s_addr, ntohs(sin.sin_port));
    
    if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == -1){

        printf("test_socket: Fail to bind socket\n");
        close(s);

        return;
     }

    char *msg = "Hello, World";

    test_sendto(s, msg, strlen(msg)+1);

    close(s);
 }

 int main(){

    //test_socket();

    portman_t * man1 = portman_new();
    portman_t * man2 = portman_new();
    
    if(!man1){

       printf("Portman not available\n");
       return -1;
    }
    
    xrtp_port_t * port_send = port_new("127.0.0.1", 8000, RTP_PORT);
    if(!port_send){

       return -1;
    }
    portman_add_port(man1, port_send);
    
    xrtp_port_t * port_recv = port_new("127.0.0.1", 9000, RTP_PORT);
    if(!port_recv){

       return -1;
    }
    portman_add_port(man2, port_recv);

    xrtp_port_t * port1 = port_new("127.0.0.1", 7000, RTP_PORT);
    xrtp_port_t * port2 = port_new("127.0.0.1", 6000, RTP_PORT);
    portman_add_port(man2, port1);
    portman_add_port(man1, port2);
    
    xrtp_teleport_t * teleport = teleport_new("127.0.0.1", 9000);
    session_connect_t * conn = connect_new(port_send, teleport);
    
    xrtp_teleport_t * teleport2 = teleport_new("127.0.0.1", 8000);
    session_connect_t * conn_back = connect_new(port_recv, teleport2);
    
    if(!conn){
      
       printf("Fail to create connect\n");
       port_done(port_send);
       port_done(port_recv);
       return -1;
    }
    
    if(connect_from_teleport(conn, teleport)){

       printf("The connect is from the teleport\n");
    }
    
    char *msg1 = "Hello, World";
    char *msg2 = "Hello, form World";

    int i;
    for(i=0; i<4; i++){
      
      printf("%d outgoing\n", i);
      connect_send(conn, msg1, strlen(msg1)+1);
      connect_send(conn_back, msg2, strlen(msg2)+1);

      /* NOTE: Will cause segment fault as no session given yet */
      int n_in = portman_poll(man1);
      printf("%d incoming waiting on send port\n", n_in);
      n_in = portman_poll(man2);
      printf("%d incoming waiting on recv port\n", n_in);
    }

    connect_done(conn);
    connect_done(conn_back);
    teleport_done(teleport);
    teleport_done(teleport2);
    
    portman_remove_port(man2, port_recv);
    portman_remove_port(man1, port_send);
    
    port_done(port_send);
    port_done(port_recv);

    portman_done(man1);
    portman_done(man2);

    return 0;
 }

