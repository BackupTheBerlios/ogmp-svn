/***************************************************************************
                          ogmp_ui.c
                             -------------------
    begin                : Tue Jul 20 2004
    copyright            : (C) 2004 by Heming
    email                : heming@bigfoot.com; heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <timedia/ui.h>
#include "ogmp.h"

#include <stdarg.h>

extern ogmp_ui_t* global_ui = NULL;

int ui_print_log(char *fmt, ...)
{
	int ret;
    int loglen;
    char* logbuf;
    
    va_list ap;
    
	va_start (ap, fmt);

    if(global_ui == NULL)
    {
        ret = vprintf(fmt, ap);
        
        va_end(ap);

        return ret;
    }

    loglen = global_ui->logbuf(global_ui, &logbuf);

    vsnprintf(logbuf, loglen, fmt, ap);

	va_end(ap);

    ret = global_ui->print_log(global_ui, logbuf);

	return ret;
}

int ogmp_done_ui(void* gen)
{
    ogmp_ui_t* ui = (ogmp_ui_t*)gen;

    ui->done(ui);
    global_ui = NULL;

    return UA_OK;
}

ogmp_ui_t* client_new_ui(module_catalog_t* mod_cata, char* type)
{
    ogmp_ui_t* ui = NULL;
    
    xlist_user_t lu;
    xlist_t* uis  = xlist_new();
    int found = 0;
    
    int nmod = catalog_create_modules (mod_cata, "ui", uis);
    if(nmod)
    {
        ui = (ogmp_ui_t*)xlist_first(uis, &lu);
        while(ui)
        {
            if(ui->match_type(ui, type))
            {
                xlist_remove_item(uis, ui);
                found = 1;
                break;
            }

            ui = xlist_next(uis, &lu);
        }
    }

    xlist_done(uis, ogmp_done_ui);

    if(!found)
        return NULL;

    global_ui = ui;
    
    return ui;
}
