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

struct xrtp_media_s
{
	char * type; /* identical to handler id */
	xrtp_session_t *session;

	int clockrate;
	int coding_parameter;
	int sampling_instance;
	int bps;
     
	int (*match_mime)(xrtp_media_t * media, char *mime);

	int (*done)(xrtp_media_t *media);

	int (*set_parameter)(xrtp_media_t *media, char *key, void *param);
	int (*parameter)(xrtp_media_t *media, char *key, void *param);

	void* (*info)(xrtp_media_t *media, void *rtpcap);
	int (*sdp)(xrtp_media_t *media, void *sdp_info);
	int (*new_sdp)(xrtp_media_t *media, char *nettype, char *addrtype, char *netaddr, int* rtp_portno, int* rtcp_portno, int pt, int clockrate, int coding_param, int bw_budget, void* control, void* sdp_info);

	int (*set_callback)(xrtp_media_t *media, int type, int(*cb)(), void *user);
     
	int (*set_coding)(xrtp_media_t *media, int clockrate, int param);
	int (*coding)(xrtp_media_t *media, int *clockrate, int *param);
     
	uint32 (*sign)(xrtp_media_t * media);

	/* For media, this means the time to display
     * xrtp will schedule the time to send media by this parameter.
     */
	int (*post)(xrtp_media_t * media, media_data_t *data, int bytes, uint32 timestamp);
	int (*play)(void *play, media_data_t *data, int64 count, int ts_last, int eos);
	int (*sync)(void *play, uint32 stamp, uint32 hi_ntp, uint32 lo_ntp);
   
	/**
	 * retrieve current media timestamp;
     */
    uint32 (*timestamp)(xrtp_media_t *media);

	struct media_callbacks_s
	{
        #define CALLBACK_RTPMEDIA_ON_MEMBER_UPDATE      0x1
        #define CALLBACK_RTPMEDIA_ON_REPORT				0x2
        
        /* callid: CALLBACK_MEDIA_FINISH */
        int (*on_member_update)();
        void *on_member_update_user;

        /* callid: CALLBACK_MEDIA_REPORT */
        int (*on_report)();
        void *on_report_user;

    } $callbacks;
};

/* Convert realtime to rtp ts */
uint32 media_realtime_to_rtpts(xrtp_media_t * media, xrtp_lrtime_t lrt, xrtp_hrtime_t hrt);

extern DECLSPEC
int
rtp_media_set_callback(xrtp_media_t *media, int type, int (*cb)(), void* user);
