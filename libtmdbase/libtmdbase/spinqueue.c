/***************************************************************************
                          spinqueue.c  -  description
                             -------------------
    begin                : Sun Nov 30 2003
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

 #include "spinqueue.h"

 #include <stdlib.h>
 
 #ifdef QUEUE_LOG
   const int spin_queue_log = 1;
 #else
   const int spin_queue_log = 0;
 #endif
 #define queue_log(fmtargs)  do{if(spin_queue_log) printf fmtargs;}while(0)

 spin_queue_t * queue_new(int size){

    spin_queue_t * q = malloc(sizeof(struct spin_queue_s));
    if(q){

        q->nodes = malloc(sizeof(struct queue_node_s) * size);
        if(!q->nodes){

          free(q);
          return NULL;
        }

        q->size = size;
        q->occu = q->vacu = q->n_node = 0;
    }

    return q;
 }

 int queue_done(spin_queue_t * q){

    if(q->n_node != 0)
       return OS_EBUSY;
    
    free(q->nodes);
    free(q);

    return OS_OK;
 }

 int queue_wait(spin_queue_t * q, void * data){

    if(q->n_node == q->size) return OS_EFULL;

    q->nodes[q->vacu].data = data;
    q->vacu = (++q->vacu) % q->size;
    q->n_node++;

    return OS_OK;
 }

 void * queue_serve(spin_queue_t * q){

    void * d;
    if(q->n_node == 0) return NULL;

    d = q->nodes[q->occu].data;
    q->nodes[q->occu].data = NULL;
    q->occu = (++q->occu) % q->size;
    q->n_node--;

    return d;
 }

 void * queue_head(spin_queue_t * q){

    if(q->n_node == 0) return NULL;
    return q->nodes[q->occu].data;
 }
 
 int queue_length(spin_queue_t * q){

    return q->n_node;
 }

 int queue_is_full(spin_queue_t * q){

    return q->n_node == q->size;
 }
 
 int queue_is_empty(spin_queue_t * q){

    return q->n_node == 0;
 }
