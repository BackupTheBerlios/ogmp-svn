/***************************************************************************
                          media.h  -  Media interface of a handler
                             -------------------
    begin                : Tue Apr 8 2003
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

  typedef struct xrtp_media_s xrtp_media_t;
  typedef void media_data_t;

  typedef uint64 media_time_t;

  struct xrtp_media_s{

     char * type; /* identical to handler id */
     xrtp_session_t *session;

     uint clockrate;
     uint sampling_instance;
     
     const char* (*mime)(xrtp_media_t * media);

     int (*done)(xrtp_media_t *media);

     int (*set_parameter)(xrtp_media_t *media, int key, void *param);
     
     int (*set_rate)(xrtp_media_t *media, int rate);
     int (*rate)(xrtp_media_t *media);
     
     uint32 (*sign)(xrtp_media_t * media);

     /* For media, this means the time to display
      * xrtp will schedule the time to send media by this parameter.
      */
     int (*post)(xrtp_media_t * media, media_data_t * data, int len, int allow_usec);

     struct media_callbacks_s{

        #define CALLBACK_MEDIA_ARRIVED      0x0
        #define CALLBACK_MEDIA_FINISH       0x1
        #define CALLBACK_MEDIA_REPORT       0x2
        
        /* callid: CALLBACK_MEDIA_ARRIVED */
        int (* media_arrived)(void * user, media_data_t * data, int len, uint32 src, uint32 timestamp);
        void * media_arrived_user;

        /* callid: CALLBACK_MEDIA_FINISH */
        int (* media_finish)(void * user, uint32 src, xrtp_hrtime_t ts);
        void * media_finish_user;

        /* callid: CALLBACK_MEDIA_REPORT */
        int (* media_report)(void * user, uint32 src, xrtp_hrtime_t ts, int result);
        void * media_report_user;

     } $callbacks;
  };

  /* Convert realtime to rtp ts */
  uint32 media_realtime_to_rtpts(xrtp_media_t * media, xrtp_lrtime_t lrt, xrtp_hrtime_t hrt);

extern DECLSPEC int
media_set_callback(xrtp_media_t *media, int type, void* call, void* user);
