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
 
#include <timedia/os.h>
#include <timedia/ui.h>
#include <timedia/xmalloc.h>

#include "ogmp.h"

#define LOG_LEN 256
char ogui_log_buf[LOG_LEN];

int ogui_done(ui_t* ui)
{
   xfree(ui);

	return UA_OK;
}

int ogui_match_type(ui_t* ui, char *type)
{
	if(strcmp(type, "dummyui")==0)
	{
		printf("ogui_match_type: Dummy ui\n");

		return 1;
	}

	return 0;
}

int ogui_show(ui_t* ui)
{
	return UA_OK;
}

int ogui_beep(ui_t* ui)
{
    return UA_OK;
}

int ogui_logbuf(ui_t* ui, char **buf)
{
   *buf = ogui_log_buf;

   return LOG_LEN;
}

/* return actual line number output */
int ogui_print_log(ui_t* ui, char *buf)
{
	printf("%s\n", buf);

	return 0;
}

int ogui_set_sipua(ogmp_ui_t* ui, sipua_t* sipua)
{
	return UA_OK;
}

module_interface_t* ogmp_new_ui()
{
	ogmp_ui_t* ogui = xmalloc(sizeof(ogmp_ui_t));

	if(!ogui)
	{
		printf("ogmp_new_ui: No memory\n");
		return NULL;
	}
    
	memset(ogui, 0, sizeof(ogmp_ui_t));
    
	ogui->ui.done = ogui_done;
   ogui->ui.match_type = ogui_match_type;

	ogui->ui.show = ogui_show;
   ogui->ui.beep = ogui_beep;

   ogui->ui.logbuf = ogui_logbuf;

   ogui->ui.print_log = ogui_print_log;
    
	ogui->set_sipua = ogui_set_sipua;

	return ogui;
}

/**
 * Loadin Infomation Block
 */
extern DECLSPEC module_loadin_t mediaformat =
{
   "ui",   /* Label */

   000001,         /* Plugin version */
   000001,         /* Minimum version of lib API supportted by the module */
   000001,         /* Maximum version of lib API supportted by the module */

   ogmp_new_ui   /* Module initializer */
};
