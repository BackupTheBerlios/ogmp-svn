/***************************************************************************
                          xstring.c  -  description
                             -------------------
    begin                : Sat Mar 29 2003
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

 #include "xstring.h"
 
 #include "xmalloc.h"

 #ifdef XSTRING_LOG
	#define xstr_log(fmtargs)  do{printf fmtargs;}while(0)
 #else
	#define xstr_log(fmtargs)
 #endif

 char * xstr_clone(char *str)
 {
     char * new_str = (char *)xmalloc(sizeof(char) * strlen(str));

     strcpy(new_str, str);

     return new_str;
 }

 char * xstr_nclone(char *str, int len)
 {
     char *new_str = (char *)xmalloc(sizeof(char) * (len+1));
     if(new_str)
     {
         strncpy(new_str, str, len);
         new_str[len] = '\0';
     }
     return new_str;
 }

 int xstr_ncomp(char *src, char *des, int len)
 {
    return memcmp(src, des, len);
 }

 char * xstr_new_string(char str[])
 {
    char * s = (char *)xmalloc(strlen(str) + 1);
    if(!s)
       return NULL;
    
    strcpy(s, str);

    return s;    
 }

 int xstr_done_string(char * str)
 {
    if(str)
		xfree(str);
    
    return OS_OK;
 }
