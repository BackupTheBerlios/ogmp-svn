/***************************************************************************
                          rtp_format.c  -  Format defined by rtp capabilities
                             -------------------
    begin                : Web Jul 22 2004
    copyright            : (C) 2004 by Heming Ling
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

#include "rtp_format.h"

#include <timedia/xstring.h>

#define CATALOG_VERSION  0x0001
#define DIRNAME_MAXLEN  128

#define RTP_LOG
#define RTP_DEBUG

#ifdef RTP_LOG
 #define rtp_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define rtp_log(fmtargs)  
#endif

#ifdef RTP_DEBUG
 #define rtp_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define rtp_debug(fmtargs)  
#endif

int rtp_done_stream(rtp_stream_t *strm)
{
	strm->rtp_cap->descript.done((capable_descript_t*)strm->rtp_cap);

	xstr_done_string(strm->source_cname);
	session_done(strm->session);

	free(strm);

	return MP_OK;
}

int rtp_done_format(media_format_t * mf)
{
   media_stream_t *cur = mf->first;
   media_stream_t *next;

   while (cur != NULL)
   {
      next = cur->next;
      
      if(cur->player)
         cur->player->done(cur->player);
         
      rtp_done_stream((rtp_stream_t*)cur);
      free(cur);
      
      cur = next;
   }

   free(mf);

   return MP_OK;
}

/* New stream add to group */
int rtp_add_stream(media_format_t * rtp, media_stream_t *strm, int strmno, unsigned char type)
{
  media_stream_t *cur = rtp->first;

  if (rtp->first == NULL)
  {
    rtp->first = strm;
    rtp->first->next = NULL;
  }
  else
  {
    cur = rtp->first;
    while (cur->next != NULL)
      cur = cur->next;

    cur->next = strm;
    strm->next = NULL;
  }

  switch(type)
  {
    case(MP_AUDIO): rtp->nastreams++;
      
		/* default first audio stream as time reference */
        if (rtp->nastreams == 1)
		{
			rtp_log(("rtp_set_mime_player: stream #%d as default time stream\n", rtp->numstreams));
            rtp->time_ref = strmno;
        }
                  
        break;
               
    case(MP_VIDEO): rtp->nvstreams++; break;
    
    case(MP_TEXT): rtp->ntstreams++; break;
  }

  rtp->numstreams++;

  return MP_OK;
}

media_stream_t * rtp_find_stream(media_format_t * mf, int strmno)
{
  media_stream_t *cur = mf->first;

  while (cur != NULL)
  {
    if ((((rtp_stream_t*)cur)->sno) == strmno)
		break;

    cur = cur->next;
  }

  return cur;
}

media_player_t * rtp_stream_player(media_format_t * mf, int strmno)
{
  media_stream_t *cur = mf->first;

  while (cur != NULL)
  {
    if ((((rtp_stream_t*)cur)->sno) == strmno)
      return cur->player;
      
    cur = cur->next;
  }

  return NULL;
}

media_stream_t * rtp_find_mime(media_format_t * mf, const char * mime)
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

media_player_t * rtp_mime_player(media_format_t * mf, const char * mime)
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

int rtp_set_mime_player (media_format_t * mf, media_player_t * player, const char * mime)
{
   media_stream_t *cur = mf->first;

   while (cur != NULL)
   {
      if (!strcmp(cur->mime, mime))
	  {
         cur->player = player;

         return MP_OK;
      }

      cur = cur->next;
   }

   rtp_log(("rtp_set_mime_player: stream '%s' not exist\n", mime));
   
   return MP_FAIL;
}

media_stream_t * rtp_find_fourcc(media_format_t * mf, const char *fourcc)
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

media_player_t * rtp_fourcc_player(media_format_t * mf, const char *fourcc)
{
   media_stream_t *cur = mf->first;

   while (cur != NULL)
   {
      if ( strncmp(cur->fourcc, fourcc, 4) )
         return cur->player;

      cur = cur->next;
   }

   return NULL;
}

int rtp_set_fourcc_player (media_format_t * mf, media_player_t * player, const char *fourcc)
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

   rtp_log(("rtp_set_fourcc_player: stream [%c%c%c%c] not exist\n"
                  , fourcc[0], fourcc[1], fourcc[2], fourcc[3]));

   return MP_FAIL;
}

/**
 * There is no operate on file, all data from from net
 */
int rtp_open(media_format_t *mf, char * fname, media_control_t *ctrl, config_t *conf, char *mode)
{
	return MP_NOP;
}

int rtp_stream_on_member_update(void *gen, uint32 ssrc, char *cn, int cnlen)
{
   rtp_stream_t *rtpstrm = (rtp_stream_t*)gen;
   rtp_format_t *rtpfmt = rtpstrm->rtp_format;

   if(strncmp(rtpstrm->source_cname, cn, rtpstrm->source_cnlen) != 0)
   {
	   rtp_log(("rtp_stream_on_member_update: Stream Receiver[%s] discovered\n", cn));
	   return MP_OK;
   }

   rtp_log(("rtp_stream_on_member_update: source[%s] connected\n", cn));
   
   return MP_OK;
}

