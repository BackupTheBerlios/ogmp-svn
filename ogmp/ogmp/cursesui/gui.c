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


#include "gui.h"
#include "gui_topline.h"
#include "gui_icon.h"
#include "gui_menu.h"
#include "gui_new_call.h"
#include "gui_sessions_list.h"
#include "gui_subscriptions_list.h"
#include "gui_insubscriptions_list.h"
#include "gui_loglines.h"
#include "gui_online.h"

#include <timedia/os.h>
#include <timedia/xmalloc.h>

gui_t *gui_windows[10] =
{
	&gui_window_topline,
	&gui_window_icon,
	&gui_window_loglines,
	&gui_window_menu,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

gui_t *active_gui = NULL;

sipua_t *sipua = NULL;
xlist_t *profiles = NULL;
user_profile_t *sipuser = NULL;

struct colordata 
{
	int fore;
	int back;
	int attr;
};

struct colordata color[]= 
{
	/* fore              back            attr */
	{COLOR_WHITE,        COLOR_BLACK,    0	}, /* 0 default */
	{COLOR_WHITE,        COLOR_RED,      A_BOLD	}, /* 1 title */
	{COLOR_WHITE,        COLOR_GREEN,    0	}, /* 2 list */
	{COLOR_WHITE,        COLOR_YELLOW,   A_REVERSE}, /* 3 listsel */
	{COLOR_WHITE,        COLOR_BLUE,     0	}, /* 4 calls */
	{COLOR_WHITE,        COLOR_MAGENTA,  0	}, /* 5 query */
	{COLOR_WHITE,        COLOR_CYAN,     0	}, /* 6 info */
	{COLOR_BLACK,        COLOR_BLACK,    0	}, /* 7 help */
	{COLOR_BLACK,        COLOR_RED,      0	}, /* 7 help */
	{COLOR_BLACK,        COLOR_GREEN,    0	}, /* 7 help */
	{COLOR_BLACK,        COLOR_YELLOW,   0	}, /* 7 help */
	{COLOR_BLACK,        COLOR_BLUE,     0	}, /* 7 help */
	{COLOR_BLACK,        COLOR_MAGENTA,  0	}, /* 7 help */
	{COLOR_BLACK,        COLOR_CYAN,     0	}, /* 7 help */
	{COLOR_BLACK,        COLOR_WHITE,    0	}, /* 7 help */
};

int use_color = 0; /* 0: yes,      1: no */

/*
  jmosip_message_t *jmsip_context;
*/

static int cursesareon= 0;
void curseson() {
  if (!cursesareon) {
#ifdef HAVE_NCURSES
    const char *cup, *smso;
#endif
    initscr();
    start_color();

    if (has_colors() && start_color()==OK && COLOR_PAIRS >= 9) {
      short i;
      use_color = 0;
      for (i = 1; i < 15; i++) {
	if (init_pair(i, (short)color[i].fore, (short)color[i].back) != OK)
	  fprintf(stderr, "failed to allocate colour pair");
      }
    }
    else
      use_color = 1;

#ifdef HAVE_NCURSES
    cup= tigetstr("cup");
    smso= tigetstr("smso");
    if (!cup || !smso) {
      endwin();
      if (!cup)
        fputs("Terminal does not appear to support cursor addressing.\n",stderr);
      if (!smso)
        fputs("Terminal does not appear to support highlighting.\n",stderr);
      fputs("Set your TERM variable correctly, use a better terminal,\n"
	      "or make do with the per-package management tool ",stderr);
      fputs("JOSUA" ".\n",stderr);
      printf("terminal lacks necessary features, giving up");
      exit(1);
    }
#endif
  }
  cursesareon= 1;
}

void cursesoff() {
  if (cursesareon) {
    clear();
    refresh();
    endwin();
  }
  cursesareon=0;
}

#if 0
void josua_printf(char *chfr, ...)
{
  va_list ap;  
  char buf1[256];
  
  VA_START (ap, chfr);
  vsnprintf(buf1,255, chfr, ap);

  if (log_mutex==NULL)
    {
      log_mutex = osip_mutex_init();
    }

  osip_mutex_lock(log_mutex);
  /* snprintf(log_buf1,199, "%80.80s\n", buf1); */
  if (log_buf1=='\0')
    snprintf(log_buf1,255, "[%s]", buf1);
  else if (log_buf2=='\0')
    {
      if (log_buf1!='\0')
	snprintf(log_buf2,255, "%s", log_buf1);
      snprintf(log_buf1,255, "[%s]", buf1);
    }
  else
    {
      if (log_buf2!='\0')
	snprintf(log_buf3,255, "%s", log_buf2);
      snprintf(log_buf2,255, "%s", log_buf1);
      snprintf(log_buf1,255, "[%s]", buf1);
    }
    
  osip_mutex_unlock(log_mutex);

  va_end (ap);

}
#endif

int gui_clear(ogmp_curses_t* ocui)
{
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	refresh();
	clear();

	return 0;
}

int gui_print(ogmp_curses_t* ocui)
{
	int i;

	for (i=0; ocui->gui_windows[i]!=NULL; i++) 
    {
		ocui->gui_windows[i]->gui_print(ocui->gui_windows[i]);
		refresh();
    }
  
	return 0;
}

int gui_run_command(ogmp_curses_t* ocui, int c)
{
	int i;

	/* find the active window */
	for(i = 0; ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off!=GUI_ON; i++)
    {}

	if (ocui->gui_windows[i]==NULL)
    {
		beep();
		return -1;
    }

	if (ocui->gui_windows[i]->gui_run_command!=NULL)
		ocui->gui_windows[i]->gui_run_command(ocui->gui_windows[i], c);

	return 0;
}

int
josua_print_command(char **commands, int ypos, int xpos)
{
	int i;
	int y,x;
	char buf[250];
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);
	attrset(A_NORMAL);

	if (commands[0]!=NULL) /* erase with default background */
		attrset(COLOR_PAIR(10));
	else
		attrset(COLOR_PAIR(0));
  
	snprintf(buf, 199, "%199.199s", " ");
	mvaddnstr(ypos, xpos, buf, x-xpos-1);
	mvaddnstr(ypos+1, xpos, buf, x-xpos-1);

	for (i=0;commands[i]!=NULL;i=i+2)
    {
		int maxlen = strlen(commands[i+1]);
		if (commands[i+2]!=NULL)
		{
			int len = strlen(commands[i+3]);
			if (len>maxlen)
				maxlen = len;
		}

		attrset(COLOR_PAIR(1));
		snprintf(buf, strlen(commands[i])+1, commands[i]);
		mvaddnstr(ypos, xpos, buf, strlen(commands[i]));
      
		attrset(COLOR_PAIR(10));
		snprintf(buf, strlen(commands[i+1])+1, "%s", commands[i+1]);
		mvaddnstr(ypos, xpos+3, buf, maxlen);
      
		i=i+2;
		if (commands[i]==NULL)
			break;
      
		attrset(COLOR_PAIR(1));
		snprintf(buf, strlen(commands[i])+1, commands[i]);
		mvaddnstr(ypos+1, xpos, buf, strlen(commands[i]));
      
		attrset(COLOR_PAIR(10));
		snprintf(buf, strlen(commands[i+1])+1, "%s", commands[i+1]);
		mvaddnstr(ypos+1, xpos+3, buf, maxlen);
      
		xpos = xpos+maxlen+4; /* position for next column */
    }
  
	return 0;
}

