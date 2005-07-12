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
#include <timedia/timer.h>

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
#define MP_NOP		  -9  /* NULL OPERATE */
#define MP_INVALID   -11

#define MP_AUDIO 'a'
#define MP_VIDEO 'v'
#define MP_TEXT  't'

#define DEVICE_INPUT  0x01
#define DEVICE_OUTPUT 0x10

#define MIME_MAXLEN  31

#define MAX_STREAMS 256    /* uint8 max value */

/*#define BYTE_BITS    8     of cause */

#define MAX_MODEBYTES 32

typedef struct media_source_s media_source_t;
typedef struct media_format_s media_format_t;
typedef struct media_stream_s media_stream_t;

typedef struct media_receiver_s media_receiver_t;
typedef struct media_maker_s media_maker_t;
typedef struct media_player_s media_player_t;

typedef struct media_info_s media_info_t;
typedef struct media_control_s media_control_t;
typedef struct control_setting_s control_setting_t;

struct control_setting_s
{
   int (*done)(control_setting_t* setting);
};

typedef int control_setting_call_t(void*, control_setting_t*);
typedef struct media_device_s media_device_t;

struct media_control_s
{
   int (*done) (media_control_t * cont);

   media_player_t* (*find_player) (media_control_t *cont, char *mode, char *mime, char *fourcc);
   media_player_t* (*new_player) (media_control_t *cont, char *mode, char *mime, char *fourcc, void* extra);

   media_maker_t* (*find_creater)(media_control_t *cont, char *name, media_info_t* minfo);

   int (*add_device)(media_control_t *cont, char *name, control_setting_call_t *call, void*user);
   media_device_t* (*find_device)(media_control_t *cont, char *name, char *mode);

   control_setting_t* (*fetch_setting)(media_control_t *cont, char *name, media_device_t *dev);

   int (*set_format) (media_control_t * cont, char *format_id, media_format_t * format);
   int (*seek_millisec) (media_control_t * cont, int msec);

   int (*demux_next) (media_control_t * cont, int strm_end);

   int (*config)(media_control_t *cont, config_t *conf, module_catalog_t *cata);
   module_catalog_t* (*modules)(media_control_t *cont);

   int (*match_type)(media_control_t * cont, char *type);

   /* return the rest bw */
   int (*set_bandwidth_budget)(media_control_t * cont, int bw);
   int (*book_bandwidth)(media_control_t * cont, int bw);
   int (*release_bandwidth)(media_control_t * cont, int bw);
   int (*reset_bandwidth)(media_control_t * cont);

   /* media ports management */
   int (*media_ports)(media_control_t* cont, char* mediatype, int* rtp_portno, int* rtcp_portno);

   xclock_t* (*clock)(media_control_t* cont);
};

DECLSPEC
module_interface_t* new_media_control ();

typedef struct capable_descript_s capable_descript_t;
struct capable_descript_s
{
	int (*done)(capable_descript_t *cap);
	int (*match)(capable_descript_t *cap1, capable_descript_t *cap2);
	int (*match_value)(capable_descript_t *cap, char *type, char *value, int vbytes);
};

struct media_format_s
{
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
   
   /* playback or netcast, etc, for all streams. */
   char default_mode[MAX_MODEBYTES];

   /* release a format */
   int (*done) (media_format_t * mf);

   int (*support_type) (media_format_t *mf, char *type, char *subtype);

   /* Open/Close a media source */
   int (*open) (media_format_t *mf, char * fname, media_control_t *ctrl);
   int (*close) (media_format_t * mf);
   
   /* Stream management */
   int (*add_stream) (media_format_t * mf, media_stream_t *strm, int strmno, unsigned char type);
   media_stream_t* (*find_stream) (media_format_t * mf, int strmno);
   media_stream_t* (*find_mime) (media_format_t * mf, char * mime);
   media_stream_t* (*find_fourcc) (media_format_t * mf, char *fourcc);

   int (*set_control) (media_format_t * mf, media_control_t * control);
   
   int (*set_player) (media_format_t * mf, media_player_t * player);

   /* Stream info */
   int (*nstream) (media_format_t * mf);
   
   const char* (*stream_mime) (media_format_t * mf, int strmno);
   const char* (*stream_fourcc) (media_format_t * mf, int strmno);

   media_player_t* (*stream_player) (media_format_t * mf, int strmno);
   media_player_t* (*mime_player) (media_format_t * mf, const char *mime);
   media_player_t* (*fourcc_player) (media_format_t * mf, const char *fourcc);

   /* link stream to player */
   int (*new_all_player) (media_format_t *mf, media_control_t *control, char* mode, void* mode_param);
   media_player_t* (*new_stream_player) (media_format_t *mf, int strmno, media_control_t *control, char* mode, void* mode_param);
   media_player_t* (*new_mime_player) (media_format_t * mf, char *mime, media_control_t *control, char* mode, void* mode_param);
   int (*set_mime_player) (media_format_t *mf, char *mime, media_player_t* player);

   /* return available capable number */
   int (*players)(media_format_t * mf, char *type, media_player_t *caps[], int nmax);

   /* Seek the media by time */
   int (*seek_millisecond) (media_format_t *mf, int milli);

   /* demux next sync stream group */
   int (*demux_next) (media_format_t *mf, int stream_end);
};

#define SAMPLE_TYPE_NATIVE       0
#define SAMPLE_TYPE_BIGEND       1
#define SAMPLE_TYPE_LITTLEEND    2

struct media_info_s
{
	int sample_rate;
	int sample_bits;
	int sample_byteorder;
   int sampling_constant;
	int bps;

	int coding_parameter;
    
