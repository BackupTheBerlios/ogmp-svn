/*
 * Copyright (C) 2000-2002 the xine project
 *
 * This file is part of rtpsession, a modulized rtp library.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: handler_catalog.h,v 0.1 11/12/2002 15:47:39 heming$
 *
 */

#ifndef CATALOG_H

#define CATALOG_H

#include "list.h"

#define MAX_HANDLER 128
#define TYPENAME_LEN 31

typedef void module_interface_t;

typedef struct{

   char          *label;
    
   unsigned int  version; /* The version of the module */

   /* The API version be supported */
   unsigned int  min_api;
   unsigned int  max_api;

   module_interface_t* (*init)(); /* init(user_data) */

} module_loadin_t;

typedef struct{
    
   char  *filename;
   void  *mtime;
   void  *filesize;
   
   void  *lib;

   module_loadin_t  * loadin;
   
   module_interface_t	* iface;
  
} module_info_t;

typedef struct module_catalog_s {
   
   /* Only this type name module will be loaded */
   char  module_type[TYPENAME_LEN];
   
   xrtp_list_t  *infos;
   struct xrtp_list_user_s $lu;
    
} module_catalog_t;

/*
 * Create a new catalog structure
 */
extern DECLSPEC module_catalog_t * catalog_new( char * type );

/*
 * Finalize the catalog.
 */
extern DECLSPEC int catalog_done(module_catalog_t *catalog);

/*
 * Scan directory to collect information of all the plugins
 * Return:
 *   The number of available plugins, or
 *   Minus value for error.
 */
extern DECLSPEC int catalog_scan_modules(module_catalog_t *catalog, unsigned int ver, char *dir);

/*
 * Dispose all the handler info in the catalog, reset the catalog.
 */
extern DECLSPEC int catalog_reset(module_catalog_t *catalog);

/*
 * Create a new handler instance from the handler catalog
 *
 * Fail, return null.
 */
extern DECLSPEC module_interface_t * catalog_new_module(module_catalog_t *catalog, char* id);

/*
 * Create all modules from source, return numbers of module created
 */
extern DECLSPEC int catalog_create_modules(module_catalog_t *catalog, char *label, xrtp_list_t *list);

#endif
