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

#include "gui_setup.h"
#include "editor.h"

int cursor_setup = 0;

int window_setup_print(gui_t* gui, int wid)
{
	int k;
	int y,x;
	char buf[250];

	int h_playlist = 0;

	sipua_setting_t *setting;

	ogmp_curses_t* ocui = gui->topui;

	gui->parent = wid;

	if(ocui->gui_windows[EXTRAGUI])
		josua_clear_box_and_commands(ocui->gui_windows[EXTRAGUI]);

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);
	attrset(A_NORMAL);

	/* Window Title */
	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(4));

	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0));
	snprintf(buf, x-gui->x0-1, "Setup");
	mvaddstr(gui->y0, gui->x0+1, buf);

	/* Window Body */
	attrset(COLOR_PAIR(1));

	if (gui->x1 != -999)
		x = gui->x1;

	setting = ocui->sipua->setting(ocui->sipua);

	for (k=0; k<setting->ncoding; k++)
    {
		snprintf(buf, 250, " %c%c %-25.25s %d   %d   %199.199s",
				(cursor_setup==k) ? '-' : ' ', (cursor_setup==k) ? '>' : ' ',
				setting->codings[k].mime,
				setting->codings[k].clockrate,
				setting->codings[k].param, " ");

		attrset((k==cursor_setup) ? A_REVERSE : COLOR_PAIR(1));

		mvaddnstr(gui->y0+1+h_playlist+k, gui->x0, buf, x-gui->x0-1);

		if (k > y + gui->y1 - gui->y0 + 1)
			break; /* do not print next one */
    }

	window_setup_draw_commands(gui);
  
	return 0;
}

void window_setup_draw_commands(gui_t* gui)
{
	int x,y;
	char *setup_commands[] = 
	{
		"^A",  "OK",
		"^E",  "Enable",
		"^D",  "Disable",
		"^Q",  "Move Up",
		"^W",  "Move Down",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(setup_commands, y-5, 0);
}

int window_setup_run_command(gui_t* gui, int c)
{
	int max;
	int k,y,x;

	sipua_setting_t* setting;
	ogmp_curses_t* ocui = gui->topui;

	int *codex = ocui->codex;
	coding_t *codings = ocui->codings;
  
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);

	if (gui->x1 != -999)
		x = gui->x1;

	setting = ocui->sipua->setting(ocui->sipua);

	max = setting->ncoding;

	k = 0;
	switch (c)
    {
		case KEY_DOWN:
		{
			cursor_setup++;
			cursor_setup %= max;

			break;
		}
		case KEY_UP:
		{
			cursor_setup += max-1;
			cursor_setup %= max;

			break;
		}
		case 4:   /* ^D */
		{
			codings[codex[cursor_setup]].enabled = -1;
			k = 1; /* rebuilt set of codec */

			break;
		}
		case 5:   /* ^E */
		{
			codings[codex[cursor_setup]].enabled = 0;
			k = 1; /* rebuilt set of codec */

			break;
		}
		case 17:   /* ^Q */
		{
			if (cursor_setup==0)
			{ 
				beep(); 
				break; 
			}

			cursor_setup += max-1;
			cursor_setup %= max;
			k = 1;
			{
				int i = codex[cursor_setup];
				codex[cursor_setup] = codex[cursor_setup+1];
				codex[cursor_setup+1] = i;
			}

			break;
		}
		case 23:   /* ^W */
		{
			if (cursor_setup==max-1)
			{ 
				beep(); 
				break; 
			}

			cursor_setup++;
			cursor_setup %= max;
			k = 1;
			{
				int i = codex[cursor_setup];
				codex[cursor_setup] = codex[cursor_setup-1];
				codex[cursor_setup-1] = i;
			}

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

int window_setup_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_setup_new(ogmp_curses_t* topui)
{
	gui_window_setup.topui = topui;

	return &gui_window_setup;
}

int window_setup_done(gui_t* gui)
{
	return 0;
}

gui_t gui_window_setup =
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
    window_setup_event,
	NULL,
	window_setup_print,
	window_setup_run_command,
	NULL,
	window_setup_draw_commands,
	-1,
	-1,
	-1,
	NULL
};
