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
#include <timedia/ui.h>

#define PBK_LOG

#ifdef PBK_LOG
 #include <stdio.h>
 #define pbk_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define pbk_log(fmtargs)
#endif

struct sipua_phonebook_s
{
	char *storage_location;		/* the storage location, maybe a URL extension */

	char buf[256];

	char *pc;
	int nc;

	xlist_t* contacts;
	int modified;

   user_t* user;
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

sipua_phonebook_t* sipua_new_book(user_t* user)
{
	sipua_phonebook_t* book = xmalloc(sizeof(sipua_phonebook_t));
	if(!book)
	{
		pbk_log(("sipua_new_book: no memory\n"));
		return NULL;
	}
	memset(book, 0, sizeof(sipua_phonebook_t));

	book->contacts = xlist_new();
   book->user = user;

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
	if(xlist_addto_last(book->contacts, contact) < OS_OK)
		return -1;

	book->modified = 1;
   book->user->modified = 1;

	return xlist_size(book->contacts);
}

int sipua_remove_contact(sipua_phonebook_t* book, sipua_contact_t* contact)
{
	if(xlist_remove_item(book->contacts, contact) < OS_OK)
		return -1;

	book->modified = 1;



	return xlist_size(book->contacts);
}

int sipua_match_contact_id(void* t, void* p)
{
   sipua_contact_t* target = (sipua_contact_t*)t;
   char* patern = (char*)p;

   return strcmp(target->sip, patern);  /* if 0, match */
}

int sipua_delete_contact_id(sipua_phonebook_t* book, const char* regname)
{
   int m;
   int n = xlist_size(book->contacts);

   if(xlist_delete_if(book->contacts, regname, sipua_match_contact_id, sipua_done_contact) < OS_OK)
   {
      pbk_log(("sipua_delete_contact_id: no contact[%s] deleted\n", regname));
		return -1;
   }
   
   pbk_log(("sipua_delete_contact_id: contact[%s]\n", regname));

   m = xlist_size(book->contacts);   
   if(n != m)
   {
      pbk_log(("sipua_delete_contact_id: no contact[%s] deleted\n", regname));

      book->modified = 1;
      book->user->modified = 1;
   }
   
	return m;
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

int sipua_save_user_file_v1(user_t* user, FILE* f, char* tok, int tsz)
{
	char str[64];

	user_profile_t* prof;
	xlist_user_t lu;

	fwrite("version=1\n", 1, strlen("version=1\n"), f);

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

	fwrite("\n", 1, strlen("\n"), f);

	return UA_OK; /* ok */
}

int sipua_save_user_file_v2(user_t* user, FILE* f, const char* tok, int tsz)
{
	char str[64];
   int pindex = 0;
   int nbook = 0;

	user_profile_t* prof;
	xlist_user_t lu;

	fwrite("version=2\n", 1, strlen("version=2\n"), f);

	fwrite("uid=", 1, strlen("uid="), f);
	fwrite(user->uid, 1, strlen(user->uid), f);

	snprintf(str, 63, "\ntok[%i]=", tsz);
	fwrite(str, 1, strlen(str), f);
	fwrite(tok, 1, tsz, f);

	if(!user->profiles)
		return UA_OK;

	fwrite("\n\n[profiles]\n", 1, strlen("\n\n[profiles]\n"), f);
	snprintf(str, 63, "number=%i\n", xlist_size(user->profiles));
	fwrite(str, 1, strlen(str), f);
   
	prof = (user_profile_t*)xlist_first(user->profiles, &lu);
	while(prof)
	{
      if(prof->phonebook)
         nbook++;
         
	   snprintf(str, 63, "\nprofile.index=%i\n", pindex);
	   fwrite(str, 1, strlen(str), f);
      
		snprintf(str, 63, "fullname[%i]=", prof->fbyte);
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
      pindex++;
	}
   
   if(nbook)
   {
	   fwrite("\n[phonebooks]\n", 1, strlen("\n[phonebooks]\n"), f);
      snprintf(str, 63, "number=%i\n", nbook);
	   fwrite(str, 1, strlen(str), f);
   }

   pindex = 0;
	prof = (user_profile_t*)xlist_first(user->profiles, &lu);
	while(prof)
   {
      sipua_contact_t* contact;
      xlist_user_t clu;
      
      sipua_phonebook_t* book = prof->phonebook;
      if(book)
      {
         int cindex = 0;
         
	      fwrite("\n[contacts]\n", 1, strlen("\n[contacts]\n"), f);
         snprintf(str, 63, "profile.ref=%i\n", pindex);
	      fwrite(str, 1, strlen(str), f);
         
         snprintf(str, 63, "number=%i\n", xlist_size(book->contacts));
	      fwrite(str, 1, strlen(str), f);

	      contact = (sipua_contact_t*)xlist_first(book->contacts, &clu);
         while(contact)
         {
	         snprintf(str, 63, "\ncontact.index=%i\n", cindex);
	         fwrite(str, 1, strlen(str), f);

		      snprintf(str, 63, "name[%i]=", contact->nbytes);
		      fwrite(str, 1, strlen(str), f);
		      fwrite(contact->name, 1, contact->nbytes, f);
            
		      snprintf(str, 63, "\nmemo[%i]=", contact->mbytes);
		      fwrite(str, 1, strlen(str), f);
		      fwrite(contact->memo, 1, contact->mbytes, f);

		      snprintf(str, 63, "\nreg.name=%s\n", contact->sip);
		      fwrite(str, 1, strlen(str), f);

		      contact = (sipua_contact_t*)xlist_next(book->contacts, &clu);
            cindex++;
         }
      }
      
		prof = (user_profile_t*)xlist_next(user->profiles, &lu);
      pindex++;
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

      *i = 0;
      
      if(feof(f))
         return -1;
         
      nr = fread(buf, 1, *bsize, f);
      
      if(nr == *bsize)
	      *bsize = nr;
	   else
      {
			/* last block extra opt */
			buf[nr] = '\0';
			*bsize = nr+1;
	   }
   }
   
   while(buf[*i] != '\n' && buf[*i] != '\0')
	{
		ln[n++] = buf[*i];
		(*i)++;

      if(*i == *bsize) 
	   {
         *i = 0;
         
         if(feof(f))
         {
	         *bsize = 0;
            break;
         }
         else
         {      
	         nr = fread(buf, 1, *bsize, f);

	         if(nr == *bsize)
	            *bsize = nr;
	         else
	         {
				   /* last block extra opt */
				   buf[nr] = '\0';
				   *bsize = nr+1;
	         }
         }
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
int sipua_verify_user_file(FILE* f, const char* uid, const char* tok, int tsz, char *buf, int *bsize, int *version)
{
	int bi;
	
	char line[128];
	char *start, *end;

	char *pc;
	int len;

	char* tok0;

	int veri = -1;

	bi = *bsize;
    
	/* verify version */
	len = sipua_get_line(line, 128, buf, bsize, &bi, f);
	if(len < 9 || 0 != strncmp(line, "version=", 8))
   {
      pbk_log(("sipua_verify_user_file: wrong format 1\n"));
		return -1;
   }
	else
	{
		start = line;

		while(*start != '=')
			start++;

		start++;
		*version = strtol(start, NULL, 0);
      if(*version <= 0)
      {
         pbk_log(("sipua_verify_user_file: wrong version(v%d)\n", *version));
         return -1;
      }
	}

	/* check uid */
	len = sipua_get_line(line, 128, buf, bsize, &bi, f);
	if(len < 4 || 0 != strncmp(line, "uid=", 4))
   {
      pbk_log(("sipua_verify_user_file: wrong format 2\n"));
		return -1;
   }

	else
	{
		start = line;

		while(*start != '=')
			start++;

		end = ++start;

		while(*end != '\n')
			end++;

		if(0 != strcmp(start, uid))
      {
         pbk_log(("sipua_verify_user_file: wrong user\n"));
			return -1;
      }
	}

	/* verify token */
	len = sipua_get_line(line, 128, buf, bsize, &bi, f);
	if(len < 6 || 0 != strncmp(line, "tok[", 4))
	{
      pbk_log(("sipua_verify_user_file: wrong format 3, len[%d]line[%s]\n", len, line));
		return -1;
	}
	else
	{
		int n, sz;
      
	   pc = line;
		while(*pc != '[')
			pc++;

		start = pc+1;
		sz = strtol(start, &end, 10);

		if(tsz != sz)
			return -1;

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
      else
         pbk_log(("sipua_verify_user_file: wrong token\n"));
	}
	xfree(tok0);
   
	return veri;
}

int sipua_save_user(user_t* user, const char* dataloc, const char* tok, int tsz)
{
	FILE* f;
   const char* fname;

	char buf[256];
	int bsize = 256;
   int version;

   if(dataloc)
   {
	   f = fopen(dataloc, "w");
      fname = dataloc;
   }
   else
   {
      f = fopen(user->loc, "r");
	   if(f)
	   {
		   if(sipua_verify_user_file(f, user->uid, tok, tsz, buf, &bsize, &version) == -1)
		   {
			   pbk_log(("sipua_save_user: Unauthrized operation to '%s' denied !\n", user->loc));
			   return UA_FAIL;
		   }

		   fclose(f);
	   }
      
	   f = fopen(user->loc, "w");
      fname = user->loc;
   }
   
	if (f==NULL)
	{
		pbk_log(("sipua_save_user: can not open '%s'\n", fname));

		return UA_FAIL;
	}

   pbk_log(("sipua_save_user: save to [%s]\n", fname));

	if(sipua_save_user_file_v2(user, f, tok, tsz) >= UA_OK)
		user->modified = 0;

	fclose(f);

	return UA_OK;
}

/*no use any more*/
user_t* sipua_load_user_file_v0(FILE* f, const char* uid, const char* tok, int tsz)
{
	char buf[256];

	int bsize = 256;
	int bi;
   int ver;
	
	char line[128];
	char *start, *end;

	char *pc;
	int len;

	int nitem = 0;

	user_t* user = NULL;

	bi = sipua_verify_user_file(f, uid, tok, tsz, buf, &bsize, &ver);
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

			u->user = user;

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

         u->realm = u->regname;

         while(*u->realm != '@')
            u->realm++;
         u->realm++;

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

		xlist_addto_last(user->profiles, u);
		nitem = 0;
	}

	return user;
}

user_t* sipua_load_user_file_v1(FILE* f, const char* uid, char* buf, int *bsize, int *bi)
{
	char line[128];
	char *start, *end;

	char *pc;
	int len;

	int nitem = 0;

	user_t* user = NULL;


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

		len = sipua_get_line(line, 128, buf, bsize, bi, f);
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

			u->user = user;

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

				len = sipua_load_nchar(u->fullname+n, u->fbyte-n, buf, bsize, bi, f);
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

         u->realm = u->regname;
         while(*u->realm != '@')
            u->realm++;
         u->realm++;

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

		xlist_addto_last(user->profiles, u);
		nitem = 0;
	}

	return user;
}

int sipua_get_section(char* title, char* ln, int n, char* buf, int* bsize, int* bi, FILE* f)
{
   int len;

   len = sipua_get_line(ln, n, buf, bsize, bi, f);
   while(len >= 0)
   {
      if(len > 0 && 0==strcmp(title, ln))
         return 1;

      len = sipua_get_line(ln, len, buf, bsize, bi, f);
   }

   return 0;
}

user_t* sipua_load_user_file_v2(FILE* f, const char* uid, char* buf, int *bsize, int *bi)
{
	char line[128];
	char *start, *end;

	char *pc;
	int len;
   
   int nprof = 0;
   int nitem = 0;
   int nbook = 0;

	user_t* user = NULL;

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

   /* load profile */
   if(sipua_get_section("[profiles]", line, 128, buf, bsize, bi, f))
   {
	   len = sipua_get_line(line, 128, buf, bsize, bi, f);
      if(len > 7 && 0==strncmp(line, "number=", 7))
      {
         pc = line;
         while(*pc != '=')
				pc++;

			start = pc+1;

         nprof = strtol(start, &end, 10);
      }
   }

   pbk_log(("sipua_load_user_file_v2: %d profiles\n", nprof));
   
   while(nprof)
	{
		user_profile_t* u;

		len = sipua_get_line(line, 128, buf, bsize, bi, f);
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

			u->user = user;

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

				len = sipua_load_nchar(u->fullname+n, u->fbyte-n, buf, bsize, bi, f);
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

         u->realm = u->regname;
         while(*u->realm != '@')
            u->realm++;
         u->realm++;

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

		xlist_addto_last(user->profiles, u);
		nitem = 0;
      nprof--;
	}

   /* load phonebook */
   if(sipua_get_section("[phonebooks]", line, 128, buf, bsize, bi, f))
   {
	   len = sipua_get_line(line, 128, buf, bsize, bi, f);
      if(len > 7 && 0==strncmp(line, "number=", 7))
      {
         pc = line;
         while(*pc != '=')
				pc++;

			start = pc+1;

         nbook = strtol(start, &end, 10);
      }
   }

   pbk_log(("sipua_load_user_file_v2: %d books\n", nbook));

   while(nbook)
   {
      int ref;
		user_profile_t* prof;

      /* load phonebook */
      if(sipua_get_section("[contacts]", line, 128, buf, bsize, bi, f))
      {
         int ncontact = 0;
         
         len = sipua_get_line(line, 128, buf, bsize, bi, f);
         if(len < 12 || 0!=strncmp(line, "profile.ref=", 7))

         {
            nbook--;
            continue;
         }

         pc = line;
         while(*pc != '=')
		      pc++;

		   start = pc+1;

         ref = strtol(start, &end, 10);
            
         prof = (user_profile_t*)xlist_at(user->profiles, ref);
         if(!prof)
         {
            nbook--;
            continue;
         }

         pbk_log(("sipua_load_user_file_v2: phonebook[%s]\n", prof->regname));

         prof->phonebook = sipua_new_book(user);
         if(!prof->phonebook)
            return user;
         
         len = sipua_get_line(line, 128, buf, bsize, bi, f);
         if(len > 7 && 0==strncmp(line, "number=", 7))
         {
            pc = line;
            while(*pc != '=')
				   pc++;

			   start = pc+1;


            ncontact = strtol(start, &end, 10);
         }
         
         while(ncontact)
         {
		      sipua_contact_t* contact;

		      len = sipua_get_line(line, 128, buf, bsize, bi, f);
		      if(len < 0)
			      break;

		      pc = line;

		      if(len > 4 && 0==strncmp(pc, "name[", 4))
		      {
			      int n;

			      contact = xmalloc(sizeof(sipua_contact_t));
			      if(!contact)
				      return user;
                  
			      memset(contact, 0, sizeof(sipua_contact_t));

			      while(*pc != '[')
				      pc++;

			      start = pc+1;
			      contact->nbytes = strtol(start, &end, 10);
			      contact->name = xmalloc(contact->nbytes+1);

			      while(*end != '=')
				      end++;

			      start = end+1;

			      n = len-(start-line);
			      if(n >= contact->nbytes)
			      {
				      contact->name = xstr_nclone(start, contact->nbytes);
			      }
			      else
			      {
				      strncpy(contact->name, start, n);
				      len = sipua_load_nchar(contact->name+n, contact->nbytes-n, buf, bsize, bi, f);
			      }

			      contact->name[contact->nbytes] = '\0';
               pbk_log(("sipua_load_user_file_v2: contact.name[%s]\n", contact->name));


			      nitem++;
		      }
            else if(len > 4 && 0==strncmp(pc, "memo[", 4))
		      {
			      int n;

			      while(*pc != '[')
				      pc++;

			      start = pc+1;
			      contact->mbytes = strtol(start, &end, 10);
			      contact->memo = xmalloc(contact->mbytes+1);

			      while(*end != '=')
				      end++;

			      start = end+1;

			      n = len-(start-line);
			      if(n >= contact->mbytes)
			      {
				      contact->memo = xstr_nclone(start, contact->mbytes);
			      }
			      else
			      {
				      strncpy(contact->memo, start, n);

				      len = sipua_load_nchar(contact->memo+n, contact->mbytes-n, buf, bsize, bi, f);
			      }

			      contact->memo[contact->mbytes] = '\0';
               pbk_log(("sipua_load_user_file_v2: contact.memo[%s]\n", contact->memo));

			      nitem++;
		      }
		      else if(len > 9 && 0==strncmp(pc, "reg.name=", 9))
		      {
			      while(*pc != '=')
				      pc++;

			      start = pc+1;

			      contact->sip = xstr_clone(start);
               pbk_log(("sipua_load_user_file_v2: contact.sip[%s]\n", contact->sip));

			      nitem++;
		      }

		      if(nitem < 3)
			      continue;

		      xlist_addto_last(prof->phonebook->contacts, contact);
		      nitem = 0;
            ncontact--;
         }
      }

      nbook--;
   }

	return user;
}

user_t* sipua_load_user_file(FILE* f, const char* uid, const char* tok, int tsz)
{
	char buf[256];


	int bsize = 256;
	int bi;
   int ver;

	bi = sipua_verify_user_file(f, uid, tok, tsz, buf, &bsize, &ver);
	if(bi == -1)
		return NULL;

	if(ver == 1)
      return sipua_load_user_file_v1(f, uid, buf, &bsize, &bi);
      
	if(ver == 2)
      return sipua_load_user_file_v2(f, uid, buf, &bsize, &bi);
      
	return NULL;
}

user_t* sipua_load_user(const char* loc, const char *uid, const char* tok, int tsz)
{
	FILE* f;
	user_t *user;      

	pbk_log(("sipua_load_user: open '%s'\n", loc));
   

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
user_profile_t* user_add_profile(user_t* user, const char* fullname, int fbytes, const char* book_loc, const char* home, const char* regname, int sec)
{
	user_profile_t* prof = xmalloc(sizeof(user_profile_t));

	if(!prof)
   {
      pbk_log(("user_add_profile: no memory for profile"));

      return NULL;
   }

	memset(prof, 0, sizeof(user_profile_t));

	prof->user = user;

	prof->username = user->uid;

	prof->fullname = xstr_nclone(fullname, fbytes);
    
	prof->fbyte = fbytes;

	prof->registrar = xstr_clone(home);
	prof->regname = xstr_clone(regname);
	prof->seconds = sec;

	if(book_loc)
		prof->book_location = xstr_clone(book_loc);

	if(xlist_addto_last(user->profiles, prof) >= OS_OK)
		user->modified = 1;

   pbk_log(("user_add_profile: added\n"));

	return prof;
}

int user_set_profile(user_t* user, user_profile_t* prof, char* fullname, int fbytes, char* book_loc, char* home, char* regname, int sec)
{
	if(prof->fbyte < fbytes)
	{
		xstr_done_string(prof->fullname);
		prof->fullname = xstr_nclone(fullname, fbytes);
	}
	else
	{

		strncpy(prof->fullname, fullname, fbytes);

		prof->fullname[fbytes] = '\0';
	}
	
	prof->fbyte = fbytes;

	if(strlen(prof->registrar) < strlen(home))
	{
		xstr_done_string(prof->registrar);
		prof->registrar = xstr_clone(home);
	}
	else
	{
		strcpy(prof->registrar, home);
	}

	if(strlen(prof->regname) < strlen(regname))
	{
		xstr_done_string(prof->regname);
		prof->regname = xstr_clone(regname);
	}
	else
	{
		strcpy(prof->regname, regname);
	}

	prof->seconds = sec;

	if(book_loc)
	{
		if(strlen(prof->regname) < strlen(regname))
		{
			xstr_done_string(prof->book_location);
			prof->book_location = xstr_clone(book_loc);
		}
		else
		{
			strcpy(prof->book_location, book_loc);
		}
	}
		
	user->modified = 1;

	return UA_OK;
}

int user_remove_profile(user_t* user, user_profile_t* prof)
{
	if(xlist_remove_item(user->profiles, prof) < OS_OK)
		return -1;

	user_done_profile(prof);
	user->modified = 1;

	return xlist_size(user->profiles);
}

int user_remove_profile_by_number(user_t* user, int profile_no)
{
   user_profile_t *prof;

   prof = (user_profile_t*)xlist_at(user->profiles, profile_no);
   if(prof)
   {
      if(prof->online_status != SIPUA_STATUS_DISABLE)
         return UA_BUSY;

      if(user_remove_profile(user, prof) < 0)
         return UA_FAIL;

      return UA_OK;
   }

   return UA_FAIL;
}

int user_profile_number(user_t* user)
{
   return xlist_size(user->profiles);
}

int user_profile_index(user_t* user, const char* regname)

{
   xlist_user_t lu;
   int n = 0;
   
   user_profile_t *prof = (user_profile_t*)xlist_first(user->profiles, &lu);
   while(prof)
   {
      if(strcmp(regname, prof->regname) == 0)
         return n;
         
      n++;
      prof = (user_profile_t*)xlist_next(user->profiles, &lu);
   }

   return -1;
}

int user_profile(user_t* user, int num, char** fullname, int* fbytes, char** regname)
{
   user_profile_t *prof = (user_profile_t*)xlist_at(user->profiles, num);
   if(!prof)

   {
      *fullname = NULL;
      *fbytes = 0;
      *regname = NULL;

      pbk_log(("phonebook.user_profile: no profile found!\n"));
      return UA_INVALID;
   }

   *fullname = prof->fullname;
   *fbytes = prof->fbyte;
   *regname = prof->regname;

   return UA_OK;
}

int user_profile_enabled(user_t* user, int num)
{
   user_profile_t *prof = (user_profile_t*)xlist_at(user->profiles, num);
   if(!prof)
      return SIPUA_STATUS_DISABLE;

   return prof->online_status;
}


int user_profile_add_contact(user_t* user, int profile_no, const char* regname,
                              const char* name, int nbytes,
                              const char* memo, int mbytes)
{
   sipua_contact_t* contact;
   
   user_profile_t *prof = (user_profile_t*)xlist_at(user->profiles, profile_no);
   if(!prof)
      return UA_FAIL;

   contact = sipua_new_contact(name, nbytes, memo, mbytes, regname);
   
   if(!contact)
      return UA_FAIL;

   if(!prof->phonebook)
      prof->phonebook = sipua_new_book(user);
      
   if(!prof->phonebook || sipua_add_contact(prof->phonebook, contact) < 0)
      return UA_FAIL;

   return UA_OK;
}

int user_profile_delete_contact(user_t* user, int profile_index, int contact_index)
{
   user_profile_t *prof;

   prof = (user_profile_t*)xlist_at(user->profiles, profile_index);
   if(!prof)
   {
      pbk_log(("user_profile_delete_contact: no profile[#%i]\n", profile_index));
      return UA_FAIL;
   }
   
   if(!prof->phonebook || !prof->phonebook->contacts)
   {
      pbk_log(("user_profile_delete_contact: [%s] has no phonebook\n", prof->regname));
      return UA_OK;
   }

   if(xlist_delete_at(prof->phonebook->contacts, contact_index, sipua_done_contact) < OS_FAIL)
      return UA_FAIL;

   prof->phonebook->modified = 1;
   user->modified = 1;
   
   return UA_OK;
}

int user_profile_contact_number(user_t* user, int profile_no)
{
   user_profile_t *prof = (user_profile_t*)xlist_at(user->profiles, profile_no);

   if(!prof || !prof->phonebook)
      return 0;

   return xlist_size(prof->phonebook->contacts);
}

int user_profile_contact_index(user_t* user, int profile_no, const char* regname)
{
	sipua_contact_t *contact;
   xlist_user_t lu;
   int n = 0;

   user_profile_t *prof = (user_profile_t*)xlist_at(user->profiles, profile_no);
   if(!prof || !prof->phonebook)
      return -1;

   contact = (sipua_contact_t*)xlist_first(prof->phonebook->contacts, &lu);
   while(contact)
   {
      if(strcmp(regname, contact->sip) == 0)
         return n;

      n++;
      contact = (sipua_contact_t*)xlist_next(prof->phonebook->contacts, &lu);
   }

   return -1;
}

int user_profile_contact(user_t* user, int profile_no, int contact_no, char** regname, char** name, int* nbytes, char** memo, int* mbytes)
{
	sipua_contact_t *contact;
   user_profile_t *prof = (user_profile_t*)xlist_at(user->profiles, profile_no);
   if(!prof || !prof->phonebook)
      return UA_FAIL;

   contact = (sipua_contact_t*)xlist_at(prof->phonebook->contacts, contact_no);
   if(!contact)
      return UA_FAIL;

   *regname = contact->sip;
   *name = contact->name;
   *nbytes = contact->nbytes;
   *memo = contact->memo;
   *mbytes = contact->mbytes;

   return UA_OK;
}

int user_find_contact_index(user_t* user, const char* regname, int *profile_index, int *contact_index)
{
   int p_index = 0, c_index = 0;
   
   int nprof = user_profile_number(user);
   
   *profile_index = -1;
   *contact_index = -1;

   for(p_index=0; p_index<nprof; p_index++)
   {
       c_index = user_profile_contact_index(user, p_index, regname);
       if(c_index >= 0)
       {
          *profile_index = p_index;
          *contact_index = c_index;

          return UA_OK;
       }
   }
   
   return UA_FAIL;
}

int user_modified(user_t* user)
{
   return user->modified;
}

char* user_location(user_t* user)
{
   return user->userloc;
}

char* user_id(user_t* user)
{
   return user->uid;
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

user_t* user_new(const char* uid, int sz)
{
	user_t* u;
    
	u = xmalloc(sizeof(user_t));
	if(u)
	{
		memset(u, 0, sizeof(user_t));
        
		u->uid = xstr_nclone(uid, sz);
	}
   else
        printf("user_new: MALLOC ERROR!!");

	return u;
}

