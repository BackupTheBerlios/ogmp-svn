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

#include "gui_profiles.h"

int cursor_profiles_pos = 0;
int cursor_profiles_view  = 0;

int window_profiles_print(gui_t* gui, int wid)
{
	int k, y, x, nview, nprof;
	char buf[250];

	xlist_t* profiles;
	xlist_user_t lu;

	user_profile_t* prof;

	ogmp_curses_t* ocui = gui->topui;
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);

	gui->parent = wid;

	if(ocui->gui_windows[EXTRAGUI])
		josua_clear_box_and_commands(ocui->gui_windows[EXTRAGUI]);

	curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);

	getmaxyx(stdscr,y,x);

	attrset(A_NORMAL);

	/* Window Title */
	snprintf(buf, 250, "%199.199s", " ");

	attrset(COLOR_PAIR(4));
	mvaddnstr(gui->y0, gui->x0, buf, (x-gui->x0));
	if(ocui->user)
		snprintf(buf, x-gui->x0-1, "Profile list of <%s>", ocui->user->userloc);
	else
		snprintf(buf, x-gui->x0-1, "User profile list");
	mvaddstr(gui->y0, gui->x0+1, buf);

	/* Window Body */
	attrset(COLOR_PAIR(1));

	if (gui->x1==-999)
    {}
	else 
		x = gui->x1;
	
	profiles = ocui->user->profiles;

	nprof = xlist_size(profiles);

	if(nprof == 0)
	{
		snprintf(buf, 199, "No profile available yet");

		attrset(COLOR_PAIR(1));

		mvaddnstr(gui->y0+1, gui->x0, buf, x-gui->x0);
	
		window_profiles_draw_commands(gui);
  
		return 0;
	}

	attrset(COLOR_PAIR(0));

	k = 0;
	prof = (user_profile_t*)xlist_first(profiles, &lu);

	while(prof)
    {
		if (k == cursor_profiles_view)
			break;

		prof = (user_profile_t*)xlist_next(profiles, &lu);
		k++;
    }

	nview = 0;
    
	while (prof)
    {
		if (prof->reg_status == SIPUA_STATUS_REG_OK)
		{
			snprintf(buf, 199, " %c%c %c %d.'%s'<%s> [%-s] %ds %-100.100s",
						(cursor_profiles_pos==k) ? '-' : ' ',
						(cursor_profiles_pos==k) ? '>' : ' ',
						(prof == user_profile) ? '*' : ' ',
						k+1, prof->fullname, prof->regname, prof->registrar, prof->seconds_left, " ");
		}
		else if (prof->reg_status == SIPUA_STATUS_REG_FAIL)
		{
			snprintf(buf, 199, " %c%c %c %d.'%s'<%s> [%-s] %-100.100s",
						(cursor_profiles_pos==k) ? '-' : ' ',
						(cursor_profiles_pos==k) ? '>' : ' ',
						(prof == user_profile) ? '*' : ' ',
						k+1, prof->fullname, prof->regname, prof->registrar, "FAIL");
		}
		else if (prof->reg_status == SIPUA_STATUS_REG_DOING)
		{
			snprintf(buf, 199, " %c%c %c %d.'%s'<%s> [%-s] %-100.100s",
						(cursor_profiles_pos==k) ? '-' : ' ',
						(cursor_profiles_pos==k) ? '>' : ' ',
						(prof == user_profile) ? '*' : ' ',
						k+1, prof->fullname, prof->regname, prof->registrar, "R...");
		}
		else if (prof->reg_status == SIPUA_STATUS_UNREG_DOING)
		{
			snprintf(buf, 199, " %c%c %c %d.'%s'<%s> [%-s] %-100.100s",
						(cursor_profiles_pos==k) ? '-' : ' ',
						(cursor_profiles_pos==k) ? '>' : ' ',
						(prof == user_profile) ? '*' : ' ',
						k+1, prof->fullname, prof->regname, prof->registrar, "U...");
		}
		else
		{
			snprintf(buf, 199, " %c%c %c %d.'%s'<%s> [%-s] %-100.100s",
						(cursor_profiles_pos==k) ? '-' : ' ',
						(cursor_profiles_pos==k) ? '>' : ' ',
						(prof == user_profile) ? '*' : ' ',
						k+1, prof->fullname, prof->regname, prof->registrar, "OFF");
		}
      
		attrset((k==cursor_profiles_pos) ? COLOR_PAIR(10) : COLOR_PAIR(1));
		mvaddnstr(gui->y0+1+nview, gui->x0, buf, x-gui->x0);

		if (nview > y + gui->y1 - gui->y0 - 1)
			break; /* do not print next one */

		prof = (user_profile_t*)xlist_next(profiles, &lu);

		k++;
		nview++;
    }

	window_profiles_draw_commands(gui);
  
	return 0;
}

void window_profiles_draw_commands(gui_t* gui)
{
	int x,y;


	char *profiles_commands[] = {
									"^A",   "Add",
									"^D",   "Delete",
									"^R",	"Register",
									"^U",	"Unregister",
									"^F",	"Refresh",
									"S",	"Save List",
									"CR",	"Select",
									NULL
								};

	char *profiles_nosave_commands[] = {
											"^A",   "Add",
											"^D",   "Delete",
											"^R",	"Register",
											"^U",	"Unregister",
											"^F",	"Refresh",
											"CR",	"Select",
											NULL
										};

	user_t* user = gui->topui->user;

	getmaxyx(stdscr,y,x);
  
	if(user && user->modified)
		josua_print_command(profiles_commands, y-5, 0);
	else
		josua_print_command(profiles_nosave_commands, y-5, 0);
}

