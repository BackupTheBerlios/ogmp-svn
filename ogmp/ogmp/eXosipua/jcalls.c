/*
 * josua - Jack's open sip User Agent
 *
 * Copyright (C) 2002,2003   Aymeric Moizard <jack@atosc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include "eXosipua.h"
#include "../rtp_cap.h"

//jcall_t jcalls[MAX_NUMBER_OF_CALLS];
//char   _localip[30];

//static int ___call_init = 0;

#define MAX_MIME_BYTES 64
#define MAX_IP_BYTES  64

int jcall_init(eXosipua_t *jua)
{
	int k;
	for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
		memset(&(jua->jcalls[k]), 0, sizeof(jcall_t));
		jua->jcalls[k].state = NOT_USED;
    }

	eXosip_get_localip(jua->localip);
  
	return 0;
}

void __call_free()
{
}

#if defined(ORTP_SUPPORT) /*|| defined(WIN32)*/
void
rcv_telephone_event(RtpSession *rtp_session, jcall_t *ca)
{
  /* telephone-event received! */
  
}
#endif

int
jcall_get_number_of_pending_calls(eXosipua_t *jua)
{
	int pos=0;
	int k;
	for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
		if (jua->jcalls[k].state != NOT_USED)
		{
			pos++;
		}
    }

	return pos;
}

jcall_t *jcall_find_call(eXosipua_t *jua, int pos)
{
  int k;
  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
  {
      if (jua->jcalls[k].state != NOT_USED)
	  {
		if (pos==0)
			return &(jua->jcalls[k]);
		pos--;
	  }
  }
  return NULL;
}

int jcall_new(eXosipua_t *jua, eXosip_event_t *je)
{
	jcall_t *ca;
	int k;

	for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
		if (jua->jcalls[k].state == NOT_USED)
			break;
    }
  
	if (k==MAX_NUMBER_OF_CALLS)
		return -1;

	ca = &(jua->jcalls[k]);
	memset(&(jua->jcalls[k]), 0, sizeof(jcall_t));

	ca->cid = je->cid;
	ca->did = je->did;

	if (ca->did<1 && ca->cid<1)
    {
		exit(0);
		return -1; /* not enough information for this event?? */
    }

	osip_strncpy(ca->textinfo,   je->textinfo, 255);
	osip_strncpy(ca->req_uri,    je->req_uri, 255);
	osip_strncpy(ca->local_uri,  je->local_uri, 255);
	osip_strncpy(ca->remote_uri, je->remote_uri, 255);
	osip_strncpy(ca->subject,    je->subject, 255);

	if (ca->remote_sdp_audio_ip[0]=='\0')
    {
		osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
		ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
		ca->payload = je->payload;
		osip_strncpy(ca->payload_name, je->payload_name, 49);

#if defined(MEDIASTREAMER_SUPPORT)
		if (0!=os_sound_start(ca))
		{
		}
		else
		{
			ca->enable_audio=1; /* audio is started */
		}
#elif defined(XRTP_SUPPORT)
		{
			/* Retrieve capable from the sdp message */

			sdp_message_t *sdp;
			sdp_message_init(&sdp);

			if (sdp_message_parse(sdp, je->sdp_body) != 0)
			{
				/* No sdp message found, in this case, caller could require the capable of the callee 
				 * When answer the call, reture all media capabilities. 
				 */
			}
			else
			{
				/* generate capable_descript from sdp */
				char *media_type; /* "audio"; "video"; etc */

				uint media_port = 0;
				uint control_port = 0;

				int payload_no = 0;

				char *protocol; /* "RTP/AVP" */

				char *attr = NULL;
				char *rtpmap = NULL;

				int rtpmapno = 0;

				char coding_type[MAX_MIME_BYTES] = "";

				int clockrate;
				int coding_param;

				char mime[MAX_MIME_BYTES] = "";

				char media_ip[MAX_IP_BYTES] = "";
				char control_ip[MAX_IP_BYTES] = "";

				int pos_media = 0;
				while (!sdp_message_endof_media (sdp, pos_media) && ca->ncap < MAX_CAPABLES)
				{
					int pos_attr = 0;
					sdp_media_t *sdp_media;
					sdp_connection_t *connect;

					int maskbits;
					int rtpmap_ok, rtcp_ok;

					media_type = sdp_message_m_media_get (sdp, pos_media);
					media_port = (uint)strtol(sdp_message_m_port_get (sdp, pos_media), NULL, 10);
					payload_no = atoi(sdp_message_m_number_of_port_get (sdp, pos_media));
					protocol = sdp_message_m_proto_get (sdp, pos_media);

					sdp_media = osip_list_get(sdp->m_medias, pos_media);
					if(sdp_media->c_connections != NULL)
						connect = osip_list_get(sdp_media->c_connections, 0);
					else
						connect = sdp->c_connection;

					if(0 == strcmp(connect->c_addrtype, "IP4"))
						jua_parse_ipv4(connect->c_addr, media_ip, MAX_IP_BYTES, &maskbits);

					/*
					if(0 == strcmp(connect->c_addrtype, "IP6"))
						jua_parse_ipv6(connect->c_addr, media_ip, ...);
					*/

					/* find rtpmap attribute */
					rtpmap_ok = rtcp_ok = 0;
					attr = sdp_message_a_att_field_get (sdp, pos_media, pos_attr);
					while(attr)
					{
						if(0 == strcmp(attr, "rtpmap"))
						{
							rtpmap = sdp_message_a_att_value_get(sdp, pos_media, pos_attr);
							sdp_parse_rtpmap(rtpmap, &rtpmapno, coding_type, &clockrate, &coding_param);
							rtpmap_ok = 1;
							if(rtcp_ok)
								break;
						}
					
						if(0 == strcmp(attr, "rtcp"))
						{
							char *rtcp = sdp_message_a_att_value_get(sdp, pos_media, pos_attr);
							jua_parse_rtcp(rtcp, control_ip, MAX_IP_BYTES, &control_port);
							rtcp_ok = 1;
							if(rtpmap_ok)
								break;
						}
					
						attr = sdp_message_a_att_field_get (sdp, pos_media, ++pos_attr);
					}

					if(rtpmapno == payload_no)
					{
						snprintf(mime, MAX_MIME_BYTES, "%s/%s", media_type, coding_type);

						if(control_port == 0)
							control_port = media_port + 1;

						/* open issue, rtcp and rtp flow may use different ip */
						ca->capables[ca->ncap++] = (capable_descript_t*)rtp_capable_descript(payload_no, media_ip, 
																							media_port, control_port, 
																							mime, clockrate, coding_param);
					}

					pos_media++;
				}
			}
		}
#endif
    }

	if (je->reason_phrase[0]!='\0')
    {
		osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
		ca->status_code = je->status_code;
    }

	ca->state = je->type;

	return 0;
}

