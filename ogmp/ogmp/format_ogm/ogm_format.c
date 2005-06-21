/*   
  Most modified codes are token from ogmtool, it's only for testing. Detail refer
  to the origional project:
  
  http://www.bunkus.org/videotools/ogmtools/

  ogmdemux -- utility for extracting raw streams from an OGG media file

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/      

#include "ogm_format.h"

#include <timedia/xmalloc.h>
#include <string.h>
#include <timedia/ui.h>

#define CATALOG_VERSION  0x0001
#define DIRNAME_MAXLEN  128
/*
*/
#define DEMUX_OGM_LOG
#define DEMUX_OGM_DEBUG

#ifdef DEMUX_OGM_LOG
 #define ogm_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define ogm_log(fmtargs)  
#endif

#ifdef DEMUX_OGM_DEBUG
 #define ogm_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define ogm_debug(fmtargs)  
#endif

int done_stream_handler(void *gen)
{
   ogm_media_t *handler = (ogm_media_t *)gen;

   handler->done(handler);


   return MP_OK;
}

int ogm_done_format(media_format_t * mf)
{
   media_stream_t *cur = mf->first;
   media_stream_t *next;

   while (cur != NULL)
   {
      media_player_t* player = (media_player_t*)cur->player;
      
      next = cur->next;
      
      if(player)
         player->done(player);
         
      xfree(((ogm_stream_t*)cur)->instate);
      xfree(cur);
      
      cur = next;
   }

   if(mf->stream_handlers)
      xrtp_list_free(mf->stream_handlers, done_stream_handler);

   xfree(mf);

   return MP_OK;
}

/* New detected stream add to group */
int ogm_add_stream(media_format_t * ogm, media_stream_t *strm, int sno, unsigned char type)
{
  media_stream_t *cur = ogm->first;

  if (ogm->first == NULL) 
  {
    ogm->first = strm;
    ogm->first->next = NULL;
  } 
  else 
  {
    cur = ogm->first;
    while (cur->next != NULL)
      cur = cur->next;

    cur->next = strm;
    strm->next = NULL;
  }

  switch(type)
  {
    case('a'): ogm->nastreams++;
    /* default first audio stream as time reference */
    if (ogm->nastreams == 1)
    {
       ogm_log(("ogm_set_mime_player: audio stream #%d as default time stream\n", sno));
       ogm->time_ref = sno;
    }
    break;
               
    case('v'): ogm->nvstreams++; break;
    
    case('t'): ogm->ntstreams++; break;
  }

  ogm->numstreams++;

  return MP_OK;
}

media_stream_t * ogm_find_stream(media_format_t * mf, int strmno)
{
  media_stream_t *cur = mf->first;

  while (cur != NULL)
  {
    if ((((ogm_stream_t*)cur)->serial) == strmno)
		break;

    cur = cur->next;
  }

  return cur;
}

media_player_t * ogm_stream_player(media_format_t * mf, int strmno)
{
  media_stream_t *cur = mf->first;

  while (cur != NULL)
  {
    if ((((ogm_stream_t*)cur)->serial) == strmno)
      return (media_player_t*)cur->player;
      
    cur = cur->next;
  }

  return NULL;
}

media_stream_t * ogm_find_mime(media_format_t * mf, char * mime)
{
   media_stream_t *cur = mf->first;

   while (cur != NULL)
   {
      if (!strcmp(cur->mime, mime))
	  {
         return cur;
      }

      cur = cur->next;
   }

   return NULL;
}

