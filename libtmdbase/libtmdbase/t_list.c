/***************************************************************************
                          t_list.c  -  description
                             -------------------
    begin                : Tue Apr 1 2003
    copyright            : (C) 2003 by Heming Ling
    email                : heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

 #include "list.h"
 #include <stdio.h>

 int nums[7] = {3,4,2,7,5,8,9};

 int match(void * t, void * p){
    printf("Try matching pattern[%d] to target(%d)\n", *(int*)t, *((int*)p));
    return *(int*)t - *((int*)p);
 }

 int main(){

   xrtp_list_t * list;
   xrtp_list_user_t *list_u;
   int i, find, *found;
   
   printf("List tester\n");

   list = xrtp_list_new();
   
   list_u = xrtp_list_newuser(list);
   for(i=0; i<7; i++){
     xrtp_list_add_first(list, &nums[i]);
   }

   printf("%d items in the list\n", xrtp_list_size(list));

   find = 5;
   found = xrtp_list_find(list, &find, match, list_u);
   if(found == NULL)
   {
      printf("[%d] is not found in list\n", find);     
   }
   else
   {
      printf("[%d] matches (%d) in list\n", *found, find);
   }
   xrtp_list_freeuser(list_u);

   return 1;

 }
