/***************************************************************************
                          vorbis_rtp.c  -  Vorbis rtp stream
                             -------------------
    begin                : Mon Jan 19 2004
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
 
#include <timedia/xstring.h>

#include "dev_rtp.h"

#define RTP_LOG
#define RTP_DEBUG

#ifdef RTP_LOG
   const int rtp_log = 1;
#else
   const int rtp_log = 0;
#endif

#define rtp_log(fmtargs)  do{if(rtp_log) printf fmtargs;}while(0)

#ifdef RTP_DEBUG
   const int rtp_debug = 1;
#else
   const int rtp_debug = 0;
#endif
#define rtp_debug(fmtargs)  do{if(rtp_debug) printf fmtargs;}while(0)

int rtp_done_setting(control_setting_t *gen){

   free(gen);

   return MP_OK;
}

control_setting_t* rtp_new_setting(media_device_t *dev){

   rtp_setting_t * set = malloc(sizeof(struct rtp_setting_s));
   if(!set){

      rtp_debug(("rtp_new_setting: No memory"));

      return NULL;
   }

   memset(set, 0, sizeof(struct rtp_setting_s));

   set->setting.done = rtp_done_setting;

   return (control_setting_t*)set;
}

typedef struct dev_rtp_s dev_rtp_t;
struct dev_rtp_s{

   struct media_device_s dev;

   char *cname;
   int cnlen;

   xrtp_session_t *session;
   xrtp_media_t *media;

   rtp_pipe_t * pipe;
};

media_frame_t* rtp_new_frame (media_pipe_t *pipe, int bytes, char *init_data) {

   /* better recycle to garrantee */
   rtp_frame_t * rtpf = (rtp_frame_t*)malloc(sizeof(struct rtp_frame_s));
   if(!rtpf){

      rtp_debug(("rtp_new_frame: No memory for frame\n"));
      return NULL;
   }   
   memset(rtpf, 0, sizeof(*rtpf));

   rtpf->media_unit = (char*)malloc(bytes);
   if(rtpf->media_unit == NULL){
		
	   free(rtpf);
	   rtp_debug(("rtp_new_frame: No memory for data"));
	   
	   return NULL;
   }

   rtpf->unit_bytes = bytes;

   /* initialize data */
   if(init_data != NULL){

		//rtp_log(("rtp_new_frame: initialize %d bytes data\n", bytes));
		memcpy(rtpf->media_unit, init_data, bytes);
   }

   return (media_frame_t*)rtpf;
}

int rtp_recycle_frame (media_pipe_t *pipe, media_frame_t *f) {

   rtp_frame_t * rtpf = (rtp_frame_t*)f;

   if(rtpf->media_unit) free(rtpf->media_unit);
   free(rtpf);

   //rtp_debug(("rtp_recycle_frame: OK\n"));

   return MP_OK;
}

int rtp_put_frame (media_pipe_t *pipe, media_frame_t *frame, int last) {

   xrtp_media_t *rtp_media = ((rtp_pipe_t*)pipe)->rtp_media;
   rtp_frame_t *rtpf = (rtp_frame_t*)frame;

   return rtp_media->post(rtp_media, frame, rtpf->unit_bytes, last);
}

/*
 * return 0: stream continues
 * return 1: stream ends
 */
int rtp_pick_content (media_pipe_t *pipe, media_info_t *media_info, char* raw, int nraw_once) {

   rtp_log(("rtp_pick_frame: No operation\n"));

   return MP_EUNSUP;
}

int rtp_pipe_done (media_pipe_t *pipe){

   free(pipe);

   return MP_OK;
}

rtp_pipe_t * rtp_pipe_new() {

   media_pipe_t *mp = NULL;

   rtp_pipe_t *rp = (rtp_pipe_t*)malloc(sizeof(struct rtp_pipe_s));
   if(!rp){

      rtp_debug(("rtp_pipe_new: No memory to allocate\n"));
      return NULL;
   }
   memset(rp, 0, sizeof(struct rtp_pipe_s));

   mp = (media_pipe_t *)rp;

   mp->new_frame = rtp_new_frame;
   mp->recycle_frame = rtp_recycle_frame;
   mp->put_frame = rtp_put_frame;
   mp->pick_content = rtp_pick_content;
   mp->done = rtp_pipe_done;

   return rp;
}

