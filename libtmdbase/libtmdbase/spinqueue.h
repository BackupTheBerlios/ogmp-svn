/***************************************************************************
                          spinqueue.h  -  The Spin Queue
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

 #include "os.h"

 typedef struct spin_queue_s spin_queue_t;

 struct queue_node_s{

    void * data;
 };
 
 struct spin_queue_s{

    struct queue_node_s *nodes;
    int size, n_node, occu, vacu;
 };

 spin_queue_t * queue_new(int size);

extern DECLSPEC  int queue_done(spin_queue_t * queue);

 /* Add to the queue end */
extern DECLSPEC  int queue_wait(spin_queue_t * queue, void * data);

 /* Remove first from the queue */
extern DECLSPEC  void * queue_serve(spin_queue_t * queue);
 
 /* Only peek the first but not remove from the queue */
extern DECLSPEC  void * queue_head(spin_queue_t * queue);

extern DECLSPEC  int queue_length(spin_queue_t * queue);
extern DECLSPEC  int queue_is_full(spin_queue_t * queue);
extern DECLSPEC  int queue_is_empty(spin_queue_t * queue);
