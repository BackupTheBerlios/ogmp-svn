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


#include "gui_topline.h"

int window_topline_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	gui->parent = wid;

	sprintf(buf,"Josua evolution 0.0.1 - by Heming Ling %-50.50s"," ");
  
	getmaxyx(stdscr,y,x);
  
	attrset(A_NORMAL);
	attrset(COLOR_PAIR(1));
	mvaddnstr(gui->y0, gui->x0, buf, x);
  
	return 0;
}

int window_topline_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_topline_new(ogmp_curses_t* topui)
{
	gui_window_topline.topui = topui;

	return &gui_window_topline;
}

int window_topline_done(gui_t* gui)
{
	return 0;
}

gui_t gui_window_topline =
{
	GUI_DISABLED,
	0,
	-999,
	0,
	1,
    window_topline_event,
	NULL,
	window_topline_print,
	NULL,
	NULL,
	NULL,
	-1,
	-1,
	-1,
	NULL
};
