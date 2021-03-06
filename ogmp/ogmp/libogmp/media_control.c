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
#include <timedia/xmalloc.h>
#include <timedia/xstring.h>
#include <timedia/xthread.h>
#include <timedia/list.h>
#include <timedia/ui.h>

#define MEDIA_CONTROL_LOG
#define MEDIA_CONTROL_DEBUG

#ifdef MEDIA_CONTROL_LOG
 #define cont_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define cont_log(fmtargs)
#endif             

#ifdef MEDIA_CONTROL_DEBUG
 #define cont_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define cont_debug(fmtargs)
#endif

typedef struct control_setting_item_s
{
   char *name;

   control_setting_call_t *call;
   void *call_user;
  
} control_setting_item_t;

typedef struct control_impl_s
{
   struct media_control_s control;
   
   char *name;
   
   int namelen;

   int demuxing;
   
   xclock_t * clock;

   rtime_t catchup_us;
   rtime_t sleep_us;

   rtime_t period_start;
   rtime_t period_us;

   rtime_t prev_period_start;

   rtime_t prev_period_us;

   module_catalog_t *catalog;

   xlist_t *devices;

   xlist_t *setting_calls;

   int bandwidth;
   int bandwidth_budget;
   xthr_lock_t* bandwidth_lock;
   
} control_impl_t;

int cont_done_setting (void* gen)
{
   control_setting_t *setting = (control_setting_t*)gen;

   setting->done(setting);

   return MP_OK;
}

int cont_done_device(void * gen)
{
   media_device_t *dev = (media_device_t *)gen;

   dev->done(dev);

   return MP_OK;
}

int cont_done (media_control_t * cont)
{
   control_impl_t *impl = (control_impl_t *)cont;
   
   time_end(impl->clock);

   xlist_done(impl->devices, cont_done_device);

   xlist_done(impl->setting_calls, cont_done_setting);

   xthr_done_lock(impl->bandwidth_lock);

   return MP_OK;
}

#if 0
int cont_set_format (media_control_t * cont, char *name, media_format_t * format)
{
   control_impl_t *impl = (control_impl_t *)cont;

   int len = strlen(name)+1;
   
   impl->name = xmalloc (len);
   if (!impl->name)
   {
      cont_debug(("cont_set_format: no enough memory\n"));

      return MP_EMEM;
   }
   
   impl->format = format;
   
   strcpy(impl->name, name);
   impl->namelen = len;

   format->set_control(format, cont);

   return MP_OK;
}
#endif

int cont_done_player(void * gen)
{
   media_player_t *player = (media_player_t *)gen;

   player->done(player);

   return MP_OK;
}

int cont_done_creater(void * gen)
{
   media_maker_t *creater = (media_maker_t *)gen;

   creater->done(creater);

   return MP_OK;
}

int cont_add_device (media_control_t *cont, char *name, control_setting_call_t *call, void*user)
{
   control_impl_t *impl = (control_impl_t*)cont;
   
   control_setting_item_t *item;
   media_device_t *dev;

   xlist_user_t $lu;
   xlist_t *devs = xlist_new();

   if(!impl->catalog)
   {
	   cont_debug(("cont_add_device: device->config need to be call first\n"));
	   return MP_INVALID;
   }

   if (catalog_create_modules(impl->catalog, "device", devs) <= 0)
   {
	   cont_log(("cont_add_device: no device module found\n"));

	   xrtp_list_free(devs, NULL);

	   return MP_FAIL;
   }

   dev = xlist_first(devs, &$lu);
   while (dev)
   {
      if (dev->match_type(dev, name))
	   {
         cont_log(("cont_add_device: found '%s' device\n", name));

         xlist_remove_item(devs, dev);

         break;
      }

      dev = xlist_next(devs, &$lu);
   }

   xlist_done(devs, cont_done_device);
   if(!dev)
   {
	   cont_log(("cont_add_device: No '%s' device found.\n", name));
	   return MP_FAIL;
   }

   item = xmalloc(sizeof(struct control_setting_item_s));
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
   
   dev->init(dev, cont);
   
   return MP_OK;
}         

int cont_match_device(void* t, void *p)
{
   media_device_t *dev = (media_device_t*)t;
   char *name = (char*)p;

   int ret = dev->match_type(dev, name);

   /* return 0 means match, see xlist.h */
   if(ret == 1) return 0;

   return -1;
}

media_device_t* cont_find_device(media_control_t *cont, char *mediatype, char* mode)
{
   xlist_user_t lu;
   xlist_t *devs;
   
   control_impl_t *impl = (control_impl_t*)cont;

   /* give back the cached version */
   media_device_t *dev = xlist_find(impl->devices, mediatype, cont_match_device, &lu);
   
   if(dev)
      return dev;

   devs = xlist_new();

   if (catalog_create_modules(impl->catalog, "device", devs) <= 0)
   {
      cont_log(("cont_find_device: no device module found\n"));

      xlist_done(devs, cont_done_device);

      return NULL;
   }

   dev = xlist_first(devs, &lu);
   while (dev)
   {
      if (dev->match_type(dev, mediatype) && dev->match_mode(dev, mode))
	   {
         cont_log(("cont_find_device: found '%s/%s' device\n", mediatype, mode));

         xlist_remove_item(devs, dev);

         break;
      }

      dev = xlist_next(devs, &lu);
   }

   xlist_done(devs, cont_done_device);

   if(!dev) 
	   cont_log(("cont_find_device: can't find '%s' device\n", mediatype));
      
   /* cache the device */
   xlist_addto_first(impl->devices, dev);

   return dev;
}

