/***************************************************************************
                          ogmp_server.c  -  description
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

#include "ogmp.h"

/* ui to config rtp module */
int ogmp_config_rtp(void *conf, control_setting_t *setting){

   rtp_setting_t *rset = (rtp_setting_t*)setting;

   rset->cname = "ogmp_server";
   rset->cnlen = strlen("ogmp_server")+1;

   rset->ipaddr = "127.0.0.1";

   rset->rtp_portno = 3000;
   rset->rtcp_portno = 3001;

   rset->total_bw = 128*1024;
   rset->rtp_bw = 96*1024;

   rset->ncallback = 0;
   rset->callbacks = NULL;

   return MP_OK;
}
