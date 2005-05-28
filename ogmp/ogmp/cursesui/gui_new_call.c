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

#include "gui_new_call.h"
#include "gui_menu.h"

#include "editor.h"

#ifndef	LINE_MAX
#define LINE_MAX 128
#endif

#define NEWCALL_TO		0
#define NEWCALL_SUBJ	1
#define NEWCALL_DESC	2

char newcall_inputs[3][LINE_MAX];


editline_t *newcall_edit[3];
int cursor_newcall = 0;

int window_new_call_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];

	ogmp_curses_t* ocui = gui->topui;
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);

	int pos;
	char c, *ch;
	
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
	mvaddnstr(gui->y0+1, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+2, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+3, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+4, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+5, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+6, gui->x0, buf, (x-gui->x0));

	snprintf(buf, x-gui->x0-1, "New call from: '%s'<%s>", user_profile->fullname, user_profile->regname);
	mvaddstr(gui->y0, gui->x0+1, buf);

	/* Window Body */
	pos = editline_pos(newcall_edit[cursor_newcall]);
	editline_char(newcall_edit[cursor_newcall], &ch);
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
  
	
	if(cursor_newcall == NEWCALL_TO)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%15.15s", "To :");
	mvaddstr(gui->y0+1, gui->x0, buf);
  
	
	if(cursor_newcall == NEWCALL_SUBJ)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%15.15s", "Subject :");
	mvaddstr(gui->y0+2, gui->x0, buf);


	if(cursor_newcall == NEWCALL_DESC)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%15.15s", "Description :");
	mvaddstr(gui->y0+3, gui->x0, buf);

	if(newcall_inputs[NEWCALL_TO][0] == '\0' && ocui->contact)
	{
		editline_set_line(newcall_edit[NEWCALL_TO], ocui->contact->sip, strlen(ocui->contact->sip));

		ocui->contact = NULL;
	}
  
	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+1, gui->x0+16, newcall_inputs[NEWCALL_TO]);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+2, gui->x0+16, newcall_inputs[NEWCALL_SUBJ]);

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+3, gui->x0+16, newcall_inputs[NEWCALL_DESC]);

	attrset(COLOR_PAIR(10));
	mvaddch(gui->y0+1+cursor_newcall, gui->x0+16+pos, c);

	window_new_call_draw_commands(gui);

	return 0;
}


int window_new_call_run_command(gui_t* gui, int c)
{
	ogmp_curses_t* ocui = gui->topui;

	int max = 3;

	switch (c)
    {
		case KEY_DC:
		{
			editline_remove_char(newcall_edit[cursor_newcall]);
			
			delch();

			break;
		}
		case '\b':
		{
			if (editline_move_pos(newcall_edit[cursor_newcall], -1) >= 0)
				editline_remove_char(newcall_edit[cursor_newcall]);
			else
				beep();

			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		case KEY_DOWN:
		{
			cursor_newcall++;
			cursor_newcall %= max;

			break;
		}
		case KEY_UP:
		{
			cursor_newcall += max-1;
			cursor_newcall %= max;

			break;
		}
		case KEY_RIGHT:
		{
			if (editline_move_pos(newcall_edit[cursor_newcall], 1) < 0)
				beep();

			break;
		}
		case KEY_LEFT:
		{
			if (editline_move_pos(newcall_edit[cursor_newcall], -1) < 0)
				beep();
			break;
		}
		case 1: /* Ctrl-A */
		{
			/* if (_josua_start_call(cfg.identity, to, subject, route) != 0) beep(); */
			sipua_set_t* call;

         call = ocui->sipua->new_call(ocui->sipua, newcall_inputs[NEWCALL_SUBJ], strlen(newcall_inputs[NEWCALL_SUBJ]), newcall_inputs[NEWCALL_DESC], strlen(newcall_inputs[NEWCALL_DESC]));
         if(call)
         {
				ocui->sipua->call(ocui->sipua, call, newcall_inputs[NEWCALL_TO], call->sdp_body);
         }
            
			gui_activate_window(gui, GUI_SESSION);

			break;
		}
		case 3:  /* Ctrl-C */
		{
			gui_hide_window(gui);

			break;
		}
		case 4:  /* Ctrl-D */
		{
			editline_clear(newcall_edit[cursor_newcall]);
	
			break;
		}
		case 15: /* Ctrl-O */
		{
			/* if (_josua_start_options(cfg.identity, to, route) != 0) beep(); */
			break;
		}
		case 19: /* Ctrl-S */
		{
			/* if (_josua_start_subscribe(cfg.identity, to, route) != 0) beep(); */
			break;
		}   
		default:
		{
			if(editline_append(newcall_edit[cursor_newcall], (char*)&c, 1) == 0)
         {
				beep();
            return -1;
         }
		}
	}

   gui_update(gui);

	return 0;
}

void window_new_call_draw_commands(gui_t* gui)
{
	int x,y;

	char *new_call_commands[] = 
	{
		"^A", "Call" ,
		"^O", "Options" ,
		"^U", "Subscribe" ,
		"^P", "PhoneBook",
		"^D", "ClearLine",
		NULL
	};

	getmaxyx(stdscr,y,x);

	josua_print_command(new_call_commands, y-5, 0);
}

int window_new_call_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_new_call_new(ogmp_curses_t* topui)
{
	gui_window_new_call.topui = topui;

	newcall_edit[NEWCALL_TO] = editline_new(newcall_inputs[NEWCALL_TO], LINE_MAX);
	newcall_edit[NEWCALL_SUBJ] = editline_new(newcall_inputs[NEWCALL_SUBJ], LINE_MAX);
	newcall_edit[NEWCALL_DESC] = editline_new(newcall_inputs[NEWCALL_DESC], LINE_MAX);

	return &gui_window_new_call;
}

int window_new_call_done(gui_t* gui)
{
	editline_done(newcall_edit[NEWCALL_TO]);
	editline_done(newcall_edit[NEWCALL_SUBJ]);
	editline_done(newcall_edit[NEWCALL_DESC]);

	return 0;
}

gui_t gui_window_new_call =
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
    window_new_call_event,
	NULL,
	window_new_call_print,
	window_new_call_run_command,
	NULL,
	window_new_call_draw_commands,
	10,
	1,
	-1,
	NULL
};
