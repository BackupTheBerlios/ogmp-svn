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

typedef struct editline_s
{ 
	char *line;
	int max; 
	
	int pos;
	int num;  

} editline_t;

editline_t* editline_new(char* buf, int bsize);

int editline_done(editline_t* edl);

int editline_size(editline_t* edl);

int editline_set_line(editline_t* edl, char *ascii, int asize);

int editline_line(editline_t* edl, char *out, int osize);

int editline_set_pos(editline_t* edl, int pos);

int editline_move_pos(editline_t* edl, int move);

int editline_pos(editline_t* edl);

int editline_char(editline_t* edl, char** ch);

int editline_add(editline_t* edl, char* in, int isize);

int editline_append(editline_t* edl, char* c, int bytes);

int editline_remove_char(editline_t* edl);

int editline_clear(editline_t* edl);