	char fourcc[4];
	char* mime;
};

typedef struct audio_info_s audio_info_t;
struct audio_info_s
{
   struct media_info_s info;

   int channels;
   int channels_bytes;
};

#define MP_STREAM_STANDBY     0
#define MP_STREAM_STREAMING   1

struct media_stream_s
{
	int (*start)(media_stream_t* stream);
	int (*stop)(media_stream_t* stream);

	char mime[MIME_MAXLEN];
	char fourcc[4];
   
	void           *handler;

	int            playable;
	long           sample_rate;
	int            eos;

	media_info_t   *media_info;

	media_stream_t *next;
	media_stream_t *peer;
   
	media_maker_t *maker;
   
	//media_receiver_t *player;
	media_player_t *player;

   int status;
};

typedef struct media_pipe_s media_pipe_t;
typedef struct media_frame_s media_frame_t;
struct media_frame_s
{
   media_frame_t *next;
   media_pipe_t *owner; /* the pipe that created the frame */
   
   int bytes;			/* frame raw size >= nraw */
   int nraw;			/* samples */

   int eos;				/* last frame of the stream */
   int eots;			/* end of frames with same timestamp */

   int ts;				/* timestamp of frame */

   int64 sno;			/* frame serialno */
   int64 samplestamp;	/* total stamps by now */
   
   int usec;			/* frame duration */
   
   int nraw_done;		/* samples played */

   char * raw;
};

typedef struct media_input_s media_input_t;

struct media_pipe_s
{
   int (*done) (media_pipe_t *pipe);
   
   media_frame_t* (*new_frame) (media_pipe_t *pipe, int bytes, char *init_data);
   int (*recycle_frame) (media_pipe_t *pipe, media_frame_t *frame);

   int (*put_frame) (media_pipe_t *pipe, media_frame_t *frame, int last);
   
   media_frame_t* (*pick_frame)(media_pipe_t *pipe);

   /*
    * return 0: stream continues
    * return 1: stream ends
    */
   int (*pick_content) (media_pipe_t *pipe, media_info_t *media_info, char* raw, int nraw_once);
};

media_pipe_t * media_new_pipe (void);

struct media_device_s
{
   int running;
   
   media_pipe_t* (*pipe) (media_device_t *dev);

   int (*init) (media_device_t * dev, media_control_t *ctrl);
   int (*start) (media_device_t * dev, int mode);   
   int (*stop) (media_device_t * dev, int mode);

   media_stream_t* (*new_media_stream) (media_device_t* dev, media_receiver_t *mr, media_info_t *media_info);

   int (*set_input_media)(media_device_t *dev, media_receiver_t* recvr, media_info_t *in_info);
   int (*set_output_media)(media_device_t *dev, media_info_t *out_info);

   int (*set_io)(media_device_t *dev, media_info_t *minfo, media_receiver_t *receiver);

   int (*setting) (media_device_t * dev, control_setting_t *setting, module_catalog_t *catalog);

   control_setting_t* (*new_setting) (media_device_t * dev);
   
   int (*match_type) (media_device_t *dev, char *type);

   int (*match_mode) (media_device_t *dev, char *mode);

   int (*done) (media_device_t * dev);
};

media_device_t * new_media_device (void* setting);

struct media_receiver_s
{
   int (*match_type) (media_receiver_t *recvr, char *mime, char *fourcc);

   int (*receive_media) (media_receiver_t *recvr, media_frame_t * media_packet, int64 samplestamp, int last_packet);
};

struct media_maker_s
{
   struct media_receiver_s receiver;
	
   int (*done) (media_maker_t *mm);
   
   int (*start) (media_maker_t *mm);
   int (*stop) (media_maker_t *mm);
   
   media_stream_t* (*new_media_stream) (media_maker_t *mm, media_control_t* control, media_device_t* dev, media_receiver_t* player, media_info_t *media_info);
   int (*link_stream) (media_maker_t *mm, media_stream_t* stream, media_control_t* control, media_device_t* dev, media_info_t *media_info);
};

struct media_player_s
{
   struct media_receiver_s receiver;
	
   media_device_t * device;

   //const char* (*play_type) (media_player_t * playa);
   char* (*media_type) (media_player_t * playa);
   char* (*codec_type) (media_player_t * playa);
   
   #define CALLBACK_PLAYER_READY  1
   #define CALLBACK_MEDIA_STOP  2
   
   int (*done) (media_player_t * playa);
   
   int (*init) (media_player_t * mp, media_control_t *control, void* data);
   
   int (*match_play_type) (media_player_t * playa, char *playtype);

   int (*set_callback) (media_player_t * playa, int type, int(*call)(), void *user);

   int (*set_options) (media_player_t * playa, char *opt, void *value);

   int (*set_device) (media_player_t * mp, media_control_t *control, module_catalog_t *cata, void* extra);
   int (*link_stream) (media_player_t * mp, media_stream_t *stream, media_control_t *control, void* extra);

   media_pipe_t* (*pipe) (media_player_t * playa);

   int (*open_stream) (media_player_t *playa, media_info_t *media_info);
   int (*close_stream) (media_player_t *playa);
   
   void* (*media)(media_player_t *playa);

   capable_descript_t* (*capable)(media_player_t *playa, void *data);
   int (*match_capable)(media_player_t *playa, capable_descript_t *cap);

   int (*start) (media_player_t * playa);
   int (*stop) (media_player_t * playa);
};

/* media is drived by source */
struct media_source_s
{
	media_format_t* format;

	int (*done)(media_source_t* msrc);
    
	int (*start)(media_source_t* msrc);
	int (*stop)(media_source_t* msrc);
};

#endif
