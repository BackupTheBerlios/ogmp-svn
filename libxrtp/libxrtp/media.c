/***************************************************************************
                          media.c  -  media commen function
                             -------------------
    begin                : Wed Dec 31 2003
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

#include "internal.h"

 #ifdef MEDIA_LOG
   const int media_log = 1;
 #else
   const int media_log = 0;
 #endif
 #define media_log(fmtargs)  do{if(media_log) printf fmtargs;}while(0)

/* Convert realtime to rtp ts */
uint32 media_realtime_to_rtpts(xrtp_media_t * media, rtime_t msec, rtime_t usec){

   uint32 rtpts = msec / 1000 * media->clockrate * media->sampling_instance;
   rtpts += (usec % LRT_HRT_DIVISOR) / (HRTIME_SECOND_DIVISOR / media->clockrate) * media->sampling_instance;

   media_log(("media_realtime_to_rtpts: lrt[%d]:hrt[%d] on arrival\n", msec, usec));
   media_log(("media_realtime_to_rtpts: rtp_ts on arrival is %d\n", rtpts));
   return rtpts;
}

int rtp_media_set_callback(xrtp_media_t *media, int type, int (*cb)(), void *user)
{
	return session_set_callback(media->session, type, cb, user);
}


