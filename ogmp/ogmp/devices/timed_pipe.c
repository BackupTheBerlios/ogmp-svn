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

#include "timed_pipe.h"
#include "../log.h"

#include <timedia/xmalloc.h>
#include <string.h>
/*
#define PIPE_LOG
*/
#define PIPE_DEBUG

#ifdef PIPE_LOG
 #define pout_log(fmtargs)  do{log_printf fmtargs;}while(0)
#else
 #define pout_log(fmtargs)  
#endif


#ifdef PIPE_DEBUG
 #define pout_debug(fmtargs)  do{log_printf fmtargs;}while(0)
#else
 #define pout_debug(fmtargs) 
#endif

#define USEC_JUMP 100

#define DEFAULT_USEC_PER_BUF  1000000  /* 1s */
#define MIN_USEC_BUFFER       10000     /* 10ms */

#define MUTE_VALUE 0
#define PIPE_ADJUST_FACTOR 200

#define N_SWITCH_PER_ADJUSTMENT 2 /* adjust output speed pre N_SWITCH_PER_ADJUSTMENT switch */

media_frame_t * pout_new_frame(media_pipe_t * mo, int bytes, char *data)
{
   media_frame_t * found = NULL;
   media_frame_t * prev = NULL;
   
   timed_pipe_t * out = (timed_pipe_t *)mo;
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

      found->bytes = rawbytes;
      found->raw = raw;

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
      pout_debug (("pout_new_frame: no memory for more frame\n"));
      return NULL;
   }
   
   raw = xmalloc (bytes);
   if (!raw)
   {
      pout_debug (("pout_new_frame: no memory for media raw data\n"));
      xfree(found);
      return NULL;
   }
   
   memset(found, 0, sizeof(struct media_frame_s));

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
   timed_pipe_t *out = (timed_pipe_t *)mo;

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

