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

gui_t gui_window_setup = 
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
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

int cursor_setup = 0;

typedef struct _j_codec 
{
	char payload[10];
	char codec[200];
	int enabled;

} j_codec_t;

j_codec_t j_codec[] = 
{
	{ {"0"}, {"PCMU/8000"}, 0},
	{ {"8"}, {"PCMA/8000"}, 0},
	{ {"3"}, {"GSM/8000"}, 0},
	{ {"110"}, {"speex/8000"}, 0},
	{ {"111"}, {"speex/16000"}, 0},
	{ {"-1"}, {""} , -1}
};

int window_setup_print(gui_t* gui)
{
	int k;
	int y,x;
	char buf[250];

	ogmp_curses_t* ocui = gui->topui;

	josua_clear_box_and_commands(gui_windows[EXTRAGUI]);

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);
	attrset(A_NORMAL);
	attrset(COLOR_PAIR(1));

	if (gui->x1==-999)
    {}
	else 
		x = gui->x1;

	attrset(COLOR_PAIR(0));

	for (k=0; k<ocui->ncoding; k++)
    {
		snprintf(buf, 199, "   %c%c %s %-150.150s ",
					(cursor_setup==k) ? '-' : ' ',
					(cursor_setup==k) ? '>' : ' ',
					(ocui->codings[ocui->codex[k]].enabled==0) ? "ON " : "   ",
					ocui->codings[ocui->codex[k]].coding.mime);

		/* 
		attrset(COLOR_PAIR(0));
		attrset((k==cursor_setup) ? A_REVERSE : A_NORMAL); 
		*/
		attrset((k==cursor_setup) ? A_REVERSE : COLOR_PAIR(1));

		mvaddnstr(gui->y0+1+k, gui->x0, buf, x-gui->x0-1);
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
		"<-",  "PrevWindow",
		"->",  "NextWindow",
		"e",  "Enable",
		"d",  "Disable" ,
		"q",  "Move Up",
		"w",  "Move Down" ,
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(setup_commands, y-5, 0);
}

int window_setup_run_command(gui_t* gui, int c)
{
	int max;
	int k,y,x;
	ogmp_curses_t* ocui = gui->topui;

	int *codex = ocui->codex;
	coding_t *codings = ocui->codings;
  
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);

	if (gui->x1 == -999)
    {}
	else 
		x = gui->x1;

	if (gui->y1 < 0)
		max = y + gui->y1 - gui->y0 + 2;
	else
		max = gui->y1 - gui->y0 + 2;
  
	if (ocui->ncoding < max) 
		max = ocui->ncoding;

	k = 0; /* codec list untouched */
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
		case 'q':
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
		case 'w':
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
		case 'e':
		{
			codings[codex[cursor_setup]].enabled = 0;
			k = 1; /* rebuilt set of codec */
			break;
		}
		case 'd':
		{
			codings[codex[cursor_setup]].enabled = -1;
			k = 1; /* rebuilt set of codec */
			break;
		}
		default:
		{
			beep();
			return -1;
		}
    }
/*
	if (k==1)
    {
		eXosip_sdp_negotiation_remove_audio_payloads();

		for (k=0;j_codec[k].codec[0]!='\0';k++)
		{
			char tmp[40];
			if (j_codec[k].enabled==0)
			{
				snprintf(tmp, 40, "%s %s", j_codec[k].payload, j_codec[k].codec);
				
				eXosip_sdp_negotiation_add_codec(osip_strdup(j_codec[k].payload),
					       NULL,
					       osip_strdup("RTP/AVP"),
					       NULL, NULL, NULL,
					       NULL,NULL,
					       osip_strdup(tmp));
			}
		}
    }
*/
	if (gui->on_off==GUI_ON)
		gui->gui_print(gui);
  
	return 0;
}
