/*
 * josua - Jack's open sip User Agent
 *
 * Copyright (C) 2002,2003   Aymeric Moizard <jack@atosc.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with dpkg; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>

#include <timedia/os.h>
#include <timedia/xmalloc.h>

#include "editor.h"

editline_t* editline_new(char* buf, int bsize)
{
	editline_t* edl = xmalloc(sizeof(editline_t));

	if(!edl) 
		return NULL;

	memset(edl, 0, sizeof(editline_t));

	edl->line = buf;
	edl->max = bsize;

	return edl;
}

int editline_done(editline_t* edl)
{
	xfree(edl);
	return OS_OK;
}

int editline_size(editline_t* edl)
{
	return edl->num+1;
}

int editline_set_line(editline_t* edl, char *ascii, int asize)
{
	int ncpy;

	int m = strlen(ascii);
	
	ncpy = edl->max < m+1 ? edl->max-1 : m;

	memcpy(edl->line, ascii, ncpy);
	edl->line[ncpy] = '\0';

	edl->pos = ncpy+1;
	edl->num = ncpy;

	return edl->num;
}

int editline_line(editline_t* edl, char *out, int osize)
{
	if(edl->num < osize)
	{
		memcpy(out, edl->line, edl->num);
		out[edl->num] = '\0';

		return edl->num;
	}

	memcpy(out, edl->line, osize-1);
	out[osize-1] = '\0';

	return osize;
}

int editline_set_pos(editline_t* edl, int pos)
{
	if(pos < 0 || pos > edl->num)
		return -1;
		
	edl->pos = pos;

	return edl->pos;
}

int editline_move_pos(editline_t* edl, int move)
{
	int pos = edl->pos + move;

	if(pos < 0 || pos > edl->num)
		return -1;
		
	edl->pos = pos;

	return edl->pos;
}

int editline_pos(editline_t* edl)
{
	if(edl->num == 0)
		return 0;

	return edl->pos;
}

int editline_char(editline_t* edl, char** ch)
{
	*ch = &edl->line[edl->pos];

	return 1; /* char bytes */
}

int editline_add(editline_t* edl, char* in, int isize)
{
	int i;

	if(edl->num == edl->max-1)
		return 0;

	if(edl->pos == edl->num)
	{
		edl->line[edl->num] = *in;
		edl->line[edl->num+1] = '\0';
	}
	else
	{
		for(i = edl->num+1; i >= edl->pos; i--)
			edl->line[i+1] = edl->line[i];

		edl->line[edl->pos] = *in;
	}

	return ++edl->num;
}

int editline_append(editline_t* edl, char* c, int bytes)
{
	int ret = editline_add(edl, c, bytes);

	if(ret > 0)
		edl->pos++;

	return ret;
}

int editline_remove_char(editline_t* edl)
{
	int i;

	if(edl->num == 0)
		return edl->num;

	if(edl->pos > edl->num)
		edl->pos = edl->num;
	
	if(edl->pos == edl->num)
	{
		edl->pos--;
		edl->num--;

		edl->line[edl->num] = '\0';
		return edl->num;
	}

	for(i = edl->pos; i < edl->num; i++)
		edl->line[i] = edl->line[i+1];

	return --edl->num;
}

int editline_clear(editline_t* edl)
{
	memset(edl->line, 0, edl->max);

	edl->num = 0;
	edl->pos = 0;

	return OS_OK;
}
