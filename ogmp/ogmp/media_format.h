/***************************************************************************
                          media_format.h  -  Generic media format
                             -------------------
    begin                : Wed Jan 28 2004
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

#ifndef MEDIA_FORMAT_H

#define MEDIA_FORMAT_H

#include <timedia/os.h>
#include <timedia/config.h>
#include <timedia/catalog.h>

#include <stdio.h>
#include <stdlib.h>

#define MP_OK         0
#define MP_FAIL       -1
#define MP_EMEM       -2
#define MP_EOVER      -3
#define MP_EFORMAT    -4
#define MP_EOS        -5
#define MP_EOF        -6
#define MP_EUNSUP     -7  /* Not support */
#define MP_EIMPL      -8  /* Not implemented */
#define MP_INVALID   -11

#define MIME_MAXLEN  31

#define MAX_STREAMS 64    /* no reason */

#define BYTE_BITS    8    /* of cause */

typedef struct media_format_s media_format_t;
typedef struct media_stream_s media_stream_t;
typedef struct media_player_s media_player_t;

typedef struct media_control_s media_control_t;
typedef struct control_setting_s control_setting_t;

struct control_setting_s {

   int (*done)(control_setting_t* setting);
};

typedef int control_setting_call_t(void*, control_setting_t*);
typedef struct media_device_s media_device_t;

struct media_control_s {

   int (*done) (media_control_t * cont);

   media_player_t* (*find_player) (media_control_t *cont, char *mime, char *fourcc);

   int (*put_configer)(media_control_t *cont, char *name, control_setting_call_t call, void*user);
   control_setting_t* (*fetch_setting)(media_control_t *cont, char *name, media_device_t *dev);

   int (*set_format) (media_control_t * cont, char *format_id, media_format_t * format);
   int (*seek_millisec) (media_control_t * cont, int msec);

   int (*demux_next) (media_control_t * cont, int strm_end);

   int (*config)(media_control_t * cont, char *type, config_t *conf, module_catalog_t *cata);

   int (*match_type)(media_control_t * cont, char *type);
};

module_interface_t* new_media_control ();

struct media_format_s {

   /* generic info goes to here */
   FILE                 *fdin;
   off_t                file_bytes;

   int                  time_length;   /* 1/1000 seconds unit */

   off_t                avg_bitrate;

   unsigned char        strms_no[MAX_STREAMS];   /* strean number */
   unsigned char        strms_type[MAX_STREAMS]; /* stream type: 'a' for audio; 'v' for video; 't' for text */

   int                  n_strm;
   media_stream_t       *first;

   int                  nastreams;
   int                  nvstreams;
   int                  ntstreams;
   int                  numstreams;

   int                  time_ref;    /* the streamno is the timeline reference, normally is audio stream */

   xrtp_list_t          *stream_handlers;   /* Media handlers */
   
   media_control_t      *control;
   
   /* release a format */
   int (*done) (media_format_t * mf);

   /* Open/Close a media source */
   int (*open) (media_format_t *mf, char * name, module_catalog_t *cata, config_t *conf);
   int (*close) (media_format_t * mf);

   /* Stream management */
   int (*add_stream) (media_format_t * mf, media_stream_t *strm, int strmno, unsigned char type);
   media_stream_t* (*find_stream) (media_format_t * mf, int strmno);
   media_stream_t* (*find_mime) (media_format_t * mf, const char * mime);
   media_stream_t* (*find_fourcc) (media_format_t * mf, const char *fourcc);

   int (*set_control) (media_format_t * mf, media_control_t * control);
   
   int (*set_player) (media_format_t * mf, media_player_t * player);

   /* Stream info */
   int (*nstream) (media_format_t * mf);
   
   const char* (*stream_mime) (media_format_t * mf, int strmno);
   const char* (*stream_fourcc) (media_format_t * mf, int strmno);