int ogm_new_all_player(media_format_t *mf, media_control_t* ctrl, char* mode, void* mode_param)
{
	int c=0;

	media_stream_t *cur = mf->first;

	while (cur != NULL)
	{
		cur->player = ctrl->find_player(ctrl, mode, cur->media_info->mime, cur->media_info->fourcc);
			
		if(!cur->player)
		{
			cur->playable = -1;
			ogm_debug(("ogm_new_all_player: No %s player\n", cur->media_info->mime));
		}
		else if(cur->player->set_device(cur->player, ctrl, ctrl->modules(ctrl), mode_param) < MP_OK)
		{
	      cur->player->done(cur->player);
         cur->player = NULL;
			cur->playable = -1;
			ogm_debug(("ogm_new_all_player: %s stream fail to set device\n", cur->media_info->mime));
		}
      else if( cur->player->open_stream(cur->player, cur->media_info) < MP_OK)
		{      
			cur->playable = -1;
			ogm_debug(("ogm_new_all_player: open %s stream fail!\n", cur->media_info->mime));
		}
		else
		{
			cur->playable = 1;
			c++;
			ogm_log(("ogm_new_all_player: %s stream ok\n", cur->media_info->mime));
		}

		cur = cur->next;
	}

	return c;
}

media_player_t* ogm_new_stream_player(media_format_t *mf, int strmno, media_control_t* ctrl, char* mode, void* mode_param)
{
	media_stream_t *cur = mf->first;

	while (cur != NULL)
	{
		ogm_stream_t* ogm_strm = (ogm_stream_t*)cur;
      

		if (ogm_strm->serial == strmno)
		{
			cur->player = ctrl->find_player(ctrl, mode, cur->media_info->mime, cur->media_info->fourcc);

         if(!cur->player)
			{
				ogm_debug(("ogm_new_stream_player: stream can't be played\n"));
				cur->playable = -1;
				return NULL;
			}
		   else if(cur->player->set_device(cur->player, ctrl, ctrl->modules(ctrl), mode_param) < MP_OK)
         {
	         cur->player->done(cur->player);
            cur->player = NULL;
			   cur->playable = -1;
			   ogm_debug(("ogm_new_stream_player: %s stream fail to set device\n", cur->media_info->mime));
         }

			if( cur->player->open_stream(cur->player, cur->media_info) < MP_OK)
			{
				cur->playable = -1;
				return NULL;
			}         
         
			cur->playable = 1;

			ogm_log(("ogm_new_stream_player: player ok\n"));

			return cur->player;
		}

		cur = cur->next;
	}

	return NULL;
}

media_player_t * ogm_new_mime_player(media_format_t * mf, char * mime, media_control_t* ctrl, char* mode, void* mode_param)
{
	media_stream_t *cur = mf->first;

	while (cur != NULL)
	{
		if (!strcmp(cur->mime, mime))
		{
			cur->player = ctrl->find_player(ctrl, mode, mime, NULL);
			
			if(!cur->player)
			{
				ogm_debug(("ogm_new_mime_player: stream can't be played\n"));
				cur->playable = -1;
				return NULL;
			}
		   else if(cur->player->set_device(cur->player, ctrl, ctrl->modules(ctrl), mode_param) < MP_OK)
         {
	         cur->player->done(cur->player);
            cur->player = NULL;
			   cur->playable = -1;
			   ogm_debug(("ogm_new_mime_player: %s stream fail to set device\n", cur->media_info->mime));
				return NULL;
         }
			else if( cur->player->open_stream(cur->player, cur->media_info) < MP_OK)
			{
				cur->playable = -1;
				return NULL;
			}

         cur->playable = 1;

			ogm_log(("ogm_new_mime_player: player ok\n"));
		}

		cur = cur->next;
	}

	return NULL;
}

int ogm_set_mime_player(media_format_t *mf, char *mime, media_player_t* player)
{
	media_stream_t *cur = mf->first;

	while (cur != NULL)
	{
		if (!strcmp(cur->mime, mime))
		{
			int ret = player->open_stream(player, cur->media_info);
			if(ret < MP_OK)
				return ret;

			cur->player = player;

			return MP_OK;
		}
    
		cur = cur->next;
	}

   ogm_log(("ogm_set_mime_player: stream '%s' not exist\n", mime));
   
	return MP_FAIL;
}

media_player_t * ogm_mime_player(media_format_t * mf, const char * mime)
{
   media_stream_t *cur = mf->first;

   while (cur != NULL)
   {
      if (!strcmp(cur->mime, mime))
	   {
         return cur->player;
      }
    
      cur = cur->next;
   }

   return NULL;
}

