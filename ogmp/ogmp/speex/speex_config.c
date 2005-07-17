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

#include "speex_info.h"

speex_setting_t spx_setting = 
{
	/* rtp_portno, rtcp_portno, media_bps */
	{3080, 3081, 16*1024},

	8000,	/*sample_rate*/
	1,		/*channels*/

	2,		/*mode*/

	100,	/*ptime_max*/
	0,		/*cng*/
	1,		/*penh*/

	0,		/*vbr*/
	0.0,	/*vbr_quality*/
	0,		/*abr*/

	8000,		/*cbr*/
	4,		/*cbr_quality*/

	3,		/*complexity*/
	1,		/*denoise*/
	0		/*agc*/
};

speex_setting_t* speex_setting(media_control_t *control)
{
	return &spx_setting;
}

int speex_info_setting(speex_info_t *spxinfo, speex_setting_t *spxsetting)
{
	if(spxsetting->rtp_setting.media_bps > 0)
		spxinfo->audioinfo.info.bps = spxsetting->rtp_setting.media_bps;

	spxinfo->audioinfo.info.sample_rate = spxsetting->sample_rate;
	spxinfo->audioinfo.channels = spxsetting->channels;

	spxinfo->penh = spxsetting->penh;
	spxinfo->cng = spxsetting->cng;
	spxinfo->ptime = spxsetting->ptime_max;

	spxinfo->vbr = spxsetting->vbr;
	spxinfo->abr = spxsetting->abr;
	spxinfo->cbr = spxsetting->cbr;

	spxinfo->complexity = spxsetting->complexity;

	spxinfo->vbr_quality = spxsetting->vbr_quality;
	spxinfo->cbr_quality = spxsetting->cbr_quality;

	spxinfo->denoise = spxsetting->denoise;
	spxinfo->agc = spxsetting->agc;

	return MP_OK;
}