int pout_put_frame (media_pipe_t *mp, media_frame_t *f, int last)
{
   timed_pipe_t *pipe = (timed_pipe_t *)mp;
   sample_buffer_t * bufw = NULL;
   int nsample_total = 0;
   int nsample_inbuf = 0;
   int skip_recyc = 0;
   //rtime_t us_jump_start = 0;

   if (!pipe->buffered)
   {
      /* Buffer[0] to start bufferring */
      pipe->bufn_write = 0;
   }

   bufw = &(pipe->buffer[pipe->bufn_write]);

   if(!bufw->frame_head)
   {
      /* put into an empty buffer */
      bufw->frame_head = bufw->frame_tail = f;
      bufw->usec_in = 0;
      bufw->samples_in = 0;
   }
   else
   {
      /* buffer already contains frames */
      bufw->frame_tail->next = f;
      bufw->frame_tail = f;
   }

   bufw->samples_in += f->nraw;
   bufw->usec_in += f->usec;

   if (pipe->switch_buffer != 0)
   {
      /* doing switch right now */ 
      pipe->nsample_write_left += f->nraw;
   }
   else
   {
      /* switch finished, in normal condition,
       * bufw->samples_in already includes new frame
       */ 
      pipe->nsample_write_left = bufw->samples_in - bufw->nsample_prepick;
   }
   
   nsample_total = pipe->nsample_write_left + pipe->nsample_read_left;
   /*
   pout_log(("pout_put_frame: [@%dms] (+)%d, %dw + %dr = %d samples avail.\n"
             , f->ts, f->nraw, pipe->nsample_write_left
             , pipe->nsample_read_left, nsample_total));
   */
   if (last)
   {
      f->eots = 1;

      nsample_inbuf = bufw->samples_in - bufw->nsample_prepick;
      /*
      pout_log(("pout_put_frame: put frame[%d->%d] in Buf[%d]:%d samples (%d/%dus), last timestamp[%d] packet(%d)\n\n"
               , (int)f, (int)f->next, pipe->bufn_write, nsample_inbuf
               , bufw->usec_in, pipe->usec_per_buf, f->ts, last));
      */
      if (last == MP_EOS)
	  {
         pout_log(("pout_put_frame: stream end received\n"));
         f->eos = 1;
      }
   }

   /* switch buffer only when write buffer is usec_per_buf */
   if ( (bufw->usec_in > pipe->usec_per_buf) || last == MP_EOS || pipe->prepick)
   {
      if(!pipe->buffered)
	  {
         pipe->eots = 0;         
         skip_recyc = 1; /* Actually nothing to recycle yet for first time */
         
         pipe->frame_read_now = bufw->frame_head;

		 pipe->usec_prepick = time_usec_now(pipe->clock);

		 pipe->last_avg_usec_inbuf = 0;
		 pipe->avg_usec_inbuf = 0;
		 pipe->adjust_reset = 1;
         
         pout_debug(("pout_put_frame: +++++++++++++ pick Buf#%d[%dus]  +++++++++\n", pipe->bufn_write, bufw->usec_in));
      }

      pipe->prepick = 0;  /* reset prepick signal */
	  
	  pipe->buffered = 1;   /* buffer available */

      /* check if the last signal is ackno'd before trigger next one */
      //us_jump_start = time_usec_now(pipe->clock);
      while (pipe->switch_buffer)
		  time_usec_sleep( pipe->clock, USEC_JUMP, NULL);

      /* trigger switch signal */
      pipe->switch_buffer = pipe->bufn_write + 1;  /* avoid zero */
      
      //pout_log(("pout_put_frame: switch cost %dns\n", time_usec_spent(pipe->clock, jump_start)));

      pipe->bufn_write = (pipe->bufn_write + 1) % PIPE_NBUF;
      bufw = &(pipe->buffer[pipe->bufn_write]);

      bufw->state = WRITE_AUDIO;
      bufw->usec_in = 0;
      bufw->samples_in = 0;

      /* adjust buffer size */
      if (pipe->buffer_dyna_usec != 0)
	  {
         pout_log(("pout_put_frame: previous %dus, dyna refer size %dus\n"
                  , pipe->usec_per_buf, pipe->buffer_dyna_usec));
                  
         //pipe->usec_per_buf = (pipe->usec_per_buf + pipe->buffer_dyna_usec) / 2;

         pout_log(("pout_put_frame: buffer adjust to %dus\n", pipe->usec_per_buf));
      }

   } /* if ( bufw->usec_in > pipe->usec_per_buf || last == MP_EOS) */

   return MP_OK;
}

int pout_prepick (timed_pipe_t *pipe)
{
   int nsample_prepick = 0;
   int usec_prepick = 0;
   rtime_t us_prepick_interval;

   sample_buffer_t * bufr = &(pipe->buffer[pipe->bufn_read]);
   sample_buffer_t * nextbuf = &(pipe->buffer[pipe->bufn_write]);

   media_frame_t * ztail = NULL;
   media_frame_t * newhead = nextbuf->frame_head;
   
   while (newhead && newhead->next)
   {
      nsample_prepick += newhead->nraw;
      usec_prepick += newhead->usec;
      
      ztail = newhead;
      newhead = newhead->next;
   }
   
   if (!ztail) return 0;
   
   pipe->frame_read_now = nextbuf->frame_head;

   ztail->next = NULL;
   bufr->frame_tail = ztail;
   
   nextbuf->frame_head = newhead;

   bufr->samples_in += nsample_prepick;
   bufr->usec_in += usec_prepick;
   
   nextbuf->nsample_prepick += nsample_prepick;
   nextbuf->usec_prepick += usec_prepick;
   
   pipe->nsample_read_left += nsample_prepick;
   pipe->nsample_write_left -= nsample_prepick;
   
   /* prepick next buffer */
   pout_log(("pout_prepick: Buf[%d]:%d -> %d/%d -> Buf[%d]:%d/%d samples\n"
            , pipe->bufn_write, nextbuf->samples_in, nsample_prepick
            , nextbuf->nsample_prepick, pipe->bufn_read
            , bufr->samples_out, bufr->samples_in));

   pipe->prepick = 1;

   us_prepick_interval = time_usec_checkpass(pipe->clock, &pipe->usec_prepick);
   
   pout_debug(("\n++++++[%dus]++++++ prepick Buf#%d[%dus] ++++++[%df/%df]++++++\n\n", us_prepick_interval, pipe->bufn_write, bufr->usec_in, pipe->n_freed_frame, pipe->n_frame));
   
   return nsample_prepick;
}

