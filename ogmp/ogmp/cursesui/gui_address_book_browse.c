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

#include "gui_address_book_browse.h"
#include "gui_address_book_newentry.h"

#include "gui_menu.h"
#include "gui_new_call.h"

/*  extern eXosip_t eXosip; */

gui_t gui_window_address_book_browse = 
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
	NULL,
	window_address_book_browse_print,
	window_address_book_browse_run_command,
	NULL,
	window_address_book_browse_draw_commands,
	-1,
	-1,
	-1,
	NULL,
	NULL
};

int cursor_address_book_browse = 0;
int cursor_address_book_start  = 0;
int show_mail=-1;

void window_address_book_browse_draw_commands(gui_t* gui)
{
	int x,y;
	char *address_book_browse_commands[] = 
	{
		"<-",  "PrevWindow",
		"->",  "NextWindow",
		"X",  "StartCall" ,
		"a",  "AddEntry" ,
		"d",  "DeleteEntry" ,
		"t",  "Toggle",
		NULL
	};
  
	getmaxyx(stdscr,y,x);

	josua_print_command(address_book_browse_commands, y-5, 0);
}

int window_address_book_browse_print(gui_t* gui, int wid)
{
	int y,x;
	char buf[250];
	int pos;
	int pos_fr;

	sipua_contact_t* contact;
	xlist_t* contacts;
	xlist_user_t u;
  
	ogmp_curses_t* ocui = gui->topui;

	gui->parent = wid;

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);
  
	getmaxyx(stdscr,y,x);

	if(!ocui->phonebook)
	{
		snprintf(buf, gui->x1 - gui->x0, "No phonebook available !");
      
		attrset(COLOR_PAIR(10));

		mvaddnstr(gui->y0, gui->x0, buf, x-gui->x0-1);

		return 0;
	}

	contacts = sipua_contacts(ocui->phonebook);

	pos_fr = 0;
	contact = (sipua_contact_t*)xlist_first(contacts, &u);

	while(contact)
    {
		if (cursor_address_book_start==pos_fr)
			break;

		pos_fr++;

		contact = (sipua_contact_t*)xlist_next(contacts, &u);
    }

	pos = 0;
	while(contact)
    {
		if (pos == y+gui->y1 - gui->y0)
			break;

		snprintf(buf, gui->x1 - gui->x0, "%c%c %-15.15s %-80.80s",
					(cursor_address_book_browse==pos) ? '-' : ' ',
					(cursor_address_book_browse==pos) ? '>' : ' ',
					contact->name, contact->memo);
      
		attrset((pos==cursor_address_book_browse) ? A_REVERSE : COLOR_PAIR(0));
		mvaddnstr(pos+gui->y0, gui->x0, buf, x-gui->x0-1);
      
		pos++;

		contact = (sipua_contact_t*)xlist_next(contacts, &u);
    }
  
	refresh();

	gui->gui_draw_commands(gui);
	/*window_address_book_browse_draw_commands(gui);*/
  
	return 0;
}

int window_address_book_browse_run_command(gui_t* gui, int c)
{
	int y, x;
	int pos;
	int max;
  
	sipua_contact_t* contact;
	xlist_t* contacts;
	xlist_user_t u;
  
	ogmp_curses_t* ocui = gui->topui;
	sipua_phonebook_t* phonebook = ocui->phonebook;

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);

	if(!ocui->phonebook)
		return 0;

	contacts = sipua_contacts(ocui->phonebook);

	max = xlist_size(contacts);
  
	switch (c)
    {
		case KEY_DOWN:
		{
			/* 
			cursor_address_book_browse++;
			cursor_address_book_browse %= max; 
			*/
			if (cursor_address_book_browse < y+gui->y1-gui->y0-1 && cursor_address_book_browse < max-1)
				cursor_address_book_browse++;
			else if (cursor_address_book_start < max - (y + gui->y1 - gui->y0))
				cursor_address_book_start++;
			else 
				beep();
      
			break;
		}
		case KEY_UP:
		{
			if (cursor_address_book_browse > 0)
				cursor_address_book_browse--;
			else if (cursor_address_book_start > 0)
				cursor_address_book_start--;
			else 
				beep();

			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		case 24: /* ctrl-X */
		{
			/* start call */
			pos=0;
			contact = (sipua_contact_t*)xlist_first(contacts, &u);

			while(contact)
			{
				if(cursor_address_book_browse + cursor_address_book_start == pos)
					break;

				pos++;
				contact = (sipua_contact_t*)xlist_next(contacts, &u);
			}
      
			if (contact)
				window_new_call_with_to(gui, contact->name);

			gui_show_window(gui, GUI_NEWCALL, GUI_BROWSE);

			break;
		}
		case 't':
		{
			if (show_mail!=0)
				show_mail=0;
			else 
				show_mail=-1;
      
			break;
		}
		case 'a':
		{
			gui_show_window(gui, GUI_ENTRY, GUI_BROWSE);
			break;
		}
		case 'd':
		{
			/* delete entry */
			pos=0;
			contact = (sipua_contact_t*)xlist_first(contacts, &u);

			while(contact)
			{
				if (cursor_address_book_browse+cursor_address_book_start==pos)
					break;

				pos++;
				contact = (sipua_contact_t*)xlist_next(contacts, &u);
			}

			if (contact)
				sipua_remove_contact(phonebook, contact);
      
			break;
		}
		default:
		{
			beep();  
			return -1;
		}
	}

	if(gui->on_off==GUI_ON)
		gui->gui_print(gui, gui->parent);
  
	return 0;
}

gui_t* window_address_book_browse_new(ogmp_curses_t* topui)
{
	gui_window_address_book_browse.topui = topui;

	return &gui_window_address_book_browse;
}

int window_address_book_browse_done(gui_t* gui)
{
	return 0;
}
