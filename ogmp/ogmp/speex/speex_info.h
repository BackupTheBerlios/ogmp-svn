/***************************************************************************
                          speex_info.h  -  Speex media info
                             -------------------
    begin                : Fri Jan 16 2004
    copyright            : (C) 2004 by Heming Ling
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

#define OGM_SPEEX_H

#include "media_format.h"
#include "rtp_cap.h"

#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#include <speex/speex_preprocess.h>

#define SPX_FRAME_MSEC 20

/* Same as SpeexHeader Structure Reference */
#define SPX_NB_MODE  0
#define SPX_WB_MODE  1
#define SPX_UWB_MODE  2

typedef struct speex_info_s speex_info_t;
typedef struct speex_setting_s speex_setting_t;

struct speex_info_s
{
	struct audio_info_s audioinfo;
   
	/* decoding parameters */
	SpeexBits decbits;   
	SpeexStereoState decstereo;

	const SpeexMode *spxmode;
	int mode;

	int version;
	int bitstream_version;
	
	int nframe_per_packet;  /* number of speex frames group together */
	int nsample_per_frame;

	int ptime;		/* determine the frams number in a rtp packet */

	int vbr;		/* variable bit rate */
	int abr;
	int cbr;

	int cng;		/* comfortable noise generation */
	int penh;		/* perceptual enhancement */

	void *dst;		/* decode state */

	/* encoding parameters */
	SpeexBits encbits;
	SpeexStereoState encstereo;
	SpeexPreprocessState *encpreprocess;
   
	void *est;		/* encode state */

	float vbr_quality;	/* 1-10 */
	int cbr_quality;	/* 1-10 */

	int complexity;
	int denoise;
	int agc;

	int bitrate_now;

	int head_packets;

	int nheader;

	/* judge if audio packet started, as first audio packet has not ready samples */
	int audio_start;
};

/*
	SDP for Speex (Internet-Draft: "draft-herlein-speex-rtp-profile-02" March 1, 2004)
	
	The following example illustrates the case where the offerer 
	cannot receive more than 10 kbit/s. "b=AS:10"   	
	
	m=audio 8088 RTP/AVP 97
	b=AS:10
	a=rtmap:97 speex/8000

	Or use speex specific parameters via the a=fmtp: directive 
	to make recommendations to the remote Speex encoder. 
	
	The following parameters are defined for use in this way:
   
	 ptime: duration of each rtp packet in milliseconds.
			speex frame is 20ms duration, ptime/20 = nframe per rtp packet

	 sr:    actual sample rate in Hz.

	 ebw:   encoding bandwidth - either 'narrow' or 'wide' or 'ultra' 
			(corresponds to nominal 8000, 16000, and 32000 Hz sampling rates).

	 vbr:   variable bit rate  - either 'on' 'off' or 'vad'
			(defaults to off).  If on, variable bit rate is
			enabled.  If off, disabled.  If set to 'vad' then
			constant bit rate is used but silence will be encoded
			with special short frames to indicate a lack of voice
			for that period.

	 cng:   comfort noise generation - either 'on' or 'off'. If
			off then silence frames will be silent; if 'on' then
			those frames will be filled with comfort noise.

	 mode:  Speex encoding mode. Can be {1,2,3,4,5,6,any}
            defaults to 3 in narrowband, 6 in wide and ultra-wide.

	 penh:	use of perceptual enhancement. 1 indicates 
	 		to the decoder that perceptual enhancement is recommended,
			0 indicates that it is not. Defaults to on (1).

   Examples:

   	m=audio 8008 RTP/AVP 97
	a=rtpmap:97 speex/8000
	a=fmtp:97 mode=4

   This examples illustrate an offerer that wishes to receive
   a Speex stream at 8000Hz, but only using speex mode 3.
 
   The offerer may suggest to the remote decoder to activate
   its perceptual enhancement filter like this:
   
	m=audio 8088 RTP/AVP 97
	a=rtmap:97 speex/8000
	a=fmtp:97 penh=1 
	
   Several Speex specific parameters can be given in a single
   a=fmtp line provided that they are separated by a semi-colon:
   
   	a=fmtp:97 mode=any;penh=1

   The offerer may indicate that it wishes to send variable bit rate
   frames with comfort noise:

	m=audio 8088 RTP/AVP 97
	a=rtmap:97 speex/8000
	a=fmtp:97 vbr=on;cng=on

   The "ptime" attribute is used to denote the packetization 
   interval (ie, how many milliseconds of audio is encoded in a 
   single RTP packet).  Since Speex uses 20 msec frames, ptime values 
   of multiples of 20 denote multiple Speex frames per packet.  
   Values of ptime which are not multiples of 20 MUST be ignored 
   and clients MUST use the default value of 20 instead.
   
   In the example below the ptime value is set to 40, indicating that 
   there are 2 frames in each packet.	
   
   	m=audio 8008 RTP/AVP 97
	a=rtpmap:97 speex/8000
	a=ptime:40
	
   Note that the ptime parameter applies to all payloads listed
   in the media line and is not used as part of an a=fmtp directive.

   Values of ptime not multiple of 20 msec are meaningless, so the 
   receiver of such ptime values MUST ignore them.  If during the 
   life of an RTP session the ptime value changes, when there are 
   multiple Speex frames for example, the SDP value must also reflect 
   the new value. 

   Care must be taken when setting the value of ptime so that the 
   RTP packet size does not exceed the path MTU. 
*/

int speex_info_to_sdp(media_info_t *info, rtpcap_descript_t *rtpcap, sdp_message_t *sdp, int pos);
int speex_info_from_sdp(media_info_t *info, int rtpmap_no, sdp_message_t *sdp, int pos);

struct speex_setting_s
{
	rtp_profile_setting_t rtp_setting;

	int sample_rate;
	int channels;
	int mode; 
	int ptime_max;

	int cng;
	int penh;

	int vbr; 
	float vbr_quality;

	int abr;
	
	int cbr;
	int cbr_quality;

	int complexity;
	int denoise;
	int agc;
};

speex_setting_t* speex_setting(media_control_t *control);
int speex_info_setting(speex_info_t *spxinfo, speex_setting_t *spxset);
