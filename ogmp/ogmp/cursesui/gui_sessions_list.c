/**
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

/*#include "jcalls.h"*/
#include "gui_sessions_list.h"

int calllist_line = 0;

int window_sessions_list_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];

	sipua_set_t* call;

	int nbusy, line, n, view;
	int busylines[MAX_SIPUA_LINES];
	int max;

	ogmp_curses_t* ocui = gui->topui;
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);

	if(gui->topui->gui_windows[EXTRAGUI])
		josua_clear_box_and_commands(gui->topui->gui_windows[EXTRAGUI]);

	gui->parent = wid;

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);
	max = y + gui->y1 - gui->y0 - 1;

	attrset(A_NORMAL);

	/* Window Title */
	attrset(COLOR_PAIR(4));
	snprintf(buf, 250, "%199.199s", " ");
	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0));

	snprintf(buf, x-gui->x0, "All Calls of '%s'<%s>", user_profile->fullname, user_profile->regname);
	mvaddstr(gui->y0, gui->x0+1, buf);

	attrset(COLOR_PAIR(0));
	snprintf(buf, 250, "%199.199s", " ");
	mvaddnstr(gui->y0+1, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+2, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+3, gui->x0, buf, (x-gui->x0));

	/* Window Body */
	ocui->sipua->lock_lines(ocui->sipua);

	nbusy = ocui->sipua->busylines(ocui->sipua, busylines, MAX_SIPUA_LINES);
	if(nbusy == 0)
	{
		attrset(COLOR_PAIR(1));

		snprintf(buf, x - gui->x0, "No call available !");
		mvaddstr(gui->y0+2, gui->x0+1, buf);

		gui->gui_draw_commands(gui);
		return 0;
	}

	n = 0;
	while(busylines[n] != calllist_line) n++;

	if(n > max/2)
		view = n - max/2;
	else
		view = 0;
	
	for(line = view; line < nbusy; line++)
    {
#if 0
		snprintf(buf, 199, "%c%c %i//%i %i %s with: %s %199.199s",
					(cursor_sessions_list==pos-1) ? '-' : ' ',
					(cursor_sessions_list==pos-1) ? '>' : ' ',
					jcalls[k].cid,
					jcalls[k].did,
					jcalls[k].status_code,
					jcalls[k].reason_phrase,
					jcalls[k].remote_uri, " ");
#endif
		call = ocui->sipua->line(ocui->sipua, busylines[line]);

		if(line==calllist_line)
		{
			snprintf(buf, x - gui->x0, " %c%c %d. %s - [%s]%-80.80s",
					'-', '>', line, call->setid.id, call->subject, " ");
      
			attrset(COLOR_PAIR(10));
		}
		else
		{
			snprintf(buf, x - gui->x0, " %c%c %d. %s - [%s]%-80.80s",
					' ', ' ', line, call->setid.id, call->subject, " ");
      
			attrset(COLOR_PAIR(1));
		}

		mvaddnstr(gui->y0+1+n, gui->x0, buf, x-gui->x0);

		if (n == max)
			break;

		n++;
    }
  
	ocui->sipua->unlock_lines(ocui->sipua);

	gui->gui_draw_commands(gui);
  
	return 0;
}

void window_sessions_list_draw_commands(gui_t* gui)
{
	int x,y;
	char *sessions_list_commands[] = 
	{
		"^C",  "Close" ,
		"A",  "Answer",
		"C",  "Hangup" ,
		"D",  "Decline",
		"R",  "Reject",
		"B",  "Busy",
		"H",  "Hold"  ,
		"U",  "Unhold",
		"O",  "SendOptions",
		"digit",  "SendInfo",
		NULL
	};
  
	getmaxyx(stdscr,y,x);

	josua_print_command(sessions_list_commands, y-5, 0);
}

