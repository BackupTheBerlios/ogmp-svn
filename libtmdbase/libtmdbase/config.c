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

struct config_item_s
{
   int klen;
   char * key;

   int vlen;
   char * value;
};

struct config_s
{
   int nitem;
   xrtp_list_t * items;

   FILE * file;
   
	char buf[256];

	char *pc;
	int nc;
};

char* conf_next_nchar (config_t* conf, FILE* f, int n)
{
	if(n >= 256)
		return NULL;

	if(conf->pc+n >= conf->buf + conf->nc)
	{
		if(feof(f))
			return NULL;

		conf->nc = fread(conf->buf, 1, 256, f);

		conf->pc = conf->pc + n - conf->nc;
	}

	conf->pc += n;

	return conf->pc;
}

char* conf_next_line (config_t* conf, FILE* f)
{
	while(*conf->pc != '\n')
	{
		if(conf->pc >= conf->buf + conf->nc)
		{
			if(feof(f))
				return NULL;

			conf->nc = fread(conf->buf, 1, 256, f);
			conf->pc = conf->buf;
		}
		else
			conf->pc++;
	}

	return conf->pc;
}

int conf_next_token (config_t* conf, FILE* f, char **start, char **end)
{
	while(*conf->pc == '\t' || *conf->pc == '\n' || *conf->pc == ' ')
	{
		if(conf->pc >= conf->buf + conf->nc)
		{
			if(feof(f))
				goto e;

			conf->nc = fread(conf->buf, 1, 256, f);
			conf->pc = conf->buf;
		}
		else
			conf->pc++;
	}
	*start = conf->pc;

	while(*conf->pc != '\t' || *conf->pc != '\n' || *conf->pc != ' ')
	{
		if(conf->pc >= conf->buf + conf->nc)
		{
			int d = *start - conf->buf;

			if(d==0)
				goto e;

			memmove(conf->buf, *start, conf->nc-(*start-conf->buf));
			*start = conf->buf;

			if(feof(f))
				goto e;

			conf->nc = fread(conf->buf+conf->nc-d, 1, d, f);
			conf->pc -= d;
		}
		else
			conf->pc++;
	}

	*end = conf->pc;

	return OS_OK;

e:
	*start = *end = NULL;
	return OS_FAIL;
}

int sipua_read_nchar (config_t* conf, FILE* f, char *dest, int n)
{
	int nr = conf->pc - conf->buf;
	int c=0;

	if(conf->nc - nr > n)
	{
		strncpy(dest, conf->pc, n);
		conf->pc += n;
		return n;
	}

	while(nr < conf->nc && n>0)
	{
		strncpy(dest, conf->buf, conf->nc - nr);

		n -= conf->nc - nr;
		c += conf->nc - nr;

		if(feof(f))
			return c;

		conf->nc = fread(conf->buf, 1, 256, f);
		conf->pc = conf->buf;
		nr = 0;
	}

	return c;
}

int conf_parse (config_t *conf, FILE * file)
{
   return OS_OK;
}

config_t * conf_new (char *fname)
{
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