rtp_stream_t* rtp_open_stream(rtp_format_t *rtp_format, int sno, char *src_cn, int src_cnlen, capable_descript_t *cap, media_control_t *ctrl, char *mode)
{
	unsigned char stype;
	rtp_stream_t *strm;

	rtpcap_descript_t *rtpcap;

	rtp_setting_t *rset = NULL;

	uint8 profile_no;
	uint16 rtp_portno, rtcp_portno;

	int i, total_bw, rtp_bw;

	module_catalog_t *cata = ctrl->modules(ctrl);

	if(!cap->match_type(cap, "rtp"))
	{
		rtp_log(("rtp_open_stream: Not RTP capable\n"));
		return NULL;
	}

	strm = malloc(sizeof(rtp_stream_t));
	if(!strm)
	{
		rtp_log(("rtp_open_stream: No memory for rtp stream\n"));
		return NULL;
	}
	memset(strm, 0, sizeof(rtp_stream_t));

	rtpcap = (rtpcap_descript_t*)cap;

	if(strncmp(rtpcap->profile_mime, "audio", 5) == 0)
	{
		rtp_log(("rtp_open_stream: audio media\n"));
		stype = MP_AUDIO;
	}
	else if(strncmp(rtpcap->profile_mime, "video", 5) == 0)
	{
		rtp_log(("rtp_open_stream: video media type\n"));
		stype = MP_VIDEO;
	}
	else if(strncmp(rtpcap->profile_mime, "text", 4) == 0)
	{
		rtp_log(("rtp_open_stream: text media type\n"));
		stype = MP_TEXT;
	}
	else
	{
		rtp_log(("rtp_open_stream: Unknown media type\n"));
		free(strm);
		return NULL;
	}

	rset = (rtp_setting_t*)ctrl->fetch_setting(ctrl, "rtp", (media_device_t*)rtp_format->rtp_in);
	if(!rset)
	{
		rtp_log(("rtp_open_stream: No rtp setting\n"));
		free(strm);
		return NULL;
	}

	rtp_portno = rset->default_rtp_portno;
	rtcp_portno = rset->default_rtcp_portno;

	for(i=0; i<rset->nprofile; i++)
	{
		if(strcmp(rset->profiles[i].profile_mime, rtpcap->profile_mime) == 0)
		{
			if(rset->profiles[i].rtp_portno)
				rtp_portno = rset->profiles[i].rtp_portno;

			if(rset->profiles[i].rtcp_portno)
				rtcp_portno = rset->profiles[i].rtcp_portno;

			profile_no = rset->profiles[i].profile_no;
			total_bw = rset->profiles[i].total_bw;
			rtp_bw = rset->profiles[i].rtp_bw;

			break;
		}
	}

	strm->session = rtp_format->rtp_in->rtp_session(
										rtp_format->rtp_in, cata, ctrl,
										rset->cname, rset->cnlen,
										rset->ipaddr, rtp_portno, rtcp_portno,
										profile_no, rtpcap->profile_mime,
										total_bw, rtp_bw);

	if(!strm->session)
	{
		free(strm);
		return NULL;
	}

	strm->rtp_cap = rtp_capable_descript();
	strm->rtp_cap->ipaddr = xstr_clone(rset->ipaddr);
	strm->rtp_cap->profile_mime = xstr_clone(rtpcap->profile_mime);
	strm->rtp_cap->profile_no = profile_no;
	strm->rtp_cap->rtp_portno = rtp_portno;
	strm->rtp_cap->rtcp_portno = rtcp_portno;

	strm->rtp_format = rtp_format;
	rtp_log(("rtp_open_stream: FIXME - stream mime string overflow possible!!\n"));
	strcpy(strm->stream.mime, rtpcap->profile_mime);

	rtp_add_stream((media_format_t*)rtp_format, (media_stream_t*)strm, sno, stype);

	rtp_log(("rtp_open_stream: for [%s@%s:%u|%u]\n", src_cn, rtpcap->ipaddr, rtpcap->rtp_portno, rtpcap->rtcp_portno));
	
	session_add_cname(strm->session, src_cn, src_cnlen, rtpcap->ipaddr, rtpcap->rtp_portno, rtpcap->rtcp_portno, NULL);

	/* waiting to source to be available */
	strm->source_cname = xstr_nclone(src_cn, src_cnlen);
	strm->source_cnlen = src_cnlen;
	session_set_callback(strm->session, CALLBACK_SESSION_MEMBER_UPDATE, rtp_stream_on_member_update, strm);

	return strm;
}

capable_descript_t* rtp_stream_capable(rtp_stream_t *strm)
{
	return (capable_descript_t*)strm->rtp_cap;
}

/**
 * return how many stream opened
 */