sample_buffer_t * pout_switch_buffer(timed_pipe_t *pipe)
{
   int bufn_next = 0;
   sample_buffer_t *nextbuf = NULL;
   int nsample_delta = 0;
   int usec_delta = 0;
   sample_buffer_t * recyc = NULL;
   media_frame_t *f = NULL;

   sample_buffer_t * buf = &(pipe->buffer[pipe->bufn_read]);

   pout_log(("pout_switch_buffer: Buf[%d], frame_read_now[%d->%d]\n"
            , pipe->bufn_read, (int)pipe->frame_read_now, (int)pipe->frame_read_now->next));

   pipe->buffer_dyna_usec = buf->usec_out;
   pipe->samples_last_buffered = buf->samples_in;

   /* transfer to next buffer */
   bufn_next = pipe->switch_buffer - 1;
   nextbuf = &(pipe->buffer[bufn_next]);

   nextbuf->samples_out = 0;
   nextbuf->usec_out = 0;

   /* count prepick */
   nextbuf->samples_in -= nextbuf->nsample_prepick;
   nextbuf->usec_in -= nextbuf->usec_prepick;
   nextbuf->nsample_prepick = 0;
   nextbuf->usec_prepick = 0;
   pipe->nsample_write_left = 0;

   if ( pipe->frame_read_now->nraw_done == pipe->frame_read_now->nraw &&
         pipe->frame_read_now->next == NULL )
   {
      pipe->frame_read_now = nextbuf->frame_head;
   }
   else
   {
      /* some frame left in the buffer */
      buf->frame_tail->next = nextbuf->frame_head;
      nextbuf->oldhead = nextbuf->frame_head;

      if (pipe->frame_read_now->nraw_done == pipe->frame_read_now->nraw)
	  {
         pipe->last_played_frame = pipe->frame_read_now;
         pipe->frame_read_now = pipe->frame_read_now->next;
      }
      
      nextbuf->frame_head = pipe->frame_read_now;

      if (pipe->last_played_frame != NULL)
	  {
         /* cut off the tail */
         pipe->last_played_frame->next = NULL;
         buf->frame_tail = pipe->last_played_frame;
      }

      /* measure the buffer level */
      nsample_delta = buf->samples_in - buf->samples_out;
      usec_delta = buf->usec_in - buf->usec_out;

      if (nsample_delta < 0)
	  {
         nsample_delta = 0;
         usec_delta = 0;
      }

      nextbuf->samples_in += nsample_delta;
      nextbuf->usec_in += usec_delta;
   }

   /* recycle used buffer */
   buf->state = RECYCLE_BUFFER;

   pipe->bufn_recyc = pipe->bufn_read;
   recyc = &(pipe->buffer[pipe->bufn_recyc]);

   f = recyc->frame_head;
   while (f)
   {
      recyc->frame_head = f->next;
      pout_recycle_frame((media_pipe_t*)pipe, f);
      f = recyc->frame_head;
   }

   /* switch reading buffer */
   pipe->bufn_read = pipe->switch_buffer - 1; /* to the right buffer */   
   buf = &(pipe->buffer[pipe->bufn_read]);

   buf->state = READ_AUDIO;

   buf->usec_out = 0;
   buf->usec_mute = 0;
   buf->samples_mute = 0;

   pout_log(("pout_switch_buffer: Switched to Buffer[%d] contains %d samples (%dus)\n"
            , pipe->bufn_read, buf->samples_in, buf->usec_in));

   return buf;
}

