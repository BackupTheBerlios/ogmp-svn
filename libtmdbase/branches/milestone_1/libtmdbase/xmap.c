/***************************************************************************
                          xmap.c  -  map container
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

 #include "xmap.h"
 
 #include <string.h>
 #include "xmalloc.h"

 #define MAP_MAX_SIZE 1024 /* equal to select.FD_SETSIZE */

 #include <stdio.h>
 #ifdef XMAP_LOG
   const int xmap_log = 1;
 #else
   const int xmap_log = 0;
 #endif
 #define map_log(fmtargs)  do{if(xmap_log) printf fmtargs;}while(0)

 struct xrtp_map_s{

    int max_key;
    int max_size;
    int n_item;
    void *items[];
 };

 xrtp_map_t * map_new(int size){

    int sz = (size > 0) ? size : MAP_MAX_SIZE;
    
    xrtp_map_t * map = xmalloc(sizeof(struct xrtp_map_s) + sizeof(void *) * sz);
    if(map){

       map->max_key = 0;
       map->max_size = sz;
       map->n_item = 0;
       
       memset(map->items, 0, sz);
    }

    return map;
 }

 int map_done(xrtp_map_t * map){

    xfree(map);

    return OS_OK;
 }

 int map_add(xrtp_map_t * map, void * item, int key){

    if(key > map->max_size || key < 0) return OS_ERANGE;

    map_log(("map_add: key = %d\n", key));
    if(!map->items[key]) map->n_item++;
    map_log(("map_add: n_item = %d\n", map->n_item));
    map->items[key] = item;

    map->max_key = map->max_key < key ? key : map->max_key;
    map_log(("map_add: max_key = %d\n", map->max_key));

    return OS_OK;
 }

 void * map_remove(xrtp_map_t * map, int key){

    void * item;
    
    if(key > map->max_size || key < 0) return NULL;
    
    item = map->items[key];
    if(item != NULL) map->n_item--;

    map->items[key] = NULL;
    
    if(map->n_item == 0) map->max_key = 0;

    if(key == map->max_key && map->n_item > 0){

       --map->max_key;
       while(map->max_key > 0 && !map->items[map->max_key]){
         
          map->max_key--;
       }
    }

    map_log(("map_remove: key = %d\n", key));
    map_log(("map_remove: n_item = %d\n", map->n_item));
    map_log(("map_remove: max_key = %d\n", map->max_key));
    
    return item;
 }

 int map_blank(xrtp_map_t * map){

    map->max_key = 0;
    map->n_item = 0;
    memset(map->items, 0, map->max_size);

    return OS_OK;
 }

 void * map_item(xrtp_map_t * map, int key){

    if(key > map->max_size || key < 0) return NULL;

    return map->items[key];
 }

 int map_max_key(xrtp_map_t * map){

    return map->max_key;
 }