media_stream_t * ogm_find_fourcc(media_format_t * mf, char *fourcc)
{
   media_stream_t *cur = mf->first;

   while (cur != NULL)
   {
      if ( strncmp(cur->fourcc, fourcc, 4) )
	   {
         return cur;
      }

      cur = cur->next;

   }

   return NULL;
}

media_player_t * ogm_fourcc_player(media_format_t * mf, const char *fourcc)
{
   media_stream_t *cur = mf->first;

   while (cur != NULL)
   {
      if ( strncmp(cur->fourcc, fourcc, 4) )
	  {
         return cur->player;
      }

      cur = cur->next;
   }

   return NULL;
}

int ogm_set_fourcc_player (media_format_t * mf, media_player_t * player, const char *fourcc)
{
   media_stream_t *cur = mf->first;

   while (cur != NULL)
   {
      if ( !strncmp(cur->fourcc, fourcc, 4) )
	  {
         cur->player = player;

         return MP_OK;
      }

      cur = cur->next;
   }

   ogm_log(("ogm_set_fourcc_player: stream [%c%c%c%c] not exist\n"
            , fourcc[0], fourcc[1], fourcc[2], fourcc[3]));

   return MP_FAIL;
}

int demux_ogm_flush_page(ogm_stream_t *stream, ogg_packet *op)
{
   ogg_page page;

   char * page_data;

   while (ogg_stream_flush(&stream->outstate, &page))
   {
      page_data = xmalloc(page.header_len + page.body_len);


      if(!page_data)
	  {
         ogm_log(("ogm_flush_page: Fail to allocate memery\n"));
         return MP_EMEM;
      }

      memcpy(page_data, page.header, page.header_len);
      memcpy(page_data + page.header_len, page.body, page.body_len);

      ogm_log(("ogm_flush_page: x/a%d: %ld + %ld written, sending rtp simulated\n", stream->sno,
              page.header_len, page.body_len));

      xfree(page_data);
   }

   return MP_OK;
}

int demux_ogm_write_page(ogm_stream_t *stream, ogg_packet *op)
{
   ogg_page page;

   char * page_data;


   while (ogg_stream_pageout(&stream->outstate, &page))
   {
      page_data = xmalloc(page.header_len + page.body_len);

      if(!page_data)
	  {
         ogm_log(("ogm_write_page: Fail to allocate memery\n"));
         return MP_EMEM;
      }

      memcpy(page_data, page.header, page.header_len);
      memcpy(page_data + page.header_len, page.body, page.body_len);
      
      ogm_log(("ogm_write_page: x/a%d: %ld + %ld written, send to rtp channel\n", stream->sno,
              page.header_len, page.body_len));

      xfree(page_data);
   }

   return MP_OK;
}


/* Read ogm packet from file */
static int ogm_read_page (ogm_format_t * ogm)
{
   char *buffer;
   long bytes;

   int sync = ogg_sync_pageout(&(ogm->sync), &(ogm->page));
   while ( sync != 1 )
   {
      if( sync == 0 ) ogm_log(("\nogm_read_page: page need more data\n"));
      if( sync == -1 ) ogm_log(("\nogm_read_page: page out of sync, skipping...\n"));

      buffer = ogg_sync_buffer(&(ogm->sync), CHUNKSIZE);

      if( feof(ogm->format.fdin) )
	  {
         ogm_log(("ogm_read_page: End of file\n"));
         return 0;
      }

      bytes = fread(buffer, 1, CHUNKSIZE, ogm->format.fdin);
	  /*
      ogm_log(("ogm_read_page: %ld bytes read\n", bytes));
	  */
      ogg_sync_wrote(&(ogm->sync), bytes);

      sync = ogg_sync_pageout(&(ogm->sync), &(ogm->page));
   }
   /*
   ogm_log(("ogm_read_page: read #%d:page[%ld]\n\n", ogg_page_serialno(&(ogm->page)), ogg_page_pageno(&(ogm->page))));
   */
   ogm->page_ready = 1;

   return 1;
}

