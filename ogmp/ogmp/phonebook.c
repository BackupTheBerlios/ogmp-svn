/***************************************************************************
                          phonebook.c
                             -------------------
    begin                : Tue Oct 12 2004
    copyright            : (C) 2004 by Heming
    email                : heming@bigfoot.com; heming@sourceforge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "phonebook.h"

#include <stdio.h>
#include <timedia/xmalloc.h>
#include <timedia/xstring.h>

#ifdef PBK_LOG
 #include <stdio.h>
 #define pbk_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define pbk_log(fmtargs)
#endif

#define UA_OK		0
#define UA_FAIL		-1

struct sipua_phonebook_s
{
	char *storage_location;		/* the storage location, maybe a URL extension */

	char buf[256];

	char *pc;
	int nc;

	xlist_t* contacts;
};

char* sipua_next_nchar (sipua_phonebook_t* book, FILE* f, int n)
{
	if(n >= 256)
		return NULL;

	if(book->pc+n >= book->buf + book->nc)
	{
		if(feof(f))
			return NULL;

		book->nc = fread(book->buf, 1, 256, f);

		book->pc = book->pc + n - book->nc;
	}

	book->pc += n;

	return book->pc;
}

char* sipua_next_line (sipua_phonebook_t* book, FILE* f)
{
	while(*book->pc != '\n')
	{
		if(book->pc >= book->buf + book->nc)
		{
			if(feof(f))
				return NULL;

			book->nc = fread(book->buf, 1, 256, f);
			book->pc = book->buf;
		}
		else
			book->pc++;
	}

	return book->pc;
}

int sipua_next_token (sipua_phonebook_t* book, FILE* f, char **start, char **end)
{
	while(*book->pc == '\t' || *book->pc == '\n' || *book->pc == ' ')
	{
		if(book->pc >= book->buf + book->nc)
		{
			if(feof(f))
				goto e;

			book->nc = fread(book->buf, 1, 256, f);
			book->pc = book->buf;
		}
		else
			book->pc++;
	}
	*start = book->pc;

	while(*book->pc != '\t' || *book->pc != '\n' || *book->pc != ' ')
	{
		if(book->pc >= book->buf + book->nc)
		{
			int d = *start - book->buf;

			if(d==0)
				goto e;

			memmove(book->buf, *start, book->nc-(*start-book->buf));
			*start = book->buf;

			if(feof(f))
				goto e;

			book->nc = fread(book->buf+book->nc-d, 1, d, f);
			book->pc -= d;
		}
		else
			book->pc++;
	}

	*end = book->pc;

	return UA_OK;

e:
	*start = *end = NULL;
	return UA_FAIL;
}

int sipua_read_nchar (sipua_phonebook_t* book, FILE* f, char *dest, int n)
{
	int nr = book->pc - book->buf;
	int c=0;

	if(book->nc - nr > n)
	{
		strncpy(dest, book->pc, n);
		book->pc += n;
		return n;
	}

	while(nr < book->nc && n>0)
	{
		strncpy(dest, book->buf, book->nc - nr);

		n -= book->nc - nr;
		c += book->nc - nr;

		if(feof(f))
			return c;

		book->nc = fread(book->buf, 1, 256, f);
		book->pc = book->buf;
		nr = 0;
	}
		
	return c;
}

int sipua_next_name (sipua_phonebook_t* book, FILE* f, sipua_contact_t* c)
{
	char *end;
	char *start;

	/* find a name tag */
	start = book->pc;
	while(strncmp(start, "\nname[", 6) != 0)
		while(*start != '\n')
			start = sipua_next_nchar(book, f, 1);

	/* name size */
	while(*start != '[')
		start = sipua_next_nchar(book, f, 1);

	start = sipua_next_nchar(book, f, 1);

	c->nbytes = strtol(start, &end, 10);

	start = end;
	if(strncmp(start, "]=", 2) != 0)
	{
		c->nbytes = 0;
		return UA_FAIL;
	}

	start = sipua_next_nchar(book, f, 2);

	c->name = xmalloc(c->nbytes);
	sipua_read_nchar(book, f, c->name, c->nbytes);

	return UA_OK;
}

int sipua_next_memo (sipua_phonebook_t* book, FILE* f, sipua_contact_t* c)
{
	char *end;
	char *start;

	/* find a name tag */
	start = book->pc;
	while(strncmp(start, "\nmemo[", 6) != 0)
		while(*start != '\n')
			start = sipua_next_nchar(book, f, 1);

	/* name size */
	while(*start != '[')
		start = sipua_next_nchar(book, f, 1);

	start = sipua_next_nchar(book, f, 1);

	c->nbytes = strtol(start, &end, 10);

	start = end;
	if(strncmp(start, "]=", 2) != 0)
	{
		c->nbytes = 0;
		return UA_FAIL;
	}

	start = sipua_next_nchar(book, f, 2);

	c->name = xmalloc(c->nbytes);
	sipua_read_nchar(book, f, c->name, c->nbytes);

	return UA_OK;
}

