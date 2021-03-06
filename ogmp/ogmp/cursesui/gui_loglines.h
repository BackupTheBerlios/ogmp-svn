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

#ifndef __GUI_LOGLINES_H__
#define __GUI_LOGLINES_H__

#include "gui.h"

#define LOG_MAXLINE 10
#define LOG_MAXLEN	100

char log_buf[LOG_MAXLINE][LOG_MAXLEN];
int log_maxline, log_maxlen, log_nline;
int log_ln1, log_lnn;

int window_loglines_print(gui_t* gui, int wid);

gui_t* window_loglines_new(ogmp_curses_t* topui);
int window_loglines_done(gui_t* gui);

extern gui_t gui_window_loglines;

#endif

