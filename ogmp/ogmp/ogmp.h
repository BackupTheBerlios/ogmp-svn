/***************************************************************************
                          ogmp.h  -  description
                             -------------------
    begin                : Wed May 19 2004
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

#include "devices/dev_rtp.h"

#include "media_format.h"

#include <timedia/timer.h>
#include <timedia/xthread.h>
#include <timedia/catalog.h>
#include <timedia/config.h>

#include <stdlib.h>
#include <string.h>

int ogmp_config_rtp(void *conf, control_setting_t *setting);