int sipua_next_public (sipua_phonebook_t* book, FILE* f, sipua_contact_t* c)
{
	char *start;
	char *end;

	/* find a name tag */
	start = book->pc;

	do
	{	
		start = sipua_next_line(book, f);
	}
	while(strncmp(start, "public=sip:", 11) != 0);

	sipua_next_token(book, f, &start, &end);

	while(*start != ':')
		start++;

	start++;

	c->public_sip = xmalloc(end-start+1);

	strncpy(c->public_sip, start, end-start);

	c->public_sip[end-start] = '\0';

	return UA_OK;
}

int sipua_next_private (sipua_phonebook_t* book, FILE* f, sipua_contact_t* c)
{
	char *end;
	char *start;

	/* find a name tag */
	start = book->pc;

	do
	{	
		start = sipua_next_line(book, f);
	}
	while(strncmp(start, "provate=sip:", 12) != 0);

	sipua_next_token(book, f, &start, &end);

	while(*start != ':')
		start++;

	start++;

	c->private_sip = xmalloc(end-start+1);

	strncpy(c->private_sip, start, end-start);

	c->private_sip[end-start] = '\0';

	return UA_OK;
}

int sipua_done_contact(void* gen)
{
	sipua_contact_t* c = (sipua_contact_t*)gen;

	xfree(c->name);
	xfree(c->memo);
	xfree(c->public_sip);
	xfree(c->private_sip);
	xfree(c);

	return UA_OK;
}

int sipua_load_file(sipua_phonebook_t* book, FILE* f)
{
	while(!feof(f))
    {
		sipua_contact_t *c;

		c = (sipua_contact_t*)xmalloc(sizeof(sipua_contact_t));
		if(c == NULL)
		{
			xlist_done(book->contacts, sipua_done_contact);
			return UA_FAIL;
		}

		if(sipua_next_name(book, f, c) < UA_OK)
			goto e1;
		if(sipua_next_public(book, f, c) < UA_OK)
			goto e2;
		if(sipua_next_private(book, f, c) < UA_OK)
			goto e3;
		if(sipua_next_memo(book, f, c) < UA_OK)
			goto e4;

		sipua_add_contact(book, c);
		continue;
		
	e4:
		xfree(c->private_sip);
	e3:
		xfree(c->public_sip);
	e2:
		xfree(c->name);
	e1:
		xfree(c);
    }

	return UA_OK; /* ok */
}

sipua_phonebook_t* sipua_new_book()
{
	sipua_phonebook_t* book = xmalloc(sizeof(sipua_phonebook_t));
	if(!book)
	{
		pbk_log(("sipua_new_book: no memory\n"));
		return NULL;
	}
	memset(book, 0, sizeof(sipua_phonebook_t));

	book->contacts = xlist_new();

	return book;
}

int sipua_done_book(sipua_phonebook_t* book)
{
	xlist_done(book->contacts, sipua_done_contact);

	xfree(book);

	return UA_OK;
}

sipua_phonebook_t* sipua_load_book(char* location)
{
	FILE* f;
	sipua_phonebook_t* book;

	f = fopen(location, "r");
	if (f==NULL)
	{
		pbk_log(("sipua_new_book: can not open '%s'\n", location));
		return NULL;
	}

	book = xmalloc(sizeof(sipua_phonebook_t));
	if(!book)
	{
		pbk_log(("sipua_new_book: no memory\n"));
		return NULL;
	}
	memset(book, 0, sizeof(sipua_phonebook_t));

	book->contacts = xlist_new();

	sipua_load_file(book, f);

	fclose(f);

	return book;
}

/**
 * name[size]={bytes}
 * public={sipid}
 * private={sipid}
 * memo[size]={bytes}
 */
int sipua_save_file(sipua_phonebook_t* book, FILE* f)
{
	xlist_user_t u;
	char prefix[16];

	sipua_contact_t *contact = xlist_first(book->contacts, &u);

	while (contact)
    {
		snprintf(prefix, 16, "name[%i]=", contact->nbytes);
		fwrite(prefix, 1, strlen(prefix), f);
		fwrite(contact->name, 1, contact->nbytes, f);

		fwrite("\npublic=sip:", 1, strlen("\npublic=sip:"), f);
		fwrite(contact->public_sip, 1, strlen(contact->public_sip), f);

		fwrite("\nprivate=sip:", 1, strlen("\nprivate=sip:"), f);
		fwrite(contact->private_sip, 1, strlen(contact->private_sip), f);

		snprintf(prefix, 16, "memo[%i]=", contact->mbytes);
		fwrite(prefix, 1, strlen(prefix), f);
		fwrite(contact->memo, 1, contact->mbytes, f);

		contact = xlist_next(book->contacts, &u);
    }

	return UA_OK; /* ok */
}

