/***************************************************************************
                          config.h  -  config interface
                             -------------------
    begin                : Wed Dec 3 2003
    copyright            : (C) 2003 by Heming Ling
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

#ifndef CONFIG_H

#define CONFIG_H

#include "os.h"

typedef struct config_s config_t;

extern DECLSPEC config_t * conf_new (char *fname);

extern DECLSPEC int conf_done(config_t *conf);

/* If fname is NOT NULL, save as new file name */
extern DECLSPEC int conf_save(config_t *conf, char * fname);

extern DECLSPEC int conf_set(config_t *conf, char *key, int klen, char *value, int vlen);

extern DECLSPEC int conf_get(config_t *conf, char *key, int klen, char *buf, int blen);

#endif
