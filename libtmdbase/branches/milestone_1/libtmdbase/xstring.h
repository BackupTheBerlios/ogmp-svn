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

 #include "os.h"
 
 #include <string.h>
 
extern DECLSPEC  char * xstr_clone(char *str);

/* against buffer overflow */
extern DECLSPEC  char * xstr_nclone(char *str, int len);

extern DECLSPEC  int xstr_ncomp(char *src, char *des, int len);

extern DECLSPEC  char * xstr_new_string(char str[]);

extern DECLSPEC  int xstr_done_string(char * str);