/*
 * Return 0: stream continues
 * return 1: stream ends
 */
int pout_pick_content(media_pipe_t *mp, media_info_t *mi, char * out, int nraw_once)
{
   sample_buffer_t * bufr = NULL;
   int do_switch = 0;
   int nraw_tofill = 0;
   int i;
   int usec_mute = 0;
   int ncpy = 0;
   int nsample_total = 0;
   rtime_t usec_inbuf = 0;
   char *in = NULL;
   
   timed_pipe_t *pipe = (timed_pipe_t *)mp;
   audio_info_t *ai = (audio_info_t *)mi;

   if (!pipe->fillgap)
      pipe->fillgap = xmalloc (ai->channels_bytes);

   bufr = &(pipe->buffer[pipe->bufn_read]);

   if (!pipe->buffered)
   {
      /* buffer certain number frames before play back */
      sample_buffer_t * bufw = &(pipe->buffer[pipe->bufn_write]);
      
      memset(out, MUTE_VALUE, nraw_once * ai->channels_bytes);
      
      bufr->samples_mute += nraw_once;
      
      pout_log(("pout_pick_content: Buf[%d] is buffering %d samples (%dus), mute %d samples\n"
               , pipe->bufn_write, bufw->samples_in, bufw->usec_in, bufr->samples_mute));

      return 0;
   }

   /* If there is a switch signal, do switch and ackno switch
    * copy global varible, void value corrupt
    */
   do_switch = pipe->switch_buffer;
   if ( do_switch )
   {
      if (do_switch - 1 != pipe->bufn_read )
	  {
         bufr = pout_switch_buffer (pipe);
      }
	  else
	  {
         pout_log(("pout_pick_content: start playing\n"));
         //pipe->beat_start = time_usec_now(pipe->clock);
      }

	  if(pipe->n_switch_per_adjustment > 0)
	  {
		  pipe->n_switch_per_adjustment--;
	  }
	  else if(pipe->avg_usec_inbuf != 0)
	  {
		  rtime_t us_adj;
		  int nsamp_adj;
		  
		  pipe->n_switch_per_adjustment = N_SWITCH_PER_ADJUSTMENT;

		  if(pipe->last_avg_usec_inbuf != 0)
		  {
			  us_adj = pipe->avg_usec_inbuf - pipe->last_avg_usec_inbuf;
		  }
		  else
		  {
			  pipe->last_avg_usec_inbuf = pipe->avg_usec_inbuf;
			  us_adj = 0;
		  }

		  /* work out nsample to adjust */
		  nsamp_adj = (int)((double)pipe->sample_rate/1000000*us_adj);

		  pipe->nsample_shift = (nsamp_adj==0) ? 0 : pipe->nsample_adjust / nsamp_adj;
		  
		  pout_debug(("\nBuf#%d[%dus]avg[%dus]#adj[%dus:%dn/%dn]#frm[%df/%df:%dB]\n", pipe->bufn_read, bufr->usec_in, pipe->avg_usec_inbuf, us_adj, nsamp_adj, pipe->nsample_adjust, pipe->n_freed_frame, pipe->n_frame, pipe->bytes_allbuf));
		  
		  if(pipe->nsample_shift > 0)
			  pout_debug(("more %d sample(1/%d) - %d pcm once\n", nsamp_adj, pipe->nsample_shift, nraw_once));

		  if(pipe->nsample_shift < 0)
			  pout_debug(("less %d sample(1/%d) - %d pcm once\n", -nsamp_adj, -pipe->nsample_shift, nraw_once));

		  pipe->last_avg_usec_inbuf = pipe->avg_usec_inbuf;

		  pipe->avg_usec_inbuf = 0;
		  pipe->adjust_reset = 1;
	  }
   }
         
   /* finish handling switch signal */
   pipe->switch_buffer = 0;

   /*   
   adjust_period = pipe->frame_read_now->ts - pipe->lts_last;

   if (adjust_period != 0 && pipe->usec_to_adjust > 0)
   {
      nraw_add = (double)pipe->usec_to_adjust / adjust_period * nraw_once;

      if( nraw_add > nraw_once / PIPE_ADJUST_FACTOR)
         nraw_add = nraw_once / PIPE_ADJUST_FACTOR;

      pout_log(("pout_pick_content: adjust %d/%d samples\n", nraw_add, nraw_once));

      nraw_tofill = nraw_once - nraw_add;
   }
   else
   {
      nraw_tofill = nraw_once;
   }
   */

   nraw_tofill = nraw_once;
   
   while (nraw_tofill)
   {
      int shift_copy;

	  if (pipe->frame_read_now->nraw_done == pipe->frame_read_now->nraw)
	  {
         /* timing measuring */
         if (pipe->frame_read_now->eots)
		 {
            /* the end of this timestamp */
            pipe->eots = 1;
            pipe->lts_last = pipe->frame_read_now->ts;
            
            nsample_total = pipe->nsample_write_left + pipe->nsample_read_left;
            usec_inbuf = (int)((double)nsample_total / pipe->sample_rate * 1000000);

            pout_log(("pout_pick_content: Total %d samples (%dus) in the buffer\n", nsample_total, usec_inbuf));
            pout_log(("************************ END OF TIMESTAMP *********************\n\n"));
         }

         if (pipe->lts_last != pipe->frame_read_now->ts && pipe->eots)
		 {
            /* the beginning of the next timestamp */
            pipe->eots = 0;            
            
            pout_log(("\n********************* BEGINNING OF TIMESTAMP ******************\n"));
         }

         /* finish when the stream ends */
         if (pipe->frame_read_now->eos)
		 {
            pout_log(("pout_pick_content: reach stream end\n"));

            memset((void*)out, 0, nraw_tofill * ai->channels_bytes);

            return 1; /* end */
         }
       
         /* to next frame */
         pipe->last_played_frame = pipe->frame_read_now;

         if (pipe->frame_read_now->next != NULL)
		 {
            pipe->frame_read_now = pipe->frame_read_now->next;

            if (pipe->frame_read_now == bufr->oldhead)
			{
               pout_log(("============================================================\n"));
               pout_log(("(pout_pick_content: playout the transfer part, now play my own)\n\n"));
            }
         }
		 else
		 {
            /* end of buffer */
            if (bufr->samples_in > bufr->samples_out)
			{
               pout_log(("\n"));
               pout_log(("        >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"));
               pout_log(("        >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"));
               pout_log(("        >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"));
               pout_log(("        >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"));
               pout_log(("        >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"));
               pout_log(("        >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"));
               pout_log(("        >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"));
               pout_log(("        (pout_pick_content: Sth. Wrong! %d/%d samples played only)\n\n"
                        , bufr->samples_out, bufr->samples_in));
            }
            
            if (pout_prepick(pipe) == 0)
			{
               /* the buffer is overflowed, handle it */
               if (pipe->fillgap)
			   {
                  pout_log(("\n"));
                  pout_log(("        0000000000000000000000000000000000000000000000000000000000000\n"));
                  pout_log(("        0000000000000000000000000000000000000000000000000000000000000\n"));
                  pout_log(("        0000000000000000000000000000000000000000000000000000000000000\n"));
                  pout_log(("        0000000000000000000000000000000000000000000000000000000000000\n"));
                  pout_log(("        0000000000000000000000000000000000000000000000000000000000000\n"));
                  pout_log(("        0000000000000000000000000000000000000000000000000000000000000\n"));
                  pout_log(("        0000000000000000000000000000000000000000000000000000000000000\n"));
                  pout_log(("        (pout_pick_content: fill in %d sample gap with last sample)\n\n", nraw_tofill));

                  for (i=0; i<nraw_tofill; i++)
				  {
                     memcpy(out, pipe->fillgap, ai->channels_bytes);
                     out += ai->channels_bytes;
                  }
               }
			   else
			   {
                  memset(out, 0, nraw_tofill * ai->channels_bytes);
               }

               bufr->samples_mute += nraw_tofill;

               usec_mute = pipe->usec_pulse * nraw_tofill / nraw_once;
               
               bufr->usec_mute += usec_mute;
               bufr->usec_out += pipe->usec_pulse - usec_mute;

               return 0;

            } /* if (pout_prepick(pipe) == 0) */
  
         } /* if (frame_now->next != NULL) */

      } /* if (frame_now->nraw_played == frame_now->nraw && nraw_tofill) */

      ncpy = pipe->frame_read_now->nraw - pipe->frame_read_now->nraw_done;

      if (ncpy > nraw_tofill)
         ncpy = nraw_tofill;
      
      in = pipe->frame_read_now->raw;
      in += pipe->frame_read_now->nraw_done * ai->channels_bytes;
      
	  if(pipe->nsample_shift < 0)
	  {
		  shift_copy = -pipe->nsample_shift;
		  shift_copy--;
	  }
	  else
	  {
		  shift_copy = pipe->nsample_shift;
		  shift_copy++;
	  }

/*
	  shift = ncpy - pipe->under_shift;
	  while(shift > shift_copy)
	  {
		  memcpy(out, in, shift_copy * ai->channels_bytes); //output

		  in = in + (shift_copy) * ai->channels_bytes;
		  out = out + (shift_copy) * ai->channels_bytes;
		  if(pipe->nsample_shift < 0)
		  {
			  // duplicate last sample
			  memcpy(out, in, ai->channels_bytes); //output
		  
			  out = out + ai->channels_bytes;

		  }
		  memcpy(out, in, ai->channels_bytes); //output
      
		  in = in + ai->channels_bytes;
		  out = out + ai->channels_bytes;

		  shift = shift - shift_copy;
	  }
	  if(shift > 0)
	  {
		  memcpy(out, in, shift * ai->channels_bytes); //output

		  in = in + shift * ai->channels_bytes;
		  out = out + shift * ai->channels_bytes;

		  pipe->under_shift = shift;
	  }
*/
	  memcpy(out, in, ncpy * ai->channels_bytes); //output

	  in = in + ncpy * ai->channels_bytes;
	  out = out + ncpy * ai->channels_bytes;

      bufr->samples_out += ncpy;
      nraw_tofill -= ncpy;

      /* keep the last sample for a possible gap 
      if (pipe->fillgap)
		  memcpy(pipe->fillgap, out - ai->channels_bytes, ai->channels_bytes);
	  */

	  if(pipe->adjust_reset)
	  {
		  pipe->adjust_reset = 0;
		  pipe->nsample_adjust = 0;
	  }

	  pipe->nsample_adjust += ncpy;

      /* update buffer meter */
      pipe->frame_read_now->nraw_done += ncpy;
      pipe->nsample_read_left = bufr->samples_in - bufr->samples_out;

      nsample_total = pipe->nsample_write_left + pipe->nsample_read_left;
      usec_inbuf = (int)((double)nsample_total / pipe->sample_rate * 1000000);

	  if(pipe->avg_usec_inbuf == 0)
		  pipe->avg_usec_inbuf = usec_inbuf;
	  else
		  pipe->avg_usec_inbuf = (pipe->avg_usec_inbuf * 7 + usec_inbuf)/8;
      
      /*
      pout_log(("pout_pick_content: [@%dms] (-)%d, %dw + %dr = %d samples left\n"
          , pipe->frame_read_now->ts, ncpy, pipe->nsample_write_left
          , pipe->nsample_read_left, nsample_total));
      */
      
   } /* while (nraw_tofill) */

   /*
   pout_log("pout_pick_content: pick Buf[%d], %d/%d samples, %d/%d us, frame_read_now[%d->%d]:%d/%d\n"
            , pipe->bufn_read, bufr->samples_out, bufr->samples_in, bufr->usec_out, bufr->usec_in
            , (int)pipe->frame_read_now, (int)pipe->frame_read_now->next
            , pipe->frame_read_now->nraw_done, pipe->frame_read_now->nraw);
   */
   
   bufr->usec_out += pipe->usec_pulse;

   if (!pipe->pulsing)
      pipe->pulsing = 1;
   
   if (pipe->frame_read_now->nraw_done == pipe->frame_read_now->nraw && pipe->frame_read_now->eos)
   {
      pipe->pulsing = 0;
      pout_log(("pout_pick_content: stream end, player stop\n"));
   }
   
   return pipe->frame_read_now->eos;
}

