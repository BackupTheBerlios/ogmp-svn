/***************************************************************************
                          fifo.h  -  media packet buffer
                             -------------------
    begin                : June 15 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia-project.org digitalme@sourceforge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include "fifo.h"
#include <string.h>

#define LOG_FIFO
#define DEBUG_FIFO

#ifdef LOG_FIFO
 #define fifo_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define fifo_log(fmtargs)
#endif

#ifdef DEBUG_FIFO
 #define fifo_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define fifo_debug(fmtargs)
#endif

media_frame_t* fifo_new_frame (media_pipe_t *pipe, int bytes, char *init_data) {

   /* better recycle to garrantee */
   fifo_frame_t *ff = (fifo_frame_t*)malloc(sizeof(struct fifo_frame_s));
   if(!ff){

      fifo_debug(("fifo_new_frame: No memory for frame\n"));
      return NULL;
   }   
   memset(ff, 0, sizeof(*ff));

   ff->media_unit = (char*)malloc(bytes);
   if(ff->media_unit == NULL){
		
	   free(ff);
	   fifo_debug(("fifo_new_frame: No memory for data"));
	   
	   return NULL;
   }

   ff->unit_bytes = bytes;

   /* initialize data */
   if(init_data != NULL){

		//fifo_log(("fifo_new_frame: initialize %d bytes data\n", bytes));
		memcpy(ff->media_unit, init_data, bytes);
   }

   return (media_frame_t*)ff;
}

int fifo_recycle_frame (media_pipe_t *pipe, media_frame_t *f) {

   fifo_frame_t * ff = (fifo_frame_t*)f;

   if(ff->media_unit) free(ff->media_unit);

   free(ff);

   return MP_OK;
}

int fifo_put_frame (media_pipe_t *pipe, media_frame_t *frame, int flag) {

   fifo_frame_t *ff = (fifo_frame_t*)frame;
   fifo_t *fifo = (fifo_t*)pipe;

   ff->flag |= flag;

   xthr_lock(fifo->queue_lock);
   xlist_addto_last(fifo->queue, ff);
   fifo->no_more = 0;
   xthr_unlock(fifo->queue_lock);
   
   xthr_cond_signal(fifo->in_queue);

   return MP_OK;
}

/*
 * Return 0: stream continues
 * return 1: stream ends
 */
int fifo_pick_content(media_pipe_t *mp, media_info_t *mi, char *out, int out_len) {

	fifo_log(("fifo_pick_frame_content: Use fifo_pick_frame instead\n"));

	return MP_EIMPL;
}

/*
 * return 0: No more frame available to pick
 * return n: The data size of the frame retrieved
 * 
 * param no in use here, api with generic pipe implement.
 */
media_frame_t* fifo_pick_frame (media_pipe_t *pipe) {

   fifo_t *fifo = (fifo_t*)pipe;
   fifo_frame_t *ff = NULL;

   if (fifo->no_more) return NULL;

   while(1){
	   
	   xthr_lock(fifo->queue_lock);

	   if (xlist_size(fifo->queue) == 0) 
	   {
		   //fifo_log(("fifo_pick_frame: no packet in queue, wait!\n"));
		   xthr_cond_wait(fifo->in_queue, fifo->queue_lock);
		   //fifo_log(("fifo_pick_frame: wakeup! %d packets in queue\n", xlist_size(fifo->queue)));
	   }

	   /* sometime it's still empty, weired? */
	   if (xlist_size(fifo->queue) == 0)
	   {
		   xthr_unlock(fifo->queue_lock);
		   continue;
	   }
	   
	   ff = (fifo_frame_t*)xlist_remove_first(fifo->queue);
	   if (ff->flag & MP_FIFO_END_ALL) fifo->no_more = 1;
	   
	   xthr_unlock(fifo->queue_lock);
	   break;
   }

   return (media_frame_t*)ff;
}

int fifo_free_frame (void *gen) {

   fifo_frame_t * ff = (fifo_frame_t*)gen;

   if(ff->media_unit) free(ff->media_unit);

   free(ff);

   return MP_OK;
}

int fifo_pipe_done (media_pipe_t *pipe){

   fifo_t *fifo = (fifo_t*)pipe;

   xthr_lock(fifo->queue_lock);
   xlist_done(fifo->queue, fifo_free_frame);
   xthr_unlock(fifo->queue_lock);

   xthr_done_lock(fifo->queue_lock);
   xthr_done_cond(fifo->in_queue);

   free(fifo);

   return MP_OK;
}

media_pipe_t * fifo_new() {

   media_pipe_t *mp;
   fifo_t *fifo = malloc(sizeof(struct fifo_s));
   if(!fifo){

      fifo_debug(("fifo_new: No memory to allocate\n"));
      return NULL;
   }
   memset(fifo, 0, sizeof(*fifo));

   mp = (media_pipe_t *)fifo;

   mp->new_frame = fifo_new_frame;
   mp->recycle_frame = fifo_recycle_frame;
   mp->put_frame = fifo_put_frame;
   mp->pick_frame = fifo_pick_frame;
   mp->pick_content = fifo_pick_content;
   mp->done = fifo_pipe_done;

   return mp;
}
