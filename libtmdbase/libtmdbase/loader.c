/***************************************************************************
                          loader.c  -  description
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
 #include "loader.h"
 
 void * modu_dlopen(char *fn, int flag){
    return dlopen(fn, flag);
 }     
 
 int modu_dlclose(void * lib){
    return dlclose(lib);
 }

 void * modu_dlsym(void *h, char *name){
    return dlsym(h, name);
 }   

 const char * modu_dlerror(void){
    return dlerror();
 }
