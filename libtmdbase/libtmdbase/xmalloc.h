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

#include "os.h"
#include <stdlib.h>

#define MONITE_MEM

#ifndef MONITE_MEM

#define xmalloc malloc
#define xfree  free

#else

extern DECLSPEC
void*
xmalloc(size_t bytes);

extern DECLSPEC
void
xfree(void *p);

#endif
#endif

