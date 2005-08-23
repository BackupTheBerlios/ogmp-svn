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

int ogmp_done_ui(void* gen)
{
    ui_t* ui = (ui_t*)gen;

    global_set_ui(NULL);
	//ui_share.print_log = NULL;

    ui->done(ui);

    return UA_OK;
}

ui_t*
client_new_ui(ogmp_client_t *client, module_catalog_t* mod_cata, char* type)
{
    ui_t* ui = NULL;
    
    xlist_user_t lu;
    xlist_t* uis  = xlist_new();
    int found = 0;
    
    int nmod = catalog_create_modules (mod_cata, "ui", uis);
    if(nmod)
    {
        ui = (ui_t*)xlist_first(uis, &lu);
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

    global_set_ui(ui);
    
    return ui;
}
