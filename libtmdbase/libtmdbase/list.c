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

#define XLIST_LOG

#ifdef XLIST_LOG
   const int xlist_log = 1;
#else
   const int xlist_log = 0;
#endif
#define xlist_log(fmtargs)  do{if(xlist_log) printf fmtargs;}while(0)

int _xrtp_list_freenode(xrtp_list_node_t * node);

xrtp_list_t * xrtp_list_new(){
  
   xrtp_list_t * list;

   list = (xrtp_list_t *)malloc(sizeof(xrtp_list_t));
   if(list != NULL){
     
      list->num = 0;
      list->head = list->end = NULL;
   }

   return list;
}

int xrtp_list_free(xrtp_list_t *list, int(*free_data)(void *)){

   if(list == NULL)
      return OS_OK;

   xrtp_list_node_t *curr = list->head;
   
   while(curr != NULL){

      list->head = curr->next;
      list->num--;

      free_data(curr->data);
      _xrtp_list_freenode(curr);
      
      curr = list->head;
   }

   free(list);
   return OS_OK;
}

int xrtp_list_reset(xrtp_list_t *list, int(*free_data)(void *)){

	 xrtp_list_node_t *curr = NULL;
	 xrtp_list_node_t *next = NULL;
   
   if(list == NULL)
	   return OS_OK;

	 curr = list->head;
   
	 while(curr != NULL){
     
     next = curr->next;
	   list->head = next;
		 list->num--;
     free_data(curr->data);
		 _xrtp_list_freenode(curr);
		 curr = next;
	 }

	 list->head = list->end = NULL;
	 list->num = 0;
   
	 return OS_OK;
}

xrtp_list_user_t * xrtp_list_newuser(xrtp_list_t * list){
   xrtp_list_user_t * user;

   /* Current arg list is not used
   if(list == NULL)
	   return NULL;
   */

	user = (xrtp_list_user_t *)malloc(sizeof(xrtp_list_user_t));
   if(user != NULL){
		user->curr = user->prev = user->next = NULL;
	}

	return user;
}

int xrtp_list_freeuser(xrtp_list_user_t * user){
   free(user);
	return OS_OK;
}

void * xrtp_list_first(xrtp_list_t * list, xrtp_list_user_t * u){
  
   if(list->num == 0) return NULL;
  
   xrtp_list_node_t * n = list->head;

   u->curr = n;
   u->prev = NULL;

   if(n) u->next = n->next;

   return n->data;
}

void * xrtp_list_next(xrtp_list_t * list, xrtp_list_user_t * u){
  
   xrtp_list_node_t * curr = u->curr;

   if(!curr || !curr->next) return NULL;
    
   u->prev = curr;
   curr = curr->next;
      
   u->curr = curr;
   u->next = curr->next;

   return curr->data;
}

void * xrtp_list_current(xrtp_list_t * list, xrtp_list_user_t * u){

   if(u->curr)
      return u->curr->data;
   else
      return NULL;
}

int xrtp_list_size(xrtp_list_t * list){
   return list->num;
}

xrtp_list_node_t * _xrtp_list_newnode(){

   xrtp_list_node_t * node;

   node = (xrtp_list_node_t *)malloc(sizeof(xrtp_list_node_t));

   if(!node){

      xlist_log(("< _xrtp_list_newnode: Fail to allocate list node memery >\n"));
      return NULL;
   }
  
   bzero(node, sizeof(xrtp_list_node_t));
  
   return node;
}

int _xrtp_list_freenode(xrtp_list_node_t * node){

   free(node);
	return OS_OK;
}

int xrtp_list_add_first(xrtp_list_t * list, void * data){
  
   xrtp_list_node_t * node;

   if(!list) return OS_EPARAM;

   node = _xrtp_list_newnode();

   if(!node) return OS_EMEM;

   node->next = list->head;
   node->data = data;
   list->head = node;
   
   list->num++;

   return OS_OK;
}

int xrtp_list_add_last(xrtp_list_t * list, void * data){

   xrtp_list_node_t * node;

   if(!list) return OS_EPARAM;

   node = _xrtp_list_newnode();

   if(!node) return OS_EMEM;

   node->next = NULL;
   node->data = data;
  
   if(list->end){
    
      list->end->next = node;

   }else{

      list->head = node;
   }
  
   list->end = node;
  
   list->num++;

	return OS_OK;
}

int xrtp_list_add_ascent_once(xrtp_list_t * list, void * ndata, int cmp(void *, void *)){

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

void * xrtp_list_remove_first(xrtp_list_t * list){
  
   xrtp_list_node_t * node;
   void * data;

   if(!list || list->num == 0) return NULL;

   node = list->head;
   list->head = node->next;
   list->num--;

   if(!list->num)  xlist_log(("< xrtp_list_remove_first: list empty now >\n"));

   data = node->data;
   _xrtp_list_freenode(node);

   return data;
}

int xrtp_list_remove_item (xrtp_list_t * list, void *data) {

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

void * xrtp_list_remove(xrtp_list_t * list, void *data, int (*match)(void*, void*)){

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

int xrtp_list_delete_if(xrtp_list_t * list, void * cdata,
                          int(*condition)(void*, void*),
                          int(*freeman)(void*)){

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
         
         freeman(data);
      }
      
      prev = curr;
      curr = curr->next;
   }

   return OS_OK;
}

void * xrtp_list_find(xrtp_list_t * list, void * data, int (*match)(void*, void*), xrtp_list_user_t * u){

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

void * list_find(xrtp_list_t * list, void * data, int (*match)(void*, void*)){

   xrtp_list_node_t *cur;

   if(!list || list->num == 0)
      return NULL;

   cur = list->head;

   while(cur){

      if( match(cur->data, data) == 0 )/* match */
         return cur->data;

      cur = cur->next;
   }

   return NULL;
}

int xrtp_list_visit(xrtp_list_t * list, int(*visitor)(void*, void*), void * vdata){
                           
   int ret = OS_OK;
   xrtp_list_node_t * curr = NULL;
   xrtp_list_node_t * next = NULL;

   
   if(!list || list->num == 0)
      return ret;

   curr = list->head;
   
   while(curr){

      next = curr->next;

      ret = visitor(curr->data, vdata);  /* visit node */
      
      if(ret < OS_OK) return ret; 

      curr = next;
   }

   return ret;
}