int jcall_remove(eXosipua_t *jua, jcall_t *ca)
{
	if (ca==NULL)
		return -1;

#if defined(MEDIASTREAMER_SUPPORT)
	if (ca->enable_audio>0)
    {
		ca->enable_audio = -1;
		os_sound_close(ca);
    }
#elif defined(XRTP_SUPPORT)
	{
		int i;
		for(i=0; i<ca->ncap; i++)
			ca->capables[i]->done(ca->capables[i]);

		ca->ncap = 0;
	}
#endif

	ca->state = NOT_USED;
	return 0;
}

int jcall_proceeding(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid
	  && jua->jcalls[k].did==je->did)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    {
      for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
	{
	  if (jua->jcalls[k].state == NOT_USED)
	    break;
	}
      if (k==MAX_NUMBER_OF_CALLS)
	return -1;

      ca = &(jua->jcalls[k]);
      memset(&(jua->jcalls[k]), 0, sizeof(jcall_t));
      
      ca->cid = je->cid;
      ca->did = je->did;
      
      if (ca->did<1 && ca->cid<1)
	{
	  exit(0);
	  return -1; /* not enough information for this event?? */
	}
    }

  ca = &(jua->jcalls[k]);
  osip_strncpy(ca->textinfo,   je->textinfo, 255);
  osip_strncpy(ca->req_uri,    je->req_uri, 255);
  osip_strncpy(ca->local_uri,  je->local_uri, 255);
  osip_strncpy(ca->remote_uri, je->remote_uri, 255);
  osip_strncpy(ca->subject,    je->subject, 255);

  if (ca->remote_sdp_audio_ip[0]=='\0')
    {
      osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
      ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
      ca->payload = je->payload;
      osip_strncpy(ca->payload_name, je->payload_name, 49);
    }

  if (je->reason_phrase[0]!='\0')
    {
      osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
      ca->status_code = je->status_code;
    }
  ca->state = je->type;
  return 0;

}

int jcall_ringing(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid
	  && jua->jcalls[k].did==je->did)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    {
      for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
	{
	  if (jua->jcalls[k].state == NOT_USED)
	    break;
	}
      if (k==MAX_NUMBER_OF_CALLS)
	return -1;
      ca = &(jua->jcalls[k]);
      memset(&(jua->jcalls[k]), 0, sizeof(jcall_t));
      
      ca->cid = je->cid;
      ca->did = je->did;
      
      if (ca->did<1 && ca->cid<1)
	{
	  exit(0);
	  return -1; /* not enough information for this event?? */
	}
    }

  ca = &(jua->jcalls[k]);
  osip_strncpy(ca->textinfo,   je->textinfo, 255);
  osip_strncpy(ca->req_uri,    je->req_uri, 255);
  osip_strncpy(ca->local_uri,  je->local_uri, 255);
  osip_strncpy(ca->remote_uri, je->remote_uri, 255);
  osip_strncpy(ca->subject,    je->subject, 255);

  if (ca->remote_sdp_audio_ip[0]=='\0')
    {
      osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
      ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
      ca->payload = je->payload;
      osip_strncpy(ca->payload_name, je->payload_name, 49);
    }

  if (je->reason_phrase[0]!='\0')
    {
      osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
      ca->status_code = je->status_code;
    }
  ca->state = je->type;;
  return 0;
}