int cont_match_call(void* t, void *p)
{
   control_setting_item_t *item = (control_setting_item_t*)t;
   char *name = (char*)p;

   return strcmp(name, item->name);
}

control_setting_t* cont_fetch_setting(media_control_t *cont, char *name, media_device_t *dev)
{
   xlist_user_t lu;

   control_setting_t *setting = NULL;

   control_setting_item_t *item = NULL;

   control_impl_t *impl = (control_impl_t*)cont;

   if(!dev->new_setting)
   {
      cont_log(("cont_fetch_setting: No need to set device\n"));
      return NULL;
   }
   
   item = xlist_find(impl->setting_calls, name, cont_match_call, &lu);   
   if(item)
   {
      setting = dev->new_setting(dev);
      
      if(!setting) 
		  return NULL;
      
      if(item->call(item->call_user, setting) >= MP_OK) 
		  return setting;

      cont_log(("cont_fetch_setting: no device setting\n"));
      setting->done(setting);
   }

   return NULL;
}

media_player_t * cont_find_player (media_control_t *cont, char *mode, char *mime, char *fourcc)
{
   media_player_t * player = NULL;
   
   xrtp_list_user_t $lu;   
   xrtp_list_t * players = xrtp_list_new();

   int num = 0;
   
   control_impl_t *impl = (control_impl_t *)cont;
   
   cont_log(("cont_find_player: need '%s' module \n", mode));
   
   num = catalog_create_modules(impl->catalog, mode, players);
   if (num <= 0)
   {
      cont_log(("cont_find_player: no '%s' module \n", mode));

      xrtp_list_free(players, NULL);
      return NULL;
   }

   player = xlist_first(players, &$lu);
   while (player)
   {
      if(player->receiver.match_type(&player->receiver, mime, fourcc))
	  {
         cont_log(("cont_find_player: found player(mime:%s, fourcc:%s)\n", mime, fourcc));

         xrtp_list_remove_item(players, player);
         break;
      }
      
      player = xrtp_list_next(players, &$lu);
   }

   xrtp_list_free(players, cont_done_player);

   if(!player)
	   return NULL;

   return player; 
}

media_player_t * cont_new_player (media_control_t *cont, char *mode, char *mime, char *fourcc, void* data)
{
   media_player_t * player = NULL;
   
   xrtp_list_user_t $lu;   
   xrtp_list_t * players = xrtp_list_new();

   int num = 0;
   
   control_impl_t *impl = (control_impl_t *)cont;
   
   cont_log(("cont_find_player: need '%s' module \n", mode));

   num = catalog_create_modules(impl->catalog, mode, players);
   if (num <= 0)
   {
      cont_log(("cont_find_player: no '%s' module \n", mode));

      xrtp_list_free(players, NULL);
      return NULL;
   }

   player = xlist_first(players, &$lu);
   while (player)
   {
      if(player->receiver.match_type(&player->receiver, mime, fourcc))
	  {
         cont_log(("cont_find_player: found player(mime:%s, fourcc:%s)\n", mime, fourcc));

         xrtp_list_remove_item(players, player);
         break;
      }
      
      player = xrtp_list_next(players, &$lu);
   }

   xrtp_list_free(players, cont_done_player);

   if(!player)
	   return NULL;

   if(player->init(player, cont, data) < MP_OK)
   {
	   player->done(player);
	   return NULL;
   }

   return player; 
}


media_maker_t* cont_find_creater(media_control_t *cont, char *mime, media_info_t* minfo)
{
	media_maker_t* maker;
	xrtp_list_user_t $lu; 
	
	xrtp_list_t * makers = xrtp_list_new();

	int num = 0;
	int found = 0;
   
	control_impl_t *impl = (control_impl_t *)cont;
   
	num = catalog_create_modules(impl->catalog, "create", makers);
	if (num <= 0)
	{
		cont_log(("cont_find_player: no 'create' module\n"));

		xrtp_list_free(makers, NULL);
		return NULL;
	}

	maker = xlist_first(makers, &$lu);
	while (maker)
	{
		if(maker->receiver.match_type(&maker->receiver, mime, ""))
		{
			cont_log(("cont_find_creater: found creater(mime:%s)\n", mime));

			xrtp_list_remove_item(makers, maker);
			found = 1;

			break;
		}
      
		maker = xrtp_list_next(makers, &$lu);
	}

	xrtp_list_free(makers, cont_done_creater);

	if(!found)
		return NULL;

	return maker;
}

