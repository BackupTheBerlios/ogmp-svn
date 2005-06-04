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

#include "gui_sessions_list.h"

int calllist_line = 0;

int window_sessions_list_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];
	char status[20];

	sipua_call_t* call;

	int nbusy, line, n, view;
	int busylines[MAX_SIPUA_LINES];
	int max;

	ogmp_curses_t* ocui = gui->topui;
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);

	int ntitle=0, nincall=0;

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

	ntitle = 1;

	/* Window Body */

	/* Current incall */
	if(ocui->sipua->incall)
	{
		call = ocui->sipua->incall;
			
		switch(call->status)
		{
			case SIPUA_EVENT_NEW_CALL:
				strcpy(status, "Incoming"); break;
			case SIPUA_EVENT_PROCEEDING:
				strcpy(status, "Proceeding"); break;
			case SIPUA_EVENT_RINGING:
				strcpy(status, "Ringing"); break;
			case SIPUA_EVENT_ANSWERED:
				strcpy(status, "Answered"); break;
			case SIPUA_EVENT_ACK:
				strcpy(status, "Established"); break;
			case SIPUA_EVENT_REQUESTFAILURE:
				strcpy(status, "Request Fail"); break;
			case SIPUA_EVENT_SERVERFAILURE:
				strcpy(status, "Server Fail"); break;
			case SIPUA_EVENT_GLOBALFAILURE:
				strcpy(status, "Global Fail"); break;
			case SIPUA_EVENT_ONHOLD:
				strcpy(status, "On hold"); break;
			case SIP_STATUS_CODE_TEMPORARILYUNAVAILABLE:
				strcpy(status, "Rejected"); break;
			case SIP_STATUS_CODE_BUSYHERE:
				strcpy(status, "Busy"); break;
			case SIP_STATUS_CODE_DECLINE:
				strcpy(status, "Declined"); break;
			default:
				strcpy(status, "Unknown");
		}
        
		if(call->from)
			snprintf(buf, x - gui->x0, " In call from <%s>: (%s) %s - %-80.80s",
						call->from, status, call->subject, call->info);
		else
			snprintf(buf, x - gui->x0, " In call of mine: (%s) %s - %-80.80s",
						status, call->subject, call->info);
      
		attrset(COLOR_PAIR(4));
	}
	else
	{
		snprintf(buf, x - gui->x0, " %-80.80s", "You are free of call");

		attrset(COLOR_PAIR(10));
	}
      
	mvaddnstr(gui->y0+ntitle, gui->x0, buf, x-gui->x0);
	nincall = 1;

	/* Show all lines */

	ocui->sipua->lock_lines(ocui->sipua);

	nbusy = ocui->sipua->busylines(ocui->sipua, busylines, MAX_SIPUA_LINES);
	if(nbusy == 0)
	{
		attrset(COLOR_PAIR(1));

		snprintf(buf, x - gui->x0, " %-80.80s", "No call waiting!");
		mvaddnstr(gui->y0+ntitle+nincall, gui->x0, buf, x-gui->x0);

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
		call = ocui->sipua->line(ocui->sipua, busylines[line]);

		switch(call->status)
		{
			case SIPUA_EVENT_NEW_CALL:
				strcpy(status, "Incoming"); break;
			case SIPUA_EVENT_PROCEEDING:
				strcpy(status, "Proceeding"); break;
			case SIPUA_EVENT_RINGING:
				strcpy(status, "Ringing"); break;
			case SIPUA_EVENT_ANSWERED:
				strcpy(status, "Answered"); break;
			case SIPUA_EVENT_REQUESTFAILURE:
				strcpy(status, "Request Fail"); break;
			case SIPUA_EVENT_SERVERFAILURE:
				strcpy(status, "Server Fail"); break;
			case SIPUA_EVENT_GLOBALFAILURE:
				strcpy(status, "Global Fail"); break;
			case SIPUA_EVENT_ONHOLD:
				strcpy(status, "On hold"); break;
			case SIP_STATUS_CODE_TEMPORARILYUNAVAILABLE:
				strcpy(status, "Rejected"); break;
			case SIP_STATUS_CODE_BUSYHERE:
				strcpy(status, "Busy"); break;
			case SIP_STATUS_CODE_DECLINE:
				strcpy(status, "Declined"); break;
			case SIPUA_EVENT_ACK:
				strcpy(status, "Established"); break;
			default:
				strcpy(status, "Unknown");
		}

		if(line==calllist_line)
		{
			if(call->from)
				snprintf(buf, x - gui->x0, " %c%c [%d] (%s) From <%s>: %s - %-80.80s",
						'-', '>', line, status, call->from, call->subject, call->info);
			else
				snprintf(buf, x - gui->x0, " %c%c [%d] (%s) My call: %s - %-80.80s",
						'-', '>', line, status, call->subject, call->info);
      
			attrset(COLOR_PAIR(10));
		}
		else
		{
			if(call->from)
				snprintf(buf, x - gui->x0, " %c%c  %d  (%s) From <%s>: %s - %-80.80s",
						' ', ' ', line, status, call->from, call->subject, call->info);
			else
				snprintf(buf, x - gui->x0, " %c%c  %d  (%s) My call: %s - %-80.80s",
						' ', ' ', line, status, call->subject, call->info);
      
			attrset(COLOR_PAIR(1));
		}

		mvaddnstr(gui->y0+ntitle+nincall+line, gui->x0, buf, x-gui->x0);

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
		"N",  "New" ,
		"A",  "Answer",
      "Z",  "Queue",
		"C",  "Bye",
		"D",  "Decline",
		"R",  "Reject",
		"B",  "Busy",
		"H",  "Hold/Unhold"  ,
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

	nbusy = ocui->sipua->busylines(ocui->sipua, busylines, MAX_SIPUA_LINES);

   curseson(); cbreak(); noecho(); nonl(); keypad(stdscr, TRUE);

	switch (c)
    {
		sipua_call_t* call;

		case KEY_DOWN:
		{
			int n = 0;
            
         if(nbusy == 0)
         {
            beep();
            break;
         }
            
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

            if(nbusy == 0)
            {
                beep();
                break;
            }

            while(busylines[n] != calllist_line) n++;

			if(n==0)

			{
				beep();
				break;
			}

			calllist_line = busylines[--n];
			break;
		}
		case 'n': 
		{
            if(ocui->sipua->incall)
            {
                beep();
                break;
            }

            gui_show_window(gui, GUI_NEWCALL, GUI_SESSION);
            
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

         ocui->sipua->answer(ocui->sipua, call, SIP_STATUS_CODE_OK, NULL);

			break;
		}
		case 'z':
		{
         call = ocui->sipua->pick(ocui->sipua, calllist_line);
			if (!call)
			{
				beep();
				break;
			}

         ocui->sipua->queue(ocui->sipua, call);

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

			ocui->sipua->answer(ocui->sipua, call, SIP_STATUS_CODE_TEMPORARILYUNAVAILABLE, NULL);

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

			ocui->sipua->answer(ocui->sipua, call, SIP_STATUS_CODE_DECLINE, NULL);
			
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

			ocui->sipua->answer(ocui->sipua, call, SIP_STATUS_CODE_BUSYHERE, NULL);
			
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

			gui->gui_print(gui, gui->parent);

			break;
		}
		case 'h':   
		{
			if(ocui->sipua->incall)
				calllist_line = ocui->sipua->hold(ocui->sipua);
			else

			{
				call = ocui->sipua->pick(ocui->sipua, calllist_line);
				if (!call) 
				{ 
					beep(); 
					break; 
				}

            /* Issue: hold call signal to participant, may release bandwidth or something.
             * can not simple annswer that cause reestablish call.
             * ocui->sipua->answer(ocui->sipua, call, SIPUA_STATUS_ANSWER);
             */
			}

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

			break;
		}
		default:
			beep();
			return -1;
    }

    gui_update(gui);

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
    window_sessions_list_event,
	NULL,
	window_sessions_list_print,
	window_sessions_list_run_command,
	NULL,
	window_sessions_list_draw_commands,
	-1,
	-1,
	-1,
	NULL
};
