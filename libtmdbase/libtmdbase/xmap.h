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

 #include "os.h"

 typedef struct xrtp_map_s xrtp_map_t;

extern DECLSPEC  xrtp_map_t * map_new(int size);

extern DECLSPEC  int map_done(xrtp_map_t * map);

extern DECLSPEC  int map_add(xrtp_map_t * map, void * item, int key);

extern DECLSPEC  void * map_remove(xrtp_map_t * map, int key);

extern DECLSPEC  int map_blank(xrtp_map_t * map);

extern DECLSPEC  void * map_item(xrtp_map_t * map, int key);

extern DECLSPEC  int map_max_key(xrtp_map_t * map);
