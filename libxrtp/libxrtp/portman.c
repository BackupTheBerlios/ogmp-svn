/***************************************************************************
                          portman.c  -  port set moniter
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

 #include <timedia/socket.h>    /* POSIX 1003.1 - 2001 compliant */
 
 #include "portman.h"
 #include <timedia/xmap.h>
 #include <stdlib.h>

 #include <stdio.h>

#define PORTMAN_LOG
 
#ifdef PORTMAN_LOG
  #define portman_log(fmtargs)  do{printf fmtargs;}while(0)
#else
  #define portman_log(fmtargs) 
#endif

struct portman_s{

    int max_port;
    int n_port;

    xrtp_map_t * ports;
	xlist_t * iolist; /* used to retrieve all */

    fd_set io_set;
};

portman_t * portman_new(){

    portman_t * man = malloc(sizeof(struct portman_s));
    if(man){

        FD_ZERO(&(man->io_set));
        
        man->n_port = 0;
        man->max_port = FD_SETSIZE;
        man->ports = map_new(0);   /* Map default size, which is 1024 in this implement */
		man->iolist = xlist_new();
    }

    return man;
}

int portman_done(portman_t * man){

    map_done(man->ports);
	xlist_done(man->iolist);

    free(man);

    return XRTP_OK;
}

int portman_add_port(portman_t * man, xrtp_port_t * port){

    int io;
    int i;

	if(man->n_port == FD_SETSIZE)
        return XRTP_EFULL;
        
    io = port_io(port);

    FD_SET(io, &(man->io_set));

    map_add(man->ports, port, io);
	xlist_add(man->iolist, io);

    man->n_port++;
    
    /* DEBUG Start */
    portman_log(("portman_add_port: port[%d] added\n", io));
    portman_log(("portman_add_port: ports to detect are:"));
    
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

    uint io;
    int i;

    if(man->n_port == 0)
        return XRTP_OK;

    io = port_io(port);
    
    FD_CLR(io, &(man->io_set));
    map_remove(man->ports, io);
	xlist_remove_item(man->iolist, io);
    man->n_port--;

    /* DEBUG Start */
    for(i=0; i<= map_max_key(man->ports); i++){
       if(FD_ISSET(i, &(man->io_set))){

          portman_log((" [%d]", i));
       }
    }
    portman_log(("\n"));
    /* DEBUG End */

    return XRTP_OK;
}

int portman_free_ioitem(void *gen){
	
	return OS_OK;
}

int portman_clear_ports(portman_t * man){

    if(man->n_port == 0)
        return XRTP_OK;

    FD_ZERO(&(man->io_set));
    map_blank(man->ports);
	xlist_reset(man->iolist, port_free_ioitem);

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
    int i;
    
    int maxio = map_max_key(man->ports);
    int n = select(maxio+1, &io_mask, NULL, NULL, &tv);
    if(!n) return n;
    
    portman_log(("portman_poll: maxio = %d, %d incoming on sockets\n", maxio, n));

    /* retrieve port io which readable */

	for(i=0; i<=maxio; i++){

       if(FD_ISSET(i, &io_mask)){

          port = (xrtp_port_t *)map_item(man->ports, i);

          portman_log(("portman_poll: socket[%d] incoming detected !\n", port_io(port)));

          if(port_incoming(port) >= XRTP_OK) c++;
       }
    }

    return c;
}
