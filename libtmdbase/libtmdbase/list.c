/*
 * Copyright (C) 2000-2002 the timedia project
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
 * $Id: utils.h,v 0.1 11/12/2002 15:47:39 heming$
 *
 */

#include "list.h"

#include <stdlib.h>
#include <string.h>

#include <stdio.h>

/*
#define XLIST_LOG
*/

#ifdef XLIST_LOG
 #define xlist_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define xlist_log(fmtargs)
#endif

int _xrtp_list_freenode(xrtp_list_node_t * node);

xlist_t * xlist_new(){
  
   xlist_t * list;

   list = (xlist_t *)malloc(sizeof(xlist_t));
   if(list != NULL){
     
      list->num = 0;
      list->head = list->end = NULL;
   }

   return list;
}

int xlist_done(xlist_t *list, int(*free_item)(void *)){

   xrtp_list_node_t *curr = NULL;

   if(list == NULL) return OS_OK;

   curr = list->head;
   
   while(curr != NULL){

      list->head = curr->next;
      list->num--;

      if(free_item) free_item(curr->data);

      _xrtp_list_freenode(curr);
      
      curr = list->head;
   }

   free(list);
   return OS_OK;
}

int xlist_reset(xlist_t *list, int(*free_item)(void *)){

   xrtp_list_node_t *curr = NULL;

   xrtp_list_node_t *next = NULL;
   
   if(list == NULL) return OS_OK;

   curr = list->head;
   
   while(curr != NULL){
     
      next = curr->next;
      list->head = next;
      list->num--;
      free_item(curr->data);
      _xrtp_list_freenode(curr);
      curr = next;
   }

   list->head = list->end = NULL;
   list->num = 0;
   
   return OS_OK;
}

xrtp_list_user_t * xlist_new_user(xlist_t * list){
   xrtp_list_user_t * user;

   /* Current arg list is not used
   if(list == NULL) return NULL;
   */

   user = (xrtp_list_user_t *)malloc(sizeof(xrtp_list_user_t));
   if(user != NULL){
      user->curr = user->prev = user->next = NULL;
   }

   return user;
}

int xlist_done_user(xlist_user_t * user){
   free(user);
	return OS_OK;
}

void * xlist_first(xlist_t * list, xlist_user_t * u){
  
   xrtp_list_node_t * n = NULL;

   if(list->num == 0) return NULL;
  
   n = list->head;

   u->curr = n;
   u->prev = NULL;

   if(n) u->next = n->next;

   return n->data;
}

void * xlist_next(xlist_t * list, xlist_user_t * u){
  
   xrtp_list_node_t * curr = u->curr;

   if(!curr || !curr->next) return NULL;
    
   u->prev = curr;
   curr = curr->next;
      
   u->curr = curr;
   u->next = curr->next;

   return curr->data;
}

void * xlist_current(xlist_t * list, xlist_user_t * u){

   if(u->curr)
      return u->curr->data;
   else
      return NULL;
}

int xlist_size(xlist_t * list){
   return list->num;
}

xrtp_list_node_t * _xrtp_list_newnode(){

   xrtp_list_node_t * node;

   node = (xrtp_list_node_t *)malloc(sizeof(xrtp_list_node_t));

   if(!node){

      xlist_log(("< _xrtp_list_newnode: Fail to allocate list node memery >\n"));
      return NULL;
   }
  
   memset(node, 0, sizeof(xrtp_list_node_t));
  
   return node;
}

int _xrtp_list_freenode(xrtp_list_node_t * node){

   free(node);
	return OS_OK;
}

int xlist_addto_first(xlist_t * list, void * item){
  
   xrtp_list_node_t * node;

   if(!list) return OS_EPARAM;

   node = _xrtp_list_newnode();

   if(!node) return OS_EMEM;

   node->next = list->head;
   node->data = item;
   list->head = node;
   
   list->num++;


   return OS_OK;
}

