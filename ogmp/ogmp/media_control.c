/***************************************************************************
                          vorbis_poster.c  -  post vorbis packet
                             -------------------
    begin                : Sun Feb 1 2004
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

 
#include "media_format.h"

#include <timedia/timer.h>
#include <timedia/xstring.h>
#include <timedia/list.h>

#define MEDIA_CONTROL_LOG
#define MEDIA_CONTROL_DEBUG

#ifdef MEDIA_CONTROL_LOG
   const int media_ctrl_log = 1;
#else
   const int media_ctrl_log = 0;
#endif             

#define cont_log(fmtargs)  do{if(media_ctrl_log) printf fmtargs;}while(0)

#ifdef MEDIA_CONTROL_DEBUG
   const int media_ctrl_debug = 1;
#else
   const int media_ctrl_debug = 0;
#endif
#define cont_debug(fmtargs)  do{if(media_ctrl_debug) printf fmtargs;}while(0)

typedef struct control_setting_item_s {

   char *name;

   control_setting_call_t *call;
   void *call_user;
  
} control_setting_item_t;

typedef struct control_impl_s {

   struct media_control_s control;
   
   media_format_t * format;
   
   char *type;

   char *name;
   
   int namelen;

   int started;
   
   xrtp_clock_t * clock;
   xrtp_hrtime_t prev_period_start;
   xrtp_hrtime_t prev_period_us;

   module_catalog_t *catalog;

   xlist_t *devices;

   xlist_t *setting_calls;
   
} control_impl_t;

int cont_done_setting (void* gen) {

   control_setting_t *setting = (control_setting_t*)gen;

   setting->done(setting);

   return MP_OK;
}

int cont_done_device(void * gen) {

   media_device_t *dev = (media_device_t *)gen;

   dev->done(dev);

   return MP_OK;
}

int cont_done (media_control_t * cont) {

   control_impl_t *impl = (control_impl_t *)cont;
   
   time_end(impl->clock);

   xlist_done(impl->devices, cont_done_device);

   xlist_done(impl->setting_calls, cont_done_setting);

   return MP_OK;
}

int cont_set_format (media_control_t * cont, char *name, media_format_t * format) {

   control_impl_t *impl = (control_impl_t *)cont;

   int len = strlen(name)+1;
   
   impl->name = malloc (len);
   if (!impl->name) {

      cont_debug(("cont_set_format: no enough memory\n"));

      return MP_EMEM;
   }
   
   impl->format = format;
   
   strcpy(impl->name, name);
   impl->namelen = len;

   format->set_control(format, cont);

   return MP_OK;
}

int cont_done_player(void * gen) {

   media_player_t *player = (media_player_t *)gen;

   player->done(player);

   return MP_OK;
}

int cont_add_device (media_control_t *cont, char *name, control_setting_call_t *call, void*user) {

   control_impl_t *impl = (control_impl_t*)cont;
   
   control_setting_item_t *item;
   media_device_t *dev;

   xlist_user_t $lu;
   xlist_t *devs = xlist_new();

   if(!impl->catalog){
   
	   cont_debug(("cont_add_device: device->config need to be call first\n"));
	   return MP_INVALID;
   }

   if (catalog_create_modules(impl->catalog, "device", devs) <= 0) {

      cont_log(("cont_add_device: no device module found\n"));

      xrtp_list_free(devs, NULL);

      return MP_FAIL;
   }

   dev = xlist_first(devs, &$lu);
   while (dev) {

      if (dev->match_type(dev, name)) {

         cont_log(("cont_add_device: found '%s' device\n", name));

         xlist_remove_item(devs, dev);

         break;
      }

      dev = xlist_next(devs, &$lu);
   }

   xlist_done(devs, cont_done_device);
   if(!dev){
   
	   cont_log(("cont_add_device: No '%s' device found.\n", name));
	   return MP_FAIL;
   }

   item = malloc(sizeof(struct control_setting_item_s));
   if(!item)
   {
      cont_log(("cont_add_device: No memory for setting\n"));
	  dev->done(dev);

      return MP_EMEM;
   }

   item->name = xstr_clone(name);
   item->call = call;
   item->call_user = user;

   xlist_addto_first(impl->setting_calls, item);
   xlist_addto_first(impl->devices, dev);

   dev->start(dev);

   return MP_OK;
}

int cont_match_device(void* t, void *p){

   media_device_t *dev = (media_device_t*)t;
   char *name = (char*)p;

   int ret = dev->match_type(dev, name);

   /* return 0 means match, see xlist.h */
   if(ret == 1) return 0;

   return -1;
}

media_device_t* cont_find_device(media_control_t *cont, char *name) {

   xlist_user_t lu;
   xlist_t *devs;

   control_impl_t *impl = (control_impl_t*)cont;

   media_device_t *dev = xlist_find(impl->devices, name, cont_match_device, &lu);
   
   if(dev) return dev;

   devs = xlist_new();

   if (catalog_create_modules(impl->catalog, "device", devs) <= 0) {

      cont_log(("cont_find_device: no device module found\n"));

      xlist_done(devs, cont_done_device);

      return NULL;
   }

   dev = xlist_first(devs, &lu);
   while (dev) {
	   
      if (dev->match_type(dev, name)) {

         cont_log(("cont_find_device: found '%s' device\n", name));

         xlist_remove_item(devs, dev);

         break;
      }

      dev = xlist_next(devs, &lu);
   }

   xlist_done(devs, cont_done_device);

   if(!dev) cont_log(("cont_find_device: can't find '%s' device\n", name));

   return dev;
}

