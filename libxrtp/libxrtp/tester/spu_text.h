/***************************************************************************
                          spu_text.h  -  Text subtitle reader
                             -------------------
    begin                : Wed Nov 12 2003
    copyright            : (C) 2003 by Heming Ling
    email                : heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/*
 * Most of the code is stealed from xine project - http://xinehq.de, Thanks ;)
 */

#include <timedia/os.h>

#include <stdio.h>
#include <stdlib.h>

#define ERR (void *)-1
#define SUB_MAX_TEXT  5
#define SUB_BUFSIZE 1024
#define SUB_MAXLEN  1024

typedef struct {

  int lines;

  int start_ms; /* millisec unit */
  int end_ms;   /* millisec unit */

  char *text[SUB_MAX_TEXT];        /* Maximum 5 lines */

} subtitle_t;

typedef struct {

  FILE  *file;

  char               buf[SUB_BUFSIZE];
  off_t              buflen;

  float              mpsub_position;

  int                uses_time;
  int                errs;
  subtitle_t        *subtitles;
  
  int                num;            /* number of subtitle structs */
  int                cur;            /* current subtitle           */
  int                format;         /* constants see below        */
  subtitle_t        *previous_aqt_sub ;
  char               next_line[SUB_BUFSIZE]; /* a buffer for next line read from file */

} demux_sputext_t;

/*
 * Demuxer code start
 */

#define FORMAT_SUBRIP    0
#define FORMAT_MICRODVD  1
#define FORMAT_SUBVIEWER 2
#define FORMAT_SAMI      3
#define FORMAT_VPLAYER   4
#define FORMAT_RT        5
#define FORMAT_SSA       6 /* Sub Station Alpha */
#define FORMAT_DUNNO     7 /*... erm ... dunnowhat. tell me if you know */
#define FORMAT_MPSUB     8
#define FORMAT_AQTITLE   9

demux_sputext_t *demux_sputext_new (FILE * file);

void demux_sputext_done (demux_sputext_t *this);

int demux_sputext_rewind(demux_sputext_t *this);

int demux_sputext_seek_msec(demux_sputext_t *this, int millisec);

subtitle_t * demux_sputext_next_subtitle(demux_sputext_t *this);

void demux_sputext_show_title(subtitle_t * subt);

int demux_sputext_get_stream_msec (demux_sputext_t *this);
