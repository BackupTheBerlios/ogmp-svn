/***************************************************************************
                          xmalloc.h  -  memory monitor
                             -------------------
    begin                : Tue Aug 24 2004
    copyright            : (C) 2004 by Heming Ling
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

#include "xmalloc.h"

#ifdef MONITE_MEM
/*
#define XMALLOC_LOG
*/
#define XMALLOC_DEBUG

#ifdef XMALLOC_LOG
   #include <stdio.h>
   #define xmalloc_log(fmtargs)  do{printf fmtargs;}while(0)
#else
   #define xmalloc_log(fmtargs)
#endif

#ifdef XMALLOC_DEBUG
   #include <stdio.h>
   #define xmalloc_warning(fmtargs)  do{printf fmtargs;}while(0)
#else
   #define xmalloc_warning(fmtargs)
#endif

#include <timedia/xthread.h>
#include <string.h>

#define MAX_BLOCKS 4096

#define FIRST_BLOCK  monitor.blocks[0].prev
#define LAST_BLOCK  monitor.blocks[0].next

struct memory_monitor_s
{
   struct mblock
   {
      char* start;
      char* end;

      int prev;
      int next;

   }blocks[MAX_BLOCKS];

   int nblock;
   int allbytes;

   xthr_lock_t *lock;

} monitor;

static int monitor_inited = 0;

void xmalloc_init()
{
   memset(&monitor, 0, sizeof(struct memory_monitor_s));
   monitor.lock = xthr_new_lock();
   monitor_inited = 1;
}

void xmalloc_done()
{
   if(monitor_inited == 0)
   {
      xthr_done_lock(monitor.lock);
      monitor_inited = 0;
   }
}

void*
xmalloc(size_t bytes)
{
   int b, m, n, succ = 0;
   int i;
   char *p;
   
   if(!monitor_inited)
      xmalloc_init();
      
   p = malloc(bytes);
   if(!p)
   {
      xmalloc_warning(("xmalloc: No memory at all\n"));
      return NULL;
   }

   xthr_lock(monitor.lock);

   if(monitor.nblock == 0)
   {
      /* the first malloc */
      b = 1;
      monitor.blocks[b].prev = 0;
      monitor.blocks[b].next = 0;
      
      FIRST_BLOCK = b;  /* first index */
      LAST_BLOCK = b;  /* last index */

      goto ok;
   }

   for(i=1; i<MAX_BLOCKS; i++)
      if(monitor.blocks[i].start == NULL)
      {
         b = i;
         break;
      }
         
   if(i == MAX_BLOCKS)
   {
      xmalloc_warning(("xmalloc: too many block allocated, cancelled\n"));
      free(p);
      p = NULL;
      goto fail;
   }
      
   i = FIRST_BLOCK;
   
   //xmalloc_log(("xmalloc: "));
   
   while(i!=0)
   {
      if(p > monitor.blocks[i].start)
      {
         /* continue search */
         //xmalloc_log(("#%d,", i));
      }
      else if(p == monitor.blocks[i].start)
      {
         /* conflict, fail */
         xmalloc_warning(("\nxmalloc: p@%d:%dB conflict with next block#%d@[%d-%d)\n", (uint)p, bytes, i, (uint)monitor.blocks[i].start, (uint)monitor.blocks[i].end));
         p = NULL;
         goto fail;
      }
      else
      {
         /* add before */
         int prev = monitor.blocks[i].prev;
         
         if(p < monitor.blocks[prev].end)
         {
            xmalloc_warning(("\nxmalloc: p@%d:%dB conflict with prev block#%d@[%d-%d)\n", (uint)p, bytes, prev, (uint)monitor.blocks[prev].start, (uint)monitor.blocks[prev].end));
            p = NULL;
            goto fail;
         }
         
         if(p+bytes >= monitor.blocks[i].start)
         {
            xmalloc_warning(("\nxmalloc: p@%d:%dB conflict with next block#%d@[%d-%d)\n", (uint)p, bytes, i, (uint)monitor.blocks[i].start, (uint)monitor.blocks[i].end));
            p = NULL;
            goto fail;
         }
         
         //xmalloc_log(("(#%d),#%d\n", b, i));

         monitor.blocks[b].next = i;
         monitor.blocks[b].prev = prev;
         
         monitor.blocks[prev].next = b;
         monitor.blocks[i].prev = b;
         
         if(prev == 0)
            FIRST_BLOCK = b;

         break;
      }

      if(monitor.blocks[i].next == 0)
      {
         /* last one, add anyway after that */
         //xmalloc_log(("(#%d)\n", b));
         monitor.blocks[b].next = 0;
         monitor.blocks[b].prev = i;
         
         monitor.blocks[i].next = b;

         LAST_BLOCK = b;

         break;
      }

      i = monitor.blocks[i].next;
   }

ok:
   monitor.blocks[b].start = p;
   monitor.blocks[b].end = p + bytes;
   
   monitor.allbytes += bytes;
   n = ++monitor.nblock;
   succ = 1;
   
   m = monitor.allbytes;
   
fail:
   xthr_unlock(monitor.lock);

   if(succ)
      xmalloc_log(("xmalloc: block#%d@[%d-%d:%dB] approved, %db:%dB now\n", b, (uint)p, (uint)p+bytes, bytes, n, m));

   return p;
}

void
xfree(void *p)
{
   int m, n, i, freed = 0;
   uint start, end;

   if(!monitor_inited)
      xmalloc_init();

   xthr_lock(monitor.lock);
   
   i = FIRST_BLOCK;
   while(i != 0)
   {
      if((char*)p >= monitor.blocks[i].start && (char*)p < monitor.blocks[i].end)
      {
         /* found block to release */
         start = (uint)monitor.blocks[i].start;
         end = (uint)monitor.blocks[i].end;

         if(monitor.blocks[i].prev != 0)
         {
            int prev = monitor.blocks[i].prev;
            monitor.blocks[prev].next = monitor.blocks[i].next;
            
            if(monitor.blocks[prev].next == 0)
               LAST_BLOCK = prev;
         }

         if(monitor.blocks[i].next != 0)
         {
            int next = monitor.blocks[i].next;
            monitor.blocks[next].prev = monitor.blocks[i].prev;
            
            if(monitor.blocks[next].prev == 0)
               FIRST_BLOCK = next;
         }

         free(monitor.blocks[i].start);
         
         monitor.allbytes -= monitor.blocks[i].end - monitor.blocks[i].start;

         monitor.blocks[i].start = monitor.blocks[i].end = NULL;

         n = --monitor.nblock;
         
         m = monitor.allbytes;
         
         freed = 1;
         
         break;
      }

      i = monitor.blocks[i].next;
   }

   xthr_unlock(monitor.lock);
   
   if(!freed)
      xmalloc_warning(("xfree warning: p@%d can not free!!!\n", (uint)p));
   else
      xmalloc_log(("xfree: p@%d of block#%d@[%d-%d:%dB], %db:%dB now\n", (uint)p, i, start, end, end-start, n, m));
}

#endif
