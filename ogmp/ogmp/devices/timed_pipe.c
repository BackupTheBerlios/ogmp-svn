/***************************************************************************
                          timed_outpipe.c  -  Timed Pipe
                             -------------------
    begin                : Wed Feb 4 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org, heming@bigfoot.com
 ***************************************************************************/
 
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "dev_portaudio.h"

#include <timedia/ui.h>
#include <timedia/xmalloc.h>
#include <string.h>
/*
#define PIPE_LOG
*/
#define PIPE_DEBUG
#ifdef PIPE_LOG
 #define pout_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define pout_log(fmtargs)  
#endif

#ifdef PIPE_DEBUG
 #define pout_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define pout_debug(fmtargs) 
#endif

media_frame_t * pout_new_frame(media_pipe_t * mo, int bytes, char *data)
{
   media_frame_t * found = NULL;
   media_frame_t * prev = NULL;
   
   pa_pipe_t * out = (pa_pipe_t *)mo;
   media_frame_t * f = out->freed_frame_head;

   char *raw = NULL;
   
   while(f)
   {
      if (f->bytes >= bytes)
      {
         found = f;
         
         if(prev)
            prev->next = f->next;
         else
            out->freed_frame_head = f->next;

         out->n_freed_frame--;
         break;
      }
      
      prev = f;
      f = f->next;
   }

   if (found)
   {
      /* init the frame but keep databuf info */
      int rawbytes = found->bytes;
      char* raw = found->raw;

      memset(found, 0, sizeof(struct media_frame_s));

      found->owner = mo;
      found->raw = raw;
      found->bytes = rawbytes;

      /* initialize data */
      if(data != NULL)
      {
         pout_log(("pout_new_frame: initialize %d bytes data\n", bytes));
         memcpy(found->raw, data, bytes);
      }

      pout_log(("pout_new_frame: reuse %dB/%dB, free frames %d/%d\n", found->bytes, out->bytes_allbuf, out->n_freed_frame, out->n_frame));
      
      return found;
   }

   found = xmalloc (sizeof(struct media_frame_s));
   if (!found)
   {
      pout_log (("pout_new_frame: no memory for more frame\n"));
      return NULL;
   }
      
   raw = xmalloc (bytes);
   if (!raw)
   {
      pout_log (("pout_new_frame: no memory for media raw data\n"));
      xfree(found);
      return NULL;
   }
   
   memset(found, 0, sizeof(struct media_frame_s));

   found->owner = mo;
   found->raw = raw;
   found->bytes = bytes;

   out->bytes_allbuf += bytes;
   out->n_frame++;

   /* initialize data */
   if(data != NULL)
   {
      pout_log(("pout_new_frame: initialize %d bytes data\n", bytes));
      memcpy(found->raw, data, bytes);
   }
   /*
   pout_debug ("pout_new_frame: new frame %d bytes / total %d bytes\n"
                        , bytes, out->bytes_allbuf);
   */
   return found;
}

int pout_recycle_frame(media_pipe_t * mo, media_frame_t * used)
{
   pa_pipe_t *out = (pa_pipe_t *)mo;

   media_frame_t * auf = out->freed_frame_head;
   media_frame_t * prev = NULL;

   while (auf && auf->bytes < used->bytes)
   {
      prev = auf;
      auf = auf->next;
   }

   if (!prev)
   {
      used->next = out->freed_frame_head;
      out->freed_frame_head = used;
   }
   else
   {
      used->next = prev->next;
      prev->next = used;
   }

   out->n_freed_frame++;

   return MP_OK;
}

/*
 * Return: 0, frame accepted; n>0, frame delayed, try again in n usec.
int pout_put_frame (media_pipe_t *mp, media_frame_t *mf, int last)
{
   int nbyte_tofill, nbyte_toread;

   pa_pipe_t *pp = (pa_pipe_t*)mp;

   portaudio_device_t *pa = pp->pa;
   
   pa_output_t *outbufw = &pa->outbuf[pa->outbuf_w];

   if(outbufw->nbyte_wrote == pa->io_nbyte_once)
      return pa->usec_pulse / 2;
      
   while(pa->outbuf_nbyte_read < mf->bytes)
   {
      nbyte_toread = mf->bytes - pa->outbuf_nbyte_read;
      nbyte_tofill = pa->io_nbyte_once - outbufw->nbyte_wrote;
      
      if(nbyte_tofill > nbyte_toread)
         nbyte_tofill = nbyte_toread;

      memcpy(&outbufw->pcm[outbufw->nbyte_wrote], &mf->raw[pa->outbuf_nbyte_read], nbyte_tofill);

      outbufw->nbyte_wrote += nbyte_tofill;
      pa->outbuf_nbyte_read += nbyte_tofill;

      if(outbufw->nbyte_wrote == pa->io_nbyte_once)
      {
         outbufw->last = mf->eos;

         pa->outbuf_w = (pa->outbuf_w+1) % pa->outbuf_n;
         outbufw = &pa->outbuf[pa->inbuf_w];

         if(pa->outbuf_nbyte_read < mf->bytes)
            return pa->usec_pulse;
      }
   }

   mf->owner->recycle_frame(mf->owner, mf);
   pa->outbuf_nbyte_read = 0;

   return 0;
}
 */

