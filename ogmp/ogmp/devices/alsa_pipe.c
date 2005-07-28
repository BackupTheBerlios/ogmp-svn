/***************************************************************************
                          alsa_outpipe.c  -  ALSA Pipe
                             -------------------
    begin                : Tue Jul 26 2005
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

#include "dev_alsa.h"

#include <timedia/ui.h>
#include <timedia/xmalloc.h>
#include <string.h>
/*
#define PIPE_LOG
*/
#define PIPE_DEBUG
#ifdef PIPE_LOG
 #define alsap_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define alsap_log(fmtargs)  
#endif

#ifdef PIPE_DEBUG
 #define alsap_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define alsap_debug(fmtargs) 
#endif

media_frame_t * alsap_new_frame(media_pipe_t * mo, int bytes, char *data)
{
   media_frame_t * found = NULL;
   media_frame_t * prev = NULL;
   
   alsa_pipe_t * out = (alsa_pipe_t *)mo;
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
         alsap_log(("alsap_new_frame: initialize %d bytes data\n", bytes));
         memcpy(found->raw, data, bytes);
      }

      alsap_log(("alsap_new_frame: reuse %dB/%dB, free frames %d/%d\n", found->bytes, out->bytes_allbuf, out->n_freed_frame, out->n_frame));
      
      return found;
   }

   found = xmalloc (sizeof(struct media_frame_s));
   if (!found)
   {
      alsap_log (("alsap_new_frame: no memory for more frame\n"));
      return NULL;
   }
      
   raw = xmalloc (bytes);
   if (!raw)
   {
      alsap_log (("alsap_new_frame: no memory for media raw data\n"));
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
      alsap_log(("alsap_new_frame: initialize %d bytes data\n", bytes));
      memcpy(found->raw, data, bytes);
   }
   /*
   alsap_debug ("alsap_new_frame: new frame %d bytes / total %d bytes\n"
                        , bytes, out->bytes_allbuf);
   */
   return found;
}

int alsap_recycle_frame(media_pipe_t * mo, media_frame_t * used)
{
   alsa_pipe_t *out = (alsa_pipe_t *)mo;

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

int alsap_put_frame (media_pipe_t *mp, media_frame_t *mf, int last)
{
   alsa_pipe_t *ap = (alsa_pipe_t*)mp;
   alsa_device_t *alsa = ap->alsa;

   if(alsa->outframes[alsa->outframe_w] != NULL)
      return alsa->usec_pulse / 2;

   alsa->outframes[alsa->outframe_w] = mf;
   alsa->outframe_w = (alsa->outframe_w+1) % PIPE_NBUF;

   return 0;
}

int alsap_pick_content (media_pipe_t *pipe, media_info_t *media_info, char* out, int npcm)
{
   int npcm_ready = 0;
   int npcm_tofill, npcm_filled, i;

   short *rpcm;
   short *wpcm = (short*)out;

   alsa_pipe_t *ap = (alsa_pipe_t*)pipe;
   alsa_device_t *alsa = ap->alsa;

   media_frame_t *outf = alsa->outframes[alsa->outframe_r];

   if(outf == NULL)
      return 0;

	npcm_filled = 0;
   while(npcm_filled < npcm)
   {
      npcm_tofill = npcm - npcm_filled;

	   if(alsa->outframe_npcm_done == outf->nraw)
      {
         alsa->outframes[alsa->outframe_r] = NULL;
         outf->owner->recycle_frame(outf->owner, outf);

         alsa->outframe_r = (alsa->outframe_r + 1) % PIPE_NBUF;
         outf = alsa->outframes[alsa->outframe_r];

         if(!outf)
		   {
			   alsa->outframe_npcm_done = 0;
            
			   return npcm_filled;
         }

         alsa->outframe_npcm_done = 0;
      }

	   rpcm = (short*)outf->raw;

	   npcm_ready = outf->nraw - alsa->outframe_npcm_done;
      npcm_tofill = npcm_tofill < npcm_ready ? npcm_tofill : npcm_ready;

      for(i=0; i<npcm_tofill; i++)
		  wpcm[npcm_filled+i] = rpcm[alsa->outframe_npcm_done+i];

      npcm_filled += npcm_tofill;
	   alsa->outframe_npcm_done += npcm_tofill;
   }

	return npcm_filled;
}

int alsap_done (media_pipe_t * mp)
{
   media_frame_t * f = NULL;
   media_frame_t * next = NULL;

   alsa_pipe_t * pipe = (alsa_pipe_t *)mp;
   
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
   alsap_log(("alsap_done: %d bytes buffers freed\n", pipe->bytes_freed));

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

media_pipe_t * alsa_new_pipe(alsa_device_t* alsa)
{
   media_pipe_t * mout = NULL;

   alsa_pipe_t *alsap = xmalloc( sizeof(struct alsa_pipe_s) );
   if(!alsap)
   {
      alsap_debug (("timed_outpipe_new: no memory\n"));
      return NULL;
   }             
   memset(alsap, 0, sizeof(struct alsa_pipe_s));

   alsap->alsa = alsa;
   alsap_debug (("\rtimed_outpipe_new: alsap@%x\n", (int)alsap));
   
   mout = (media_pipe_t *)alsap;
   
   mout->done = alsap_done;      
   mout->new_frame = alsap_new_frame;
   mout->recycle_frame = alsap_recycle_frame;
      
   mout->put_frame = alsap_put_frame;
   mout->pick_content = alsap_pick_content;

   return mout;
}
