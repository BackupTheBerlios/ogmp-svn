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

#include "gui_preference.h"
#include "editor.h"

int cursor_preference = 0;

#define DEFAULT_SOURCE_LINEMAX 128

#define DEFAULT_SOURCE_FILE	0
#define DEFAULT_SOURCE_SUBJ	1
#define DEFAULT_SOURCE_INFO	2

#define DEFAULT_SOURCE_MAXITEM	3

char default_source_inputs[DEFAULT_SOURCE_MAXITEM][DEFAULT_SOURCE_LINEMAX];
editline_t *default_source_edit[DEFAULT_SOURCE_MAXITEM];

int cursor_default_source = 0;

media_source_t* default_source = NULL;

int window_preference_print(gui_t* gui, int wid)
{
	int k;
	int y,x;
	char buf[250];

	int h_playlist = 0;

	sipua_setting_t *setting;

	ogmp_curses_t* ocui = gui->topui;

	char *ch, c;
	int pos;

	gui->parent = wid;

	if(ocui->gui_windows[EXTRAGUI])
		josua_clear_box_and_commands(ocui->gui_windows[EXTRAGUI]);

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	pos = editline_pos(default_source_edit[cursor_default_source]);
	editline_char(default_source_edit[cursor_default_source], &ch);

	if(!*ch)
		c = ' ';
	else
		c = *ch;

	getmaxyx(stdscr,y,x);
	attrset(A_NORMAL);

	/* Window Title */
	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(4));

	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0));
	snprintf(buf, x-gui->x0-1, "Preference");
	mvaddstr(gui->y0, gui->x0+1, buf);

	/* Window Body */
	attrset(COLOR_PAIR(1));

	if (gui->x1 != -999)
		x = gui->x1;

	{
		h_playlist++;

		if(cursor_default_source == DEFAULT_SOURCE_FILE)
			attrset(COLOR_PAIR(10));
		else
			attrset(COLOR_PAIR(1));
		snprintf(buf, 25, "%19.19s", "Source File :");

		mvaddstr(gui->y0 + h_playlist, gui->x0, buf);
	
		attrset(COLOR_PAIR(0));
		mvaddstr(gui->y0 + h_playlist, gui->x0+20, default_source_inputs[DEFAULT_SOURCE_FILE]);
	}

	{
		h_playlist++;

		if(cursor_default_source == DEFAULT_SOURCE_SUBJ)
			attrset(COLOR_PAIR(10));
		else
			attrset(COLOR_PAIR(1));

		snprintf(buf, 25, "%19.19s", "Source Subject :");

		mvaddstr(gui->y0 + h_playlist, gui->x0, buf);
	
		attrset(COLOR_PAIR(0));
		mvaddstr(gui->y0 + h_playlist, gui->x0+20, default_source_inputs[DEFAULT_SOURCE_SUBJ]);
	}

	{
		h_playlist++;

		if(cursor_default_source == DEFAULT_SOURCE_INFO)
			attrset(COLOR_PAIR(10));
		else
			attrset(COLOR_PAIR(1));
	
		snprintf(buf, 25, "%19.19s", "Source Info :");

		mvaddstr(gui->y0 + h_playlist, gui->x0, buf);

		attrset(COLOR_PAIR(0));
		mvaddstr(gui->y0 + h_playlist, gui->x0+20, default_source_inputs[DEFAULT_SOURCE_INFO]);
	}

	attrset(COLOR_PAIR(10));
	mvaddch(gui->y0+1+cursor_default_source, gui->x0+20+pos, c);

	h_playlist++;

	setting = ocui->sipua->setting(ocui->sipua);

	for (k=0; k<setting->ncoding; k++)
    {
		snprintf(buf, 250, " %c%c %-25.25s %d   %d   %199.199s",
				(cursor_preference==k) ? '-' : ' ', (cursor_preference==k) ? '>' : ' ',
				setting->codings[k].mime,
				setting->codings[k].clockrate,
				setting->codings[k].param, " ");

		attrset((k==cursor_preference) ? A_REVERSE : COLOR_PAIR(1));

		mvaddnstr(gui->y0+1+h_playlist+k, gui->x0, buf, x-gui->x0-1);

		if (k > y + gui->y1 - gui->y0 + 1)
			break; /* do not print next one */
    }

	//window_preference_draw_commands(gui);
  
	return 0;
}

