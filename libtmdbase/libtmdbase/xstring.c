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
 
 #include <stdlib.h>

 #ifdef XSTRING_LOG
   const int xstring_log = 1;
 #else
   const int xstring_log = 0;
 #endif
 #define xstr_log(fmtargs)  do{if(xstring_log) printf fmtargs;}while(0)

 char * xstr_clone(char *str){
   
     char * new_str = (char *)malloc(sizeof(char) * strlen(str));

     strcpy(new_str, str);

     return new_str;
 }

 int xstr_ncomp(char *src, char *des, uint len){

    return memcmp(src, des, len);
 }

 char * xstr_new_string(char str[]){

    char * s = (char *)malloc(strlen(str) + 1);
    if(!s)
       return NULL;
    
    strcpy(s, str);

    return s;    
 }

 int xstr_done_string(char * str){

    free(str);
    
    return OS_OK;
 }
