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

#include "gui_new_identity.h"

#include "editor.h"

#ifndef	LINE_MAX
#define LINE_MAX 128
#endif

#define NEWID_FULLNAME	0
#define NEWID_BOOKLOC	1
#define NEWID_REGISTARY	2
#define NEWID_REGNAME	3
#define NEWID_REGSEC	4



char newid_fullname[128];
char newid_bookloc[128];
char newid_registary[128];
char newid_regname[128];
char newid_regsec[128];

char* newid_inputs[5];
editline_t *newid_edit[5];

int cursor_newid = 0;

int window_new_identity_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];

	int pos;
	char c, *ch;
	
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	gui->parent = wid;

	getmaxyx(stdscr,y,x);

	attrset(A_NORMAL);

	/* Window Title */
	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(4));
	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0-1));
	snprintf(buf, x-gui->x0-1, "Create new Identity");
	mvaddstr(gui->y0, gui->x0+1, buf);
	
	/* Window Body */
	pos = editline_pos(newid_edit[cursor_newid]);
	editline_char(newid_edit[cursor_newid], &ch);
	if(!*ch)
		c = ' ';
	else
		c = *ch;

	attrset(COLOR_PAIR(0));

	snprintf(buf, 250, "%199.199s", " ");
	mvaddnstr(gui->y0+1, gui->x0, buf, (x-gui->x0-1));
	mvaddnstr(gui->y0+2, gui->x0, buf, (x-gui->x0-1));
	mvaddnstr(gui->y0+3, gui->x0, buf, (x-gui->x0-1));
	mvaddnstr(gui->y0+4, gui->x0, buf, (x-gui->x0-1));
	mvaddnstr(gui->y0+5, gui->x0, buf, (x-gui->x0-1));

	if(cursor_newid == NEWID_FULLNAME)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 250, "%29.29s", "Full name :");
	mvaddstr(gui->y0+1, gui->x0, buf);
  
	if(cursor_newid == NEWID_BOOKLOC)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 250, "%29.29s", "Phonebook location:");
	mvaddstr(gui->y0+2, gui->x0, buf);

	if(cursor_newid == NEWID_REGISTARY)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 250, "%29.29s", "Register server :");
	mvaddstr(gui->y0+3, gui->x0, buf);
  
	if(cursor_newid == NEWID_REGNAME)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 250, "%29.29s", "Register name :");
	mvaddstr(gui->y0+4, gui->x0, buf);

	if(cursor_newid == NEWID_REGSEC)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 250, "%29.29s", "Register period in seconds :");
	mvaddstr(gui->y0+5, gui->x0, buf);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+1, gui->x0+30, newid_inputs[NEWID_FULLNAME]);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+2, gui->x0+30, newid_inputs[NEWID_BOOKLOC]);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+3, gui->x0+30, newid_inputs[NEWID_REGISTARY]);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+4, gui->x0+30, newid_inputs[NEWID_REGNAME]);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+5, gui->x0+30, newid_inputs[NEWID_REGSEC]);

	attrset(COLOR_PAIR(10));
	mvaddch(gui->y0+1+cursor_newid, gui->x0+30+pos, c);

	gui->gui_draw_commands(gui);
  
	return 0;
}

int window_new_identity_run_command(gui_t* gui, int c)
{
	ogmp_curses_t* ocui = gui->topui;

	int max = 5;

	switch (c)
    {
		case KEY_DC:
		{
			editline_remove_char(newid_edit[cursor_newid]);
			
			delch();

			break;
		}
		case '\b':
		{
			if (editline_move_pos(newid_edit[cursor_newid], -1) >= 0)
				editline_remove_char(newid_edit[cursor_newid]);
			else
				beep();

			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		case KEY_DOWN:
		{
			cursor_newid++;
			cursor_newid %= max;

			break;
		}
		case KEY_UP:
		{
			cursor_newid += max-1;
			cursor_newid %= max;

			break;
		}
		case KEY_RIGHT:
		{
			if (editline_move_pos(newid_edit[cursor_newid], 1) < 0)
				beep();

			break;
		}
		case KEY_LEFT:
		{
			if (editline_move_pos(newid_edit[cursor_newid], -1) < 0)
				beep();

			break;
		}
		case 1:  /* Ctrl-A */
		{
			int sec = 0;

			if(newid_regsec[0])
				sec = strtol(newid_regsec, NULL, 10);

			if(sec <= 0)
				break;
			
			if(newid_fullname[0] && newid_registary[0] && newid_regname[0])
			{
				user_add_profile(ocui->user, newid_fullname, strlen(newid_fullname),
											newid_bookloc, newid_registary, 
											newid_regname, sec);
				
				gui_hide_window(gui);
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
			editline_clear(newid_edit[cursor_newid]);
	
			break;
		}
		default:
		{
			if(editline_append(newid_edit[cursor_newid], (char*)&c, 1) == 0)
            {
				beep();
	
                return -1;
            }
		}
	}

    gui_update(gui);

	return 0;
}

void window_new_identity_draw_commands(gui_t* gui)
{
	int x,y;
	char *new_identity_commands[] = 
	{
		"^A", "AddIdentity" ,
		"^D", "DeleteLine",
		"^C", "Cancel",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(new_identity_commands, y-5, 0);
}

int window_new_identity_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_new_identity_new(ogmp_curses_t* topui)
{
	gui_window_new_identity.topui = topui;

	memset(newid_inputs, 0, sizeof(newid_inputs));

	newid_edit[NEWID_FULLNAME] = editline_new(newid_fullname, LINE_MAX);
	newid_edit[NEWID_BOOKLOC] = editline_new(newid_bookloc, LINE_MAX);
	newid_edit[NEWID_REGISTARY] = editline_new(newid_registary, LINE_MAX);
	newid_edit[NEWID_REGNAME] = editline_new(newid_regname, LINE_MAX);
	newid_edit[NEWID_REGSEC] = editline_new(newid_regsec, LINE_MAX);

	newid_inputs[NEWID_FULLNAME] = newid_fullname;
	newid_inputs[NEWID_BOOKLOC] = newid_bookloc;
	newid_inputs[NEWID_REGISTARY] = newid_registary;
	newid_inputs[NEWID_REGNAME] = newid_regname;
	newid_inputs[NEWID_REGSEC] = newid_regsec;

	return &gui_window_new_identity;
}

int window_new_identity_done(gui_t* gui)
{
	editline_done(newid_edit[NEWID_FULLNAME]);
	editline_done(newid_edit[NEWID_BOOKLOC]);
	editline_done(newid_edit[NEWID_REGISTARY]);
	editline_done(newid_edit[NEWID_REGNAME]);
	editline_done(newid_edit[NEWID_REGSEC]);

	return 0;
}

gui_t gui_window_new_identity =
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
    window_new_identity_event,
	NULL,
	window_new_identity_print,
	window_new_identity_run_command,
	NULL,
	window_new_identity_draw_commands,
	10,
	0,
	-1,
	NULL
};
