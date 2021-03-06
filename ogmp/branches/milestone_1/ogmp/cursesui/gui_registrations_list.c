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

#include "gui_registrations_list.h"
#include "gui_new_identity.h"

/* extern eXosip_t eXosip; */

gui_t gui_window_registrations_list = 
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
	NULL,
	window_registrations_list_print,
	window_registrations_list_run_command,
	NULL,
	window_registrations_list_draw_commands,
	-1,
	-1,
	-1,
	NULL
};

int cursor_registrations_list = 0;
int cursor_registrations_start  = 0;
int last_reg_id = -1;

user_profile_t *selected = NULL;

void window_registrations_list_draw_commands(gui_t* gui)
{
	int x,y;
	char *registrations_list_commands[] = 
	{
		"<-",  "PrevWindow",
		"->",  "NextWindow",
		"a",  "AddRegistration",
		"d",  "DeleteRegistration",
		"r",  "Register",
		"u",  "Un-Register",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(registrations_list_commands, y-5, 0);
}

int window_registrations_list_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];
	int pos;
	int pos_id;

	user_profile_t* prof;

	ogmp_curses_t* ocui = gui->topui;

	xlist_t* profiles = ocui->user->profiles;
	xlist_user_t u;
  
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	gui->parent = wid;
  
	getmaxyx(stdscr,y,x);
	
	pos_id = 0;
	prof = (user_profile_t*)xlist_first(profiles, &u);
	while(prof)
    {
		if (pos_id == cursor_registrations_start)
			break;

		pos_id++;
		prof = xlist_next(profiles, &u);
    }

	pos = 0;
	while(prof)
    {
		if (pos == y + gui->y1 - gui->y0)
			break;

		if (last_reg_id==pos)
		{
			snprintf(buf, gui->x1 - gui->x0, "%c%c %-5.5s %-30.30s %-80.80s",
						(cursor_registrations_list==pos) ? '-' : ' ',
						(cursor_registrations_list==pos) ? '>' : ' ',
						"[ok]", user->regname, user->registrar);
		}
		else
		{
			snprintf(buf, gui->x1 - gui->x0, "%c%c %-5.5s %-30.30s %-80.80s",
						(cursor_registrations_list==pos) ? '-' : ' ',
						(cursor_registrations_list==pos) ? '>' : ' ',
						"[--]", user->regname, user->registrar);
		}
      
		attrset((pos == cursor_registrations_list) ? A_REVERSE : COLOR_PAIR(1));
      
		mvaddnstr(pos + gui->y0, gui->x0, buf, (x - gui->x0 - 1));
      
		pos++;
		user = xlist_next(profiles, &u);
    }

	gui->gui_draw_commands(gui);
  
	return 0;
}

int window_registrations_list_run_command(gui_t* gui, int c)
{
	int y, x;
	int pos;
	int max;

	xlist_user_t u;
	user_profile_t *user;
	ogmp_curses_t* ocui = gui->topui;
  
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);

	max = xlist_size(ocui->profiles);

	switch (c)
    {
		case KEY_DOWN:
		{
			if (cursor_registrations_list < (y + gui->y1 - gui->y0 - 1) && cursor_registrations_list < max-1)
				cursor_registrations_list++;
			else if (cursor_registrations_start < max - (y + gui->y1 - gui->y0))
				cursor_registrations_start++;
			else 
				beep();
      
			break;
		}
		case KEY_UP:
		{
			if (cursor_registrations_list > 0)
				cursor_registrations_list--;
			else if (cursor_registrations_start > 0)
				cursor_registrations_start--;
			else 
				beep();
      
			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		{
			/* registrations_list selected! */
			pos=0;
			user = xlist_first(ocui->profiles, &u);
			while(user)
			{
				pos++;
				if (cursor_registrations_list==pos)
				{
					if(selected == user)
						selected = NULL;
					else
						selected = user;

					break;
				}

				user = xlist_next(ocui->profiles, &u);
			}
			
			break;
		}
		case 'r':
		{
			/* start registration
			int i = _josua_register(cursor_registrations_list + cursor_registrations_start); 
			if (i!=0) 
			{ 
				beep(); 
				return -1;
			}
			*/
			last_reg_id = cursor_registrations_list + cursor_registrations_start;
      
			break;
		}
		case 'u':
		{
			/* start registration
			int i = _josua_unregister(cursor_registrations_list + cursor_registrations_start);
			if (i!=0) 
			{ 
				beep(); 
				return -1; 
			}
			*/
			last_reg_id = cursor_registrations_list + cursor_registrations_start;
      
			break;
		}
		case 'd':
		{
			/* delete entry */
			break;
		}
		case 'a':
		{
			/* add new entry */
			gui_show_window(gui, GUI_ENTRY, GUI_REGLIST);
			break;
		}
		default:
		{
			beep();
			return -1;
		}
	}

	if (gui->on_off==GUI_ON)
		gui->gui_print(gui, gui->parent);
  
	return 0;
}

gui_t* window_registrations_list_new(ogmp_curses_t* topui)
{
	gui_window_registrations_list.topui = topui;

	return &gui_window_registrations_list;
}

int window_registrations_list_done(gui_t* gui)
{
	return 0;
}

