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
#include "log.h"

#include <stdio.h>
#include <timedia/xmalloc.h>
#include <timedia/xstring.h>

#ifdef PBK_LOG
 #include <stdio.h>
 #define pbk_log(fmtargs)  do{log_printf fmtargs;}while(0)
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
	int modified;
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

int sipua_next_sip (sipua_phonebook_t* book, FILE* f, sipua_contact_t* c)
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

	c->sip = xmalloc(end-start+1);

	strncpy(c->sip, start, end-start);

	c->sip[end-start] = '\0';

	return UA_OK;
}

int sipua_done_contact(void* gen)
{
	sipua_contact_t* c = (sipua_contact_t*)gen;

	xfree(c->name);
	xfree(c->memo);
	xfree(c->sip);
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
		if(sipua_next_memo(book, f, c) < UA_OK)
			goto e2;
		if(sipua_next_sip(book, f, c) < UA_OK)
			goto e3;

		sipua_add_contact(book, c);
		continue;
		
	e3:
		xfree(c->sip);
	e2:
		xfree(c->name);
	e1:
		xfree(c);
    }

	return xlist_size(book->contacts); /* ok */
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

sipua_phonebook_t* sipua_load_book(char* bookloc)
{
	FILE* f;
	sipua_phonebook_t* book;

	f = fopen(bookloc, "r");
	if(!f)
	{
		pbk_log(("sipua_new_book: can not open '%s'\n", bookloc));
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

		snprintf(prefix, 16, "memo[%i]=", contact->mbytes);
		fwrite(prefix, 1, strlen(prefix), f);
		fwrite(contact->memo, 1, contact->mbytes, f);

		fwrite("\nsip=sip:", 1, strlen("\nsip=sip:"), f);
		fwrite(contact->sip, 1, strlen(contact->sip), f);

		contact = xlist_next(book->contacts, &u);
    }

	return UA_OK; /* ok */
}

int sipua_save_book(sipua_phonebook_t* book, char* location)
{
	FILE* f;

	if(!book->modified)
		return UA_OK;

	f = fopen(location, "w");
	if (f==NULL)
	{
		pbk_log(("sipua_save_book: can not open '%s'\n", location));
		return UA_FAIL;
	}

	sipua_save_file(book, f);

	book->modified = 0;

	fclose(f);

	return UA_OK;
}

sipua_contact_t* sipua_new_contact(char* name, int nbytes, char* memo, int mbytes, char* sip)
{
	sipua_contact_t* contact = xmalloc(sizeof(sipua_contact_t));
	if(!contact)
	{
		pbk_log(("sipua_new_contact: no memory\n"));
		return NULL;
	}

	contact->name = xmalloc(nbytes+1);
	strncpy(contact->name, name, nbytes);
	contact->name[nbytes] = '\0';
	contact->nbytes = nbytes;

	contact->memo = xmalloc(mbytes+1);
	strncpy(contact->memo, memo, mbytes);
	contact->memo[mbytes] = '\0';
	contact->mbytes = mbytes;

	contact->sip = xstr_clone(sip);

	return contact;
}

int sipua_contact(sipua_contact_t* contact, char** name, int* nbytes, char** memo, int* mbytes, char** sip)
{
	*name = contact->name;
	*nbytes = contact->nbytes;
	*memo = contact->memo;
	*mbytes = contact->mbytes;
	*sip = contact->sip;

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
		return -1;

	book->modified = 1;

	return xlist_size(book->contacts);
}

int sipua_remove_contact(sipua_phonebook_t* book, sipua_contact_t* contact)
{
	if(xlist_remove_item(book->contacts, contact) < OS_OK)
		return -1;

	book->modified = 1;

	return xlist_size(book->contacts);
}

xlist_t* sipua_contacts(sipua_phonebook_t* book)
{
	return book->contacts;
}

/**
 * uid={string}
 * tok[bytes]={data}
 *
 * fullname[bytes]={string}
 * (email={email string})
 * reg.home={registrar url}
 * reg.name={string}
 * reg.seconds={number}
 * book.location={file:path|usb:key}
 */

