#ifndef __DLFCN_H__
# define __DLFCN_H__

#include "../os.h"

/*
 * $Id: dlfcn.h,v 1.2 2001/09/05 19:48:03 cwolf Exp $
 * $Name:  $
 * 
 *
 */
extern DECLSPEC void *dlopen  (const char *file, int mode);
extern DECLSPEC int   dlclose (void *handle);
extern DECLSPEC void *dlsym   (void * handle, const char * name);
extern DECLSPEC char *dlerror (void);

/* These don't mean anything on windows */
#define RTLD_NEXT      ((void *) -1l)
#define RTLD_DEFAULT   ((void *) 0)
#define RTLD_LAZY					-1
#define RTLD_NOW					-1
#define RTLD_BINDING_MASK -1
#define RTLD_NOLOAD				-1
#define RTLD_GLOBAL				-1

#endif /* __DLFCN_H__ */