   media_player_t* (*stream_player) (media_format_t * mf, int strmno);
   media_player_t* (*mime_player) (media_format_t * mf, const char *mime);
   media_player_t* (*fourcc_player) (media_format_t * mf, const char *fourcc);

   /* Seek the media by time */
   int (*seek_millisecond) (media_format_t *mf, int milli);

   /* demux next sync stream group */
   int (*demux_next) (media_format_t *mf, int stream_end);
};

#define SAMPLE_TYPE_NATIVE       0
#define SAMPLE_TYPE_BIGEND       1
#define SAMPLE_TYPE_LITTLEEND    2

typedef struct media_info_s media_info_t;
struct media_info_s {

   int sample_rate;
   int sample_bits;
   int sample_byteorder;
};

typedef struct audio_info_s audio_info_t;
struct audio_info_s {

   struct media_info_s info;

   int channels;
   int channels_bytes;
};

struct media_stream_s {

   char mime[MIME_MAXLEN];
   char fourcc[4];
   
   void           *handler;
   
   media_player_t *player;

   int            playable;
   long           sample_rate;
   int            eos;

   media_info_t   *media_info;

   media_stream_t *next;
};

typedef struct media_frame_s media_frame_t;
struct media_frame_s {

   media_frame_t *next;
   
   int bytes;        /* frame raw size >= nraw */
   int eos;          /* last frame of the stream */
   
   int eots;         /* end of frames with same timestamp */
   int ts;           /* timestamp of frame */
   
   int usec;         /* frame duration */
   
   int nraw;         /* samples */
   int nraw_done;    /* samples played */

   char * raw;
};

typedef struct media_input_s media_input_t;

typedef struct media_pipe_s media_pipe_t;
struct media_pipe_s {
   
   media_frame_t* (*new_frame) (media_pipe_t *pipe, int bytes);
   int (*recycle_frame) (media_pipe_t *pipe, media_frame_t *frame);

   int (*put_frame) (media_pipe_t *pipe, media_frame_t *frame, int last);

   /*
    * return 0: stream continues
    * return 1: stream ends
    */
   int (*pick_frame) (media_pipe_t *pipe, media_info_t *media_info, char* raw, int nraw_once);

   int (*done) (media_pipe_t *pipe);
};
media_pipe_t * media_new_pipe (void);

struct media_device_s {

   int running;
   
   //media_input_t* (*input) (media_device_t *);

   media_pipe_t* (*pipe) (media_device_t *);

   int (*start) (media_device_t * dev, void * setting, int us_min, int us_max);
   
   int (*stop) (media_device_t * dev);

   int (*setting) (media_device_t * dev, control_setting_t *setting, module_catalog_t *catalog);
   control_setting_t* (*new_setting) (media_device_t * dev);
   
   int (*match_type) (media_device_t *dev, char *type);

   int (*done) (media_device_t * dev);
};

media_device_t * new_media_device (void* setting);

struct media_player_s {

   media_device_t * device;

   char* (*play_type) (media_player_t * playa);
   char* (*media_type) (media_player_t * playa);
   char* (*codec_type) (media_player_t * playa);
   
   #define CALLBACK_PLAY_MEDIA  1
   #define CALLBACK_STOP_MEDIA  2
   
   int (*done) (media_player_t * playa);
   
   int (*match_type) (media_player_t * playa, char *mime, char *fourcc);

   int (*set_callback) (media_player_t * playa, int type, int(*fn)(void*), void * user);

   int (*set_options) (media_player_t * playa, char *opt, void *value);
   int (*set_device) (media_player_t * mp, media_control_t *control, module_catalog_t *cata);

   media_pipe_t* (*pipe) (media_player_t * playa);

   int (*open_stream) (media_player_t *playa, media_info_t *media_info);
   int (*close_stream) (media_player_t *playa);

   int (*receive_media) (media_player_t *playa, void * media_packet, int samplestamp, int last_packet);

   int (*stop) (media_player_t * playa);
};

#endif