media_frame_t* pout_pick_frame(media_pipe_t *mp)
{
	pout_log(("pout_pick_frame: Not Implemented yet, see pout_pick_frame_content()\n"));

	return NULL;
}

int pout_done_buffer(timed_pipe_t * pipe, sample_buffer_t * buf)
{
   media_frame_t * f = buf->frame_head;
   media_frame_t * next;

   while(f)
   {
      next = f->next;

      xfree(f->raw);
      pipe->bytes_freed += f->bytes;

      xfree(f);
      f = next;
   }

   return MP_OK;
}

int pout_done (media_pipe_t * mo)
{
   int i;
   media_frame_t * f = NULL;
   media_frame_t * next = NULL;

   timed_pipe_t * pipe = (timed_pipe_t *)mo;

   time_end(pipe->clock);
   
   pipe->bytes_freed = 0;

   for(i=0; i<PIPE_NBUF; i++)
   {
      pout_done_buffer(pipe, &(pipe->buffer[i]));
   }

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

   if(pipe->fillgap) xfree(pipe->fillgap);
   
   pout_log(("vorbis_done_player: player done, %d bytes buffers freed\n", pipe->bytes_freed));

   xfree(pipe);

   return MP_OK;
}

int timed_pipe_set_usec (timed_pipe_t *pipe, int usec)
{
   if (usec < 0)
   {
      pout_debug (("timed_pipe_set_msec: %d microseconds unacceptable\n", usec));
      return MP_FAIL;
   }
   
   pipe->usec_per_buf = usec;

   return MP_OK;
}

