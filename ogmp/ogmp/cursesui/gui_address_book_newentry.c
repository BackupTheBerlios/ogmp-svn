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

gui_t gui_window_address_book_newentry = 
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
	NULL,
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

int window_address_book_newentry_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];
  
	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	gui->parent = wid;

	getmaxyx(stdscr,y,x);
  
	attrset(A_NORMAL);
	attrset(COLOR_PAIR(1));

	if (gui->x1==-999)
    {}
	else 
		x = gui->x1;

	attrset(COLOR_PAIR(0));

	snprintf(buf, 199, "%199.199s", " ");
	mvaddnstr(gui->y0+1, gui->x0, buf, x-gui->x0-1);
	mvaddnstr(gui->y0+2, gui->x0, buf, x-gui->x0-1);
	mvaddnstr(gui->y0+3, gui->x0, buf, x-gui->x0-1);
	mvaddnstr(gui->y0+4, gui->x0, buf, x-gui->x0-1);
  
	attrset(COLOR_PAIR(1));

	snprintf(buf, x - gui->x0, "Name      : ");
	mvaddnstr(gui->y0, gui->x0, buf, (x - gui->x0 - 1));
  
	snprintf(buf, x - gui->x0, "Memo      : ");
	mvaddnstr(gui->y0+1, gui->x0, buf, (x - gui->x0 - 1));
  
	snprintf(buf, x - gui->x0, "Public ID : ");
	mvaddnstr(gui->y0+2, gui->x0, buf, (x - gui->x0 - 1));
  
	snprintf(buf, x - gui->x0, "Private ID: ");
	mvaddnstr(gui->y0+3, gui->x0, buf, (x - gui->x0 - 1));
  
	gui->gui_draw_commands(gui);
	/*window_address_book_newentry_draw_commands();*/

	return 0;
}


int window_address_book_newentry_run_command(gui_t* gui, int c)
{
	int y,x;

	ogmp_curses_t *ocui = gui->topui;
  
	getmaxyx(stdscr,y,x);

	if (gui->x1==-999)
    {}
	else 
		x = gui->x1;

	switch (c)
    {
		case KEY_DC:
		{
			delch();
			break;
		}
		case KEY_BACKSPACE:
		case 127:
		{
			if (ocui->active_gui->xcursor>10)
			{
				int xcur,ycur;

				ocui->active_gui->xcursor--;
				getyx(stdscr,ycur,xcur);
				move(ycur,xcur-1);
				delch();
			}
			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		case KEY_DOWN:
		{
			if (gui->ycursor<3)
			{
				gui->ycursor++;
				gui->xcursor=10;
			}
			break;
		}
		case KEY_UP:
		{
			if (gui->ycursor>0)
			{
				gui->ycursor--;
				gui->xcursor=10;
			}
			break;
		}
		case KEY_RIGHT:
		{
			if (gui->xcursor < x - gui->x0 - 1)
				gui->xcursor++;

			break;
		}
		case KEY_LEFT:
		{
			if (gui->xcursor > 0)
				gui->xcursor--;
      
			break;
		}

		/* case 20: */  /* Ctrl-T */
		case 1:  /* Ctrl-A */
		{
			int ycur = gui->y0;
			int xcur = gui->x0+10;

			char name[200];
			char memo[200];
			char pub[200];
			char pri[200];

			sipua_contact_t *c;
	
			mvinnstr(ycur, xcur, name, (x - gui->x0 - 10));
			ycur++;
	
			mvinnstr(ycur, xcur, memo, (x - gui->x0 - 10));
			ycur++;
	
			mvinnstr(ycur, xcur, pub, (x - gui->x0 - 10));
			ycur++;
	
			mvinnstr(ycur, xcur, pri, (x - gui->x0 - 10));
	
			/*_josua_add_contact(sipurl, telurl, email, phone);*/
			c = sipua_new_contact(name, strlen(name)+1, memo, strlen(memo)+1, pub, pri);
			sipua_add_contact(ocui->phonebook, c);
      
			break;
		}
		case 4:  /* Ctrl-D */
		{
			char buf[200];
	
			attrset(COLOR_PAIR(0));
		
			snprintf(buf, 199, "%199.199s", " ");
			mvaddnstr(gui->y0 + gui->ycursor, gui->x0 + 10, buf, (x - gui->x0 - 10 - 1));
	
			gui->xcursor=10;
      
			break;
		}
		case 5:  /* Ctrl-E */
		{
			gui->xcursor=10;
			gui->ycursor=0;

			gui->gui_print(gui, GUI_NEWUSER);
			/*window_address_book_newentry_print();*/

			break;
		}
		default:
		{
			if (gui->xcursor < (x - gui->x0 - 1))
			{
				gui->xcursor++;
	  
				attrset(COLOR_PAIR(0));
				echochar(c);
			}
			else
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
		"<-",  "PrevWindow",
		"->",  "NextWindow",
		"^A", "AddEntry" ,
		"^D", "DeleteLine",
		"^E", "EraseAll",
		NULL
	};
  
	getmaxyx(stdscr,y,x);
  
	josua_print_command(address_book_newentry_commands, y-5, 0);
}

gui_t* window_address_book_newentry_new(ogmp_curses_t* topui)
{
	gui_window_address_book_newentry.topui = topui;

	return &gui_window_address_book_newentry;
}

int window_address_book_newentry_done(gui_t* gui)
{
	return 0;
}