int xlist_addto_last(xlist_t * list, void * data){

   xrtp_list_node_t * node;

   if(!list) return OS_EPARAM;

   node = _xrtp_list_newnode();

   if(!node) return OS_EMEM;

   node->data = data;
  
   if(list->num == 0){

	   list->head = list->end = node;

   }else{
	   
	   list->end->next = node;
	   list->end = node;
   }
  
   list->num++;

   xlist_log(("xlist_addto_last: %d items\n", list->num));

	return OS_OK;
}

int xlist_addonce_ascent(xlist_t * list, void * ndata, int cmp(void *, void *)){

   int ret;
   xrtp_list_node_t * node = NULL;
   xrtp_list_node_t * curr = NULL;
   xrtp_list_node_t * prev = NULL;

   if(list->num == 0){

      node = _xrtp_list_newnode();
      
      if(!node){

         return OS_EMEM;
      }
      
      node->data = ndata;
      list->head = node;
      list->num++;
      
      return OS_OK;
   }
   
   xlist_log(("xrtp_list_add_ascent_once: adding item...\n"));
   
   curr = list->head;
   prev = NULL;
   
   while(curr){

      ret = cmp(curr->data, ndata);
      
      if( ret == 0 ){ /* match condition, no addition needed */

          xlist_log(("< xrtp_list_add_ascent_once: Item exist, no addition >\n"));
          return OS_OK;

      }else if( ret > 0 ){
      
         node = _xrtp_list_newnode();
         if(!node){
            
            return OS_EMEM;
         }
              
         node->data = ndata;

         if(prev){

            node->next = prev->next;
            prev->next = node;

         }else{

            if(list->head)   node->next = list->head;
             
            list->head = node;
         }

         list->num++;
         xlist_log(("xrtp_list_add_ascent_once: Item added, total %d items\n", list->num));
      }
      
      prev = curr;
      curr = curr->next;
   }

   return OS_OK;
}

void * xlist_remove_first(xlist_t * list){
  
   xrtp_list_node_t * node;
   void * data;

   if(!list || list->num == 0) return NULL;

   node = list->head;
   list->head = node->next;
   list->num--;

   if(list->num==0)  xlist_log(("xrtp_list_remove_first: list empty now\n"));

   data = node->data;
   _xrtp_list_freenode(node);

   return data;
}

int xlist_remove_item (xlist_t *list, void *data) {

   xrtp_list_node_t * curr = NULL;
   xrtp_list_node_t * prev = NULL;

   if(!list || list->num == 0) return OS_INVALID;

   curr = list->head;
   prev = NULL;
   
   while(curr){
     
      if( curr->data == data ){ /* match */

         if(prev){
           
            prev->next = curr->next;
            
         }else{
           
            list->head = curr->next;
         }
         
         list->num--;
         
         _xrtp_list_freenode(curr);
         
         xlist_log(("xrtp_list_remove_item: %d item in list\n", list->num));

         return OS_OK;
      }

      prev = curr;
      curr = curr->next;
   }

   return OS_INVALID;
}

void * xlist_remove(xrtp_list_t * list, void *data, int (*match)(void*, void*)){

   xrtp_list_node_t * curr = NULL;
   xrtp_list_node_t * prev = NULL;
   
   if(!list || list->num == 0) return NULL;

   curr = list->head;
   prev = NULL;
   
   while(curr){
     
      if( match(curr->data, data) == 0 ){/* match */

         data = curr->data;
         
         if(prev){
           
            prev->next = curr->next;
            
         }else{
           
            list->head = curr->next;
         }
         
         list->num--;
         _xrtp_list_freenode(curr);
         
         return data;
      }
     
      prev = curr;
      curr = curr->next;
   }

   return NULL;
}

