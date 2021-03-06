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

/*#include "jinsubscriptions.h"*/
#include "gui_menu.h"
#include "gui_insubscriptions_list.h"
#include "gui_online.h"

int cursor_insubscriptions_list = 0;

int window_insubscriptions_list_print(gui_t* gui, int wid)
{
	int y,x;

	gui->parent = wid;

	if(gui->topui->gui_windows[EXTRAGUI])
		josua_clear_box_and_commands(gui->topui->gui_windows[EXTRAGUI]);

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);
  
	attrset(A_NORMAL);
	attrset(COLOR_PAIR(1));

	if (gui->x1==-999)
    {}
	else 
		x = gui->x1;

	attrset(COLOR_PAIR(0));
#if 0
	int k, pos;
	char buf[250];
	pos=1;
	for (k=0; k<MAX_NUMBER_OF_INSUBSCRIPTIONS; k++)
    {
		if (jinsubscriptions[k].state != NOT_USED)
		{
			char *tmp;
			snprintf(buf, 199, "%c%c %3.3i//%3.3i    %-40.40s ",
						(cursor_insubscriptions_list==pos-1) ? '-' : ' ',
						(cursor_insubscriptions_list==pos-1) ? '>' : ' ',
						jinsubscriptions[k].nid,
						jinsubscriptions[k].did,
						jinsubscriptions[k].remote_uri);
			tmp = buf + strlen(buf);
	  
			if (jinsubscriptions[k].ss_status==EXOSIP_SUBCRSTATE_UNKNOWN)
				snprintf(tmp, 199-strlen(buf), " %-14.14s", "--unknown--");
			else if (jinsubscriptions[k].ss_status==EXOSIP_SUBCRSTATE_PENDING)
				snprintf(tmp, 199-strlen(buf), " %-14.14s", "--pending--");
			else if (jinsubscriptions[k].ss_status==EXOSIP_SUBCRSTATE_ACTIVE)
			{
				if (jinsubscriptions[k].online_status==EXOSIP_NOTIFY_UNKNOWN)
					snprintf(tmp, 199-strlen(buf), " %-14.14s", "unknown");
				else if (jinsubscriptions[k].online_status==EXOSIP_NOTIFY_PENDING)
					snprintf(tmp, 199-strlen(buf), " %-14.14s", "Pending");
				else if (jinsubscriptions[k].online_status==EXOSIP_NOTIFY_ONLINE)
					snprintf(tmp, 199-strlen(buf), " %-14.14s", "Online");
				else if (jinsubscriptions[k].online_status==EXOSIP_NOTIFY_BUSY)
					snprintf(tmp, 199-strlen(buf), " %-14.14s", "Busy");
				else if (jinsubscriptions[k].online_status==EXOSIP_NOTIFY_BERIGHTBACK)
					snprintf(tmp, 199-strlen(buf), " %-14.14s", "Be Right Back");
				else if (jinsubscriptions[k].online_status==EXOSIP_NOTIFY_AWAY)
					snprintf(tmp, 199-strlen(buf), " %-14.14s", "Away");
				else if (jinsubscriptions[k].online_status==EXOSIP_NOTIFY_ONTHEPHONE)
					snprintf(tmp, 199-strlen(buf), " %-14.14s", "On The Phone");
				else if (jinsubscriptions[k].online_status==EXOSIP_NOTIFY_OUTTOLUNCH)
					snprintf(tmp, 199-strlen(buf), " %-14.14s", "Out To Lunch");
				else if (jinsubscriptions[k].online_status==EXOSIP_NOTIFY_CLOSED)
					snprintf(tmp, 199-strlen(buf), " %-14.14s", "Closed");
			}
			else if (jinsubscriptions[k].ss_status==EXOSIP_SUBCRSTATE_TERMINATED)
				snprintf(tmp, 199-strlen(buf), " %14.14s", "--terminated--");

			tmp = buf + strlen(buf);
			
			snprintf(tmp, 199-strlen(buf), " %100.100s", " ");

			attrset(COLOR_PAIR(5));
			attrset((pos-1==cursor_insubscriptions_list) ? A_REVERSE : A_NORMAL);

			mvaddnstr(gui->y0+pos-1, gui->x0, buf, x-gui->x0-1);
	  
			pos++;
		}

		if (pos > y + gui->y1 - gui->y0 + 1)
			break; /* do not print next one */
    }
#endif
	window_insubscriptions_list_draw_commands(gui);
  
	return 0;
}

