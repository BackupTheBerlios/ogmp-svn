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

#include "gui_online.h"

int josua_online_status = EXOSIP_NOTIFY_ONLINE;
int josua_registration_status = -1;
char josua_registration_reason_phrase[100] = { '\0' };
char josua_registration_server[100] = { '\0' };

gui_t gui_window_online = 
{
  GUI_DISABLED,
  0,
  -999,
  -3,
  0,
  NULL,
  &window_online_print,
  NULL,
  NULL,
  NULL,
  -1,
  -1,
  -1,
  NULL
};

int window_online_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];

	ogmp_curses_t* ocui = gui->topui;
  
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	gui->parent = wid;

	getmaxyx(stdscr,y,x);
  
	attrset(A_NORMAL);
	attrset(COLOR_PAIR(14));

	snprintf(buf, x, "Current ID :   %s %140.140s", ocui->user_profile->fullname, " ");
	mvaddnstr(y + gui->y0, 0, buf, x);

	if (josua_online_status==EXOSIP_NOTIFY_UNKNOWN)
		snprintf(buf, x, "IM status:   %s %140.140s", "unknown", " ");
	else if (josua_online_status==EXOSIP_NOTIFY_PENDING)
		snprintf(buf, x, "IM status:   %s %140.140s", "pending", " ");
	else if (josua_online_status==EXOSIP_NOTIFY_ONLINE)
		snprintf(buf, x, "IM status:   %s %140.140s", "Online", " ");
	else if (josua_online_status==EXOSIP_NOTIFY_BUSY)
		snprintf(buf, x, "IM status:   %s %140.140s", "Busy", " ");
	else if (josua_online_status==EXOSIP_NOTIFY_BERIGHTBACK)
		snprintf(buf, x, "IM status:   %s %140.140s", "Be Right Back", " ");
	else if (josua_online_status==EXOSIP_NOTIFY_AWAY)
		snprintf(buf, x, "IM status:   %s %140.140s", "Away", " ");
	else if (josua_online_status==EXOSIP_NOTIFY_ONTHEPHONE)
		snprintf(buf, x, "IM status:   %s %140.140s", "On The Phone", " ");
	else if (josua_online_status==EXOSIP_NOTIFY_OUTTOLUNCH)
		snprintf(buf, x, "IM status:   %s %140.140s", "Out To Lunch", " ");
	else if (josua_online_status==EXOSIP_NOTIFY_CLOSED)
		snprintf(buf, x, "IM status:   %s %140.140s", "Closed", " ");
	else
		snprintf(buf, x, "IM status:   ?? %140.140s" , " ");
  
	mvaddnstr(y + gui->y0 + 1, 0, buf, x);

	if (ocui->user_profile->reg_status == SIPUA_STATUS_NORMAL)
    {
		snprintf(buf, x, "registred:   --Not registred-- %140.140s", " ");
		mvaddnstr(y + gui->y0 + 2, 0, buf, x);
    }
	else if (ocui->user_profile->reg_status == SIPUA_STATUS_REG_OK)
	{
		if (199 < josua_registration_status && josua_registration_status < 300)
		{
			snprintf(buf, x, "registred:   [%i %s] %s %140.140s", 
							josua_registration_status, 
							josua_registration_reason_phrase,
							josua_registration_server, " ");
			mvaddnstr(y + gui->y0 + 2, 0, buf, x);
		}
		else if (josua_registration_status > 299)
		{
			snprintf(buf, x, "registred:   [%i %s] %s %140.140s",
							josua_registration_status,
							josua_registration_reason_phrase,
							josua_registration_server, " ");
			mvaddnstr(y + gui->y0 + 2, 0, buf, x);
		}
	}
	else if (ocui->user_profile->reg_status == SIPUA_STATUS_REG_DOING)
	{
		snprintf(buf, x, "registred:   registering... %140.140s", " ");
		mvaddnstr(y + gui->y0 + 2, 0, buf, x);
	}
	else if (ocui->user_profile->reg_status == SIPUA_STATUS_UNREG_DOING)
	{
		snprintf(buf, x, "registred:   unregistering... %140.140s", " ");
		mvaddnstr(y + gui->y0 + 2, 0, buf, x);
	}
	else if (josua_registration_status==0)
    {
		snprintf(buf, x, "registred:   [no answer] %s %140.140s",
							josua_registration_server, " ");
		mvaddnstr(y + gui->y0 + 2, 0, buf, x);
    }

	return 0;
}

gui_t* window_online_new(ogmp_curses_t* topui)
{
	gui_window_online.topui = topui;

	return &gui_window_online;
}

int window_online_done(gui_t* gui)
{
	return 0;
}