int rtp_open_capables(media_format_t *mf, char *src_cn, int src_cnlen, capable_descript_t *caps[], int ncap, media_control_t *ctrl, config_t *conf, char *mode, capable_descript_t *supported_caps[])
{
   int i, c=0, sno=0;
   rtp_stream_t *strm;

   rtp_format_t *rtp = (rtp_format_t *)mf;

   rtp_log(("rtp_open_capables: open %d capables\n", ncap));

   if(strlen(mode) >= MAX_MODEBYTES)
   {
	   rtp_log(("rtp_open_capables: mode unacceptable\n"));
	   return MP_INVALID;
   }

   strcpy(mf->default_mode, mode);
   mf->control = ctrl;

   rtp->rtp_in = (dev_rtp_t*)ctrl->find_device(ctrl, "rtp");

   for(i=0; i<ncap; i++)
   {
	   strm = rtp_open_stream(rtp, sno++, src_cn, src_cnlen, caps[i], ctrl, mode);
	   if(strm)
		   supported_caps[c++] = rtp_stream_capable(strm);
   }

   return  c;
}

/* Return the milliseconds of the rtp media, set by sdp */
int demux_rtp_millisec (media_format_t * mf, int rescan)
{
   rtp_format_t *rtp = (rtp_format_t *)mf;

   return rtp->millisec;
}

/* Can not seek, unless talk to server with rtsp */
int rtp_seek_millisecond (media_format_t *mf, int ms)
{
   rtp_log (("rtp_seek_millisecond: unseekable to %dms\n", ms));

   return 0;
}

/************************************************************************
 * send stream granulepos data to rtp channels from preseeked position
 * return value:
 * - return positive value means interval usec to next timestamp;
 *          negetive value indicate error;
 *          zero means end of stream
 */
int rtp_demux_next (media_format_t *mf, int stream_end)
{
   return MP_NOP;
}

int rtp_close (media_format_t * mf)
{
   return MP_OK;
}

int rtp_nstream (media_format_t * mf)
{
   return mf->numstreams;
}

const char* rtp_stream_mime (media_format_t * mf, int strmno)
{
   media_stream_t * ms = rtp_find_stream(mf, strmno);
   if(!ms)  return "";

   return ms->mime;
}

const char* rtp_stream_fourcc (media_format_t * mf, int strmno)
{
   media_stream_t * ms = rtp_find_stream(mf, strmno);
   if(!ms)  return "\0\0\0\0";

   return ms->fourcc;
}

int rtp_players(media_format_t * mf, char *type, media_player_t* players[], int nmax)
{
  media_stream_t *cur = mf->first;
  int n = 0;

  while (cur != NULL)
  {
    if(strcmp(cur->player->play_type(cur->player), type) == 0)
	{
		/* playtype match */
		players[n++] = cur->player;
	}

    cur = cur->next;
  }

  return n;
}

int rtp_set_control (media_format_t * mf, media_control_t * control)
{
   mf->control = control;

   return MP_OK;
}

int rtp_set_player (media_format_t * mf, media_player_t * player)
{
   int ret;
   const char * type;

   type = player->media_type(player);
   if ( (ret = rtp_set_mime_player (mf, player, type)) >= MP_OK )
   {
      rtp_log (("rtp_set_player: '%s' stream playable\n", type));
      return ret;
   }
   
   type = player->codec_type(player);
   if ( (ret = rtp_set_mime_player (mf, player, type)) >= MP_OK )
   {
      rtp_log (("rtp_set_player: play '%s' stream\n", type));
      return ret;
   }

   rtp_debug(("rtp_set_player: can't play the media with player\n"));

   return ret;
}

module_interface_t * media_new_format()
{
   media_format_t * mf = NULL;

   rtp_format_t * rtp = malloc(sizeof(struct rtp_format_s));
   if(!rtp)
   {
      rtp_log(("rtp_new_rtp_group: No memery to allocate\n"));
      return NULL;
   }

   memset(rtp, 0, sizeof(struct rtp_format_s));

   mf = (media_format_t *)rtp;

   /* release a format */
   mf->done = rtp_done_format;

   /* Open/Close a media source */
   mf->open = rtp_open;
   mf->close = rtp_close;
   mf->open_capables = rtp_open_capables;

   /* Stream management */
   mf->add_stream = rtp_add_stream;

   mf->find_stream = rtp_find_stream;
   mf->find_mime = rtp_find_mime;
   mf->find_fourcc = rtp_find_fourcc;

   mf->set_control = rtp_set_control;
   mf->set_player = rtp_set_player;

   /* Stream info */
   mf->nstream = rtp_nstream;
   mf->stream_mime = rtp_stream_mime;
   mf->stream_fourcc = rtp_stream_fourcc;

   mf->stream_player = rtp_stream_player;
   mf->mime_player = rtp_mime_player;
   mf->fourcc_player = rtp_fourcc_player;

   mf->players = rtp_players;

   /* Seek the media by time */
   mf->seek_millisecond = rtp_seek_millisecond;

   /* demux next sync stream group */
   mf->demux_next = rtp_demux_next;

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
