/*
 * Copyright (C) 2002-2003 the Timedia project
 *
 * This file is part of libxrtp, an extendable rtp library.
 *
 * libxrtp is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libxrtp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: catalog.c,v 0.1 11/12/2002 15:47:39 heming$
 *
 */
 
#include "xstring.h"
#include "catalog.h"
#include "loader.h"

#ifdef WIN32
 #include "win32/dirent.h"
#else
 #include <dirent.h>
#endif
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define CATALOG_LOG
#define CATALOG_DEBUG
 
#ifdef CATALOG_LOG 
   const int catalog_log = 1;
#else
   const int catalog_log = 0;
#endif
#define catalog_log(fmtargs)  do{if(catalog_log) printf fmtargs;}while(0)

#ifdef CATALOG_DEBUG
   const int catalog_debug = 1;
#else
   const int catalog_debug = 0;
#endif
#define catalog_debug(fmtargs)  do{if(catalog_debug) printf fmtargs;}while(0)

module_catalog_t* catalog_new( char * type ){
   
   module_catalog_t *catalog;

   catalog = (module_catalog_t *)malloc(sizeof(module_catalog_t));
   if (!catalog) {
     
      catalog_debug(("catalog_new: NO more memory\n"));
      return NULL;
   }
   memset(catalog, 0, sizeof(module_catalog_t));

   catalog->infos = xrtp_list_new();

   strncpy (catalog->module_type, type, TYPENAME_LEN-1);
   catalog->module_type[TYPENAME_LEN-1] = '\0';
   
   return catalog;
}

int catalog_freeitem(void* generic){
   
   module_info_t * minfo = (module_info_t *)generic;
   
   modu_dlclose(minfo->lib);
   
   free(minfo);
    
   return OS_OK;
}

int catalog_reset (module_catalog_t *cata){

   if(xrtp_list_size(cata->infos) == 0){

      return OS_OK;
   }

   xrtp_list_reset(cata->infos, catalog_freeitem);
    
   return OS_OK;
}

int catalog_done (module_catalog_t *cata){

   catalog_reset(cata);

   xrtp_list_free(cata->infos, catalog_freeitem);

   return OS_OK;
}

int catalog_free(module_catalog_t *cata){

   catalog_done(cata);

   free(cata);

   return OS_OK;
}

int catalog_scan_modules (module_catalog_t* catalog, unsigned int ver, char* path){

   xrtp_list_user_t *list_u;
   int num_plugin;

   DIR* dir;
   struct dirent *entry;

   module_info_t *minfo;
   module_loadin_t *loadin;

   struct stat entinfo;
   void* lib;

   if(!catalog || !path){
      return OS_EPARAM;
   }

   dir = opendir(path); /* POSIX call */
   if(!dir){

      catalog_log(("< catalog_scan_modules: Fail to open dir:%s >\n", path));
      return OS_EPARAM;
   }      

    
   list_u = xrtp_list_newuser(catalog->infos);

   while((entry = readdir(dir)) != NULL){

      char * entname = (char *)malloc(strlen(path) + strlen(entry->d_name) + 2);
      sprintf(entname, "%s/%s", path, entry->d_name);

      catalog_log(("catalog_scan_modules: Openning %s \n", entname));

      if(stat(entname, &entinfo) == -1){

         catalog_log(("< catalog_scan_modules: Fail to get (%s) info >\n", entname));
         free(entname);
         continue;
      }

      switch(entinfo.st_mode & S_IFMT){

         case S_IFDIR: continue;

         case S_IFREG: {

            lib = modu_dlopen(entname, XRTP_DLFLAGS);
            if(lib != NULL){

               catalog_log(("catalog_scan_modules: Check module (%s)\n", entname));

               loadin = (module_loadin_t *)modu_dlsym(lib, catalog->module_type);
               if(loadin){
              
                  if(ver > loadin->max_api || ver < loadin->min_api){
                
                     catalog_log(("< catalog_scan_modules: Module (%s) version not match >\n", entname));
                     modu_dlclose(lib);
                     continue;
                  }
               
                  minfo = (module_info_t *)malloc(sizeof(module_info_t));
                  minfo->filename = xstr_clone(entname);
                  minfo->lib = lib;
                  minfo->loadin = loadin;
                  minfo->iface = NULL;

                  catalog_log(("catalog_scan_modules: Found module (%s)\n", loadin->label));

                  xrtp_list_add_first(catalog->infos, minfo);
               }
            
            }else{
            
               catalog_log(("< catalog_scan_modules: Loading error (%s) >\n", modu_dlerror()));
            }
         }
      }

      free(entname);
   }
   
   num_plugin = xrtp_list_size(catalog->infos);
   
   xrtp_list_freeuser(list_u);
   
   return num_plugin;
}

int _catalog_match(void * target, void * pattern){
   
   module_info_t * minfo = (module_info_t *)target;

   catalog_log(("_catalog_match: Try matching pattern [%s] to target(%s)\n", (char*)pattern, minfo->loadin->label));

   return strcmp(minfo->loadin->label, (char*)pattern);
}

/*
 * Load a new handler module from the handler catalog
 *
 * Fail, return null.
 */
module_interface_t * catalog_new_module (module_catalog_t *cata, char *label){
   
   module_info_t * minfo;
   module_interface_t * mod;
   void * lib;

   if(xrtp_list_size(cata->infos) == 0)
      return NULL;

   catalog_log(("catalog_new_module: Try to find module(%s) in catalog\n", label));

   minfo = (module_info_t *)xrtp_list_find(cata->infos, label, _catalog_match, &(cata->$lu));

   if(!minfo){

      catalog_log(("< catalog_new_module: NO module %s found in catalog >\n", label));

   }else{
     
     catalog_log(("catalog_new_module: Found module %s\n", label));
     
     if(!minfo->lib || !minfo->loadin){
       
        lib = modu_dlopen(minfo->filename, XRTP_DLFLAGS);
        if(!lib){
          
           catalog_log(("< catalog_new_module: Catalog:Can't load module file (%s) >\n", minfo->filename));
           return NULL;
                      
         }
         minfo->lib = lib;
         minfo->loadin = modu_dlsym(lib, cata->module_type);
      }
      
      mod = minfo->loadin->init();
      if(!mod){
        
         catalog_log(("< catalog_new_module: Catalog:Can't init module(%s) >\n", label));
         modu_dlclose(minfo->lib);
         minfo->lib = NULL;
         minfo->loadin = NULL;
         return NULL;
      }
      
      minfo->iface = mod;
      
      return mod;
     
   }

   return NULL;
}

/*
 * Create all modules from source, return numbers of module created
 */
int catalog_create_modules(module_catalog_t *cata, char *label, xrtp_list_t *list) {

   int nmod = 0;

   module_info_t * minfo;
   
   xrtp_list_user_t *u = xrtp_list_newuser(cata->infos);

   minfo = (module_info_t *)xrtp_list_first(cata->infos, u);

   while (minfo) {

      if (!strcmp(label, minfo->loadin->label)) {

         if (minfo->iface == NULL)
            minfo->iface = minfo->loadin->init();

         if (minfo->iface) {
           
            xrtp_list_add_first(list, minfo->iface);
         }
      }

      minfo = (module_info_t *)xrtp_list_next(cata->infos, u);
   }

   xrtp_list_freeuser(u);
   
   nmod = xrtp_list_size(list);

   catalog_log(("catalog_create_modules: %d '%s' modules found\n", nmod, label));

   return nmod;
}