/*
 * Create a new time armed pipe
 * - sample_rate: how many samples go through the pipe per second
 * - usec_pulse: samples are sent by group, named pulse within certain usecond.
 * - ms_min, ms_max: buffered/delayed samples are limited.
 */
media_pipe_t * timed_pipe_new(int sample_rate, int usec_pulse)
{
   media_pipe_t * mout = NULL;

   timed_pipe_t *pout = xmalloc( sizeof(struct timed_pipe_s) );
   if(!pout)
   {
      pout_debug (("timed_outpipe_new: no memory\n"));
      return NULL;
   }             
   memset(pout, 0, sizeof(struct timed_pipe_s));

   pout->clock = time_start();
   
   pout->usec_pulse = usec_pulse;

   pout->usec_per_buf = DEFAULT_USEC_PER_BUF;
   
   pout->sample_rate = sample_rate;
   /*
   pout->dad_min_nsample = (int)((double)ms_min / 1000 * sample_rate);
   pout->dad_max_nsample = (int)((double)ms_max / 1000 * sample_rate);
   */
   pout_log(("timed_pipe_new: delay adapt detect b/w [%d...%d] samples\n"
        , pout->dad_min_nsample, pout->dad_max_nsample));
   
   mout = (media_pipe_t *)pout;
   
   mout->done = pout_done;
   
   mout->new_frame = pout_new_frame;
   mout->recycle_frame = pout_recycle_frame;
   
   mout->put_frame = pout_put_frame;
   mout->pick_frame = pout_pick_frame;
   mout->pick_content = pout_pick_content;

   return mout;
}
