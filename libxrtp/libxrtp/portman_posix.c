/***************************************************************************
                          portman_posix.c  -  port manager posix implementation
                             -------------------
    begin                : Wed Dec 3 2003
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

 #include <sys/select.h>    /* POSIX 1003.1 - 2001 compliant */
 
 #include "portman.h"
 #include <timedia/xmap.h>
 #include <stdlib.h>

 #include <stdio.h>
 
 #ifdef PORTMAN_LOG
   const int portman_log = 1;
 #else
   const int portman_log = 0;
 #endif
 
 #define portman_log(fmtargs)  do{if(portman_log) printf fmtargs;}while(0)

 struct portman_s{

    int max_port;
    int n_port;

    xrtp_map_t * ports;
    fd_set io_set;
 };

 portman_t * portman_new(){

    portman_t * man = malloc(sizeof(struct portman_s));
    if(man){

        FD_ZERO(&(man->io_set));
        
        man->n_port = 0;
        man->max_port = FD_SETSIZE;
        man->ports = map_new(0);   /* Map default size, which is 1024 in this implement */
    }

    return man;
 }

 int portman_done(portman_t * man){

    map_done(man->ports);

    free(man);

    return XRTP_OK;
 }

 int portman_add_port(portman_t * man, xrtp_port_t * port){

    if(man->n_port == FD_SETSIZE)
        return XRTP_EFULL;
        
    int io = port_io(port);

    FD_SET(io, &(man->io_set));
    map_add(man->ports, port, io);
    man->n_port++;
    
    /* DEBUG Start */
    portman_log(("portman_add_port: port[%d] added\n", io));
    portman_log(("portman_add_port: ports to detect are:"));
    
    int i;
    for(i=0; i<= map_max_key(man->ports); i++){
      
       if(FD_ISSET(i, &(man->io_set))){

          portman_log((" [%d]", i));
          
       }else{

          portman_log((" (%d)", i));
       }
    }
    portman_log(("\n"));
    /* DEBUG End */

    return XRTP_OK;
 }

 int portman_remove_port(portman_t * man, xrtp_port_t * port){

    if(man->n_port == 0)
        return XRTP_OK;

    int io = port_io(port);
    
    FD_CLR(io, &(man->io_set));
    map_remove(man->ports, io);
    man->n_port--;

    /* DEBUG Start */
    int i;
    for(i=0; i<= map_max_key(man->ports); i++){
       if(FD_ISSET(i, &(man->io_set))){

          portman_log((" [%d]", i));
       }
    }
    portman_log(("\n"));
    /* DEBUG End */

    return XRTP_OK;
 }

 int portman_clear_ports(portman_t * man){

    if(man->n_port == 0)
        return XRTP_OK;

    FD_ZERO(&(man->io_set));
    map_blank(man->ports);
    man->n_port = 0;

    return XRTP_OK;
 }

 int portman_maximum(portman_t * man){

    return man->max_port;
 }

 int portman_poll(portman_t * man){

    xrtp_port_t * port;
    
    struct timeval tv = {0,0}; /* No block */

    fd_set io_mask = man->io_set;
    
    int c = 0;
    
    int maxio = map_max_key(man->ports);
    int n = select(maxio+1, &io_mask, NULL, NULL, &tv);
    
    portman_log(("portman_poll: maxio = %d, %d incoming on sockets\n", maxio, n));
    
    if(!n) return n;

    int i;
    for(i=0; i<=maxio; i++){

       if(FD_ISSET(i, &io_mask)){

          port = (xrtp_port_t *)map_item(man->ports, i);

          portman_log(("portman_poll: socket[%d] incoming detected !\n", port_io(port)));

          if(port_incoming(port) >= XRTP_OK) c++;
          
       }
    }

    return c;
 }
