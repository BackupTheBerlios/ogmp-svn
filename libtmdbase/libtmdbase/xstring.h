/***************************************************************************
                          string.h  -  description
                             -------------------
    begin                : Mon Mar 10 2003
    copyright            : (C) 2003 by Heming Ling
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

 #ifndef OS_H
 #include "os.h"
 #endif
 
 #include <string.h>
 
 char * xstr_clone(char *str);

 int xstr_ncomp(char *src, char *des, uint len);

 char * xstr_new_string(char str[]);

 int xstr_done_string(char * str);