void window_preference_draw_commands(gui_t* gui)
{
	int x,y;
	char *preference_commands[] = 
	{
		"^A",  "OK",
		"^E",  "Enable",
		"^D",  "Disable",
		"^Q",  "Move Up",
		"^W",  "Move Down",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(preference_commands, y-5, 0);
}

int window_preference_run_command(gui_t* gui, int c)
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

	max = DEFAULT_SOURCE_MAXITEM;

	k = 0;
	switch (c)
    {
		case KEY_DC:
		{
			editline_remove_char(default_source_edit[cursor_default_source]);
			
			delch();

			break;
		}
		case '\b':
		{
			if (editline_move_pos(default_source_edit[cursor_default_source], -1) >= 0)
				editline_remove_char(default_source_edit[cursor_default_source]);
			else
				beep();

			break;
		}
		case KEY_RIGHT:
		{
			if (editline_move_pos(default_source_edit[cursor_default_source], 1) < 0)
				beep();

			break;
		}
		case KEY_LEFT:
		{
			if (editline_move_pos(default_source_edit[cursor_default_source], -1) < 0)
				beep();

			break;
		}
		case '\n':
		case '\r':
		case KEY_DOWN:
		{
			cursor_default_source++;
			cursor_default_source %= max;

			break;
		}
		case KEY_UP:
		{
			cursor_default_source += max-1;
			cursor_default_source %= max;

			break;
		}
		case 1:   /* ^A: OK */
		{
			if(default_source_inputs[DEFAULT_SOURCE_FILE][0] &&
				default_source_inputs[DEFAULT_SOURCE_SUBJ][0] &&
				default_source_inputs[DEFAULT_SOURCE_INFO][0])
			{
                char* name = default_source_inputs[DEFAULT_SOURCE_FILE];
                char* subj = default_source_inputs[DEFAULT_SOURCE_SUBJ];
                char* info = default_source_inputs[DEFAULT_SOURCE_INFO];

                if(ocui->sipua->set_background_source(ocui->sipua, name, subj, info)==NULL)
                {
					beep();
					break;
				}
			}

			break;
		}
		case 4:   /* ^D */
		{
			codings[codex[cursor_preference]].enabled = -1;
			k = 1; /* rebuilt set of codec */

			break;
		}
		case 5:   /* ^E */
		{
			codings[codex[cursor_preference]].enabled = 0;
			k = 1; /* rebuilt set of codec */

			break;
		}
		case 17:   /* ^Q */
		{
			if (cursor_preference==0)
			{ 
				beep(); 
				break; 
			}

			cursor_preference += max-1;
			cursor_preference %= max;
			k = 1;
			{
				int i = codex[cursor_preference];
				codex[cursor_preference] = codex[cursor_preference+1];
				codex[cursor_preference+1] = i;
			}

			break;
		}
		case 23:   /* ^W */
		{
			if (cursor_preference==max-1)
			{ 
				beep(); 
				break; 
			}

			cursor_preference++;
			cursor_preference %= max;
			k = 1;
			{
				int i = codex[cursor_preference];
				codex[cursor_preference] = codex[cursor_preference-1];
				codex[cursor_preference-1] = i;
			}

			break;
		}
		default:
		{
			if(editline_append(default_source_edit[cursor_default_source], (char*)&c, 1) == 0)
            {
				beep();
                return -1;
            }
		}
    }

	if (gui->on_off==GUI_ON)
		gui->gui_print(gui, gui->parent);
  
	return 0;
}

int window_preference_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_preference_new(ogmp_curses_t* topui)
{
	gui_window_preference.topui = topui;

	memset(default_source_inputs, 0, sizeof(default_source_inputs));

	default_source_edit[DEFAULT_SOURCE_FILE] = editline_new(default_source_inputs[DEFAULT_SOURCE_FILE], DEFAULT_SOURCE_LINEMAX);
	default_source_edit[DEFAULT_SOURCE_SUBJ] = editline_new(default_source_inputs[DEFAULT_SOURCE_SUBJ], DEFAULT_SOURCE_LINEMAX);
	default_source_edit[DEFAULT_SOURCE_INFO] = editline_new(default_source_inputs[DEFAULT_SOURCE_INFO], DEFAULT_SOURCE_LINEMAX);

	return &gui_window_preference;
}

int window_preference_done(gui_t* gui)
{
	editline_done(default_source_edit[DEFAULT_SOURCE_FILE]);
	editline_done(default_source_edit[DEFAULT_SOURCE_SUBJ]);
	editline_done(default_source_edit[DEFAULT_SOURCE_INFO]);

	return 0;
}

gui_t gui_window_preference =
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
    window_preference_event,
	NULL,
	window_preference_print,
	window_preference_run_command,
	NULL,
	window_preference_draw_commands,
	-1,
	-1,
	-1,
	NULL
};
