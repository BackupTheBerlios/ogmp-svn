/***************************************************************************
                          config.c  -  Configure module
                             -------------------
    begin                : Ò»  5ÔÂ 10 2004
    copyright            : (C) 2004 by Heming
    email                : heming@bigfoot.com; heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "config.h"
#include "list.h"

#include "xmalloc.h"

#include <stdio.h>
#include <string.h>

#define CONFIG_LOG
#define CONFIG_DEBUG

#ifdef CONFIG_LOG
   const int config_log = 1;
#else
   const int config_log = 0;
#endif
#define config_log(fmtargs)  do{if(config_log) printf fmtargs;}while(0)

#ifdef CONFIG_DEBUG
   const int config_debug = 1;
#else
   const int config_debug = 0;
#endif
#define config_debug(fmtargs)  do{if(config_debug) printf fmtargs;}while(0)

struct config_item_s {

   int klen;
   char * key;

   int vlen;
   char * value;
};

struct config_s {

   int nitem;
   xrtp_list_t * items;

   FILE * file;
};

int conf_parse (config_t *conf, FILE * file) {

   return OS_OK;
}

config_t * conf_new (char *fname) {

   config_t * conf = (config_t*)xmalloc(sizeof(struct config_s));
   if (!conf) {

      config_debug(("config_new: No memory\n"));
      return NULL;
   }

   memset(conf, 0, sizeof(struct config_s));

   conf->items = xrtp_list_new();

   conf->file = fopen(fname, "rw");

   conf_parse(conf, conf->file);

   return conf;
}

int conf_done(config_t *conf)
{
   xfree(conf);
   return OS_OK;
}

int conf_save(config_t *conf, char * fname);

int conf_set(config_t *conf, char *key, int klen, char *value, int vlen);

int conf_get(config_t *conf, char *key, int klen, char *buf, int blen);