int sipua_save_user_file(user_t* user, FILE* f, char* tok, int tsz)
{
	char str[64];

	user_profile_t* prof;
	xlist_user_t lu;

	fwrite("uid=", 1, strlen("uid="), f);
	fwrite(user->uid, 1, strlen(user->uid), f);

	snprintf(str, 63, "\ntok[%i]=", tsz);
	fwrite(str, 1, strlen(str), f);
	fwrite(tok, 1, tsz, f);

	if(!user->profiles)
		return UA_OK;

	prof = (user_profile_t*)xlist_first(user->profiles, &lu);
	while(prof)
	{
		snprintf(str, 63, "\nfullname[%i]=", prof->fbyte);
		fwrite(str, 1, strlen(str), f);
		fwrite(prof->fullname, 1, prof->fbyte, f);
		/*
		fwrite("\nemail=", 1, strlen("\nemail="), f);
		fwrite(prof->email, 1, strlen(prof->email), f);
		*/
		fwrite("\nreg.home=", 1, strlen("\nreg.home="), f);
		fwrite(prof->registrar, 1, strlen(prof->registrar), f);

		fwrite("\nreg.name=", 1, strlen("\nreg.name="), f);
		fwrite(prof->regname, 1, strlen(prof->regname), f);

		fwrite("\nreg.seconds=", 1, strlen("\nreg.seconds="), f);
		snprintf(str, 63, "%i", prof->seconds);
		fwrite(str, 1, strlen(str), f);

		fwrite("\nphonebook.location=", 1, strlen("\nphonebook.location="), f);

		if(prof->book_location)
			fwrite(prof->book_location, 1, strlen(prof->book_location), f);
	
		fwrite("\n", 1, 1, f);

		prof = (user_profile_t*)xlist_next(user->profiles, &lu);
	}

	return UA_OK; /* ok */
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

/**
 * return length of the line, which include zero
 * if eof, return -1
 */
int sipua_get_line(char* ln, int len, char* buf, int* bsize, int* i, FILE* f)
{
	int n=0;
	int nr;

	if(*i == *bsize)
	{
		*bsize = fread(buf, 1, *bsize, f);
		*i = 0;

		if(*bsize == 0)
			return -1;
	}

	while(buf[*i] != '\n')
	{
		ln[n++] = buf[*i];
		(*i)++;

		if(*i == *bsize) 
		{
			nr = fread(buf, 1, *bsize, f);
			*i = 0;

			if(nr == 0)
			{
				*bsize = 0;
				break;
			}

			if(nr < *bsize)
			{
				/* last block extra opt */
				buf[nr] = '\n';
				*bsize = nr+1;
			}
			else
				*bsize = nr;
		}
	}

	ln[n] = '\0';
	(*i)++;

	return n;
}

/**
 * if ok, return current buf pos;
 * if failure, return -1
 */
int sipua_verify_user_file(FILE* f, char* uid, char* tok, int tsz, char *buf, int *bsize)
{
	int bi;
	
	char line[128];
	char *start, *end;

	char *pc;
	int len;

	char* tok0;

	int veri = -1;

	bi = *bsize;
	len = sipua_get_line(line, 128, buf, bsize, &bi, f);

	/* check uid */
	if(len < 4 || 0 != strncmp(line, "uid=", 4))
		return 0;
	else
	{
		start = line;

		while(*start != '=')
			start++;

		end = ++start;

		while(*end != '\n')
			end++;

		if(0 != strcmp(start, uid))
			return 0;
	}

	/* verify token */
	len = sipua_get_line(line, 128, buf, bsize, &bi, f);
	pc = line;

	if(len < 4 || 0 != strncmp(pc, "tok[", 4))
	{
		return 0;
	}
	else
	{
		int n, sz;

		while(*pc != '[')
			pc++;

		start = pc+1;
		sz = strtol(start, &end, 10);

		if(tsz != sz)
		{
			return 0;
		}

		tok0 = xmalloc(tsz);

		while(*end != '=')
			end++;

		start = end+1;

		n = len-(start-line);
		if(n >= tsz)
		{
			strncpy(tok0, start, tsz);
		}
		else
		{
			strncpy(tok0, start, n);

			len = sipua_load_nchar(tok0+n, tsz-n, buf, bsize, &bi, f);
		}

		if(memcmp(tok0, tok, tsz) == 0)
			veri = bi;
	}

	xfree(tok0);

	return veri;
}

int sipua_save_user(user_t* user, char* loc, char* tok, int tsz)
{
	FILE* f;

	char buf[256];
	int bsize = 256;

	f = fopen(loc, "r");
	if(f)
	{
		if(sipua_verify_user_file(f, user->uid, tok, tsz, buf, &bsize) == -1)
		{
			pbk_log(("sipua_save_user: Unauthrized operation to '%s' denied !\n", loc));
			
			return UA_FAIL;
		}

		fclose(f);
	}

	f = fopen(loc, "w");

	if (f==NULL)
	{
		pbk_log(("sipua_save_user: can not open '%s'\n", loc));
		return UA_FAIL;
	}

	if(sipua_save_user_file(user, f, tok, tsz) >= UA_OK)
		user->modified = 0;

	fclose(f);

	return UA_OK;
}

user_t* sipua_load_user_file(FILE* f, char* uid, char* tok, int tsz)
{
	char buf[256];

	int bsize = 256;
	int bi;
	
	char line[128];
	char *start, *end;

	char *pc;
	int len;

	int nitem = 0;

	user_t* user = NULL;

	bi = sipua_verify_user_file(f, uid, tok, tsz, buf, &bsize);

	if(bi == -1)
		return NULL;
	
	user = xmalloc(sizeof(user_t));
	if(!user)
	{
		return NULL;
	}
	memset(user, 0, sizeof(user_t));

	user->profiles = xlist_new();
	if(!user->profiles)
	{
		xfree(user);
		return NULL;
	}

	user->uid = xstr_clone(uid);

	while(1)
	{
		user_profile_t* u;

		len = sipua_get_line(line, 128, buf, &bsize, &bi, f);
		if(len < 0)
			break;

		pc = line;
	
		if(len > 9 && 0==strncmp(pc, "fullname[", 9))
		{
			int n;

			u = xmalloc(sizeof(user_profile_t));
			if(!u)
				return 0;
			memset(u, 0, sizeof(user_profile_t));
			
			while(*pc != '[')
				pc++;

			start = pc+1;
			u->fbyte = strtol(start, &end, 10);
			u->fullname = xmalloc(u->fbyte+1);

			while(*end != '=')
				end++;

			start = end+1;

			n = len-(start-line);
			if(n >= u->fbyte)
			{
				u->fullname = xstr_nclone(start, u->fbyte);
			}
			else
			{
				strncpy(u->fullname, start, n);

				len = sipua_load_nchar(u->fullname+n, u->fbyte-n, buf, &bsize, &bi, f);
			}

			u->fullname[u->fbyte] = '\0';
		
			u->username = user->uid;

			nitem++;
		}
		else if(len > 9 && 0==strncmp(pc, "reg.home=", 9))
		{
			while(*pc != '=')
				pc++;

			start = pc+1;

			u->registrar = xstr_clone(start);

			nitem++;
		}
		else if(len > 9 && 0==strncmp(pc, "reg.name=", 9))
		{
			while(*pc != '=')
				pc++;

			start = pc+1;

			u->regname = xstr_clone(start);

			nitem++;
		}

		if(len > 11 && 0==strncmp(pc, "reg.seconds=", 11))
		{
			while(*pc != '=')
				pc++;

			start = pc+1;

			u->seconds = strtol(start, &end, 10);

			nitem++;
		}
		else if(len >= 19 && 0==strncmp(pc, "phonebook.location=", 19))
		{
			while(*pc != '=')
				pc++;

			start = pc+1;

			u->book_location = xstr_clone(start);

			nitem++;
		}

		if(nitem < 5)
			continue;

		xlist_addto_first(user->profiles, u);
		nitem = 0;
	}


	return user;
}

user_t* sipua_load_user(char* loc, char *uid, char* tok, int tsz)
{
	FILE* f;
	user_t *user;

	f = fopen(loc, "r");
	if (f==NULL)
	{
		pbk_log(("sipua_load_user: can not open '%s'\n", loc));

		return NULL;
	}

	/* load locally currently */
	user = sipua_load_user_file(f, uid, tok, tsz);
	
	if(user)
	{
		user->loc = xstr_clone(loc);

		user->tok = xstr_nclone(tok, tsz);
		user->tok_bytes = tsz;
	}

	fclose(f);

	return user;
}

int user_done_profile(void* gen)
{
	user_profile_t* prof = (user_profile_t*)gen;

	xfree(prof->fullname);		/* could be multibytes char */

	xfree(prof->regname);		/* sip name notation */

	xfree(prof->registrar);		/* sip name notation */

	if(prof->reg_reason_phrase)
		xfree(prof->reg_reason_phrase);
	
	if(prof->reg_server)
		xfree(prof->reg_server);
	
	if(prof->book_location) xfree(prof->book_location);

	if(prof->phonebook) sipua_done_book(prof->phonebook);

	return OS_OK;
}

/**
 * username
 * fullname
 * reg_home
 * reg_name
 * reg_seconds
 * book_loc
 */
int user_add_profile(user_t* user, char* fullname, int fbytes, char* book_loc, char* home, char* regname, int sec)
{
	user_profile_t* prof = xmalloc(sizeof(user_profile_t));

	if(!prof) return -1;

	memset(prof, 0, sizeof(user_profile_t));

	prof->username = user->uid;

	prof->fullname = xstr_nclone(fullname, fbytes);
	prof->fbyte = fbytes;

	prof->registrar = xstr_clone(home);
	prof->regname = xstr_clone(regname);
	prof->seconds = sec;

	if(book_loc)
		prof->book_location = xstr_clone(book_loc);

	if(xlist_addto_first(user->profiles, prof) >= OS_OK)
		user->modified = 1;

	return xlist_size(user->profiles);
}

int user_remove_profile(user_t* user, user_profile_t* prof)
{
	if(xlist_remove_item(user->profiles, prof) < OS_OK)
		return -1;

	user_done_profile(prof);
	user->modified = 1;

	return xlist_size(user->profiles);
}

int user_done(user_t* u)
{
	if(u->loc)
	{
		xfree(u->loc);

		xlist_done(u->profiles, user_done_profile);
	}

	if(u->userloc)
		xfree(u->userloc);

	xfree(u->uid);

	xfree(u);

	return UA_OK;
}

user_t* user_new(char* uid, int sz)
{
	user_t* u = xmalloc(sizeof(user_t));
	if(u)
	{
		memset(u, 0, sizeof(user_t));

		u->uid = xstr_nclone(uid, sz);
	}

	return u;
}
