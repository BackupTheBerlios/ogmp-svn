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

#include "gui_message.h"

int window_message_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];

	ogmp_curses_t* ocui = gui->topui;
  
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	gui->parent = wid;

	gui_print_box(gui, -1, 1);

	getmaxyx(gui->win,y,x);

	wattrset(gui->win, A_NORMAL);

	/* Window Title */
	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(4));
	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0-1));
	snprintf(buf, x-gui->x0-1, "Message");
	mvaddstr(gui->y0, gui->x0+1, buf);

	/* Window Body */
	attrset(COLOR_PAIR(1));
	mvaddnstr(gui->y0+2, 5, ocui->message, x-1);
  
	gui->gui_draw_commands(gui);
  
	return 0;
}

void window_message_draw_commands(gui_t* gui)
{
	int x,y;
	char *login_commands[] = 
	{
		"CR",  "OK",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(login_commands, y-5, 0);
}

int window_message_run_command(gui_t* gui, int c)
{
	ogmp_curses_t *ocui = gui->topui;

	switch (c)
    {
		case KEY_ENTER:
		{			
			xfree(ocui->message);
			ocui->message = NULL;


			gui_hide_window(gui);

			break;
		}
		default:
		{
			beep();

			return -1;
		}
    }
  
	return 0;
}

int window_message_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_message_new(ogmp_curses_t* topui)
{
	gui_window_message.topui = topui;

	return &gui_window_message;
}

int window_message_done(gui_t* gui)
{
	return 0;
}

gui_t gui_window_message =
{
	GUI_OFF,
	0,
	-999,
	16,
	-6,
    window_message_event,
	NULL,
	window_message_print,
	window_message_run_command,
	NULL,
	window_message_draw_commands,
	-1,
	-1,
	-1,
	NULL,
	NULL
};
