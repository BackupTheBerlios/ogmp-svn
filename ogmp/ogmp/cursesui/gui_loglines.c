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


#include "gui_loglines.h"

gui_t gui_window_loglines = 
{
	GUI_DISABLED,
	0,
	-999,
	-3,
	0,
	NULL,
	window_loglines_print,
	NULL,
	NULL,
	NULL,
	-1,
	-1,
	-1,
	NULL,
	NULL
};

char log_buf[LOG_MAXLINE][LOG_MAXLEN];
int log_ln1=0, log_lnn=0;
int log_maxline = LOG_MAXLINE, log_maxlen = LOG_MAXLEN, log_nline = 0;

int cursor_log_view;
int log_nview = 3;

int window_loglines_print(gui_t* gui, int wid)
{
	char buf1[200];
	int x, y;

	int ln, n;

	gui->parent = wid;
  
	getmaxyx(stdscr,y,x);

	snprintf(buf1,199, "%199.199s", " ");

	xthr_lock(log_lock);

	ln = log_lnn;
	n = 0;

	while(log_buf[ln][0] != '\0')
	{
		if(n == log_nview)
			break;

		/* int xpos; */
		attrset(COLOR_PAIR(4));

		mvaddnstr(y-1-n, 0, buf1, x);
		mvaddstr(y-1-n, 0, log_buf[ln]);

		ln = (ln + log_maxline - 1) % log_maxline;
		n++;
	}

	xthr_unlock(log_lock);

	refresh();

	return 0;
}

gui_t* window_loglines_new(ogmp_curses_t* topui)
{
	gui_window_loglines.topui = topui;

	log_lock = xthr_new_lock();

	return &gui_window_loglines;
}

int window_loglines_done(gui_t* gui)
{
	xthr_done_lock(log_lock);

	return 0;
}