int ogm_set_handlers ( ogm_format_t *ogm, module_catalog_t * cata )
{
   media_format_t *mf = (media_format_t *)ogm;

   mf->stream_handlers = xlist_new();
   if (!mf->stream_handlers)
   {
      return MP_FAIL;
   }

   if(catalog_create_modules(cata, "ogm", mf->stream_handlers) <= 0)
   {
      xrtp_list_free(mf->stream_handlers, NULL);
      return MP_FAIL;
   }

   return MP_OK;
}

int ogm_support_type (media_format_t *mf, char *type, char *subtype)
{
	if(!strcmp(type, "mime") && !strcmp(subtype, "application/ogg"))
		return 1;

	if(!strcmp(type, "ext"))
	{
		if(!strcmp(type, "ogg"))
			return 1;

		if(!strcmp(type, "ogm"))
			return 1;

		if(!strcmp(type, "spx"))
			return 1;
	}

	return 0;
}

int ogm_open_stream(ogm_format_t *ogm, media_control_t *ctrl, int sno, ogg_stream_state *sstate, ogg_packet *packet)
{
   //stream_header sth;
   xrtp_list_user_t $u;
   ogm_media_t *handler;
   media_stream_t *stream = NULL;
   
   media_format_t *mf = (media_format_t *)ogm;

   //copy_headers(&sth, (old_stream_header *)&(ogm->packet.packet[0]), ogm->packet.bytes);
   handler = (ogm_media_t*) xlist_first (mf->stream_handlers, &$u);

   while (handler)
   {
      if (handler->detect_media(packet) != 0)
	  {            
         stream = handler->open_media(handler, ogm, ctrl, sstate, sno, (stream_header *)&(ogm->packet.packet[0]));
         if(stream)
         {
            break;
         }
      }
      
      handler = (ogm_media_t*) xlist_next (mf->stream_handlers, &$u);     
   }

   return MP_OK;
}

int ogm_open(media_format_t *mf, char *fname, media_control_t *ctrl)
{
   int sno = 0;
   int end = 0;   
   int head_parsed = 0;
   int n_pack = 0;
   int n = 0;

   ogm_format_t *ogm = (ogm_format_t *)mf;

   mf->fdin = fopen(fname, "rb");
   if (mf->fdin == NULL)
   {
      ogm_log(("ogm_open_file: Could not open \"%s\".\n", fname));
      return MP_FAIL;
   }

   fseek(mf->fdin, 0, SEEK_END);
   mf->file_bytes = (off_t)ftell(mf->fdin);
   fseek(mf->fdin, 0, SEEK_SET);

   ogg_sync_init(&(ogm->sync));

   ogm_log(("\n=================================================\n"));
   ogm_log(("(ogm_open_file: stream heads begin)\n"));
   
   n=0;
   while (!end)
   {
      if ( !ogm_read_page(ogm) )
      {
         ogm_log(("ogm_open_file: No more pages, scan finish\n"));
         break;
      }

      if (!ogg_page_bos(&ogm->page))
      {
         ogm->page_ready = 1;


         ogm_log(("----------------------------------------------------\n"));
         ogm_log(("ogm_open_file: total %d stream(s) found in '%s' by now\n", mf->numstreams, fname));

         ogm_log(("\n=================================================\n"));
         ogm_log(("(ogm_open_file: stream heads finish, bodys begin)\n\n"));

         /* End of stream head parsing */
         break;
      }
      else
      {
         ogg_stream_state *sstate = xmalloc(sizeof(ogg_stream_state));
         if(!sstate)

         {
            ogm_log(("ogm_open_file: No memery for stream allocation\n"));
            return MP_EMEM;
         }

         sno = ogg_page_serialno(&ogm->page);

         if (ogg_stream_init(sstate, sno) != 0)
         {
            ogm_log(("ogm_open_file: ogg_stream_init failed\n"));
            xfree(sstate);
            return MP_FAIL;
         }

         ogg_stream_pagein(sstate, &ogm->page);

         while(ogg_stream_packetout(sstate, &ogm->packet) == 1)

         {
            n_pack++;

            if (head_parsed)
            {
               ogm_log(("ogm_open_file: stream header parsed, the packet untouched yet\n"));
               ogm->packet_ready = 1;
               end = 1;
               break;
            }

            if ( mf->stream_handlers == NULL && ogm_set_handlers(ogm, ctrl->modules(ctrl)) < MP_OK )
            {
               ogm_log(("ogm_open_file: No stream handler found\n"));
               return MP_FAIL;
            }
	  
            if(ogm_open_stream(ogm, ctrl, sno, sstate, &ogm->packet) >= MP_OK)
				n++;
         }

         ogm_log(("ogm_open: %d packet(s) found in the page\n", n_pack));
      }
   }

   return n;
}

