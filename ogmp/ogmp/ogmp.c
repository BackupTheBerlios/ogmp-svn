/***************************************************************************
                          ogmplayer.c  -  Play Ogg Media
                             -------------------
    begin                : Wed Jan 28 2004
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

#include "ogmp.h"

#define VERSION 1

#ifndef MODDIR
 #define MODDIR "."
#endif

#define PLAYER_LOG

#ifdef PLAYER_LOG
   const int player_log = 1;
#else
   const int player_log = 0;
#endif

#define player_log(fmtargs)  do{if(player_log) printf fmtargs;}while(0)

typedef struct audio_server_s audio_server_t;
struct audio_server_s {

   int finish;
   int valid;

   media_control_t *control;

   xrtp_thread_t *demuxer;

   int demuxing;

   xthr_lock_t *lock;
   xthr_cond_t *wait_request;
};

int sender_start (void * user) {

   audio_server_t * sender = (audio_server_t *)user;

   if (!sender->valid) {

      player_log (("sender_loop: sender is not available\n"));
      return MP_FAIL;
   }
   
   if (!sender->demuxing) {

      sender->demuxing = 1;
      xthr_cond_signal(sender->wait_request);
   }
   
   return MP_OK;
}

int sender_stop (void * user) {

   int ret;
   
   audio_server_t * sender = (audio_server_t *)user;

   player_log (("sender_stop: to stop\n"));

   if (!sender->valid) return MP_OK;
   if (!sender->demuxing) return MP_OK;

   {/*lock*/ xthr_lock(sender->lock);}

   sender->demuxing = 0;
   sender->finish = 1;

   {/*unlock*/ xthr_unlock(sender->lock);}
   
   xthr_wait(sender->demuxer, &ret);

   return MP_OK;
}

int sender_loop(void * gen){

   audio_server_t * sender = (audio_server_t *)gen;

   xrtp_hrtime_t itv;
   
   sender->finish = 0;
   while (1) {

      {/*lock*/ xthr_lock(sender->lock);}
      
      if (!sender->demuxing) {

         player_log (("\nogmplyer: waiting for demux request\n"));
         xthr_cond_wait(sender->wait_request, sender->lock);
         sender->demuxing = 1;
         player_log (("\nogmplyer: start demux\n"));
      }

      if (sender->finish) {

         player_log (("\nogmplyer: Last demux and quit\n"));
         sender->control->demux_next(sender->control, 1);
         
         break;
      }

      itv = sender->control->demux_next(sender->control, 0);
      if( itv < 0 && itv == MP_EOF) {

         player_log(("(sender_loop: stop demux)\n"));
         break;
      }
      
      {/*unlock*/ xthr_unlock(sender->lock);}
   }
   
   {/*unlock*/ xthr_unlock(sender->lock);}

   player_log (("\n(ogmplyer: sender quit)\n"));
   
   return MP_OK;
}

typedef struct audio_client_s audio_client_t;
struct audio_client_s{

   int finish;

   audio_server_t * server;

   xrtp_clock_t * clock;

   media_player_t * player;

   int playing;
};

int done_format_handler(void *gen){

   media_format_t * fmt = (media_format_t *)gen;

   fmt->done(fmt);

   return MP_OK;
}

int main(int argc, char** argv){

   char * fname = argv[1];
   
   /* define a player */
   media_player_t *playa = NULL;
   media_format_t *format = NULL;
   xrtp_clock_t * clock_main = NULL;
   xrtp_lrtime_t remain = 0;
   
   audio_server_t sender;
   config_t * conf;

   xrtp_list_user_t $lu;
   xrtp_list_t *format_handlers = xrtp_list_new();

   int format_ok = 0;

   int num;
   
   module_catalog_t * mod_cata = NULL;
   
   player_log (("\nogmplyer: play '%s', with modules in dir:%s\n", fname, MODDIR));
   
   /* Initialise */
   conf = conf_new ( "ogmprc" );
   
   mod_cata = catalog_new( "mediaformat" );
   num = catalog_scan_modules ( mod_cata, VERSION, MODDIR );
   
   num = catalog_create_modules ( mod_cata, "format", format_handlers);
   player_log (("\nogmplyer: %d format module found\n", num));

   /* set audio server */
   sender.valid = 0;
   
   sender.lock = xthr_new_lock();
   sender.wait_request = xthr_new_cond(XTHREAD_NONEFLAGS);

   /* create a media format */
   format = (media_format_t *) xrtp_list_first (format_handlers, &$lu);
   while (format) {

      /* open media source */
      if (format->open(format, fname, mod_cata, conf) >= MP_OK) {
   
         format_ok = 1;
         
         break;
      }
      
      format = (media_format_t *) xrtp_list_next (format_handlers, &$lu);
   }

   /* open media source */
   if (!format_ok) {

      player_log (("ogmplyer: no format can open '%s'\n", fname));

      xthr_done_cond(sender.wait_request);
      xthr_done_lock(sender.lock);

      return -1;
   }
   
   /* player controler */
   sender.control = (media_control_t*)new_media_control();
   
   sender.control->config(sender.control, "playback", conf, mod_cata);

   sender.control->put_configer(sender.control, "rtp", ogmp_config_rtp, conf);
   
   sender.control->set_format (sender.control, "av", format);

   sender.valid = 1;

   /* now play */
   sender.demuxer = xthr_new(sender_loop, &sender, XTHREAD_NONEFLAGS);
   
   sender_start (&sender);

   clock_main = time_start();
   
   /* play 300 seconds audio */
   time_msec_sleep (clock_main, 300000, &remain);
   
   sender_stop (&sender);
   
   playa->stop (playa);
   
   format->close(format);
   
   xrtp_list_free(format_handlers, done_format_handler);
   
   xthr_done_cond(sender.wait_request);
   xthr_done_lock(sender.lock);

   return 0;
}
