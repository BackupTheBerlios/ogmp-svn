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

#include "editor.h"

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

#ifndef	LINE_MAX
#define LINE_MAX 128
#endif

char booksrc[LINE_MAX];
editline_t *booksrc_edit;

int cursor_address_book_browse = 0;
int cursor_address_book_start  = 0;
int show_mail=-1;

int cursor_phone_book_pos = 0;
int cursor_phone_book_view  = 0;

void window_address_book_browse_draw_commands(gui_t* gui)
{
	int x,y;
	ogmp_curses_t* ocui = gui->topui;

	char *address_book_browse_commands[] = 
	{
		"^X",  "NewCall" ,
		"^A",  "AddContact" ,
		"^D",  "DelContact" ,
		"^S",  "SaveBook" ,
		"^C",  "Cancel" ,
		NULL
	};

	char *address_book_open_commands[] = 
	{
		"^E",  "NewBook" ,
		"CR",  "OpenBook" ,
		"^C",  "Cancel" ,
		NULL
	};
  
	getmaxyx(stdscr,y,x);

	if(!ocui->phonebook)
		josua_print_command(address_book_open_commands, y-5, 0);
	else
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
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);

	gui->parent = wid;

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);
  
	getmaxyx(stdscr,y,x);

	/* Window Title */
	attrset(COLOR_PAIR(4));
	snprintf(buf, 250, "%199.199s", " ");
	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0));

	snprintf(buf, x-gui->x0, "Phone Book for '%s'<%s>", user_profile->fullname, user_profile->regname);
	mvaddstr(gui->y0, gui->x0+1, buf);

	attrset(COLOR_PAIR(0));
	snprintf(buf, 250, "%199.199s", " ");
	mvaddnstr(gui->y0+1, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+2, gui->x0, buf, (x-gui->x0));
	mvaddnstr(gui->y0+3, gui->x0, buf, (x-gui->x0));

	/* Window Body */
	attrset(A_NORMAL);

	if(!ocui->phonebook)
	{
		int pos;
		char c, *ch;
	
		if(!booksrc_edit)
		{
			booksrc_edit = editline_new(booksrc, LINE_MAX);
			editline_set_line(booksrc_edit, user_profile->book_location, strlen(user_profile->book_location));
		}
		
		pos = editline_pos(booksrc_edit);
		editline_char(booksrc_edit, &ch);
		if(!*ch)
			c = ' ';
		else
			c = *ch;

		attrset(COLOR_PAIR(10));

		snprintf(buf, 25, "%13.13s", " Phone book :");
		mvaddstr(gui->y0+2, gui->x0, buf);

		attrset(COLOR_PAIR(0));
		mvaddstr(gui->y0+2, gui->x0+14, booksrc);

		attrset(COLOR_PAIR(10));
		mvaddch(gui->y0+2, gui->x0+14+pos, c);

		gui->gui_draw_commands(gui);

		return 0;
	}

	contacts = sipua_contacts(ocui->phonebook);

	if(!contacts || xlist_size(contacts) == 0)
	{
		attrset(COLOR_PAIR(1));

		snprintf(buf, x - gui->x0, "No contacts available !");
		mvaddstr(gui->y0+2, gui->x0+1, buf);

		gui->gui_draw_commands(gui);
		return 0;
	}

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

		snprintf(buf, x - gui->x0, " %c%c '%s' <%s> - %-80.80s",
					(cursor_address_book_browse==pos) ? '-' : ' ',
					(cursor_address_book_browse==pos) ? '>' : ' ',
					contact->name, contact->sip, contact->memo);
      
		attrset((pos==cursor_address_book_browse) ? COLOR_PAIR(10) : COLOR_PAIR(1));
		mvaddnstr(pos+gui->y0+1, gui->x0, buf, x-gui->x0);
      
		pos++;

		contact = (sipua_contact_t*)xlist_next(contacts, &u);
    }
  
	gui->gui_draw_commands(gui);
  
	return 0;
}

int window_address_book_browse_run_command(gui_t* gui, int c)
{
	int y, x;
	int pos;
	int max = 0;
  
	sipua_contact_t* contact;
	xlist_t* contacts;
	xlist_user_t u;
  
	ogmp_curses_t* ocui = gui->topui;
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);
	sipua_phonebook_t* phonebook = ocui->phonebook;

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);

	if (ocui->phonebook)
	{			
		contacts = sipua_contacts(ocui->phonebook);
		max = xlist_size(contacts);
	}
  
	switch (c)
    {
		case 1: /* ctrl-A */
		{
			if (!ocui->phonebook)
				break;
				
			gui_show_window(gui, GUI_ENTRY, GUI_BROWSE);

			break;
		}
		case 3: /* ctrl-C */
		{
			if(booksrc_edit)
			{
				editline_done(booksrc_edit);
				booksrc_edit = NULL;
			}

			gui_hide_window(gui);

			break;
		}
		case 4: /* ctrl-D */
		{
			/* delete entry */
			if (!ocui->phonebook)
				break;
				
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
		case 5: /* ctrl-E */
		{
			if(ocui->phonebook)
				break;
			
			ocui->phonebook = sipua_new_book();
			if(!ocui->phonebook)
				break;

			if(strcmp(booksrc, user_profile->book_location) == 0)
				break;

			if(user_profile->book_location)
				xfree(user_profile->book_location);

			user_profile->book_location = xstr_clone(booksrc);
			ocui->user->modified = 1;

			break;
		}
		case 19: /* ctrl-S */
		{
			/* Save the phone book */
			sipua_save_book(ocui->phonebook, user_profile->book_location);

			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		case 24: /* ctrl-X */
		{
			if(!ocui->phonebook)
			{
				ocui->phonebook = sipua_load_book(booksrc);

				if(ocui->phonebook)
				{
					editline_done(booksrc_edit);
					booksrc_edit = NULL;
				}

				break;
			}

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
				ocui->contact = contact;

			gui_show_window(gui, GUI_NEWCALL, GUI_BROWSE);

			break;
		}
		case KEY_DOWN:
		{
			if (!ocui->phonebook)
				break;
				
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
			if (!ocui->phonebook)
				break;
				
			if (cursor_address_book_browse > 0)
				cursor_address_book_browse--;
			else if (cursor_address_book_start > 0)
				cursor_address_book_start--;
			else 
				beep();

			break;
		}
		case KEY_RIGHT:
		{
			if (ocui->phonebook)
				break;
				
			if (editline_move_pos(booksrc_edit, 1) < 0)
				beep();

			break;
		}
		case KEY_LEFT:
		{
			if (ocui->phonebook)
				break;
				
			if (editline_move_pos(booksrc_edit, -1) < 0)
				beep();

			break;
		}
		case KEY_DC:
		{
			if (ocui->phonebook)
				break;
				
			editline_remove_char(booksrc_edit);
			
			delch();

			break;
		}
		case '\b':
		{
			if (ocui->phonebook)
				break;
				
			if (editline_move_pos(booksrc_edit, -1) >= 0)
				editline_remove_char(booksrc_edit);
			else
				beep();

			break;
		}
		default:
		{
			if(ocui->phonebook)
			{
				beep();  
				return -1;
			}

			if(editline_append(booksrc_edit, (char*)&c, 1) == 0)
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
	if(booksrc_edit)
		editline_done(booksrc_edit);
	
	return 0;
}