int rtp_set_callbacks (xrtp_session_t *ses, rtp_callback_t *cbs, int ncbs) {

   int i;
   for (i=0; i<ncbs; i++) {

      session_set_callback (ses, cbs[i].type, cbs[i].callback, cbs[i].callback_user);
   }

   return MP_OK;
}

int rtp_setting (media_device_t *dev, control_setting_t *setting, module_catalog_t *cata) {

   xrtp_port_t * rtp_port = NULL;
   xrtp_port_t * rtcp_port = NULL;

   dev_rtp_t* rtp = (dev_rtp_t*)dev;
   rtp_setting_t* rtpset = (rtp_setting_t*)setting;
   
   rtp->cname = xstr_clone(rtpset->cname);
   rtp->cnlen = rtpset->cnlen;

   rtp_port = port_new(rtpset->ipaddr, rtpset->rtp_portno, RTP_PORT);
   rtcp_port = port_new(rtpset->ipaddr, rtpset->rtcp_portno, RTCP_PORT);
   
   rtp->session = session_new(rtp_port, rtcp_port, rtpset->cname, rtpset->cnlen, cata);

   if(!rtp->session) return MP_FAIL;

   session_set_bandwidth(rtp->session, rtpset->total_bw, rtpset->rtp_bw);

   rtp_set_callbacks(rtp->session, rtpset->callbacks, rtpset->ncallback);
   
   /* set xrtp handler */
   if (session_add_handler(rtp->session, rtpset->profile_mime) == NULL) {

      rtp_debug(("vrtp_new: Fail to set vorbis profile !\n"));
      session_done(rtp->session);
      rtp->session = NULL;

      port_done(rtp_port);
      port_done(rtcp_port);
      
      return MP_FAIL;
   }

   rtp->media = session_new_media(rtp->session, rtpset->profile_mime, rtpset->profile_no);   

   rtp->pipe = rtp_pipe_new();

   rtp->pipe->rtp_media = rtp->media;

   return MP_OK;
}

media_pipe_t * rtp_pipe (media_device_t *dev) {
  
   dev_rtp_t *rtp = (dev_rtp_t *)dev;

   return (media_pipe_t*)rtp->pipe;
}

int rtp_start (media_device_t * dev, void * info, int usec_min, int usec_max) {

   dev_rtp_t *rtp = (dev_rtp_t*)dev;
   media_info_t *mi = (media_info_t*)info;
   
   rtp->media->set_rate(rtp->media, mi->sample_rate);
   
   session_set_scheduler (rtp->session, xrtp_scheduler());

   return MP_OK;
}

int rtp_stop (media_device_t * dev) {

   session_set_scheduler (((dev_rtp_t *)dev)->session, NULL);

   return MP_OK;
}

int rtp_match_type(media_device_t *dev, char *type) {

   rtp_log(("rtp.rtp_match_type: I am rtp device\n"));

   if( !strcmp("rtp", type) ) return 1;

   return 0;
}

int rtp_done (media_device_t *dev) {

   dev_rtp_t *rtp = (dev_rtp_t *)dev;

   if(!dev){

      rtp_debug(("rtp.rtp_done: NULL dev\n"));
      return MP_OK;
   }

   if(rtp->session) session_done(rtp->session);

   if(rtp->pipe) rtp_pipe_done((media_pipe_t*)rtp->pipe);

   if(rtp->cname) xstr_done_string(rtp->cname);

   free(rtp);

   return MP_OK;
}

module_interface_t * rtp_new () {

   media_device_t * dev;
   dev_rtp_t *vrtp;

   /* Realised the xrtp lib */
   if(xrtp_init() < XRTP_OK) return NULL;
   
   vrtp = (dev_rtp_t*)malloc(sizeof(struct dev_rtp_s));
   if (!vrtp) {

      rtp_debug(("vrtp_new: No free memory\n"));
      return NULL;
   }

   memset (vrtp, 0, sizeof(struct dev_rtp_s));

   dev = (media_device_t*)vrtp;

   dev->pipe = rtp_pipe;
   dev->start = rtp_start;
   dev->stop = rtp_stop;
   dev->new_setting = rtp_new_setting;
   dev->setting = rtp_setting;
   dev->match_type = rtp_match_type;
   dev->done = rtp_done;

   return dev;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC
module_loadin_t mediaformat = {

   "device",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   rtp_new   /* Module initializer */
};