static int64 get_pts (ogm_stream_t *ogm_strm, int64 granulepos)
{
   media_stream_t *stream = (media_stream_t *)ogm_strm;

  /*calculates an pts from an granulepos*/
  if ( granulepos < 0 ) {

    if ( ogm_strm->header_granulepos >= 0 ) {

      /*return the smallest valid pts*/
      return 1;


    } else {

      return 0;
    }

/* THEORA support removed */

  } else if ( stream->sample_rate ) {

    return 1 + ( granulepos * ogm_strm->factor / stream->sample_rate );

  }

  return 0;
}

/* Return the run time milliseconds of the ogm media, some code come from http://xinehq.de */
int demux_ogm_millisec (media_format_t * mf, int rescan)
{
   /*determine the streamlenght and set this->time_length accordingly.
     ATTENTION:current_pos and oggbuffers will be destroyed by this function,
     there will be no way to continue playback uninterrupted.


     You have to seek afterwards, because after get_stream_length, the
     current_position is at the end of the file */

   ogm_format_t *ogm = (ogm_format_t *)mf;

   int filelength;

   int done=0;

   ogm_stream_t *ogm_strm = NULL;

   if(mf->time_length && mf->time_length != -1 && !rescan)
      return mf->time_length;

   mf->time_length=-1;

   filelength = ftell(mf->fdin);

   if (filelength != -1)
   {
      if (filelength > 70000)
	  {
          fseek(mf->fdin, filelength-65536, SEEK_SET);
      }

      done=0;
      while (!done)
	  {
         if (!ogm_read_page (ogm))
		 {
            if (mf->time_length)
			{
               /* this is a fine place to compute avg_bitrate */
               mf->avg_bitrate= (long)((int64)8000 * filelength / mf->time_length);
            }

            return mf->time_length;
         }

         ogm_strm = (ogm_stream_t*)ogm_find_stream(mf, ogg_page_serialno(&ogm->page));

         if ( ogm_strm != NULL )
		 {
            int64 pts = get_pts(ogm_strm, ogg_page_granulepos(&ogm->page)) / 90;

            if (mf->time_length < pts) mf->time_length = (int)pts;
         }
      }
   } 
   
   return 0;
}

