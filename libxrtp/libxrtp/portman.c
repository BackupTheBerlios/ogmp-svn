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

#include <timedia/xmalloc.h>
#include <stdlib.h>

#include <stdio.h>
/*
*/
#define PORTMAN_LOG
#ifdef PORTMAN_LOG
  #define portman_log(fmtargs)  do{printf fmtargs;}while(0)
#else
  #define portman_log(fmtargs) 
#endif

struct portman_s{

   int maxio;

   xlist_t     *portlist; /* used to retrieve all */

   fd_set io_set;
};

portman_t * portman_new()
{
   portman_t * man = xmalloc(sizeof(struct portman_s));
   if(man)
   {
      man->maxio = 0;
        
      man->portlist = xlist_new();
		
	  FD_ZERO(&(man->io_set));
   }

   return man;
}

int portman_done_io(void *gen)
{
   /* No need to free the port */
   return OS_OK;
}

int portman_done(portman_t * man)
{
   //map_done(man->ports);
   xlist_done(man->portlist, portman_done_io);

   xfree(man);

   return XRTP_OK;
}

int portman_add_port(portman_t * man, xrtp_port_t * port)
{
   int io;
   xlist_user_t lu;
   xrtp_port_t *p;

   if(xlist_size(man->portlist) == FD_SETSIZE) return XRTP_EFULL;
        
   io = port_io(port);
   FD_SET(io, &(man->io_set));

   man->maxio = man->maxio < io ? io : man->maxio;
   xlist_addto_first(man->portlist, port);
   
   /* DEBUG Start */
   portman_log(("portman_add_port: ios to detect are:"));    
   p = xlist_first(man->portlist, &lu);
   while(p)
   {
      io = port_io(p);
      if(FD_ISSET(io, &(man->io_set)))
	  {
          portman_log((" [%d]", io));
      }
	  else
	  {
          portman_log((" (%d)", io));
      }

      p = xlist_next(man->portlist, &lu);
   }

   portman_log(("\n"));
   /* DEBUG End */

   return XRTP_OK;
}

int portman_remove_port(portman_t * man, xrtp_port_t * port)
{
   int io;
   xlist_user_t lu;   
   xrtp_port_t * p;

   if(xlist_size(man->portlist) == 0) 
	   return XRTP_OK;

   io = port_io(port);
   FD_CLR(io, &(man->io_set));

   xlist_remove_item(man->portlist, port);

   portman_log(("portman_remove_port: port[%d] removed\n", io));
   portman_log(("portman_remove_port: ports to detect are:"));
   p = xlist_first(man->portlist, &lu);
   if(p) 
		man->maxio = port_io(p);
   else
		man->maxio = 0;

   while(p)
   {
      io = port_io(p);
      if(FD_ISSET(io, &(man->io_set)))
          portman_log((" [%d]", io));

	  man->maxio = man->maxio < io ? io : man->maxio;

      p = xlist_next(man->portlist, &lu);
   }
   portman_log(("\nportman_remove_port: maxio=%d\n", man->maxio));

   return XRTP_OK;
}

int portman_clear_ports(portman_t * man)
{
   if(xlist_size(man->portlist) == 0) return XRTP_OK;

   FD_ZERO(&(man->io_set));
   xlist_reset(man->portlist, portman_done_io);

   return XRTP_OK;
}

int portman_maximum(portman_t * man)
{
    return FD_SETSIZE;
}

int portman_poll(portman_t * man)
{
   xrtp_port_t * port;
   xlist_user_t lu;
    
   struct timeval tv = {0,0}; /* No block */

   fd_set io_mask = man->io_set;
    
   int io;
   int c = 0;
    
   int n;
   
   n= select(man->maxio+1, &io_mask, NULL, NULL, &tv);
   if(!n) return n;
    
   /* retrieve port io which readable */
   port = xlist_first(man->portlist, &lu);
   while(port)
   {
      io = port_io(port);
      if(FD_ISSET(io, &io_mask))
	  {
         if(port_incoming(port) >= XRTP_OK)
			 c++;
      }

      port = xlist_next(man->portlist, &lu);
   }

   return c;
}
