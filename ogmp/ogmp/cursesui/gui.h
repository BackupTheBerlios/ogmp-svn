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

#ifndef _GUI_H_
#define _GUI_H_

#ifdef WIN32
#define snprintf _snprintf
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <limits.h>
#include <ctype.h>
#include <assert.h>

#include "../ogmp.h"
#include "../log.h"

#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif
 
#ifdef NCURSES_SUPPORT
#include <term.h>
#include <ncurses.h>
#else
#include <curses.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <osip2/osip_mt.h>

#include <eXosip/eXosip.h>
#include <eXosip/eXosip_cfg.h>

typedef struct ogmp_curses_s ogmp_curses_t;
typedef struct gui gui_t;
struct gui
{
#define GUI_DISABLED -1
#define GUI_ON        0
#define GUI_OFF       1
  
	int on_off;
	int x0;
	int x1;
	int y0;
	int y1;

	int (*gui_clear)(gui_t* gui);
	int (*gui_print)(gui_t* gui, int wid);
	int (*gui_run_command)(gui_t* gui, int c);
	int (*gui_key_pressed)(gui_t* gui);
	void (*gui_draw_commands)(gui_t* gui);

	int xcursor;
	int ycursor;
	int len;

	WINDOW *win;

	ogmp_curses_t* topui;
	
	int parent;
};

#define GUI_QUIT		-1

#define TOPGUI			0
#define ICONGUI			1
#define LOGLINESGUI		2
#define MENUGUI			3
#define EXTRAGUI		4

#define GUI_BROWSE		5
#define GUI_ENTRY		6
#define GUI_INSUBS		7
#define GUI_LOGIN		8
#define GUI_NEWCALL		9
#define GUI_NEWID		10
#define GUI_ONLINE		11
#define GUI_SESSION		12
#define GUI_SETUP		13
#define GUI_SUBS		14
#define GUI_LOGLINES	15
#define GUI_NEWUSER		16
#define GUI_PROFILES	17
#define GUI_MESSAGE		18
#define GUI_AUDIOTEST	19

#define MAXGUI			30

typedef struct coding_s
{
	int enabled;
	rtp_coding_t coding;

} coding_t;

#define MAX_CODING 10

struct ogmp_curses_s
{
	struct ogmp_ui_s ui;

	int nwin;
	gui_t* gui_windows[MAXGUI];
	gui_t* active_gui;

	char* message;

	int quit;

	sipua_t *sipua;
	sipua_uas_t* uas;
	sipua_net_t* network;

	media_source_t* source;

	user_t *user;

	sipua_phonebook_t *phonebook;
	sipua_contact_t *contact;

	int ncoding;
	int codex[MAX_CODING];

	coding_t codings[MAX_CODING];
};

char log_buf3[200];
char log_buf2[200];
char log_buf1[200];

xthr_lock_t *log_lock;

/* usefull method */
int gui_show_window(gui_t* gui, int wid, int wid_parent);
int gui_hide_window(gui_t* gui);
int gui_activate_window(gui_t* gui, int wid);

int gui_next_view(ogmp_curses_t* ocui);
int gui_previous_view(ogmp_curses_t* ocui);

int  josua_clear_box_and_commands(gui_t *box);
int  josua_print_command(char **commands, int ypos, int xpos);

WINDOW *gui_print_box(gui_t *box, int draw, int color);

void curseson();
void cursesoff();

#endif
