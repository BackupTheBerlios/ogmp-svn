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

#include "gui_address_book_newentry.h"

#include "editor.h"

#ifndef	LINE_MAX
#define LINE_MAX 128
#endif

#define NEWENTRY_NAME	0
#define NEWENTRY_MEMO	1
#define NEWENTRY_SIP	2

char newentry_inputs[3][LINE_MAX];

editline_t *newentry_edit[4];
int cursor_newentry = 0;

int window_address_book_newentry_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];
  
	int pos;
	char c, *ch;
	
	ogmp_curses_t* ocui = gui->topui;
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	gui->parent = wid;

	getmaxyx(stdscr,y,x);
  
	attrset(A_NORMAL);
	attrset(COLOR_PAIR(1));

	if (gui->x1==-999)
    {}
	else 
		x = gui->x1;

	/* Window Title */
	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(4));
	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0));

	snprintf(buf, x-gui->x0, "New contact for '%s'<%s>", user_profile->fullname, user_profile->regname);
	mvaddstr(gui->y0, gui->x0+1, buf);

	/* Window Body */
	pos = editline_pos(newentry_edit[cursor_newentry]);
	editline_char(newentry_edit[cursor_newentry], &ch);
	if(!*ch)
		c = ' ';
	else
		c = *ch;

	attrset(COLOR_PAIR(0));

	snprintf(buf, 199, "%199.199s", " ");
	mvaddnstr(gui->y0+1, gui->x0, buf, x-gui->x0-1);
	mvaddnstr(gui->y0+2, gui->x0, buf, x-gui->x0-1);
	mvaddnstr(gui->y0+3, gui->x0, buf, x-gui->x0-1);
	mvaddnstr(gui->y0+4, gui->x0, buf, x-gui->x0-1);
  
	attrset(COLOR_PAIR(1));

	if(cursor_newentry == NEWENTRY_NAME)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%10.10s", "Name :");
	mvaddstr(gui->y0+1, gui->x0, buf);
  
	
	if(cursor_newentry == NEWENTRY_MEMO)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%10.10s", "Memo :");
	mvaddstr(gui->y0+2, gui->x0, buf);


	if(cursor_newentry == NEWENTRY_SIP)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%10.10s", "Contact :");
	mvaddstr(gui->y0+3, gui->x0, buf);
  
	
	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+1, gui->x0+11, newentry_inputs[NEWENTRY_NAME]);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+2, gui->x0+11, newentry_inputs[NEWENTRY_MEMO]);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+3, gui->x0+11, newentry_inputs[NEWENTRY_SIP]);

	attrset(COLOR_PAIR(10));
	mvaddch(gui->y0+1+cursor_newentry, gui->x0+11+pos, c);

	gui->gui_draw_commands(gui);

	return 0;
}


int window_address_book_newentry_run_command(gui_t* gui, int c)
{
	ogmp_curses_t* ocui = gui->topui;

	int max = 3;

	switch (c)
    {
		case KEY_DC:
		{
			editline_remove_char(newentry_edit[cursor_newentry]);
			
			delch();

			break;
		}
		case '\b':
		{
			if (editline_move_pos(newentry_edit[cursor_newentry], -1) >= 0)
				editline_remove_char(newentry_edit[cursor_newentry]);
			else
				beep();

			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		case KEY_DOWN:
		{
			cursor_newentry++;
			cursor_newentry %= max;

			break;
		}
		case KEY_UP:
		{
			cursor_newentry += max-1;
			cursor_newentry %= max;

			break;
		}
		case KEY_RIGHT:
		{
			if (editline_move_pos(newentry_edit[cursor_newentry], 1) < 0)
				beep();

			break;
		}
		case KEY_LEFT:
		{
			if (editline_move_pos(newentry_edit[cursor_newentry], -1) < 0)
				beep();

			break;
		}
		case 1:  /* Ctrl-A */
		{
			if(newentry_inputs[NEWENTRY_NAME][0] && newentry_inputs[NEWENTRY_MEMO][0] && newentry_inputs[NEWENTRY_SIP][0])
			{
				sipua_contact_t* contact = sipua_new_contact(newentry_inputs[NEWENTRY_NAME], strlen(newentry_inputs[NEWENTRY_NAME]), newentry_inputs[NEWENTRY_MEMO], strlen(newentry_inputs[NEWENTRY_MEMO]), newentry_inputs[NEWENTRY_SIP]);
				
				sipua_add_contact(ocui->phonebook, contact);

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
			editline_clear(newentry_edit[cursor_newentry]);
	
			break;
		}
		default:
		{
			if(editline_append(newentry_edit[cursor_newentry], (char*)&c, 1) == 0)
				beep();
	
			return -1;
		}
	}

	return 0;
}

void window_address_book_newentry_draw_commands(gui_t* gui)
{
	int x,y;
	char *address_book_newentry_commands[] = 
	{
		"^A", "Add" ,
		"^D", "ClearLine",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(address_book_newentry_commands, y-5, 0);
}

int window_address_book_newentry_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_address_book_newentry_new(ogmp_curses_t* topui)
{
	gui_window_address_book_newentry.topui = topui;

	newentry_edit[NEWENTRY_NAME] = editline_new(newentry_inputs[NEWENTRY_NAME], LINE_MAX);
	newentry_edit[NEWENTRY_MEMO] = editline_new(newentry_inputs[NEWENTRY_MEMO], LINE_MAX);
	newentry_edit[NEWENTRY_SIP] = editline_new(newentry_inputs[NEWENTRY_SIP], LINE_MAX);

	return &gui_window_address_book_newentry;
}

int window_address_book_newentry_done(gui_t* gui)
{
	editline_done(newentry_edit[NEWENTRY_NAME]);
	editline_done(newentry_edit[NEWENTRY_MEMO]);
	editline_done(newentry_edit[NEWENTRY_SIP]);

	return 0;
}

gui_t gui_window_address_book_newentry =
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
	NULL,
    window_address_book_newentry_event,
	window_address_book_newentry_print,
	window_address_book_newentry_run_command,
	NULL,
	window_address_book_newentry_draw_commands,
	10,
	0,
	-1,
	NULL,
	NULL
};
