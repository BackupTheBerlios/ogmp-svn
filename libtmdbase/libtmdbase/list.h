/*
 * Copyright (C) 2000-2002 the timedia project team
 *
 * This file is part of libxrtp, a modulized rtp library.
 *
 * xrtp is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xrtp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: list.h,v 0.1 11/12/2002 15:47:39 heming$
 *
 */
 
#ifndef LIST_H

#define LIST_H

#include "os.h"

#define XLIST_KEEP_ITEM 0
#define XLIST_DELETE_ITEM 1
#define XLIST_INSERT_AFTER_ITEM 2
#define XLIST_INSERT_BEFORE_ITEM 3

typedef struct xrtp_list_node_s xrtp_list_node_t;

struct xrtp_list_node_s{
   xrtp_list_node_t *next;
	 void *data;
};

typedef struct xrtp_list_s xlist_t;

typedef struct xrtp_list_s{
  
   xrtp_list_node_t *head;
   xrtp_list_node_t *end;
   int num;
   
}xrtp_list_t;

typedef struct xrtp_list_user_s xlist_user_t;

typedef struct xrtp_list_user_s{
  
   xrtp_list_node_t *curr;
   xrtp_list_node_t *prev;
   xrtp_list_node_t *next;
   
}xrtp_list_user_t;

extern DECLSPEC xlist_t * xlist_new();
extern DECLSPEC xrtp_list_t * xrtp_list_new();

extern DECLSPEC int xlist_done(xlist_t *list, int(*free_item)(void *));
extern DECLSPEC int xrtp_list_free(xrtp_list_t *list, int(*free_data)(void *));

extern DECLSPEC int xlist_reset(xlist_t *list, int(*free_item)(void *));
extern DECLSPEC int xrtp_list_reset(xrtp_list_t *list, int(*free_data)(void *));

extern DECLSPEC xlist_user_t * xlist_new_user(xlist_t *list);
extern DECLSPEC xrtp_list_user_t * xrtp_list_newuser(xrtp_list_t *list);

extern DECLSPEC int xlist_done_user(xrtp_list_user_t *user);
extern DECLSPEC int xrtp_list_freeuser(xrtp_list_user_t *user);

extern DECLSPEC void * xlist_first(xlist_t *list, xlist_user_t *u);
extern DECLSPEC void * xrtp_list_first(xrtp_list_t *list, xrtp_list_user_t *u);

extern DECLSPEC void * xlist_next(xlist_t * list, xlist_user_t * u);
extern DECLSPEC void * xrtp_list_next(xrtp_list_t * list, xrtp_list_user_t * u);

extern DECLSPEC void * xlist_current(xlist_t * list, xlist_user_t * u);
extern DECLSPEC void * xrtp_list_current(xrtp_list_t * list, xrtp_list_user_t * u);

extern DECLSPEC int xlist_size(xlist_t * list);
extern DECLSPEC int xrtp_list_size(xrtp_list_t * list);

extern DECLSPEC int xlist_addto_first(xlist_t * list, void * data);
extern DECLSPEC int xrtp_list_add_first(xrtp_list_t * list, void * data);

extern DECLSPEC int xlist_addto_last(xlist_t * list, void * data);
extern DECLSPEC int xrtp_list_add_last(xrtp_list_t * list, void * data);

/* If item with same value, no addition occurred */
extern DECLSPEC int xlist_addonce_ascent(xlist_t * list, void * data, int cmp(void *, void *));
extern DECLSPEC int xrtp_list_add_ascent_once(xrtp_list_t * list, void * data, int cmp(void *, void *));

extern DECLSPEC void * xlist_remove_first(xlist_t * list);
extern DECLSPEC void * xrtp_list_remove_first(xrtp_list_t * list);

extern DECLSPEC int xlist_remove_item(xlist_t * list, void *item);
extern DECLSPEC int xrtp_list_remove_item(xrtp_list_t * list, void *item);

extern DECLSPEC void * xlist_remove_if(xlist_t * list, void * data, int(*match)(void*, void*));
extern DECLSPEC void * xrtp_list_remove(xrtp_list_t * list, void * data, int(*match)(void*, void*));

extern DECLSPEC int xlist_delete_if(xlist_t * list, void * cdata, int(*condition)(void*, void*), int(*freer)(void*));
extern DECLSPEC int xrtp_list_delete_if(xrtp_list_t * list, void * cdata, int(*condition)(void*, void*), int(*freeman)(void*));

extern DECLSPEC void * xlist_find(xrtp_list_t * list, void * data, int (*match)(void*, void*));
extern DECLSPEC void * xrtp_list_find(xrtp_list_t * list, void * data, int(*match)(void*, void*), xrtp_list_user_t * u);

/**
 * Traverse the list by a function, if function return error, abort.
 */
extern DECLSPEC int xrtp_list_visit(xrtp_list_t * list, int(*visitor)(void*, void*),
                    void * v);
#endif