int sipua_save_book(sipua_phonebook_t* book, char* location)
{
	FILE* f;

	f = fopen(location, "w");
	if (f==NULL)
	{
		pbk_log(("sipua_save_book: can not open '%s'\n", location));
		return UA_FAIL;
	}

	sipua_save_file(book, f);

	fclose(f);

	return UA_OK;
}

sipua_contact_t* sipua_new_contact(char* name, int nbytes, char* memo, int mbytes, char* public_sip, char* private_sip)
{
	sipua_contact_t* contact = xmalloc(sizeof(sipua_contact_t));
	if(!contact)
	{
		pbk_log(("sipua_new_contact: no memory\n"));
		return NULL;
	}

	contact->name = xstr_nclone(name, nbytes);
	contact->memo = xstr_nclone(memo, nbytes);
	contact->public_sip = xstr_clone(public_sip);
	contact->private_sip = xstr_clone(private_sip);

	return contact;
}

int sipua_contact(sipua_contact_t* contact, char** name, int* nbytes, char** memo, int* mbytes, char** public_sip, char** private_sip)
{
	*name = contact->name;
	*nbytes = contact->nbytes;
	*memo = contact->memo;
	*mbytes = contact->mbytes;
	*public_sip = contact->public_sip;
	*private_sip = contact->private_sip;

	return UA_OK;
}

sipua_contact_t* sipua_pick_contact(sipua_phonebook_t* book, int pos)
{
	int i=0;
	xlist_user_t u;
	sipua_contact_t* contact = xlist_first(book->contacts, &u);
	while(i < pos && contact)
	{
		contact = xlist_next(book->contacts, &u);
		i++;
	}

	return contact;
}

int sipua_add_contact(sipua_phonebook_t* book, sipua_contact_t* contact)
{
	if(xlist_addto_first(book->contacts, contact) < OS_OK)
		return UA_FAIL;

	return UA_OK;
}

int sipua_remove_contact(sipua_phonebook_t* book, sipua_contact_t* contact)
{
	if(xlist_remove_item(book->contacts, contact) < OS_OK)
		return UA_FAIL;

	return UA_OK;
}

xlist_t* sipua_contacts(sipua_phonebook_t* book)
{
	return book->contacts;
}

/**
 * username={word}
 * fullname[]={string}
 * email={email string}
 * reg.home={registrar url}
 * reg.name={string}
 * reg.seconds={number}
 * book.location={file:path|usb:key}
 */
int sipua_save_user_file(user_profile_t* user, FILE* f)
{
	char str[64];

	fwrite("username=", 1, strlen("username="), f);
	fwrite(user->username, 1, strlen(user->username), f);

	snprintf(str, 63, "\nfullname[%i]=", user->fbyte);
	fwrite(str, 1, strlen(str), f);
	fwrite(user->fullname, 1, user->fbyte, f);

	fwrite("\nemail=", 1, strlen("\nemail="), f);
	fwrite(user->email, 1, strlen(user->email), f);

	fwrite("\nreg.home=", 1, strlen("\nreg.home="), f);
	fwrite(user->registrar, 1, strlen(user->registrar), f);

	fwrite("\nreg.name=", 1, strlen("\nreg.name="), f);
	fwrite(user->regname, 1, strlen(user->regname), f);

	fwrite("\nreg.seconds=", 1, strlen("\nreg.seconds="), f);
	snprintf(str, 63, "%i", user->seconds);
	fwrite(str, 1, strlen(str), f);

	fwrite("\nphonebook.location=", 1, strlen("\nphonebook.loc="), f);
	fwrite(user->book_location, 1, strlen(user->book_location), f);

	return UA_OK; /* ok */
}

int sipua_save_user(user_profile_t* user, char* location)
{
	FILE* f;

	f = fopen(location, "w");
	if (f==NULL)
	{
		pbk_log(("sipua_save_user: can not open '%s'\n", location));
		return UA_FAIL;
	}

	sipua_save_user_file(user, f);

	fclose(f);

	return UA_OK;
}