void josua_clear_commands_only()
{
	int x,y;
	char *commands[] = { NULL };

	getmaxyx(stdscr,y,x);

	josua_print_command(commands, y-5, 0);
}

int
josua_clear_box_and_commands(gui_t *box)
{
	int pos;
	int y,x;
	char buf[250];
	
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);
  
	attrset(A_NORMAL);
	attrset(COLOR_PAIR(1));

	{
		char *commands[] = 
		{
			NULL
		};
    
		josua_print_command(commands, y-5, 0);
	}

	if (box->x1==-999)
    {}
	else 
		x = box->x1;

	if (box->y1<0)
		y = y + box->y1;
	else
		y = box->y1;

	attrset(COLOR_PAIR(0));
  
	for (pos=box->y0;pos<y;pos++)
    {
		snprintf(buf, 199, "%199.199s", " ");
		mvaddnstr(pos, box->x0, buf, x-box->x0-1);
    }

	return 0;
}

void
__show_toggle_online_logline()
{
	static int log_or_online=0;

	if (log_or_online==0)
    {
		log_or_online = 1;
		gui_windows[LOGLINESGUI] = &gui_window_online;

		/*window_online_print();*/
    }
	else
    {
		log_or_online = 0;
		gui_windows[LOGLINESGUI] = &gui_window_loglines;

		/*window_loglines_print();*/
    }
	
	gui_windows[LOGLINESGUI]->gui_print(gui_windows[LOGLINESGUI]);
}

