/***************************************************************************
                          ogmp_log.h  -  log output
                             -------------------
    begin                : Wed May 19 2004
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

#include "ui.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

ui_t *global_ui = NULL;

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

ui_t* global_get_ui()
{
	return global_ui;
}

int global_set_ui(ui_t* ui)
{
	global_ui = ui;

	return OS_OK;
}