int sipua_load_nchar(char* data, int len, char* buf, int* bsize, int* i, FILE* f)
{
	int n=0;

	if(*bsize == 0)
		return 0;

	while(!feof(f) && n<len)
	{
		data[n++] = buf[(*i)++];

		if(*i == *bsize)
		{
			*bsize = fread(buf, 1, *bsize, f);
			*i = 0;
		}
	}

	return n;
}

int sipua_load_line(char* ln, int len, char* buf, int* bsize, int* i, FILE* f)
{
	int n=0;

	if(*bsize == 0)
		return 0;

	while(buf[*i] != '\n')
	{
		ln[n++] = buf[(*i)++];

		if(*i == *bsize)
		{
			if(!feof(f))
				*bsize = fread(buf, 1, *bsize, f);
			else
			{
				*bsize = 0;

				ln[n] = '\0';
				return n;
			}

			*i = 0;
		}
	}

	(*i)++;
	if(*i == *bsize)
	{
		if(!feof(f))
			*bsize = fread(buf, 1, *bsize, f);
		else
			*bsize = 0;

		*i = 0;
	}

	ln[n] = '\0';

	return n;
}

int sipua_load_user_file(xlist_t* profs, FILE* f)
{
	char buf[256];
	int bsize = 256;
	int bi;
	
	char line[128];
	char *start, *end;

	while(!feof(f))
	{
		char *pc;
		int len;

		user_profile_t* u;
		
		len = sipua_load_line(line, 128, buf, &bsize, &bi, f);
		if(len == 0)
			break;

		pc = line;
		if(0!=strncmp(pc, "username=", 9))
			continue;
		else
		{
			u = xmalloc(sizeof(user_profile_t));
			if(!u)
				return xlist_size(profs);
			
			while(*pc != '=')
				pc++;

			end = start = pc+1;
			while(*end != '\n' || *end != '\t' || *end != ' ')
				end++;
			
			u->username = xmalloc(end-start+1);
			strncpy(u->username, start, end-start);
			u->username[end-start] = '\0';
		}

		len = sipua_load_line(line, 128, buf, &bsize, &bi, f);
		pc = line;

		if(0==strncmp(pc, "fullname[", 9))
		{
			int n;
			while(*pc != '[')
				pc++;

			start = pc+1;
			u->fbyte = strtol(start, &end, 10);
			u->fullname = xmalloc(u->fbyte);

			while(*end != '=')
				end++;

			start = end+1;

			n = len-(start-line);
			if(n >= u->fbyte)
			{
				strncpy(u->fullname, start, u->fbyte);
			}
			else
			{
				strncpy(u->fullname, start, n);

				len = sipua_load_nchar(u->fullname+n, u->fbyte-n, buf, &bsize, &bi, f);
			}
		}

		len = sipua_load_line(line, 128, buf, &bsize, &bi, f);
		if(len == 0)
			break;
		pc = line;

		if(0==strncmp(pc, "reg.home=", 9))
		{
			while(*pc != '=')
				pc++;

			end = start = pc+1;
			while(*end != '\n' || *end != '\t' || *end != ' ')
				end++;

			strncpy(u->registrar, start, end-start);
			u->registrar[end-start] = '\0';
		}

		len = sipua_load_line(line, 128, buf, &bsize, &bi, f);
		if(len == 0)
			break;
		pc = line;

		if(0==strncmp(pc, "reg.name=", 9))
		{
			while(*pc != '=')
				pc++;

			end = start = pc+1;
			while(*end != '\n' || *end != '\t' || *end != ' ')
				end++;

			strncpy(u->regname, start, end-start);
			u->regname[end-start] = '\0';
		}

		len = sipua_load_line(line, 128, buf, &bsize, &bi, f);
		if(len == 0)
			break;
		pc = line;

		if(0==strncmp(pc, "reg.seconds=", 11))
		{
			while(*pc != '=')
				pc++;

			start = pc+1;

			u->seconds = strtol(start, &end, 10);
		}

		len = sipua_load_line(line, 128, buf, &bsize, &bi, f);
		if(len == 0)
			break;
		pc = line;

		if(0==strncmp(pc, "phonebook.location=", 19))
		{
			while(*pc != '=')
				pc++;

			end = start = pc+1;
			while(*end != '\n' || *end != '\t' || *end != ' ')
				end++;

			strncpy(u->book_location, start, end-start);
			u->book_location[end-start] = '\0';
		}
	}

	return UA_OK; /* ok */
}

int sipua_load_user(char* location, xlist_t* profs)
{
	FILE* f;
	int nprof;

	f = fopen(location, "r");
	if (f==NULL)
	{
		pbk_log(("sipua_load_user: can not open '%s'\n", location));
		return 0;
	}

	/* load locally currently */
	nprof = sipua_load_user_file(profs, f);

	fclose(f);

	return nprof;
}

