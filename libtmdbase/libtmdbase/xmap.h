/***************************************************************************
                          xmap.h  -  map interface
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

 #ifndef OS_H
 #include "os.h"
 #endif

 typedef struct xrtp_map_s xrtp_map_t;

 xrtp_map_t * map_new(int size);

 int map_done(xrtp_map_t * map);

 int map_add(xrtp_map_t * map, void * item, int key);

 void * map_remove(xrtp_map_t * map, int key);

 int map_blank(xrtp_map_t * map);

 void * map_item(xrtp_map_t * map, int key);

 int map_max_key(xrtp_map_t * map);