int window_profiles_run_command(gui_t* gui, int c)
{
	int k,y,x,h;
	int max=0;

	ogmp_curses_t* ocui = gui->topui;
	user_profile_t* user_profile = ocui->sipua->profile(ocui->sipua);

	xlist_t* profiles = ocui->user->profiles;

	/*curseson(); cbreak(); noecho(); nonl(); keypad(stdscr,TRUE);*/
	getmaxyx(stdscr,y,x);
    
    h = y - gui->y0 + gui->y1;

	if (gui->x1 != -999)
		x = gui->x1;

 	if(profiles)
		max = xlist_size(profiles);
 
	switch (c)
    {
		case KEY_DOWN:
		{
			if (cursor_profiles_pos == max-1)
				beep();
			else
			{
				cursor_profiles_pos++;
			
				if ((cursor_profiles_pos - cursor_profiles_view) > h)
					cursor_profiles_view++;
			}

			break;
		}
		case KEY_UP:
		{
			if (cursor_profiles_pos == 0)
				beep();
			else
			{
				cursor_profiles_pos--;
			
				if (cursor_profiles_view > cursor_profiles_pos)
					cursor_profiles_view--;
			}

			break;
		}
		case 1:  /* Ctrl-A */
		{
			ocui->edit_profile = NULL;
			ocui->clear_profile = 1;

			gui_show_window(gui, GUI_NEWID, GUI_PROFILES);

			break;
		}
		case 4:  /* Ctrl-D */
		{
			user_profile_t* prof;
			xlist_user_t lu;

			k = 0;
			prof = (user_profile_t*)xlist_first(profiles, &lu);
			while(prof)
			{
				if(k == cursor_profiles_pos)
					break;

				k++;
				prof = (user_profile_t*)xlist_next(profiles, &lu);
			}

			if(prof && user_profile != prof)
				user_remove_profile(ocui->user, prof);

			break;
		}
		case 5:  /* Ctrl-E */
		{
			/* edit */
			user_profile_t* prof;
			xlist_user_t lu;

			k = 0;

			prof = (user_profile_t*)xlist_first(profiles, &lu);
			while(prof)
			{
				if(k == cursor_profiles_pos)
					break;

				k++;
				prof = (user_profile_t*)xlist_next(profiles, &lu);
			}

			ocui->edit_profile = prof;
			if(prof)
				gui_show_window(gui, GUI_NEWID, GUI_PROFILES);

			break;
		}
		case 6:   /* Ctrl-F */
		{
			/* Refresh the list */

			break;
		}
		case 18:  /* Ctrl-R */
		{
			/* Register the name */
			user_profile_t* prof;
			xlist_user_t lu;

			k = 0;

			prof = (user_profile_t*)xlist_first(profiles, &lu);
			while(prof)
			{
				if(k == cursor_profiles_pos)
					break;

				k++;
				prof = (user_profile_t*)xlist_next(profiles, &lu);
			}

			if(prof)
			{
				if(prof->reg_status == SIPUA_STATUS_REG_OK)
					break;
				
				ocui->sipua->regist(ocui->sipua, prof, ocui->user->userloc);
			}

			break;
		}
		case 's':  /* Save */
		case 'S':  
		{
			/* Save profiles list to storage */
			user_t* user = ocui->user;

			if(!user->modified)
				break;

			if(user->tok)
			{
				/* tok prompt gui */
				sipua_save_user(user, user->loc, user->tok, user->tok_bytes);
			}

			break;
		}
		case 21:  /* Ctrl-U */
		{
			/* Unregister the name */
			user_profile_t* prof;
			xlist_user_t lu;

			if(!user_profile)
				break;

			k = 0;
			prof = (user_profile_t*)xlist_first(profiles, &lu);
			while(prof)
			{
				if(k == cursor_profiles_pos)
					break;

				k++;
				prof = (user_profile_t*)xlist_next(profiles, &lu);
			}

			if(prof && user_profile != prof)
			{
				if(prof->reg_status != SIPUA_STATUS_REG_OK)
					break;
				
				ocui->sipua->unregist(ocui->sipua, prof);
			}
			else
				beep();

			break;
		}
		case '\n':
		case '\r':
		case KEY_ENTER:
		{
			user_profile_t* prof;
			xlist_user_t lu;

			k = 0;

			prof = (user_profile_t*)xlist_first(profiles, &lu);
			while(prof)
			{
				if(k == cursor_profiles_pos)
					break;

				k++;
				prof = (user_profile_t*)xlist_next(profiles, &lu);
			}

			if(prof)
			{
				if(prof->reg_status == SIPUA_STATUS_REG_OK)
					ocui->sipua->set_profile(ocui->sipua, prof);
				else
					ocui->sipua->regist(ocui->sipua, prof, ocui->user->userloc);
			}

			break;
		}
		default:
		{
         printf("window_profiles_run_command: unknown command\n");
         
			beep();

			return -1;
		}
    }

    gui_update(gui);

	return 0;
}

int window_profiles_event(gui_t* gui, gui_event_t* ge)
{
    /* Nothing interesting yet */
    return GUI_EVENT_CONTINUE;
}

gui_t* window_profiles_new(ogmp_curses_t* topui)
{
	gui_window_profiles.topui = topui;

	return &gui_window_profiles;
}

int window_profiles_done(gui_t* gui)
{
	return 0;
}

gui_t gui_window_profiles =
{
	GUI_OFF,
	0,
	-999,
	10,
	-6,
    window_profiles_event,
	NULL,
	window_profiles_print,
	window_profiles_run_command,
	NULL,
	window_profiles_draw_commands,
	-1,
	-1,
	-1,
	NULL
};