int jcall_answered(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid
	  && jua->jcalls[k].did==je->did)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    {
      for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
	{
	  if (jua->jcalls[k].state == NOT_USED)
	    break;
	}
      if (k==MAX_NUMBER_OF_CALLS)
	return -1;
      ca = &(jua->jcalls[k]);
      memset(&(jua->jcalls[k]), 0, sizeof(jcall_t));
      
      ca->cid = je->cid;
      ca->did = je->did;
      
      if (ca->did<1 && ca->cid<1)
	{
	  exit(0);
	  return -1; /* not enough information for this event?? */
	}
    }

  ca = &(jua->jcalls[k]);
  osip_strncpy(ca->textinfo,   je->textinfo, 255);
  osip_strncpy(ca->req_uri,    je->req_uri, 255);
  osip_strncpy(ca->local_uri,  je->local_uri, 255);
  osip_strncpy(ca->remote_uri, je->remote_uri, 255);
  osip_strncpy(ca->subject,    je->subject, 255);

  if (ca->remote_sdp_audio_ip[0]=='\0')
    {
      osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
      ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
      ca->payload = je->payload;
      osip_strncpy(ca->payload_name, je->payload_name, 49);

#ifdef MEDIASTREAMER_SUPPORT
      if (0!=os_sound_start(ca))
	{
	}
      else
	{
	  ca->enable_audio=1; /* audio is started */
	}
#endif
    }

  if (je->reason_phrase[0]!='\0')
    {
      osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
      ca->status_code = je->status_code;
    }
  ca->state = je->type;
  return 0;
}

int jcall_redirected(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid
	  && jua->jcalls[k].did==je->did)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    return -1;

  ca = &(jua->jcalls[k]);

#ifdef MEDIASTREAMER_SUPPORT
  if (ca->enable_audio>0)
    {
      ca->enable_audio = -1;
      os_sound_close(ca);
    }
#endif

  ca->state = NOT_USED;
  return 0;
}

int jcall_requestfailure(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid
	  && jua->jcalls[k].did==je->did)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    return -1;

  ca = &(jua->jcalls[k]);

#ifdef MEDIASTREAMER_SUPPORT
  if (ca->enable_audio>0)
    {
      ca->enable_audio = -1;
      os_sound_close(ca);
    }
#endif

  ca->state = NOT_USED;
  return 0;
}

int jcall_serverfailure(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid
	  && jua->jcalls[k].did==je->did)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    return -1;

  ca = &(jua->jcalls[k]);


#ifdef MEDIASTREAMER_SUPPORT
  if (ca->enable_audio>0)
    {
      ca->enable_audio = -1;
      os_sound_close(ca);
    }
#endif

  ca->state = NOT_USED;
  return 0;
}

int jcall_globalfailure(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid
	  && jua->jcalls[k].did==je->did)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    return -1;

  ca = &(jua->jcalls[k]);

#ifdef MEDIASTREAMER_SUPPORT
  if (ca->enable_audio>0)
    {
      ca->enable_audio = -1;
      os_sound_close(ca);
    }
#endif

  ca->state = NOT_USED;
  return 0;
}

int jcall_closed(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    return -1;

  ca = &(jua->jcalls[k]);

#ifdef MEDIASTREAMER_SUPPORT
  if (ca->enable_audio>0)
    {
      ca->enable_audio = -1;
      os_sound_close(ca);
    }
#endif

  ca->state = NOT_USED;
  return 0;
}

int jcall_onhold(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid
	  && jua->jcalls[k].did==je->did)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    return -1;

  ca = &(jua->jcalls[k]);
  osip_strncpy(ca->textinfo,   je->textinfo, 255);

  if (ca->remote_sdp_audio_ip[0]=='\0')
    {
      osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
      ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
    }

  if (je->reason_phrase[0]!='\0')
    {
      osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
      ca->status_code = je->status_code;
    }
  ca->state = je->type;
  return 0;
}

int jcall_offhold(eXosipua_t *jua, eXosip_event_t *je)
{
  jcall_t *ca;
  int k;

  for (k=0;k<MAX_NUMBER_OF_CALLS;k++)
    {
      if (jua->jcalls[k].state != NOT_USED
	  && jua->jcalls[k].cid==je->cid
	  && jua->jcalls[k].did==je->did)
	break;
    }
  if (k==MAX_NUMBER_OF_CALLS)
    return -1;

  ca = &(jua->jcalls[k]);
  osip_strncpy(ca->textinfo,   je->textinfo, 255);

  if (ca->remote_sdp_audio_ip[0]=='\0')
    {
      osip_strncpy(ca->remote_sdp_audio_ip, je->remote_sdp_audio_ip, 49);
      ca->remote_sdp_audio_port = je->remote_sdp_audio_port;
    }

  if (je->reason_phrase[0]!='\0')
    {
      osip_strncpy(ca->reason_phrase, je->reason_phrase, 49);
      ca->status_code = je->status_code;
    }
  ca->state = je->type;
  return 0;
}

