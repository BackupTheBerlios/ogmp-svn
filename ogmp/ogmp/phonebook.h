/***************************************************************************
                          phonebook.h  -  description
                             -------------------
    begin                : Wed May 19 2004
    copyright            : (C) 2004 by Heming
    email                : heming@bigfoot.com; heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#include <timedia/list.h>

typedef struct sipua_phonebook_s sipua_phonebook_t;
typedef struct sipua_contact_s sipua_contact_t;

struct sipua_contact_s
{
	char		*name;
	int			nbytes;

	char		*memo;
	int			mbytes;

	char		*public_sip;
	char		*private_sip;
};

sipua_phonebook_t* sipua_new_book();
int sipua_done_book(sipua_phonebook_t* book);

sipua_phonebook_t* sipua_load_book(char* location);
int sipua_save_book(sipua_phonebook_t* book, char* location);

sipua_contact_t* sipua_new_contact(char* name, int nbytes, char* memo, int mbytes, char* public_sip, char* private_sip);
int sipua_contact(sipua_contact_t* contact, char** name, int* nbytes, char** memo, int* mbytes, char** public_sip, char** private_sip);

sipua_contact_t* sipua_pick_contact(sipua_phonebook_t* book, int pos);

int sipua_add_contact(sipua_phonebook_t* book, sipua_contact_t* contact);
int sipua_remove_contact(sipua_phonebook_t* book, sipua_contact_t* contact);

xlist_t* sipua_contacts(sipua_phonebook_t* book);

typedef struct user_profile_s user_profile_t;
struct user_profile_s
{
	char *username;

	char *fullname;		/* could be multibytes char */
	int fbyte;

	char *email;

	char *phone;

	char *regname;		/* sip name notation */
	
	int seconds;		/* lifetime */
	
	char registrar[256];

	char cname[256];	/* username@netaddr */

	char* book_location;

	sipua_phonebook_t* phonebook;
};

#endif