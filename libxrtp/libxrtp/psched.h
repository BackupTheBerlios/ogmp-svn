/***************************************************************************
                          schedule.h  -  Time schedule for rtp packet
                             -------------------
    begin                : Mon Mar 10 2003
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

/**
 * The scheduler makes sure the media unit would be sent in time, This involve
 * sereval packet sending, as each media unit could be packet in to more rtp pack
 * all these packet has same timestamp as the first one, but with increamental
 * seqno, on receive end, they will be reassambled to one media unit. all need to
 * be done during the media unit duration, in any condition, a media report call
 * will arise when deadline arrives.
 */

 typedef struct session_sched_s session_sched_t;
 typedef struct sched_schedinfo_s sched_schedinfo_t;
 //typedef struct sched_rtcp_schedinfo_s sched_rtcp_schedinfo_t;

 struct session_sched_s
 {
    int (*done)(session_sched_t * sched);

    int (*add)(session_sched_t * sched, xrtp_session_t * session);

    int (*remove)(session_sched_t * sched, xrtp_session_t * session);

    /* If sync is true, catchup with the ts */
    int (*rtp_out)(session_sched_t * sched, xrtp_session_t * session, xrtp_hrtime_t ts, int sync);

    int (*rtp_in)(session_sched_t * sched, xrtp_session_t * session, xrtp_hrtime_t ts);

    int (*rtcp_out)(session_sched_t * sched, xrtp_session_t * session);

    int (*rtcp_in)(session_sched_t * sched, xrtp_session_t * session, xrtp_lrtime_t ts);
 };

 session_sched_t * sched_new();
