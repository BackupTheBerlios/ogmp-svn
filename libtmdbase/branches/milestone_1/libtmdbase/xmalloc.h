/***************************************************************************
                          xmalloc.h  -  memory monitor
                             -------------------
    begin                : Wed Dec 3 2003
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

#ifndef XMALLOC_H
#define XMALLOC_H

#include <stdlib.h>

#ifdef MEM_DEBUG

 /* debug memory */
 #ifndef MEMWATCH
  #define MEMWATCH
 #endif

 #ifndef MW_STDIO
  #define MW_STDIO
 #endif

 #include "memwatch/memwatch.h"

 #define xmalloc malloc
 #define xfree free

 #pragma message ("xmalloc.h: debug module memory")

#elif defined(MONITE_MEM) || defined(MEMWATCH)

 /* check memory usage */
 #include "os.h"

 extern DECLSPEC
 void* 
 xmalloc(size_t bytes);

 extern DECLSPEC
 void
 xfree(void *p);

 #pragma message ("xmalloc.h: monite all memory")

#else

 /* turn off monitor */
 #define xmalloc malloc
 #define xfree free

 #pragma message ("xmalloc.h: turn off memory monitor")

#endif
#endif

