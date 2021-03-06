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

#ifndef OS_UI_H
#define OS_UI_H

#include "os.h"

typedef struct ui_s ui_t;
struct ui_s
{
	int (*done)(ui_t *ui);
	int (*match_type)(ui_t *ui, char* type);
    
	int (*show)(ui_t *ui);
    int (*beep)(ui_t *ui);
    
    int (*logbuf)(ui_t *ui, char** buf);
    int (*print_log)(ui_t *ui, char* buf);
};

extern DECLSPEC int ui_print_log(char *fmt, ...);

extern DECLSPEC ui_t* global_get_ui();
extern DECLSPEC int global_set_ui(ui_t* ui);

#endif