#if 0
int cont_seek_millisec (media_control_t * cont, int msec)
{
   control_impl_t *impl = (control_impl_t *)cont;

   impl->demuxing = 0;
   
   return impl->format->seek_millisecond(impl->format, msec);
}
#endif

int cont_demux_next (media_control_t * cont, media_format_t *format, int strm_end)
{
	rtime_t demux_us;
	control_impl_t *impl;

	impl = (control_impl_t *)cont;
   
   if(impl->demuxing != 0)
	{
	   cont_debug(("cont_demux_next: %dus period, last sleep %dus(need catchup %dus)\n", impl->period_us, impl->sleep_us, impl->catchup_us));

	   demux_us = time_usec_spent(impl->clock, impl->period_start);

	   impl->sleep_us = impl->period_us - demux_us - impl->catchup_us;
	   
	   impl->catchup_us = 0;

	   if(impl->sleep_us < 0)
		   impl->catchup_us = -impl->sleep_us;

	   time_usec_sleep(impl->clock, impl->sleep_us, NULL);
	}

	impl->period_start = time_usec_now(impl->clock);
	impl->period_us = format->demux_next(format, strm_end);

	if(impl->period_us <= 0)
		return impl->period_us; /* errno < 0 */
   
	impl->demuxing = 1;

	return MP_OK;
}

int cont_config (media_control_t *cont, config_t *conf, module_catalog_t *cata)
{
	control_impl_t *impl = (control_impl_t *)cont;

	impl->catalog = cata;

	return MP_OK;
}

module_catalog_t* cont_modules(media_control_t *cont)
{
	return ((control_impl_t*)cont)->catalog;
}

int cont_book_bandwidth(media_control_t * cont, int bw)
{
	control_impl_t *impl = (control_impl_t *)cont;

	xthr_lock(impl->bandwidth_lock);
	if(impl->bandwidth_budget < bw)
	{
		xthr_unlock(impl->bandwidth_lock);
		return -1;
	}

	xthr_unlock(impl->bandwidth_lock);
	return impl->bandwidth -= bw;
}

int cont_release_bandwidth(media_control_t * cont, int bw)
{
	control_impl_t *impl = (control_impl_t *)cont;

	if(bw < 0) bw = 0;

	xthr_lock(impl->bandwidth_lock);
	if(impl->bandwidth + bw > impl->bandwidth_budget)
	{
		xthr_unlock(impl->bandwidth_lock);
		return impl->bandwidth = impl->bandwidth_budget;
	}

	xthr_unlock(impl->bandwidth_lock);
	return impl->bandwidth += bw;
}

int cont_set_bandwidth_budget(media_control_t * cont, int budget)
{
	control_impl_t *impl = (control_impl_t *)cont;

	if(budget < 0) budget = 0;
	
	xthr_lock(impl->bandwidth_lock);

	impl->bandwidth = impl->bandwidth_budget = budget;

	xthr_unlock(impl->bandwidth_lock);

	return impl->bandwidth;
}

int cont_reset_bandwidth(media_control_t * cont)
{
	control_impl_t *impl = (control_impl_t *)cont;

	xthr_lock(impl->bandwidth_lock);

	impl->bandwidth = impl->bandwidth_budget;

	xthr_unlock(impl->bandwidth_lock);

	return impl->bandwidth;
}

xclock_t* cont_clock(media_control_t * cont)
{
	control_impl_t *impl = (control_impl_t *)cont;

   return impl->clock;
}

module_interface_t* new_media_control()
{
   media_control_t * cont;

   control_impl_t *impl = xmalloc (sizeof(struct control_impl_s));
   if(!impl)
   {
      cont_debug (("media_new_control: No memory to allocate\n"));
      return NULL;
   }

   memset(impl, 0, sizeof(struct control_impl_s));

   impl->clock = time_start();

   impl->devices = xlist_new();
   if(!impl->devices)
   {
      cont_debug (("media_new_control: No memory for devices\n"));
      xfree(impl);
      
      return NULL;
   }

   impl->setting_calls = xlist_new();
   
   if(!impl->setting_calls)
   {
      cont_debug (("media_new_control: No memory for setting list\n"));
	  
	  xlist_done(impl->devices, cont_done_device);
      xfree(impl);
      
      return NULL;
   }

   impl->bandwidth_lock = xthr_new_lock();

   cont = (media_control_t *)impl;

   cont->done = cont_done;
   
   cont->add_device = cont_add_device;
   cont->find_device = cont_find_device;

   cont->find_creater = cont_find_creater;

   cont->find_player = cont_find_player;
   cont->new_player = cont_new_player;

   cont->fetch_setting = cont_fetch_setting;
   
   //cont->set_format = cont_set_format;
   //cont->seek_millisec = cont_seek_millisec;
   cont->demux_next = cont_demux_next;
   
   cont->config = cont_config;
   cont->modules = cont_modules;

   cont->set_bandwidth_budget = cont_set_bandwidth_budget;
   cont->book_bandwidth = cont_book_bandwidth;
   cont->release_bandwidth = cont_release_bandwidth;
   cont->reset_bandwidth = cont_reset_bandwidth;
   cont->clock = cont_clock;

   return cont;
}