void window_insubscriptions_list_draw_commands(gui_t* gui)
{
	int x,y;
	char *insubscriptions_list_commands[] = 
	{
		"<-",  "PrevWindow",
		"->",  "NextWindow",
		"a",  "Accept",
		"c",  "Close" ,
		"o",  "Online",
		"g",  "Away",
		"b",  "Busy",
		"5",  "BackSoon",
		"p",  "OnthePhone",
		"l",  "OutToLunch",
		"t",  "ViewSubscribers",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(insubscriptions_list_commands, y-5, 0);
}


int window_insubscriptions_list_run_command(gui_t* gui, int c)
{
	/*jinsubscription_t *js;*/
	int max;
	int y,x;

	int do_notify = 0;
	int online_state;
	int sub_state;

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
#if 0
	int i;
	int do_allnotify = 0;
	i = jinsubscription_get_number_of_pending_insubscriptions();
	if (i<max) 
		max=i;
#endif
	if (max==0 && c!='t')
    {
		beep();
		return -1;
    }
  
	switch (c)
    {
		case KEY_DOWN:
			cursor_insubscriptions_list++;
			cursor_insubscriptions_list %= max;
			break;
		case KEY_UP:
			cursor_insubscriptions_list += max-1;
			cursor_insubscriptions_list %= max;
			break;
	
		case 't':
			gui_show_window(gui, GUI_SUBS, GUI_INSUBS);
			break;

		case 'a':
			do_notify = 1;
			online_state = EXOSIP_NOTIFY_ONLINE;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'c':
#if 0
			js = jinsubscription_find_insubscription(cursor_insubscriptions_list);
			if (js==NULL) 
			{ 
				beep(); 
				break; 
			}

			eXosip_lock();

			i = eXosip_notify(js->did, EXOSIP_SUBCRSTATE_TERMINATED, EXOSIP_NOTIFY_CLOSED);
			if (i!=0) 
				beep();
			if (i==0)
				jinsubscription_remove(js);
      
			eXosip_unlock();
			break;
		case 'O':
			do_allnotify = 1;
			online_state = EXOSIP_NOTIFY_ONLINE;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'o':
			do_notify = 1;
			online_state = EXOSIP_NOTIFY_ONLINE;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'G':
			do_allnotify = 1;
			online_state = EXOSIP_NOTIFY_AWAY;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'g':
			do_notify = 1;
			online_state = EXOSIP_NOTIFY_AWAY;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'B':
			do_allnotify = 1;
			online_state = EXOSIP_NOTIFY_BUSY;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'b':
			do_notify = 1;
			online_state = EXOSIP_NOTIFY_BUSY;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case '5':
			do_notify = 1;
			online_state = EXOSIP_NOTIFY_BERIGHTBACK;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'P':
			do_allnotify = 1;
			online_state = EXOSIP_NOTIFY_ONTHEPHONE;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'p':
			do_notify = 1;
			online_state = EXOSIP_NOTIFY_ONTHEPHONE;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'L':
			do_allnotify = 1;
			online_state = EXOSIP_NOTIFY_OUTTOLUNCH;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
		case 'l':
			do_notify = 1;
			online_state = EXOSIP_NOTIFY_OUTTOLUNCH;
			sub_state = EXOSIP_SUBCRSTATE_ACTIVE;
			break;
#endif
		default:
			beep();
			return -1;
    }
#if 0
	if (do_notify==1)
    {
		js = jinsubscription_find_insubscription(cursor_insubscriptions_list);
		if (js==NULL) 
		{ 
			beep(); 
			return -1; 
		}
		
		eXosip_lock();
      
		i = eXosip_notify(js->did, sub_state, online_state);
		if (i!=0) 
			beep();
      
		eXosip_unlock();
    }
	else if (do_allnotify==1)
    {
		int k;
		for (k=0;k<MAX_NUMBER_OF_INSUBSCRIPTIONS;k++)
		{
			if (jinsubscriptions[k].state != NOT_USED)
			{
				eXosip_lock();
				
				i = eXosip_notify(jinsubscriptions[k].did, sub_state, online_state);
				if (i!=0) 
					beep();
	      
				eXosip_unlock();
			}
		}
    }
#endif
	if (gui_window_insubscriptions_list.on_off==GUI_ON)
		window_insubscriptions_list_print(gui, GUI_INSUBS);
  
	return 0;
}

int window_insubscriptions_list_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_insubscriptions_list_new(ogmp_curses_t* topui)
{
	gui_window_insubscriptions_list.topui = topui;

	return &gui_window_insubscriptions_list;
}

int window_insubscriptions_list_done(gui_t* gui)
{
	return 0;
}

gui_t gui_window_insubscriptions_list =
{
  GUI_OFF,
  0,
  -999,
  10,
  -6,
  window_insubscriptions_list_event,
  NULL,
  window_insubscriptions_list_print,
  window_insubscriptions_list_run_command,
  NULL,
  window_insubscriptions_list_draw_commands,
  -1,
  -1,
  -1,
  NULL
};