int window_sessions_list_run_command(gui_t* gui, int c)
{
	int busylines[MAX_SIPUA_LINES];
	int nbusy;

	ogmp_curses_t* ocui = gui->topui;

	ocui->sipua->lock_lines(ocui->sipua);
	nbusy = ocui->sipua->busylines(ocui->sipua, busylines, MAX_SIPUA_LINES);
	ocui->sipua->unlock_lines(ocui->sipua);

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);
	/*
	i = jcall_get_number_of_pending_calls();
	if (i<max) 
		max=i;
	*/
	switch (c)
    {
		sipua_set_t* call;

		case KEY_DOWN:
		{
			int n = 0;
			while(busylines[n] != calllist_line) n++;

			if(n+1==nbusy)
			{
				beep();
				break;
			}

			calllist_line = busylines[++n];
			break;
		}
		case KEY_UP:
		{
			int n = 0;
			while(busylines[n] != calllist_line) n++;

			if(n==0)
			{
				beep();
				break;
			}

			calllist_line = busylines[--n];
			break;
		}
		case 3:  /* Ctrl-C */
		{
			gui_hide_window(gui);

			break;
		}
		case 'c':
		{
			int n = 0;

			call = ocui->sipua->session(ocui->sipua);

			ocui->sipua->bye(ocui->sipua, call);
			
			while(busylines[n] != calllist_line) 
				n++;

			if(n > 0)
				calllist_line = busylines[--n];
			else
			{
				if(nbusy > 1)
					calllist_line = busylines[++n];
				else
					calllist_line = -1;
			}
/*
			eXosip_lock();
			i = eXosip_terminate_call(ca->cid, ca->did);
			if (i==0) jcall_remove(ca);
			eXosip_unlock();
*/
			gui->gui_print(gui, gui->parent);
			break;
		}
		case 'a':
		{
			call = ocui->sipua->pick(ocui->sipua, calllist_line);
			if (!call) 
			{ 
				beep(); 
				break; 
			}

			ocui->sipua->answer(ocui->sipua, call, SIPUA_STATUS_ANSWER);
/*
			eXosip_lock();
			eXosip_answer_call(ca->did, 200, 0);
			eXosip_unlock();
*/
			break;
		}
		case 'r':
		{
			int n = 0;

			call = ocui->sipua->pick(ocui->sipua, calllist_line);
			if (!call) 
			{ 
				beep(); 
				break; 
			}

			ocui->sipua->answer(ocui->sipua, call, SIPUA_STATUS_REJECT);

			while(busylines[n] != calllist_line) 
				n++;

			if(n > 0)
				calllist_line = busylines[--n];
			else
			{
				if(nbusy > 1)
					calllist_line = busylines[++n];
				else
					calllist_line = -1;
			}
/*
			eXosip_lock();
			i = eXosip_answer_call(ca->did, 480, 0);
			if (i==0) jcall_remove(ca);
			eXosip_unlock();
*/
			gui->gui_print(gui, gui->parent);
			break;
		}
		case 'd':
		{
			int n = 0;

			call = ocui->sipua->pick(ocui->sipua, calllist_line);
			if (!call) 
			{ 
				beep(); 
				break; 
			}

			ocui->sipua->answer(ocui->sipua, call, SIPUA_STATUS_DECLINE);
			
			while(busylines[n] != calllist_line) 
				n++;

			if(n > 0)
				calllist_line = busylines[--n];
			else
			{
				if(nbusy > 1)
					calllist_line = busylines[++n];
				else
					calllist_line = -1;
			}
/*
			eXosip_lock();
			i = eXosip_answer_call(ca->did, 603, 0);
			if (i==0) jcall_remove(ca);
			eXosip_unlock();
*/
			gui->gui_print(gui, gui->parent);
			break;
		}
		case 'b':
		{
			int n = 0;

			call = ocui->sipua->pick(ocui->sipua, calllist_line);
			if (!call) 
			{ 
				beep(); 
				break; 
			}

			ocui->sipua->answer(ocui->sipua, call, SIPUA_STATUS_BUSY);
			
			while(busylines[n] != calllist_line) 
				n++;

			if(n > 0)
				calllist_line = busylines[--n];
			else
			{
				if(nbusy > 1)
					calllist_line = busylines[++n];
				else
					calllist_line = -1;
			}
/*
			eXosip_lock();
			i = eXosip_answer_call(ca->did, 486, 0);
			if (i==0) jcall_remove(ca);
			eXosip_unlock();
*/
			gui->gui_print(gui, gui->parent);
			break;
		}
		case 'h':
		{
			call = ocui->sipua->session(ocui->sipua);

			calllist_line = ocui->sipua->hold(ocui->sipua, call);
/*
			eXosip_lock();
			eXosip_on_hold_call(ca->did);
			eXosip_unlock();
*/
			break;
		}
		case 'u':
		{
			call = ocui->sipua->pick(ocui->sipua, calllist_line);
			if (!call) 
			{ 
				beep(); 
				break; 
			}

			ocui->sipua->answer(ocui->sipua, call, SIPUA_STATUS_ANSWER);
/*
			eXosip_lock();
			eXosip_off_hold_call(ca->did);
			eXosip_unlock();
*/
			break;
		}
		case 'o':
		{
			call = ocui->sipua->pick(ocui->sipua, calllist_line);
			if (!call) 
			{ 
				beep(); 
				break; 
			}

			ocui->sipua->options_call(ocui->sipua, call);
/*
			eXosip_lock();
			eXosip_options_call(ca->did);
			eXosip_unlock();
*/
			break;
		}
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '#':
		case '*':
		{
			char dtmf_body[1000];

			call = ocui->sipua->pick(ocui->sipua, calllist_line);
			if (!call) 
			{ 
				beep(); 
				break; 
			}

			snprintf(dtmf_body, 999, "Signal=%c\r\nDuration=250\r\n", c);

			ocui->sipua->info_call(ocui->sipua, call, "application/dtmf-relay", dtmf_body);
/*
			eXosip_lock();
			eXosip_info_call(ca->did, "application/dtmf-relay", dtmf_body);
			eXosip_unlock();
*/
			break;
		}
		default:
			beep();
			return -1;
    }

	gui->gui_print(gui, gui->parent);

	return 0;
}

int window_sessions_list_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_sessions_list_new(ogmp_curses_t* topui)
{
	gui_window_sessions_list.topui = topui;

	return &gui_window_sessions_list;
}

int window_sessions_list_done(gui_t* gui)
{
	return 0;
}

gui_t gui_window_sessions_list =
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
	NULL,
    window_sessions_list_event,
	window_sessions_list_print,
	window_sessions_list_run_command,
	NULL,
	window_sessions_list_draw_commands,
	-1,
	-1,
	-1,
	NULL
};
