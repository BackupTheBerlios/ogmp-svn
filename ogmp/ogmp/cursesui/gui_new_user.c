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

#include "gui_new_user.h"

#include "editor.h"

#define MAX_LINE 128

#define INPUT_UID 0
#define INPUT_PWD 1
#define INPUT_LOC 2

char uid[128];
char pwd[128];
char loc[128];

char* inputs[3];
editline_t *edit[3];

int cursor_user = 0;

int window_new_user_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[256];

	char *ch, c;
	int pos;
		
	ogmp_curses_t* ocui = gui->topui;

	gui->parent = wid;

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	if(ocui->gui_windows[EXTRAGUI] != NULL)
		josua_clear_box_and_commands(ocui->gui_windows[EXTRAGUI]);

	getmaxyx(stdscr,y,x);

	if (gui->x1==-999)
    {}
	else 
		x = gui->x1;

	pos = editline_pos(edit[cursor_user]);
	editline_char(edit[cursor_user], &ch);

	if(!*ch)
		c = ' ';
	else
		c = *ch;

	attrset(A_NORMAL);

	/* Window Title */
	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(4));
	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0-1));
	snprintf(buf, x-gui->x0-1, "Create a new user");
	mvaddstr(gui->y0, gui->x0+1, buf);

	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(0));
	mvaddnstr(gui->y0+1, gui->x0, buf, (x-gui->x0-1));
	mvaddnstr(gui->y0+2, gui->x0, buf, (x-gui->x0-1));
	mvaddnstr(gui->y0+3, gui->x0, buf, (x-gui->x0-1));

	if(cursor_user == INPUT_UID)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%19.19s", "User ID :");
	mvaddstr(gui->y0+1, gui->x0, buf);
  
	if(cursor_user == INPUT_PWD)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%19.19s", "Password :");
	mvaddstr(gui->y0+2, gui->x0, buf);
  
	if(cursor_user == INPUT_LOC)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%19.19s", "Profile Location :");
	mvaddstr(gui->y0+3, gui->x0, buf);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+1, gui->x0+20, inputs[INPUT_UID]);
	if(cursor_user == INPUT_UID)
	{
		attrset(COLOR_PAIR(10));
		mvaddch(gui->y0+1, gui->x0+20+pos, c);
	}

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+2, gui->x0+20, inputs[INPUT_PWD]);
	if(cursor_user == INPUT_PWD)
	{
		attrset(COLOR_PAIR(10));
		mvaddch(gui->y0+2, gui->x0+20+pos, c);
	}

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+3, gui->x0+20, inputs[INPUT_LOC]);
	if(cursor_user == INPUT_LOC)
	{
		attrset(COLOR_PAIR(10));
		mvaddch(gui->y0+3, gui->x0+20+pos, c);
	}

	gui->gui_draw_commands(gui);
  
	return 0;
}

void window_new_user_draw_commands(gui_t* gui)
{
	int x,y;
	char *new_user_commands[] = 
	{
		"<-",  "PrevWindow",

		"->",  "NextWindow",
		"^A",  "Create",
		"^E",  "Cancel" ,
		"^D",  "Clear Input",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(new_user_commands, y-5, 0);
}

int window_new_user_run_command(gui_t* gui, int c)
{
	int y,x,max;
	ogmp_curses_t* ocui = gui->topui;
  
	getmaxyx(stdscr,y,x);

	if(gui->x1 != -999)
		x = gui->x1;

	if(gui->xcursor == -1)
		gui->xcursor = 20;

	if(gui->ycursor == -1)
		gui->ycursor = 0;

	max = 3;

	switch (c)
    {
		case KEY_DC:

		{
			editline_remove_char(edit[cursor_user]);
			
			delch();

			break;
		}
		case '\b':
		{
			if (editline_move_pos(edit[cursor_user], -1) >= 0)
				editline_remove_char(edit[cursor_user]);
			else
				beep();

			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		case KEY_DOWN:
		{
			cursor_user++;
			cursor_user %= max;

			break;
		}
		case KEY_UP:
		{
			cursor_user += max-1;
			cursor_user %= max;

			if(cursor_user < 0)
				cursor_user = 0;

			break;
		}
		case KEY_RIGHT:
		{
			if (editline_move_pos(edit[cursor_user], 1) < 0)
				beep();

			break;
		}
		case KEY_LEFT:
		{
			if (editline_move_pos(edit[cursor_user], -1) < 0)
				beep();

			break;
		}
		case 1:  /* Ctrl-A */
		{
			if(ocui->message)
				xfree(ocui->message);

			if(!uid[0] || !pwd[0] || !loc[0])
			{
				ocui->message = xstr_clone("Not enough input!");
			}
			else
			{
				ocui->user = user_new(uid, strlen(uid));

				if(sipua_save_user(ocui->user, loc, pwd, strlen(pwd)) < UA_OK)
					ocui->message = xstr_clone("Fail to create new user!");
				else
					ocui->message = xstr_clone("New user created.");
			}

			gui_show_window(gui, GUI_MESSAGE, GUI_NEWUSER);

			break;
		}
		case 4:  /* Ctrl-D */
		{
			editline_clear(edit[cursor_user]);
	
			break;
		}
		case 5:  /* Ctrl-E */
		{
			gui->xcursor = 20;
			gui->ycursor = 0;
			/*
			window_new_identity_print(gui);
			*/
			break;
		}
		default:
		{
			if(editline_append(edit[cursor_user], (char*)&c, 1) == 0)
				beep();
	
			return -1;
		}
	}

	return 0;
}

int window_new_user_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_new_user_new(ogmp_curses_t* topui)
{
	gui_window_new_user.topui = topui;

	memset(inputs, 0, sizeof(inputs));

	edit[INPUT_UID] = editline_new(uid, MAX_LINE);
	edit[INPUT_PWD] = editline_new(pwd, MAX_LINE);
	edit[INPUT_LOC] = editline_new(loc, MAX_LINE);

	inputs[INPUT_UID] = uid;
	inputs[INPUT_PWD] = pwd;
	inputs[INPUT_LOC] = loc;

	return &gui_window_new_user;
}

int window_new_user_done(gui_t* gui)
{
	editline_done(edit[INPUT_UID]);
	editline_done(edit[INPUT_PWD]);
	editline_done(edit[INPUT_LOC]);

	return 0;
}

gui_t gui_window_new_user =
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
    window_new_user_event,
	NULL,
	window_new_user_print,
	window_new_user_run_command,
	NULL,
	window_new_user_draw_commands,
	-1,
	-1,
	-1,
	NULL
};
