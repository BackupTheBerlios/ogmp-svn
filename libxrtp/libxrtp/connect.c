/***************************************************************************
                          connection.c  -  Ports management
                             -------------------
    begin                : Thu Oct 23 2003
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

 #define MAX_SESSIONS_PER_SITE   64

 struct port_rec_s{

    int port_id; /* Which is socket no */

    session_port_t * port;
 };
 
 struct xrtp_connection_s{

    int num_port;
    
    struct port_rec_s[MAX_SESSIONS_PER_SITE * 2]; /* Each session has 2 port ids */
 };

 