/* Seek the ogm file by time, return the new offset */
int ogm_seek_millisecond (media_format_t *mf, int millis) {

   ogm_format_t * ogm = (ogm_format_t *)mf;

   int start_sec = millis / 1000;
   off_t start_pos = 0;
   /*
    * seek to start position
    */

   /* this->keyframe_needed = (this->num_video_streams>0); */
   if ( mf->time_length && mf->time_length!=-1 ) {

      /*do the seek via time*/
      int current_sec = -1;
      off_t current_pos = (off_t)ftell(mf->fdin);

      /*try to find out the current time*/
      if (ogm->last_pts) current_sec = (int)ogm->last_pts/90000;

      /*fixme, the file could grow, do something
        about this->time_length using get_lenght to verify, that the stream
        hasn` changed its length, otherwise no seek to "new" data is possible
       */

      ogm_log (("ogm_seek_millisecond: seek to Second[%d] called\n", millis));
      ogm_log (("ogm_seek_millisecond: current is Second[%d]\n", current_sec));

      if (current_sec > start_sec) 
	  {
         /*seek between beginning and current_pos*/

         /*fixme - sometimes we seek backwards and during
           keyframeseeking, we undo the seek
          */

         start_pos = start_sec * current_pos / current_sec ;
      } 
	  else
	  {
         /*seek between current_pos and end*/
         start_pos = current_pos + ((start_sec - current_sec) *
                  mf->file_bytes - current_pos ) /
                  ( (mf->time_length / 1000) - current_sec);
      }

      ogm_log (("ogm_seek_millisecond: current_pos is%ld\n", current_pos));
      ogm_log (("ogm_seek_millisecond: new_pos is %ld\n", start_pos));
   }
   else
   {
      /*seek using avg_bitrate*/
      start_pos = start_sec * mf->avg_bitrate/8;
   }

   ogm_log (("ogm_seek_millisecond: seeking to %d seconds => %ld bytes\n",
                     start_sec, start_pos));

   ogg_sync_reset(&ogm->sync);
   ogm->page_ready = ogm->packet_ready = 0;

   /* more research
   for (i=0; i<this->num_streams; i++)
   {
      this->header_granulepos[i]=-1;
   }
   */
   /* some strange streams have no syncpoint flag set at the beginning
   if (start_pos == 0) this->keyframe_needed = 0;
   */

   ogm_log (("ogm_seek_millisecond: seek to %ld called\n",start_pos));

   fseek (mf->fdin, start_pos, SEEK_SET);

   return start_pos;

   /* fixme - this would be a nice position to do the following tasks
      1. adjust an ogg videostream to a keyframe
      2. compare the keyframe_pts with start_time. if the difference is to
         high (e.g. larger than max keyframe_intervall, do a new seek or
         continue reading
      3. adjust the audiostreams in such a way, that the
         difference is not to high.

      In short words, do all the cleanups necessary to continue playback
      without further actions

   this->send_newpts     = 1;
   this->status          = DEMUX_OK;

   if( !playing ) {

      this->buf_flag_seek     = 0;

   } else {

      if (start_pos!=0) {

         this->buf_flag_seek = 1;

         **each stream has to continue with a packet that has an granulepos**
         for (i=0; i<this->num_streams; i++) {

            this->resync[i]=1;
         }


         this->start_pts=-1;
      }

      _x_demux_flush_engine(this->stream);
   }

   return this->status;
   */
}

int demux_ogm_process_packet(ogm_format_t * ogm, ogm_stream_t *ogm_strm, ogg_page *page, ogg_packet *pack, int64 samplestamp, int last_packet, int stream_end)
{
   int i, hdrlen;
   int64 lenbytes=0;

   int ret = MP_OK;

   ogm_media_t * handler = NULL;

   media_stream_t *stream = (media_stream_t *)ogm_strm;

   if (pack->b_o_s)
   {
      stream->eos = 0;
      ogm_log(("__________________________________________________________\n"));
      ogm_log(("demux_ogm_process_packet: Beginning of the Stream[%d:%c%d]\n", ogm_strm->serial, ogm_strm->stype, ogm_strm->sno));
      ogm_log(("----------------------------------------------------------\n"));
   }

   if (pack->e_o_s)
   {
      stream->eos = 1;
      pack->e_o_s = 1;

	  /* flag the end of a stream */
	  stream_end = 1;
   }
   /*
   if (!stream->rtp)
   {
      return XRTP_OK;
   }
   */
   hdrlen = (*pack->packet & PACKET_LEN_BITS01) >> 6;
   hdrlen |= (*pack->packet & PACKET_LEN_BITS2) << 1;

   for (i = 0, lenbytes = 0; i < hdrlen; i++)
   {
      lenbytes = lenbytes << 8;
      lenbytes += *((unsigned char *)pack->packet + hdrlen - i);
   }

   handler = (ogm_media_t *)stream->handler;
   ret = handler->process_media(ogm, ogm_strm, page, pack, hdrlen, lenbytes, samplestamp, last_packet, stream_end);
   
   if (stream->eos)
   {
      ogm_log(("----------------------------------------------------\n"));
      ogm_log(("demux_ogm_process_packet: End of the Stream[%d:%c%d]\n", ogm_strm->serial, ogm_strm->stype, ogm_strm->sno));
      ogm_log(("____________________________________________________\n"));
   }

   return ret;
}

