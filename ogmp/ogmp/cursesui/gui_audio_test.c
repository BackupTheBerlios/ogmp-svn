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

#include "gui_audio_test.h"

#include "editor.h"

gui_t gui_window_audio_test = 
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
	NULL,
	window_audio_test_print,
	window_audio_test_run_command,
	NULL,
	window_audio_test_draw_commands,
	-1,
	-1,
	-1,
	NULL
};

#define LINE_MAX 128

#define AUDIOTEST_FILE		0
#define AUDIOTEST_MIME		1

char audio_test_inputs[2][LINE_MAX];
editline_t *audio_test_edit[2];

int cursor_audio_test = 0;

media_source_t* audio_test_source = NULL;

int window_audio_test_print(gui_t* gui, int wid)
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

	pos = editline_pos(audio_test_edit[cursor_audio_test]);

	editline_char(audio_test_edit[cursor_audio_test], &ch);
	if(!*ch)
		c = ' ';
	else
		c = *ch;

	attrset(A_NORMAL);

	/* Window Title */
	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(4));
	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0));
	snprintf(buf, x-gui->x0-1, "audio_test");
	mvaddstr(gui->y0, gui->x0+1, buf);

	/* Window Body */
	attrset(COLOR_PAIR(0));

	snprintf(buf, 250, "%199.199s", " ");
	mvaddnstr(gui->y0+1, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+2, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+3, gui->x0, buf, (x-gui->x0));

	if(cursor_audio_test == AUDIOTEST_FILE)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%19.19s", "Media File :");
	mvaddstr(gui->y0+1, gui->x0, buf);
  
	if(cursor_audio_test == AUDIOTEST_MIME)
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(1));

	snprintf(buf, 25, "%19.19s", "Media MIME Type :");
	mvaddstr(gui->y0+2, gui->x0, buf);
  
	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+1, gui->x0+20, audio_test_inputs[AUDIOTEST_FILE]);
	if(cursor_audio_test == AUDIOTEST_FILE)
	{
		attrset(COLOR_PAIR(10));
		mvaddch(gui->y0+1, gui->x0+20+pos, c);
	}

	attrset(COLOR_PAIR(0));
	mvaddstr(gui->y0+2, gui->x0+20, audio_test_inputs[AUDIOTEST_MIME]);
	if(cursor_audio_test == AUDIOTEST_MIME)
	{
		attrset(COLOR_PAIR(10));
		mvaddch(gui->y0+2, gui->x0+20+pos, c);
	}

	gui->gui_draw_commands(gui);
  
	return 0;
}

void window_audio_test_draw_commands(gui_t* gui)
{
	int x,y;
	char *audio_test_commands[] = 
	{
		"^A",  "Play",
		"^C",  "Stop" ,
		"^S",  "SetBackgroud",
		"^D",  "Done",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(audio_test_commands, y-5, 0);
}

int window_audio_test_run_command(gui_t* gui, int c)
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
			editline_remove_char(audio_test_edit[cursor_audio_test]);
			
			delch();

			break;
		}
		case '\b':
		{
			if (editline_move_pos(audio_test_edit[cursor_audio_test], -1) >= 0)
				editline_remove_char(audio_test_edit[cursor_audio_test]);
			else
				beep();

			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		case KEY_DOWN:
		{
			cursor_audio_test++;
			cursor_audio_test %= max;

			break;
		}
		case KEY_UP:
		{
			cursor_audio_test += max-1;
			cursor_audio_test %= max;

			break;
		}
		case KEY_RIGHT:
		{
			if (editline_move_pos(audio_test_edit[cursor_audio_test], 1) < 0)
				beep();

			break;
		}
		case KEY_LEFT:
		{
			if (editline_move_pos(audio_test_edit[cursor_audio_test], -1) < 0)
				beep();

			break;
		}
		case 9:  /* Ctrl-I */
		{
			break;
		}
		case 1:  /* Ctrl-A */
		{
			if(!audio_test_source && audio_test_inputs[AUDIOTEST_FILE][0] && audio_test_inputs[AUDIOTEST_MIME][0])
			{
				char* fname = audio_test_inputs[AUDIOTEST_FILE];
				char* mime = audio_test_inputs[AUDIOTEST_MIME];

				audio_test_source = ocui->sipua->open_source(ocui->sipua, fname, "playback", NULL);
				
				if(!audio_test_source)
				{
					beep();
					break;
				}
				
				audio_test_source->start(audio_test_source);
			}

			break;
		}
		case 3:  /* Ctrl-C */
		{
			if(audio_test_source)
			{
				audio_test_source->stop(audio_test_source);

				ocui->sipua->close_source(ocui->sipua, audio_test_source);
				
				audio_test_source = NULL;
			}
			
			break;
		}
		case 4:  /* Ctrl-D */
		{
			gui_hide_window(gui);
	
			break;
		}
		case 18:  /* Ctrl-S */
		{
			if(!audio_test_source && audio_test_inputs[AUDIOTEST_FILE][0] && audio_test_inputs[AUDIOTEST_MIME][0])
			{
				char* name = audio_test_inputs[AUDIOTEST_FILE];
				char* mime = audio_test_inputs[AUDIOTEST_MIME];
				
				if(ocui->sipua->set_background_source(ocui->sipua, name)==NULL)
				{
					beep();
					break;
				}
				
				audio_test_source->start(audio_test_source);
			}

			break;
		}
		default:
		{
			if(editline_append(audio_test_edit[cursor_audio_test], &((char)c), 1) == 0)
				beep();
	
			return -1;
		}
	}

	return 0;
}

gui_t* window_audio_test_new(ogmp_curses_t* topui)
{
	gui_window_audio_test.topui = topui;

	memset(audio_test_inputs, 0, sizeof(audio_test_inputs));

	audio_test_edit[AUDIOTEST_FILE] = editline_new(audio_test_inputs[AUDIOTEST_FILE], LINE_MAX);
	audio_test_edit[AUDIOTEST_MIME] = editline_new(audio_test_inputs[AUDIOTEST_MIME], LINE_MAX);

	return &gui_window_audio_test;
}

int window_audio_test_done(gui_t* gui)
{
	editline_done(audio_test_edit[AUDIOTEST_FILE]);
	editline_done(audio_test_edit[AUDIOTEST_MIME]);

	return 0;
}