int gui_key_pressed(ogmp_curses_t* ocui)
{
	int c;
	int i;

    refresh();
    halfdelay(1);
      
    if (ocui->active_gui->xcursor==-1)
	{
		noecho();
		c= getch();
	}
    else
	{
		int x,y;
		x = ocui->active_gui->x0 + ocui->active_gui->xcursor;
		y = ocui->active_gui->y0 + ocui->active_gui->ycursor;
	  
		noecho();
		keypad(stdscr, TRUE);
		c= mvgetch(y,x);
		if ((c & 0x80))
	    {
	    }
		
		refresh();
	}
/*
    }
  while (c == ERR && (errno == EINTR || errno == EAGAIN));
*/  
	if (c==ERR)
    {
		if(errno != 0)
		{}
      
		return -1;
    }
	else 
    {
		if (c==22) /* Ctrl v */
		{
			__show_toggle_online_logline();
		}
		else if (c==KEY_RIGHT)
		{
			/* activate previous windows */
			for (i=0; ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off!=GUI_ON; i++)
			{}
	  
			if (ocui->gui_windows[i]==NULL)
			{
				/* activate previous windows */
				for (i=0; ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off!=GUI_OFF; i++)
				{}

				if (gui_windows[i]==NULL)
					return -2;
	      
				ocui->gui_windows[i]->on_off = GUI_ON;
				ocui->active_gui = gui_windows[i];
			}
			else if (ocui->gui_windows[i]->on_off == GUI_ON)
			{
				ocui->gui_windows[i]->on_off = GUI_OFF;
				i++;
				for (; ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off!=GUI_OFF; i++)
				{}
	      
				if (ocui->gui_windows[i]==NULL)
				{
					for (i=0; ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off!=GUI_OFF; i++)
					{}

					if (ocui->gui_windows[i]==NULL)
						return -2;
		  
					ocui->gui_windows[i]->on_off = GUI_ON;
					ocui->active_gui = gui_windows[i];
				}
				else if (ocui->gui_windows[i]->on_off == GUI_OFF)
				{
					ocui->gui_windows[i]->on_off = GUI_ON;
					ocui->active_gui = gui_windows[i];
				}
			}
			
			if (ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->gui_draw_commands!=NULL)
				ocui->gui_windows[i]->gui_draw_commands(ocui->gui_windows[i]);
			else 
				josua_clear_commands_only();

		}
		else if (c==KEY_LEFT)
		{
			/* activate previous windows */
			for (i=0; ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off != GUI_ON ;i++)
			{}
	  
			if (ocui->gui_windows[i]==NULL)
			{
				for (i=0; ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off != GUI_OFF ;i++)
				{}
	      
				if (ocui->gui_windows[i]==NULL)
					return -2; /* no window with GUI_OFF */
	      
				ocui->gui_windows[i]->on_off = GUI_ON;
				ocui->active_gui = gui_windows[i];
			}
			else if (ocui->gui_windows[i]->on_off == GUI_ON)
			{
				ocui->gui_windows[i]->on_off = GUI_OFF;
				i--;
				for (; i>=0 && ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off != GUI_OFF ;i--)
				{}
				
				if (i>=0 && ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off == GUI_OFF)
				{
					ocui->gui_windows[i]->on_off = GUI_ON;
					ocui->active_gui = gui_windows[i];
				}
				else
				{
					for (i=0; ocui->gui_windows[i]!=NULL ;i++)
					{}
					
					i--;
		  
					for (;i>=0 && ocui->gui_windows[i]->on_off != GUI_OFF ;i--)
					{}
		  
					if (i>=0 &&  ocui->gui_windows[i]->on_off == GUI_OFF)
					{
						ocui->gui_windows[i]->on_off = GUI_ON;
						ocui->active_gui = gui_windows[i];
					}
				}
			}
	  
			if (ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->gui_draw_commands!=NULL)
				ocui->gui_windows[i]->gui_draw_commands(ocui->gui_windows[i]);
			else 
				josua_clear_commands_only();
		}
		else
		{
			/* probably for the main menu */
			return c;
		}
    }

	return -1;
}