int pout_put_frame (media_pipe_t *mp, media_frame_t *mf, int last)
{
   pa_pipe_t *pp = (pa_pipe_t*)mp;

   if(pp->outframes[pp->outframe_w] != NULL)
      return pp->pa->usec_pulse / 2;

   pout_debug (("\rpout_put_frame: put outbuf[%d] frame#%lld[s%lld] bytes[%d]\n", pp->outframe_w, mf->sno, mf->samplestamp, mf->bytes));
   
   pp->outframes[pp->outframe_w] = mf;
   pp->outframe_w = (pp->outframe_w+1) % PIPE_NBUF;

   return 0;
}

int pout_pick_content (media_pipe_t *pipe, media_info_t *media_info, char* pcm, int npcm)
{
   int nbyte_done = 0;
   int nbyte_todo = 0;
   
   pa_pipe_t *pp = (pa_pipe_t*)pipe;
   int pcm_nbyte = npcm * (media_info->sample_bits / OS_BYTE_BITS);
   
   media_frame_t *outf = pp->outframes[pp->outframe_r];

   if(pp->outframes[pp->outframe_r] == NULL)
      return 0;
   
   while(pcm_nbyte)
   {
      if(pp->outframe_nbyte == 0)
      {
         pp->outframes[pp->outframe_r] = NULL;
         outf->owner->recycle_frame(outf->owner, outf);
      
         pp->outframe_r = (pp->outframe_r + 1) % PIPE_NBUF;
         outf = pp->outframes[pp->outframe_r];
      
         if(!outf)
            return nbyte_done;
         
         pp->outframe_nbyte = outf->bytes;
      }

      if(pcm_nbyte > pp->outframe_nbyte)
         nbyte_todo = pp->outframe_nbyte;
      else
         nbyte_todo = pcm_nbyte;

      memcpy (&pcm[nbyte_done], &outf->raw[outf->bytes - pp->outframe_nbyte], nbyte_todo);

      pcm_nbyte -= nbyte_todo;
      pp->outframe_nbyte -= nbyte_todo;
      nbyte_done += nbyte_todo;
   }

   return nbyte_done / (media_info->sample_bits / OS_BYTE_BITS);
}

int pout_done (media_pipe_t * mp)
{
   media_frame_t * f = NULL;
   media_frame_t * next = NULL;

   pa_pipe_t * pipe = (pa_pipe_t *)mp;

   //time_end(pipe->clock);
   
   pipe->bytes_freed = 0;

   /* free spare frames */
   f = pipe->freed_frame_head;
   while(f)
   {
      next = f->next;

      xfree(f->raw);
      pipe->bytes_freed += f->bytes;

      xfree(f);
      f = next;
   }

   /*if(pipe->fillgap) xfree(pipe->fillgap);*/
   
   pout_log(("vorbis_done_player: player done, %d bytes buffers freed\n", pipe->bytes_freed));

   xfree(pipe);

   return MP_OK;
}

/* *
 * Create a new time armed pipe
 *
 * - sample_rate: how many samples go through the pipe per second
 * - usec_pulse: samples are sent by group, named pulse within certain usecond.

 * - ms_min, ms_max: buffered/delayed samples are limited.
 */

media_pipe_t * pa_pipe_new(portaudio_device_t* pa)
{
   media_pipe_t * mout = NULL;

   pa_pipe_t *pout = xmalloc( sizeof(struct pa_pipe_s) );
   if(!pout)
   {
      pout_debug (("timed_outpipe_new: no memory\n"));
      return NULL;
   }             
   memset(pout, 0, sizeof(struct pa_pipe_s));

   pout->pa = pa;
   pout_debug (("\rtimed_outpipe_new: pout@%x\n", (int)pout));
   
   mout = (media_pipe_t *)pout;
   
   mout->done = pout_done;      
   mout->new_frame = pout_new_frame;
   mout->recycle_frame = pout_recycle_frame;   
   mout->put_frame = pout_put_frame;
   mout->pick_content = pout_pick_content;

   return mout;
}
