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
#include <timedia/xthread.h>

#define UA_OK		0
#define UA_FAIL		-1
#define UA_REJECT	-2
#define UA_IGNORE	-3
#define UA_BUSY     -4
#define UA_EIMPL     -5
#define UA_INVALID     -6

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

DECLSPEC
sipua_phonebook_t* 
sipua_new_book();

DECLSPEC
int 
sipua_done_book(sipua_phonebook_t* book);

DECLSPEC
sipua_phonebook_t* 
sipua_load_book(char* bookloc);

DECLSPEC
int 
sipua_save_book(sipua_phonebook_t* book, char* location);

DECLSPEC
sipua_contact_t* 
sipua_new_contact(char* name, int nbytes, char* memo, int mbytes, char* sip);

DECLSPEC
int 
sipua_contact(sipua_contact_t* contact, char** name, int* nbytes, char** memo, int* mbytes, char** sip);

DECLSPEC
sipua_contact_t* 
sipua_pick_contact(sipua_phonebook_t* book, int pos);

DECLSPEC
int 
sipua_add_contact(sipua_phonebook_t* book, sipua_contact_t* contact);

DECLSPEC
int 
sipua_remove_contact(sipua_phonebook_t* book, sipua_contact_t* contact);

DECLSPEC
xlist_t* 
sipua_contacts(sipua_phonebook_t* book);

#define CNAME_MAXBYTES 256

struct user_profile_s
{
	char* username;

	char* fullname;		/* could be multibytes char */
	int fbyte;

	char* regname;		/* sip name notation */
   char* realm;      /* pointer to regname realm part */
	
	int seconds;		/* lifetime */
	
	char* registrar;
   int regno;  /* for register fallback */

	char* cname;		/* username@netaddr */

	user_t* user;

	char* book_location;

	sipua_phonebook_t* phonebook;

	int online_status;
	int reg_status;

	char* reg_reason_phrase;
	char* reg_server;

	void* sipua;
	int enable;

	int seconds_left;
	
	xthread_t* thread_register;
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

DECLSPEC
int 
user_done(user_t* u);

extern DECLSPEC
user_t* 
user_new(char* uid, int sz);

extern DECLSPEC
user_t* 
sipua_load_user(const char* loc, const char* uid, const char* tok, int tsz);

extern DECLSPEC
int 
sipua_save_user(user_t* user, char* loc, char* tok, int tsz);

extern DECLSPEC
user_profile_t*
user_add_profile(user_t* user, char* fullname, int fbytes, char* book_loc, char* home, char* regname, int sec);

extern DECLSPEC
int 
user_set_profile(user_t* user, user_profile_t* profile, char* fullname, int fbytes, char* book_loc, char* home, char* regname, int sec);

extern DECLSPEC
int 
user_remove_profile(user_t* user, user_profile_t* prof);

extern DECLSPEC
int user_profile_number(user_t* user);

extern DECLSPEC
int user_profile(user_t* user, int num, char** fullname, int* fbytes, char** regname);

#endif