WINDOW *
gui_print_box(gui_t *box, int draw, int color)
{
	int y,x;
  
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);

	if (box->win==NULL)
    {
		if (box->x1==-999)
		{}
		else 
			x = box->x1;
      
		if (box->y1<0)
			y = y + box->y1;
		else
			y = box->y1;
      
		box->win = newwin(y-box->y0, x-box->x0, box->y0, box->x0);
    }

	if (draw==0)
    {
		wattrset(box->win, A_NORMAL);
		wattrset(box->win, COLOR_PAIR(color));
		box(box->win, 0, 0);
    }
  
	wrefresh(box->win);
  
	return box->win;
}

int gui_event(ogmp_ui_t* ogui)
{
	return UA_OK;
}

int gui_init(ogmp_ui_t* ogui, sipua_t* sipua)
{
	ogmp_curses_t* ocui = (ogmp_curses_t*)ogui;
	ocui->sipua = sipua;
	return UA_OK;
}

int
gui_show(ogmp_ui_t* ogui)
{
	ogmp_curses_t* ocui = (ogmp_curses_t*)ogui;

	ocui->gui_windows[MENUGUI]->on_off = GUI_ON;
	ocui->active_gui = ocui->gui_windows[MENUGUI];

	gui_clear(ocui);
	gui_print(ocui);

	ocui->quit = 0;
	while(!ocui->quit)
    {
		int key;
		int i;

		/* find the active gui window */
		for (i=0; ocui->gui_windows[i]!=NULL && ocui->gui_windows[i]->on_off != GUI_ON ; i++)
		{}

		if (ocui->gui_windows[i]->on_off != GUI_ON)
			return -1; /* no active window?? */

		if (ocui->gui_windows[i]->gui_key_pressed==NULL)
			key = gui_key_pressed(ocui);
		else
			key = ocui->gui_windows[i]->gui_key_pressed(gui_windows[i]);

		if (key==-1)
		{
			/* no interesting key */
		}
		else
		{
			gui_run_command(ocui, key);
		}

		i = gui_event(ogui);

		if (i==0 && ocui->gui_windows[EXTRAGUI])
			ocui->gui_windows[EXTRAGUI]->gui_print(ocui->gui_windows[EXTRAGUI]);
		/*
		if (i==0 && gui_windows[EXTRAGUI]==&gui_window_sessions_list)
		{
			window_sessions_list_print();
		}
		else if (i==0 && gui_windows[EXTRAGUI]==&gui_window_insubscriptions_list)
		{
			window_insubscriptions_list_print();
		}
		else if (i==0 && gui_windows[EXTRAGUI]==&gui_window_subscriptions_list)
		{
			window_subscriptions_list_print();
		}
		*/
		ocui->gui_windows[LOGLINESGUI]->gui_print(ocui->gui_windows[LOGLINESGUI]);
	}

	echo();
	cursesoff();

	window_loglines_done(ocui->gui_windows[LOGLINESGUI]);
	xfree(ocui);
	
	return UA_OK;
}

ogmp_ui_t* ogmp_new_ui()
{
	ogmp_ui_t* ogui;
	ogmp_curses_t *ocui = xmalloc(sizeof(ogmp_curses_t));

	memset(ocui, 0, sizeof(ogmp_curses_t));

	ocui->gui_windows[TOPGUI] = &gui_window_topline;
	gui_window_topline.topui = ocui;

	ocui->gui_windows[ICONGUI] = &gui_window_icon;
	gui_window_icon.topui = ocui;
	
	ocui->gui_windows[LOGLINESGUI] = window_loglines_new(ocui);
	
	ocui->gui_windows[MENUGUI] = &gui_window_menu;
	gui_window_menu.topui = ocui;

	ogui = (ogmp_ui_t*)ocui;

	ogui->init = gui_init;
	ogui->show = gui_show;

	return (ogmp_ui_t*)ocui;
}
