/***************************************************************************
                          speex_sdp.c  - SDP for Speex
                             -------------------
    begin                : Tue Sep 7 2004
    copyright            : (C) 2004 by Heming Ling
    email                : heming@timedia.org; heming@bigfoot.com
 ***************************************************************************/
 
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include <stdlib.h>
#include <timedia/xstring.h>
#include <timedia/xmalloc.h>
#include <timedia/ui.h>

#include "speex_info.h"

#define SPXSDP_LOG

#ifdef SPXSDP_LOG
 #define spxsdp_log(fmtargs)  do{ui_print_log fmtargs;}while(0)
#else
 #define spxsdp_log(fmtargs)
#endif

/**************************************************************
 * speex capable descript module
 */
int speex_info_to_sdp(media_info_t *info, rtpcap_descript_t *rtpcap, sdp_message_t *sdp, int pos)
{
	char a[128], fmt[4], *pa;

	int semicolon = 0;

	speex_info_t *spxinfo = (speex_info_t*)info;

	/* create sdp message, "m=...", "a=rtpmap:..." */
	rtp_capable_to_sdp(rtpcap, sdp, pos);

    snprintf(fmt, 4, "%d", rtpcap->profile_no);
	sdp_message_m_payload_add (sdp, pos, xstr_clone(fmt));

	/* make "a=ptime:" attribute */
	if(spxinfo->ptime != SPX_FRAME_MSEC)
	{
		spxsdp_log(("speex_info_to_sdp: a=ptime:%d\n", spxinfo->ptime));

        snprintf(a, 128, "%d", spxinfo->ptime);
		sdp_message_a_attribute_add (sdp, pos, xstr_clone("ptime"), xstr_clone(a));
	}

	if(!spxinfo->vbr && !spxinfo->cng && !spxinfo->penh)
	{
		/* No need "a=fmtp:"*/
		spxsdp_log(("speex_info_to_sdp: No need a=fmtp, sdp_media_pos=%d\n", pos));

		return MP_OK;
	}

	/* make "a=fmtp:" attribute */
	pa = a;

    sprintf(pa, "%d", rtpcap->profile_no);
    /*itoa(rtpcap->profile_no, pa, 10);*/

	while(*pa)
		pa++;
	*pa = ' '; /* blank space */
	pa++;

	if(spxinfo->vbr == 1)
	{
		strcpy(pa, "vbr=on");
		semicolon = 1;

        while(*pa)
            pa++;
	}

	if(spxinfo->cng != 0)
	{
		if(semicolon)
			strcpy(pa, ";cng=on");
		else
			strcpy(pa, "cng=on");

		semicolon = 1;

        while(*pa)
            pa++;
	}


	/* encoding mode:  
		Speex encoding mode. Can be {1,2,3,4,5,6,any}
		defaults to 3 in narrowband, 6 in wide and ultra-wide.
	if(spxinfo->audioinfo.info.sample_rate <= 8000 && spxinfo->mode != 3)
	{
		strcpy(pa, "mode=3");
		semicolon = 1;

        while(*pa)
            pa++;
	}
	*/

	if(spxinfo->penh != 0)
	{
		if(semicolon)
			strcpy(pa, ";penh=1");
		else
			strcpy(pa, "penh=1");

		semicolon = 1;
	}

	spxsdp_log(("speex_info_to_sdp: a=fmtp:%s\n", a));

	/* speex specific sdp attribute */
	sdp_message_a_attribute_add (sdp, pos, xstr_clone("fmtp"), xstr_clone(a));

	return MP_OK;
}

