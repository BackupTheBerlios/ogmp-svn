/***************************************************************************
                          loader.h  -  description
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

#ifdef WIN32
 #include "win32/dlfcn.h"
#else
 #include <dlfcn.h>
#endif
 
#define XRTP_DLFLAGS RTLD_LAZY
 
extern DECLSPEC void* modu_dlopen(char *fn, int flag);
extern DECLSPEC int modu_dlclose(void * lib);
extern DECLSPEC void* modu_dlsym(void *h, char *name);
extern DECLSPEC const char * modu_dlerror(void);