int xlist_delete_if(xlist_t * list, void * cdata, int(*condition)(void*, void*), int(*free_item)(void*)){

   void * data;
   
   xrtp_list_node_t * curr = NULL;
   xrtp_list_node_t * prev = NULL;
   
   if(!list || list->num == 0)
      return OS_OK;

   curr = list->head;
   prev = NULL;
   
   while(curr){
      
      if( condition(curr->data, cdata) == 0 ){ /* match condition */

         data = curr->data;
         
         if(prev){
            
            prev->next = curr->next;
            
         }else{
            
            list->head = curr->next;
         }
         
         list->num--;
         _xrtp_list_freenode(curr);
         
         free_item(data);
      }
      
      prev = curr;
      curr = curr->next;
   }

   return OS_OK;
}

void * xlist_find(xlist_t * list, void * data, int (*match)(void*, void*), xlist_user_t * u){

   if(!list || list->num == 0)
      return NULL;

   u->prev = NULL;
   u->next = NULL;
   u->curr = list->head;
  
   while(u->curr){

      u->next = u->curr->next;
     
      if( match(u->curr->data, data) == 0 )/* match */
         return u->curr->data;

      u->prev = u->curr;
      u->curr = u->next;
   }

   return NULL;
}

int xlist_visit(xlist_t * list, int(*visit)(void*, void*), void *visitor){
                           
   int ret = OS_OK;
   xrtp_list_node_t * curr = NULL;
   xrtp_list_node_t * next = NULL;

   
   if(!list || list->num == 0)
      return ret;

   curr = list->head;
   
   while(curr){

      next = curr->next;

      ret = visit(curr->data, visitor);  /* visit node */
      
      if(ret < OS_OK) return ret; 

      curr = next;
   }

   return ret;
}

/*old api, not use anymore*/
xrtp_list_t * xrtp_list_new(){return xlist_new();}

int xrtp_list_free(xrtp_list_t *list, int(*free_item)(void *)){return xlist_done(list, free_item);}

int xrtp_list_reset(xrtp_list_t *list, int(*free_item)(void *)){return xlist_reset(list, free_item);}

xrtp_list_user_t * xrtp_list_newuser(xrtp_list_t * list){return xlist_new_user(list);}

int xrtp_list_freeuser(xrtp_list_user_t * user){return xlist_done_user(user);}

void * xrtp_list_first(xrtp_list_t * list, xrtp_list_user_t * u){return xlist_first(list, u);}

void * xrtp_list_next(xrtp_list_t * list, xrtp_list_user_t * u){return xlist_next(list,u);}

void * xrtp_list_current(xrtp_list_t * list, xrtp_list_user_t * u){return xlist_current(list,u);}

int xrtp_list_size(xrtp_list_t * list){return xlist_size(list);}

int xrtp_list_add_first(xrtp_list_t * list, void * item){return xlist_addto_first(list,item);}

int xrtp_list_add_last(xrtp_list_t * list, void * item){return xlist_addto_last(list,item);}

int xrtp_list_add_ascent_once(xrtp_list_t * list, void * item, int cmp(void *, void *)){return xlist_addonce_ascent(list, item, cmp);}

void * xrtp_list_remove_first(xrtp_list_t * list){return xlist_remove_first(list);}

int xrtp_list_remove_item (xrtp_list_t * list, void *data) {return xlist_remove_item(list,data);}

void * xrtp_list_remove(xrtp_list_t * list, void *item, int (*match)(void*, void*)){return xlist_remove(list,item,match);}

int xrtp_list_delete_if(xrtp_list_t * list, void * cdata, int(*condition)(void*, void*), int(*free_item)(void*))
{return xlist_delete_if(list, cdata, condition, free_item);}

void * xrtp_list_find(xrtp_list_t * list, void * data, int (*match)(void*, void*), xrtp_list_user_t *u)
{return xlist_find(list,data,match,u);}

int xrtp_list_visit(xrtp_list_t * list, int(*visit)(void*, void*), void *visitor)
{return xlist_visit(list,visit,visitor);}