int speex_info_from_sdp(media_info_t *info, int rtpmap_no, sdp_message_t *sdp, int pos)
{
	int pos_media;
	int pos_attr;
	char *a;

	speex_info_t *spxinfo = (speex_info_t*)info;
	int found_pos_speex = 0;
	int ptime_ok=0, fmtp_ok=0;

	/* locate speex sdp */
	pos_media = 0;
	while (!sdp_message_endof_media (sdp, pos_media))
	{
		int pos_attr = 0;
		char* media_type = sdp_message_m_media_get (sdp, pos_media);

		if(0==strcmp(media_type, "audio"))
		{
			char* attr = sdp_message_a_att_field_get (sdp, pos_media, pos_attr);
			spxsdp_log(("speex_info_from_sdp: m=%s\n", media_type));

			while(attr)
			{
				if(0 == strcmp(attr, "rtpmap"))
				{
					int rtpmapno;
					int clockrate;
					int coding_param;
					char coding_type[MAX_MIME_BYTES] = "";
					char* rtpmap;
						
					rtpmap = sdp_message_a_att_value_get(sdp, pos_media, pos_attr);
					spxsdp_log(("speex_info_from_sdp: a=rtpmap:%s\n", rtpmap));

					sdp_parse_rtpmap(rtpmap, &rtpmapno, coding_type, &clockrate, &coding_param);
					if(0==strcmp(coding_type,"speex"))
					{
						spxinfo->audioinfo.info.sample_rate = clockrate;
						spxinfo->audioinfo.channels = coding_param;

						spxsdp_log(("speex_info_from_sdp: clockrate[%d] channels[%d]\n", clockrate, coding_param));

						found_pos_speex = 1;

						break;
					}
				}
						
				attr = sdp_message_a_att_field_get (sdp, pos_media, ++pos_attr);
			}
		}

		if(found_pos_speex)
			break;

		pos_media++;
	}

	if(!found_pos_speex)
	{
		spxsdp_log(("speex_info_from_sdp: No speex info found !\n"));
		return -1;
	}

	/* Get speex attribute */
	pos_attr = 0;

	a = sdp_message_a_att_field_get (sdp, pos_media, pos_attr);
	while(a)
	{
		if(0==strcmp(a, "ptime"))
		{
			char *ptime;
			int v;

			ptime = sdp_message_a_att_value_get(sdp, pos_media, pos_attr);
			spxsdp_log(("speex_info_from_sdp: a=ptime:%s\n", ptime));

			v = (int)strtol(ptime, NULL, 10);

			if(v > SPX_FRAME_MSEC && v % SPX_FRAME_MSEC == 0)
				spxinfo->nframe_per_packet = v / SPX_FRAME_MSEC;
			else
				spxinfo->nframe_per_packet = 1;

			spxsdp_log(("speex_info_from_sdp: %d frames per packet\n", spxinfo->nframe_per_packet));
			
			ptime_ok = 1;
			if(fmtp_ok)
				break;
		}

		if(0==strcmp(a, "fmtp"))
		{
			char *fmtp, *token;
			int v;
			
			fmtp = sdp_message_a_att_value_get(sdp, pos_media, pos_attr);
			spxsdp_log(("speex_info_from_sdp: a=fmtp:%s\n", fmtp));

			token = fmtp;

			while(*token == ' ' || *token == '\t')
				token++;

			v = (int)strtol(token, NULL, 10);
			if(v == rtpmap_no)
			{
				char *attr;

				spxinfo->vbr = 0;
				attr = strstr(token, "vbr");
				if(attr)
				{
					while(*attr != '=')
						attr++;
					while(*attr == ' ' || *attr == '\t' || *attr == '=')
						attr++;

					if(0==strcmp(attr,"on"))
						spxinfo->vbr = 1;
				}

				spxinfo->cng = 0;
				attr = strstr(token, "cng");
				if(attr)
				{
					while(*attr != '=')
						attr++;
					while(*attr == ' ' || *attr == '\t' || *attr == '=')
						attr++;

					if(0==strcmp(attr,"on"))
						spxinfo->cng = 1;
				}

				spxinfo->penh = 0;
				attr = strstr(token, "penh");
				if(attr)
				{
					while(*attr != '=')
						attr++;
					while(*attr == ' ' || *attr == '\t' || *attr == '=')
						attr++;

					if(*attr == '1')
						spxinfo->penh = 1;
				}

				/* attribute "mode=..."
				attr = strstr(token, "mode");
				if(attr)
				{
					while(*attr != '=')
						attr++;
					while(*attr == ' ' || *attr == '\t' || *attr == '=')
						attr++;

					if(*attr == '1')
						spxinfo->penh = 1;
				}
				*/
			}

			spxsdp_log(("speex_info_from_sdp: vbr[%d] cng[%d] penh[%d]\n", spxinfo->vbr, spxinfo->cng, spxinfo->penh));

			fmtp_ok = 1;
			if(ptime_ok)
				break;
		}
					
		a = sdp_message_a_att_field_get (sdp, pos_media, ++pos_attr);
	}

	return pos_media;
}