int cont_match_call(void* t, void *p){

   control_setting_item_t *item = (control_setting_item_t*)t;
   char *name = (char*)p;

   return strcmp(name, item->name);
}

control_setting_t* cont_fetch_setting(media_control_t *cont, char *name, media_device_t *dev) {

   xlist_user_t lu;

   control_setting_t *setting = NULL;

   control_setting_item_t *item = NULL;

   control_impl_t *impl = (control_impl_t*)cont;

   if(!dev->new_setting){

      cont_log(("cont_fetch_setting: No need to set device\n"));
      
      return NULL;
   }
   
   item = xlist_find(impl->setting_calls, name, cont_match_call, &lu);
   
   if(item) {
   
      setting = dev->new_setting(dev);
      
      if(!setting) return NULL;
      
      if(item->call(item->call_user, setting) >= MP_OK) return setting;

      cont_log(("cont_fetch_setting: no device setting\n"));

      setting->done(setting);
   }

   return NULL;
}

media_player_t * cont_find_player (media_control_t *cont, char *mime, char *fourcc) {

   media_player_t * player = NULL;
   
   xrtp_list_user_t $lu;   
   xrtp_list_t * players = xrtp_list_new();

   int num = 0;
   
   control_impl_t *impl = (control_impl_t *)cont;
   
   cont_log(("cont_find_player: need '%s' module \n", impl->type));

   num = catalog_create_modules(impl->catalog, impl->type, players);
   if (num <= 0) {

      cont_log(("cont_find_player: no '%s' module \n", impl->type));

      xrtp_list_free(players, NULL);
      return NULL;
   }

   player = xrtp_list_first(players, &$lu);
   while (player) {

      if(player->match_type(player, mime, fourcc)){
      
         cont_log(("cont_find_player: found player(mime:%s, fourcc:%s)\n", mime, fourcc));

         xrtp_list_remove_item(players, player);
         break;
      }
      
      player = xrtp_list_next(players, &$lu);
   }

   xrtp_list_free(players, cont_done_player);

   if(!player) return NULL;

   player->set_device(player, cont, impl->catalog);

   return player; 
}

int cont_seek_millisec (media_control_t * cont, int msec) {

   control_impl_t *impl = (control_impl_t *)cont;

   impl->started = 0;
   
   return impl->format->seek_millisecond(impl->format, msec);
}            

int cont_demux_next (media_control_t * cont, int strm_end) {

   rtime_t period_us = 0, real_period = 0, sleep_us = 0;
   rtime_t demux_start, demux_us;

   control_impl_t *impl = (control_impl_t *)cont;

   demux_start = time_usec_now(impl->clock);
   if(impl->started)
   {
	   real_period = time_usec_spent(impl->clock, impl->prev_period_start);
	   impl->prev_period_start = demux_start;
   }

   period_us = impl->format->demux_next(impl->format, strm_end);

   if(period_us <= 0) return period_us; /* errno < 0 */
   
   cont_log(("cont_demux_next: %dus period, real period %dus\n", period_us, real_period));
   
   demux_us = time_usec_spent(impl->clock, demux_start);
   
   sleep_us = period_us - demux_us;

   time_usec_sleep(impl->clock, sleep_us, NULL);
   
   impl->started = 1;

   return MP_OK;
}

int cont_config (media_control_t *cont, char *type, config_t *conf, module_catalog_t *cata) {

   control_impl_t *impl = (control_impl_t *)cont;

   impl->type = xstr_clone(type);

   impl->catalog = cata;
   
   return MP_OK;
}

module_interface_t* new_media_control () {

   media_control_t * cont;

   control_impl_t *impl = malloc (sizeof(struct control_impl_s));
   if(!impl) {

      cont_debug (("media_new_control: No memory to allocate\n"));
      return NULL;
   }

   memset(impl, 0, sizeof(struct control_impl_s));

   impl->clock = time_start();

   impl->devices = xlist_new();
   if(!impl->devices){

      cont_debug (("media_new_control: No memory for devices\n"));
      free(impl);
      
      return NULL;
   }

   impl->setting_calls = xlist_new();
   if(!impl->setting_calls){

      cont_debug (("media_new_control: No memory for setting list\n"));
	  
	  xlist_done(impl->devices, cont_done_device);
      free(impl);
      
      return NULL;
   }

   cont = (media_control_t *)impl;

   cont->done = cont_done;
   
   cont->add_device = cont_add_device;
   cont->find_device = cont_find_device;

   cont->find_player = cont_find_player;

   cont->fetch_setting = cont_fetch_setting;
   
   cont->set_format = cont_set_format;
   cont->seek_millisec = cont_seek_millisec;
   cont->demux_next = cont_demux_next;

   cont->config = cont_config;

   return cont;
}