/* send stream granulepos data to rtp channels from preseeked position
 * return value:
 * - return positive value means interval usec to next timestamp;
 *          negetive value indicate error;
 *          zero means end of stream
 */
int ogm_demux_next (media_format_t *mf, int stream_end)
{
   ogm_format_t * ogm = (ogm_format_t *)mf;

   int               sno;

   media_stream_t         *stream = NULL;
   ogm_stream_t           *ogm_strm = NULL;

   //int us_page = 0;
   int page_bytes = 0;

   /* Start to handle stream body
    * each time, it will send all packet in enable streams with same granulepos
    * each packet send to its belonged rtp media handler
    */
   int n_pack = 0;
   int ret;
         
   while (1)
   {
      if(!ogm->page_ready)
	  {
         ogm_log(("ogm_demux_next: No valid page yet, reading...\n"));

         if(!ogm_read_page(ogm)) 
		 {
            ogm_log(("ogm_demux_next: No more page to read\n"));
            return MP_EOF;
         }
         
         ogm->page_ready = 1;
         ogm->packet_ready = 0;
		 page_bytes = 0;
      }

      sno = ogg_page_serialno(&(ogm->page));
      
      stream = ogm_find_stream(mf, sno);
      ogm_strm = (ogm_stream_t*)stream;

      if (stream == NULL)
	  {

         ogm_log(("ogm_demux_next: Encountered packet for an unknown serial %d !?\n", sno));
         if(!ogg_sync_pageseek(&(ogm->sync), &(ogm->page)))
		 {
            ogm_log(("ogm_demux_next: No more page to skip\n"));
            ogm->page_ready = 0;

            return 0;
         }            
      } 
	  else
	  {
		  int send = 0;    /* MUST BE */
		  int recv = 1;    /* MUST BE */
		  ogg_packet pack2;         
		  ogg_packet *dual_pack[2] = {&ogm->packet, &pack2};

         /* Figure millisecond from page granulepos */
		 ogg_int64_t page_granul = ogg_page_granulepos(&ogm->page);

         ogg_stream_pagein(ogm_strm->instate, &(ogm->page));

         if (!ogm->packet_ready)
		 {
            if(ogg_stream_packetout(ogm_strm->instate, &ogm->packet) != 1)
			{
               ogm->page_ready = 0;
               continue;
            }
         }
         
         n_pack = 1;
         
         while (ogg_stream_packetout(ogm_strm->instate, dual_pack[recv]) == 1)
		 {


            n_pack++;

            ret = demux_ogm_process_packet(ogm, ogm_strm, &(ogm->page), dual_pack[send], page_granul, 0, stream_end);
            if(ret < MP_OK) return ret;
            
            recv = send;
            send = (send + 1) % 2;

			page_bytes += dual_pack[send]->bytes;
         }
         
         /* last packet of the page */          
         ret = demux_ogm_process_packet(ogm, ogm_strm, &(ogm->page), dual_pack[send], page_granul, 1, stream_end);
         if(ret < 0)
		 {
            ogm_log(("ogm_demux_next: Need resync stream(%d) when fail process\n", sno));
            return ret;
         }

		 page_bytes += dual_pack[send]->bytes;


         ogm->packet_ready = 0;
         
         ogm_log(("ogm_demux_next: %d packet(s) %d bytes found in the #%d:Page[%lld]\n", n_pack, page_bytes, sno, page_granul));

		 if( !ogm_read_page (ogm) )
		 {
            ogm->page_ready = 0;
            return 0;
         }

		 if ( mf->time_ref == sno )
		 {

			 ogg_int64_t delta_granu = page_granul - ogm_strm->last_granulepos;
			 ogg_int64_t us_interval = 1000000 * delta_granu / stream->sample_rate;

			 ogm_log(("............................................................................\n"));
			 ogm_log(("(ogm_demux_next: %lld samples in the page)\n", delta_granu));

			 ogm_strm->last_granulepos = page_granul;
			 ogm_strm->last_microsec += (int)us_interval;
			
			 return (int)us_interval;
		 }
      }
   }          


   /* never reach here */
   return 0;
}

