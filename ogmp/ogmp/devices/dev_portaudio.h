/***************************************************************************
                          device.h  -  Input/Output
                             -------------------
    begin                : Tue Feb 3 2004
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

#include "media_format.h"

typedef struct pa_setting_s pa_setting_t;
struct pa_setting_s
{
   struct control_setting_s setting;
   
   int time_match;
   int sample_factor;    /* the factor b/w sample rate & nsample_pulse, power of 2 */
   int nbuf_internal;    /* PortAudio Internal Buffer number, for performance tuning */
   
   int inbuf_n;          /* input buffer number */
};
