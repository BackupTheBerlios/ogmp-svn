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

typedef struct user_s user_t;
typedef struct user_profile_s user_profile_t;
typedef struct sipua_phonebook_s sipua_phonebook_t;
typedef struct sipua_contact_s sipua_contact_t;

struct sipua_contact_s
{
	char		*name;
	int			nbytes;

	char		*memo;
	int			mbytes;

	char		*sip;
};

sipua_phonebook_t* sipua_new_book();
int sipua_done_book(sipua_phonebook_t* book);

sipua_phonebook_t* sipua_load_book(char* bookloc);
int sipua_save_book(sipua_phonebook_t* book, char* location);

sipua_contact_t* sipua_new_contact(char* name, int nbytes, char* memo, int mbytes, char* sip);
int sipua_contact(sipua_contact_t* contact, char** name, int* nbytes, char** memo, int* mbytes, char** sip);

sipua_contact_t* sipua_pick_contact(sipua_phonebook_t* book, int pos);

int sipua_add_contact(sipua_phonebook_t* book, sipua_contact_t* contact);
int sipua_remove_contact(sipua_phonebook_t* book, sipua_contact_t* contact);

xlist_t* sipua_contacts(sipua_phonebook_t* book);

#define CNAME_MAXBYTES 256

struct user_profile_s
{
	char *username;

	char *fullname;		/* could be multibytes char */
	int fbyte;
	/*
	char *email;
	char *phone;
	*/
	char *regname;		/* sip name notation */
	
	int seconds;		/* lifetime */
	
	char* registrar;

	char* cname;	/* username@netaddr */

	user_t *user;

	char* book_location;

	sipua_phonebook_t* phonebook;

	int online_status;
	int reg_status;

	char *reg_reason_phrase;
	char *reg_server;
};

struct user_s
{
	char *uid;
	char *loc;

	char *userloc;

	char* nettype;
	char* addrtype;
	char* netaddr;

	xlist_t *profiles;
	int modified;

	char *tok;
	int	tok_bytes;
};

int user_done(user_t* u);
user_t* user_new(char* uid, int sz);

user_t* sipua_load_user(char* loc, char *uid, char* tok, int tsz);
int sipua_save_user(user_t* user, char* loc, char* tok, int tsz);

int user_add_profile(user_t* user, char* fullname, int fbytes, char* book_loc, char* home, char* regname, int sec);
int user_remove_profile(user_t* user, user_profile_t* prof);

#endif