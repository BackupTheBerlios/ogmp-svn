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

#include "gui_login.h"

#include "editor.h"

gui_t gui_window_login = 
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
	NULL,
	window_login_print,
	window_login_run_command,
	NULL,
	window_login_draw_commands,
	-1,
	-1,
	-1,
	NULL
};

#define LINE_MAX 128

#define INPUT_UID 0
#define INPUT_PWD 1
#define INPUT_LOC 2

char uid[128];
char pwd[128];
char loc[128];

char* inputs[3];
editline_t *edit[3];

int cursor_inputs = 0;

int window_login_print(gui_t* gui, int wid)
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

	pos = editline_pos(edit[cursor_inputs]);

	editline_char(edit[cursor_inputs], &ch);
	if(!*ch)
		c = ' ';
	else
		c = *ch;

	attrset(A_NORMAL);

	/* Window Title */
	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(4));
	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0));
	snprintf(buf, x-gui->x0-1, "Login");
	mvaddstr(gui->y0, gui->x0+1, buf);

	/* Window Body */
	attrset(COLOR_PAIR(0));

	snprintf(buf, 250, "%199.199s", " ");
	mvaddnstr(gui->y0+1, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+2, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+3, gui->x0, buf, (x-gui->x0));

	if(cursor_inputs == INPUT_UID)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%19.19s", "User ID :");
	mvaddstr(gui->y0+1, gui->x0, buf);
  
	if(cursor_inputs == INPUT_PWD)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%19.19s", "Password :");
	mvaddstr(gui->y0+2, gui->x0, buf);
  
	if(cursor_inputs == INPUT_LOC)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%19.19s", "Profile Location :");
	mvaddstr(gui->y0+3, gui->x0, buf);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+1, gui->x0+20, inputs[INPUT_UID]);
	if(cursor_inputs == INPUT_UID)
	{
		attrset(COLOR_PAIR(10));
		mvaddch(gui->y0+1, gui->x0+20+pos, c);
	}

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+2, gui->x0+20, inputs[INPUT_PWD]);
	if(cursor_inputs == INPUT_PWD)
	{
		attrset(COLOR_PAIR(10));
		mvaddch(gui->y0+2, gui->x0+20+pos, c);
	}

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+3, gui->x0+20, inputs[INPUT_LOC]);
	if(cursor_inputs == INPUT_LOC)
	{
		attrset(COLOR_PAIR(10));
		mvaddch(gui->y0+3, gui->x0+20+pos, c);
	}

	gui->gui_draw_commands(gui);
  
	return 0;
}

void window_login_draw_commands(gui_t* gui)
{
	int x,y;
	char *login_commands[] = 
	{
		"^A",  "OK",
		"^C",  "Cancel" ,
		"^D",  "ClearLine",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(login_commands, y-5, 0);
}

int window_login_run_command(gui_t* gui, int c)
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
			editline_remove_char(edit[cursor_inputs]);
			
			delch();

			break;
		}
		case '\b':
		{
			if (editline_move_pos(edit[cursor_inputs], -1) >= 0)
				editline_remove_char(edit[cursor_inputs]);
			else
				beep();

			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		case KEY_DOWN:
		{
			cursor_inputs++;
			cursor_inputs %= max;

			break;
		}
		case KEY_UP:
		{
			cursor_inputs += max-1;
			cursor_inputs %= max;

			break;
		}
		case KEY_RIGHT:
		{
			if (editline_move_pos(edit[cursor_inputs], 1) < 0)
				beep();

			break;
		}
		case KEY_LEFT:
		{
			if (editline_move_pos(edit[cursor_inputs], -1) < 0)
				beep();

			break;
		}
		case 1:  /* Ctrl-A */
		{
			if(uid[0] && pwd[0] && loc[0] && !ocui->user)
			{
				ocui->user = sipua_load_user(loc, uid, pwd, strlen(pwd));
				
				if(ocui->user)
				{
					ocui->user->userloc = ocui->sipua->userloc(ocui->sipua, ocui->user->uid);
					
					gui_show_window(gui, GUI_PROFILES, GUI_LOGIN);
				}
			}

			break;
		}
		case 3:  /* Ctrl-C */
		{
			gui_hide_window(gui);

			break;
		}
		case 4:  /* Ctrl-D */
		{
			editline_clear(edit[cursor_inputs]);
	
			break;
		}
		default:
		{
			if(editline_append(edit[cursor_inputs], &((char)c), 1) == 0)
				beep();
	
			return -1;
		}
	}

	return 0;
}

gui_t* window_login_new(ogmp_curses_t* topui)
{
	gui_window_login.topui = topui;

	memset(inputs, 0, sizeof(inputs));

	edit[INPUT_UID] = editline_new(uid, LINE_MAX);
	edit[INPUT_PWD] = editline_new(pwd, LINE_MAX);
	edit[INPUT_LOC] = editline_new(loc, LINE_MAX);

	inputs[INPUT_UID] = uid;
	inputs[INPUT_PWD] = pwd;
	inputs[INPUT_LOC] = loc;

	return &gui_window_login;
}

int window_login_done(gui_t* gui)
{
	editline_done(edit[INPUT_UID]);
	editline_done(edit[INPUT_PWD]);
	editline_done(edit[INPUT_LOC]);

	return 0;
}
