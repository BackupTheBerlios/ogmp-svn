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
	int (*gui_print)(gui_t* gui);
	int (*gui_run_command)(gui_t* gui, int c);
	int (*gui_key_pressed)(gui_t* gui);
	void (*gui_draw_commands)(gui_t* gui);

	int xcursor;
	int ycursor;
	int len;

	WINDOW *win;

	ogmp_curses_t* topui;
};

#define TOPGUI      0
#define ICONGUI     1
#define LOGLINESGUI 2
#define MENUGUI     3
#define EXTRAGUI    4

extern gui_t *gui_windows[10];
extern gui_t *active_gui;

extern sipua_t *sipua;
extern xlist_t *profiles;
extern user_profile_t *sipuser;

typedef struct coding_s
{
	int enabled;
	rtp_coding_t coding;

} coding_t;

#define MAX_CODING 10

struct ogmp_curses_s
{
	struct ogmp_ui_s ui;

	gui_t* gui_windows[10];
	gui_t* active_gui;

	int quit;

	sipua_t *sipua;
	xlist_t *profiles;
	user_profile_t *sipuser;

	int ncoding;
	int codex[MAX_CODING];

	coding_t codings[MAX_CODING];

	xthr_lock_t *log_mutex;
};

int gui_show(ogmp_ui_t* ogui);
/*
int   josua_event_get();
void  josua_printf(char *chfr, ...);
*/

/* usefull method 
int   gui_clear();
int   gui_print();
int gui_run_command(ogmp_curses_t* ocui, int c);
*/
int   josua_clear_box_and_commands(gui_t *box);
int   josua_print_command(char **commands, int ypos, int xpos);

WINDOW *gui_print_box(gui_t *box, int draw, int color);

void curseson();
void cursesoff();

#endif