int ogm_close (media_format_t * mf)
{
   ogm_format_t * ogm = (ogm_format_t *)mf;

   ogg_sync_reset(&(ogm->sync));

   fclose(mf->fdin);
   mf->fdin = NULL;

   return MP_OK;
}

int ogm_nstream (media_format_t * mf)
{
   return mf->numstreams;
}

const char* ogm_stream_mime (media_format_t * mf, int strmno)
{
   media_stream_t * ms = ogm_find_stream(mf, strmno);
   if(!ms)  return "";

   return ms->mime;
}

const char* ogm_stream_fourcc (media_format_t * mf, int strmno)
{
   media_stream_t * ms = ogm_find_stream(mf, strmno);
   if(!ms)  return "\0\0\0\0";

   return ms->fourcc;
}

int ogm_players(media_format_t * mf, char *play_type, media_player_t* players[], int nmax)
{
	int n = 0;
	media_stream_t *cur = mf->first;

	while (cur != NULL && n < nmax)
	{
		if(cur->player->match_play_type(cur->player, play_type))
			players[n++] = cur->player;

		cur = cur->next;
	}

	ogm_log (("ogm_players: %d '%s' players in ogm format\n", n, play_type));



	return n;
}


int ogm_set_control (media_format_t * mf, media_control_t * control)
{
   mf->control = control;

   return MP_OK;
}

int ogm_set_player (media_format_t * mf, media_player_t * player)
{
   int ret;
   char* type;

   type = player->media_type(player);
   if ( (ret = ogm_set_mime_player (mf, type, player)) >= MP_OK )
   {
      ogm_log (("ogm_set_player: '%s' stream playable\n", type));
      return ret;
   }
   
   type = player->codec_type(player);
   if ( (ret = ogm_set_mime_player (mf, type, player)) >= MP_OK )
   {
      ogm_log (("ogm_set_player: play '%s' stream\n", type));
      return ret;
   }

   ogm_debug(("ogm_set_player: can't play the media with player\n"));

   return ret;
}

module_interface_t * media_new_format()
{
   media_format_t * mf = NULL;

   ogm_format_t * ogm = xmalloc(sizeof(struct ogm_format_s));
   if(!ogm)
   {
      ogm_log(("ogm_new_rtp_group: No memery to allocate\n"));
      return NULL;

   }

   memset(ogm, 0, sizeof(struct ogm_format_s));

   mf = (media_format_t *)ogm;

   /* release a format */
   mf->done = ogm_done_format;

   mf->support_type = ogm_support_type;

   /* Open/Close a media source */
   mf->open = ogm_open;
   mf->close = ogm_close;
   
   /* Stream management */
   mf->add_stream = ogm_add_stream;


   mf->find_stream = ogm_find_stream;
   mf->find_mime = ogm_find_mime;
   mf->find_fourcc = ogm_find_fourcc;

   mf->set_control = ogm_set_control;
   mf->set_player = ogm_set_player;

   /* Stream info */
   mf->nstream = ogm_nstream;
   mf->stream_mime = ogm_stream_mime;
   mf->stream_fourcc = ogm_stream_fourcc;

   mf->stream_player = ogm_stream_player;
   mf->mime_player = ogm_mime_player;
   mf->fourcc_player = ogm_fourcc_player;

   mf->new_all_player = ogm_new_all_player;

   mf->new_stream_player = ogm_new_stream_player;
   mf->new_mime_player = ogm_new_mime_player;
   mf->set_mime_player = ogm_set_mime_player;

   mf->players = ogm_players;

   /* Seek the media by time */
   mf->seek_millisecond = ogm_seek_millisecond;

   /* demux next sync stream group */
   mf->demux_next = ogm_demux_next;

   return mf;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC module_loadin_t mediaformat =
{
   "format",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   media_new_format   /* Module initializer */
};
