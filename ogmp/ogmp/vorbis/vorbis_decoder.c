/***************************************************************************
                          vorbis_player.c  -  vorbis audio codec
                             -------------------
    begin                : Sun Feb 1 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org, heming@bigfoot.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include "vorbis_decoder.h"

/*
#define VORBIS_CODEC_LOG
#define VORBIS_CODEC_DEBUG
*/

#ifdef VORBIS_CODEC_LOG
   const int vorbis_codec_log = 1;
#else                     
   const int vorbis_codec_log = 0;
#endif

#define vorbis_player_log(fmtargs)  do{if(vorbis_codec_log) printf fmtargs;}while(0)

#ifdef VORBIS_CODEC_DEBUG
   const int vorbis_codec_debug = 1;
#else                
   const int vorbis_codec_debug = 0;
#endif
#define vorbis_player_debug(fmtargs)  do{if(vorbis_codec_debug) printf fmtargs;}while(0)


#ifndef _V_CLIP_MATH
#define _V_CLIP_MATH

static ogg_int32_t CLIP_TO_15(ogg_int32_t x) {
  
      int ret=x;
      ret-= ((x<=32767)-1)&(x-32767);
      ret-= ((x>=-32768)-1)&(x+32768);
      return(ret);
}
   
#endif  /* _V_CLIP_MATH */

/*
#define VORBIS_SIMULATING
*/

media_frame_t * vorbis_decode (vorbis_info_t *vorbis, ogg_packet * packet, media_pipe_t * output) {

   int channels = 0;
   media_frame_t *auf = NULL;
   int i, j;
   int clipflag = 0;

#ifdef OGG_TREMOR_CODEC

   ogg_int32_t **pcm;

#else  /* OGG_TREMOR_CODEC */

   float **pcm;
   
#endif /* OGG_TREMOR_CODEC */

   int samples = 0;
   
   vorbis_synthesis_blockin( &vorbis->vd, &vorbis->vb );

#ifdef OGG_TREMOR_CODEC

   if( vorbis_synthesis(&vorbis->vb, packet, 1) != 0 ){

#else /* OGG_TREMOR_CODEC */

   if( vorbis_synthesis(&vorbis->vb, packet) != 0 ){

#endif /* OGG_TREMOR_CODEC */

      vorbis_player_debug(("vorbis_decode: a bad vorbis packet!\n"));

      return NULL;

   }else{

      /* the packet contains vorbis data */

      samples = vorbis_synthesis_pcmout(&vorbis->vd, &pcm);

      if ( samples <= 0 ) {
      
         vorbis_player_debug(("vorbis_decode: no audio samples retrieved!\n"));

         return NULL;
      }
   }

   if(!output){

      vorbis_synthesis_read(&vorbis->vd, samples);

      vorbis_player_debug(("vorbis_decode: no output, discard\n"));

      return NULL;
   }
   
   channels=vorbis->vi.channels;

#ifdef OGG_TREMOR_CODEC

   int i, j;
   for ( i=0; i<channels; i++ ) { /* It's faster in this order */

      ogg_int32_t *src = (ogg_int32_t *)pcm[i];
      short *dest = ((short *)buf) + i;

      for( j=0; j<samples; j++ ) {

         *dest = CLIP_TO_15(src[j]>>9);
         dest += channels;
      }
   }

   vorbis_synthesis_read(&vorbis->vd, samples);

   return samples*2*channels;

#else /* OGG_TREMOR_CODEC */

   /* vorbis float version implementation:
    * convert floats to 16 bit signed ints (host order) and interleave
    */
   auf = (media_frame_t*)output->new_frame(output, channels * samples * sizeof(int16));
   if (!auf) {

      vorbis_player_debug(("vorbis_decode: no available frame retrieved\n"));

      return NULL;
   }

   for( i=0; i<channels; i++ ) {

      ogg_int16_t *ptr = ((ogg_int16_t *)auf->raw) + i;
      float  *mono = pcm[i];

      for( j=0; j<samples; j++ ) {

         int val = (int)(mono[j] * 32767.f);

         /* might as well guard against clipping */
         if (val > 32767) {

            val = 32767;
            clipflag = 1;
         }

         if (val < -32768) {

            val = -32768;
            clipflag = 1;
         }

         *ptr=val;
         ptr += channels;
      }
   }

   vorbis_synthesis_read(&vorbis->vd, samples);

   auf->nraw = samples;
   auf->usec = 1000000 * samples / vorbis->vi.rate;  /* micro second unit */

   return auf;

#endif /* OGG_TREMOR_CODEC */
}

