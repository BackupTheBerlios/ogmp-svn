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


#include "gui_menu.h"
#include "gui_new_call.h"
#include "gui_address_book_browse.h"
#include "gui_sessions_list.h"
#include "gui_registrations_list.h"
#include "gui_subscriptions_list.h"
#include "gui_online.h"
#include "gui_setup.h"

extern struct osip_mutex *log_mutex;

static const menu_t josua_menu[] = 
{
	{ "a", " PHONE BOOK         -  Manage phone book",
		GUI_BROWSE  },
	{ "i", " NEW CALL SESSION   -  Create a call session",
		GUI_NEWCALL },
	{ "u", " SUBSCRIPTIONS LIST  -  View pending subscriptions",
		GUI_SUBS},
	{ "l", " CALL LIST           -  Manage all calls",
		GUI_SESSION },
	{ "r", " PROFILE LIST        -  Change your ID",
		GUI_PROFILES  },
	{ "s", " SETUP               -  Setting",
		GUI_SETUP },
	{ "q", " QUIT                -  Quit",
		GUI_QUIT  },
	{ 0 }
};

static const menu_t login_menu[] = 
{
	{ "t", " MEDIA TEST  -    Test Media Function",
		GUI_AUDIOTEST },
	{ "n", " NEW         -    Create a new user",
		GUI_NEWUSER },
	{ "l", " LOGIN       -    Login as the selected profile",
		GUI_LOGIN },
	{ "q", " QUIT        -    Quit program",
		GUI_QUIT },
	{ 0 }
};

static int cursor_menu = 0;

int window_menu_print(gui_t *gui, int wid)
{
	int y,x, x1;
	char buf[250];
	int i;
	int pos;

	const menu_t* menu;

	ogmp_curses_t* ocui = gui->topui;
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);

	gui->parent = wid;

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);

	if (gui->x1<=0)
		x1 = x;
	else 
		x1 = gui->x1;

	pos = 0;

	if(user_profile)
		menu = josua_menu;
	else
		menu = login_menu;

	for (i=gui->y0; i<gui->y1; i++)
    {
		if(!menu[i-gui->y0].key)
			break;

		snprintf(buf, x1 - gui->x0, "%c%c [%s] %s ",
					(cursor_menu==pos) ? '-' : ' ',
					(cursor_menu==pos) ? '>' : ' ',
					menu[i-gui->y0].key, menu[i-gui->y0].text);

		attrset(COLOR_PAIR(5));
		attrset((pos==cursor_menu) ? A_REVERSE : A_NORMAL);

		mvaddnstr(i, gui->x0, buf, x-gui->x0-1);

		pos++;
    }
  
	return 0;
}

void window_menu_draw_commands(gui_t *gui)
{
	int x,y;
	char *menu_commands[] = 
	{
		"<-",  "PrevWindow",
		"->",  "NextWindow",
		"^i",  "Toggle Online status",
		"^a",  "Apply Status" ,
		NULL
	};

	getmaxyx(stdscr,y,x);

	josua_print_command(menu_commands, y-5, 0);
}

int window_menu_run_command(gui_t *gui, int c)
{
	int max;
	const menu_t* menu;

	ogmp_curses_t* ocui = gui->topui;
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);

	if(user_profile)
	{
		menu = josua_menu;
		max = 7;
	}
	else
	{
		menu = login_menu;
		max = 4;
	}

	switch (c)
    {
		case 9:
		{
			josua_online_status++;

			if (josua_online_status > EXOSIP_NOTIFY_CLOSED)
				josua_online_status = EXOSIP_NOTIFY_ONLINE;

			break;
		}
		case 1:
		{
			/* apply IM change status 
			int k;
			for (k=0; k<MAX_NUMBER_OF_INSUBSCRIPTIONS; k++)
			{
				if (jinsubscriptions[k].state != NOT_USED)
				{
					int i;

					eXosip_lock();

					i = eXosip_notify(jinsubscriptions[k].did, EXOSIP_SUBCRSTATE_ACTIVE, josua_online_status);
					
					if (i!=0) 
						beep();

					eXosip_unlock();
				}
			}
			*/
			break;
		}
		case KEY_DOWN:
			cursor_menu++;
			cursor_menu %= max;
			break;
		case KEY_UP:
			cursor_menu += max-1;
			cursor_menu %= max;
			break;
		case 'a':
			if(user_profile)
				cursor_menu = 0;
			else
				beep();
			break;
		case 'i':
			if(user_profile)
				cursor_menu = 1;
			else
				beep();
			break;
		case 'u':
			if(user_profile)
				cursor_menu = 2;
			else
				beep();
			break;
		case 'l':
			if(user_profile)
				cursor_menu = 3;
			else
				cursor_menu = 0;
			break;
		case 'r':
			if(user_profile)
				cursor_menu = 4;
			else
				beep();
			break;
		case 's':
			if(user_profile)
				cursor_menu = 5;
			else
				beep();
			break;
		case 'q':
			if(user_profile)
				cursor_menu = 6;
			else
				cursor_menu = 1;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
			cursor_menu = c-48;
			break;
		case '4':
		case '5':
		case '6':
			if(user_profile)
				cursor_menu = c-48;
			else
				beep();
			break;
		case '\n':
		case '\r':
		case KEY_ENTER:
			/* menu selected! */
			gui_show_window(gui, menu[cursor_menu].wid, MENUGUI);
			break;
		default:
			beep();
		return -1;
	}

	gui_update(gui);
  
	return 0;
}

int window_menu_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_menu_new(ogmp_curses_t* topui)
{
	gui_window_menu.topui = topui;

	return &gui_window_menu;
}

int window_menu_done(gui_t* gui)
{
	return 0;
}


gui_t gui_window_menu =
{
	GUI_OFF,
	20,
	-999,
	2,
	9,
	window_menu_event,
	NULL,
	window_menu_print,
	window_menu_run_command,
	NULL,
	window_menu_draw_commands,
	-1,
	-1,
	-1,
	NULL
};
