/***************************************************************************
                          spu_text.c  -  Text subtitle reader
                             -------------------
    begin                : Wed Nov 12 2003
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

/*
 * Most of the code is stealed from xine project - http://xinehq.de, Thanks ;)
 */

#include "spu_text.h"

#include <string.h>
#include <ctype.h>

#ifdef SPU_TEXT_LOG
   const int spu_text_log = 1;
#else
   const int spu_text_log = 0;
#endif
#define sputext_log(fmtargs)  do{if(spu_text_log) printf fmtargs;}while(0)

static int eol(char p) {
  return (p=='\r' || p=='\n' || p=='\0');
}

void trail_space(char *s) {
  int i;
  while (isspace(*s))
    strcpy(s, s + 1);
  i = strlen(s) - 1;
  while (i > 0 && isspace(s[i]))
    s[i--] = '\0';
}

/*
 * Read local file.
 */
static char *read_line(demux_sputext_t *this, char *line, off_t len) {

  off_t nread = 0;
  char *s;
  int linelen;

  if ((len - this->buflen) >512)
    s = fgets(&this->buf[this->buflen], len - this->buflen, this->file);

  if(!s)
    return NULL;
    
  nread = strlen(s);
  this->buflen += nread;

  s = strstr(this->buf, "\n");
  if (line && (s || this->buflen)) {

    linelen = s ? (s - this->buf) + 1 : this->buflen;

    memcpy(line, this->buf, linelen);
    line[linelen] = '\0';

    memmove(this->buf, &this->buf[linelen], SUB_BUFSIZE - linelen);
    this->buflen -= linelen;
    //printf("line read is --->%s\n", line);
    return line;
  }

  return NULL;
}

char *sub_readtext(char *source, char **dest) {
  int len=0;
  char *p=source;

  while ( !eol(*p) && *p!= '|' ) {
    p++,len++;
  }

  *dest= (char *)malloc(sizeof(char)*(len+1));
  if (!dest)
    return ERR;

  strncpy(*dest, source, len);
  (*dest)[len]=0;

  while (*p=='\r' || *p=='\n' || *p=='|')
    p++;

  if (*p)  return p;  /* not-last text field */
  else return NULL;   /* last text field     */
}

/* parsing the subrip format */
static subtitle_t *sub_read_line_subrip(demux_sputext_t *this, subtitle_t *current) {
  char line[1001];
  int a1,a2,a3,a4,b1,b2,b3,b4;
  char *p=NULL;
  int i,len;

  memset(current, 0 , sizeof(subtitle_t));

  while (!current->text[0]) {
    
    if (!read_line(this, line, 1000))  return NULL;
    
    if ((len=sscanf (line, "%d:%d:%d,%d --> %d:%d:%d,%d",&a1,&a2,&a3,&a4,&b1,&b2,&b3,&b4)) < 8)
      continue;
      
    current->start = a1*3600000+a2*60000+a3*1000+a4;   /* to millisec */
    current->end   = b1*3600000+b2*60000+b3*1000+b4;
    
    for (i=0; i<SUB_MAX_TEXT;) {
      
      if (!read_line(this, line, 1000)) break;
      
      len=0;
      for (p=line; *p!='\n' && *p!='\r' && *p; p++,len++);
      
      if (len) {
        
        current->text[i]=(char *)malloc(sizeof(char)*(len+1));
        if (!current->text[i]) return ERR;
        strncpy (current->text[i], line, len); current->text[i][len]='\0';
        i++;
        
      } else {
        
        break;
      }
    }
    
    current->lines=i;
  }

  return current;
}

static int sub_autodetect (demux_sputext_t *this) {

  char line[1001];
  int i,j=0;
  /*char p;*/

  while (j < 100) {
    j++;
    if (!read_line(this, line, 1000))
      return -1;
    /*
    if ((sscanf (line, "{%d}{}", &i)==1) ||
        (sscanf (line, "{%d}{%d}", &i, &i)==2)) {
      this->uses_time=0;
      sputext_log ("demux_sputext: microdvd subtitle format detected\n");
      return FORMAT_MICRODVD;
    }
    */
    if (sscanf (line, "%d:%d:%d,%d --> %d:%d:%d,%d", &i, &i, &i, &i, &i, &i, &i, &i)==8) {
      this->uses_time=1;
      sputext_log (("demux_sputext: subrip subtitle format detected\n"));
      return FORMAT_SUBRIP;
    }
    /*
    if (sscanf (line, "%d:%d:%d.%d,%d:%d:%d.%d",     &i, &i, &i, &i, &i, &i, &i, &i)==8){
      this->uses_time=1;
      sputext_log ("demux_sputext: subviewer subtitle format detected\n");
      return FORMAT_SUBVIEWER;
    }

    if (strstr (line, "<SAMI>")) {
      this->uses_time=1;
      sputext_log ("demux_sputext: sami subtitle format detected\n");
      return FORMAT_SAMI;
    }
    if (sscanf (line, "%d:%d:%d:",     &i, &i, &i )==3) {
      this->uses_time=1;
      sputext_log ("demux_sputext: vplayer subtitle format detected\n");
      return FORMAT_VPLAYER;
    }*/
    /*
     * A RealText format is a markup language, starts with <window> tag,
     * options (behaviour modifiers) are possible.
     */
    /*
    if ( !strcasecmp(line, "<window") ) {
      this->uses_time=1;
      sputext_log ("demux_sputext: rt subtitle format detected\n");
      return FORMAT_RT;
    }
    if (!memcmp(line, "Dialogue: Marked", 16)){
      this->uses_time=1;
      sputext_log ("demux_sputext: ssa subtitle format detected\n");
      return FORMAT_SSA;
    }
    if (sscanf (line, "%d,%d,\"%c", &i, &i, (char *) &i) == 3) {
      this->uses_time=0;
      sputext_log ("demux_sputext: (dunno) subtitle format detected\n");
      return FORMAT_DUNNO;
    }
    if (sscanf (line, "FORMAT=%d", &i) == 1) {
      this->uses_time=0;
      sputext_log ("demux_sputext: mpsub subtitle format detected\n");
      return FORMAT_MPSUB;
    }
    if (sscanf (line, "FORMAT=TIM%c", &p)==1 && p=='E') {
      this->uses_time=1;
      sputext_log ("demux_sputext: mpsub subtitle format detected\n");
      return FORMAT_MPSUB;
    }
    if (strstr (line, "-->>")) {
      this->uses_time=0;
      sputext_log ("demux_sputext: aqtitle subtitle format detected\n");
      return FORMAT_AQTITLE;
    }
    */
  }

  return -1;  /* too many bad lines */
}

static subtitle_t *sub_read_file (demux_sputext_t *this, FILE * file) {

  int n_max;
  
  subtitle_t *first;
  
  subtitle_t * (*func[])(demux_sputext_t *this,subtitle_t *dest)=
  {
    sub_read_line_subrip,
    /*
    sub_read_line_microdvd,
    sub_read_line_subviewer,
    sub_read_line_sami,
    sub_read_line_vplayer,
    sub_read_line_rt,
    sub_read_line_ssa,
    sub_read_line_dunnowhat,
    sub_read_line_mpsub,
    sub_read_line_aqt
    */
  };

  int i, l;

  if(!file){

     this->cur = 0;
     return this->subtitles;
  }
  
  if(this->num){
    
    for (i = 0; i < this->num; i++) {
      for (l = 0; l < this->subtitles[i].lines; l++)
        free(this->subtitles[i].text[l]);
    }

    free(this->subtitles);
  }
  
  this->file = file;

  /* Rewind (sub_autodetect() needs to read input from the beginning) */
  fseek(this->file, 0, SEEK_SET);
  this->buflen = 0;

  this->format=sub_autodetect (this);
  if (this->format==-1) {
    sputext_log (("demux_sputext: Could not determine file format\n"));
    return NULL;
  }

  sputext_log (("demux_sputext: Detected subtitle file format: %d\n",this->format));

  /* Rewind */
  fseek(this->file, 0, SEEK_SET);
  this->buflen = 0;

  this->num=0; n_max=32;
  
  first = (subtitle_t *)malloc(sizeof(subtitle_t)*n_max);
  if(!first) return NULL;

  while(1){
    subtitle_t *sub;

    if(this->num>=n_max){
      
      n_max+=16;
      first=realloc(first,n_max*sizeof(subtitle_t));
    }

    sub = func[this->format] (this, &first[this->num]);

    if (!sub)
      break;   /* EOF */

    if (sub==ERR){
    
      ++this->errs;
      
    }else {
      
      if (this->num > 0 && first[this->num-1].end == -1) {
        first[this->num-1].end = sub->start;
      }
      
      ++this->num; /* Error vs. Valid */
    }
  }

  sputext_log (("demux_sputext: Read %i subtitles", this->num));
  
  if (this->errs){
    
    sputext_log ((", %i bad line(s).\n", this->errs));
    
  }else{
    
    sputext_log ((".\n"));
  }
  
  return first;
}

void demux_sputext_show_title(subtitle_t * subt){

  int s1,s2,s3,s4,e1,e2,e3,e4;
  int r; 
  int i;

  s1 = subt->start / 3600000;  /* millisec per hour */
  r = subt->start % 3600000;
  s2 = r / 60000;			   /* millisec per minute */
  r = r % 60000;
  s3 = r / 1000;
  r = r % 1000;
  s4 = r;

  e1 = subt->end / 3600000;
  r = subt->end % 3600000;
  e2 = r / 60000;
  r = r % 60000;
  e3 = r / 1000;
  r = r % 1000;
  e4 = r;

  sputext_log(("demux_sputext_show_title: Timestamp[%d:%d:%d,%d --> %d:%d:%d,%d]\n", s1,s2,s3,s4,e1,e2,e3,e4));
  
  for(i=0; i<subt->lines; i++)
    sputext_log(("demux_sputext_show_title: Subtitle[%s]\n", subt->text[i]));

  return;
}

subtitle_t * demux_sputext_next_subtitle(demux_sputext_t *this) {
  
  if (this->cur >= this->num) return NULL;

  return &this->subtitles[this->cur++];
}

void demux_sputext_done (demux_sputext_t *this) {
  
  int i, l;

  for (i = 0; i < this->num; i++) {
    for (l = 0; l < this->subtitles[i].lines; l++)
      free(this->subtitles[i].text[l]);
  }
  
  free(this->subtitles);
  free(this);
}

int demux_sputext_get_stream_length (demux_sputext_t *this) {
  
  if( this->uses_time && this->num ) {
    return this->subtitles[this->num-1].end * 10;
  } else {
    return 0;
  }
}

int demux_sputext_rewind(demux_sputext_t *this) {
  
  this->cur = 0;

  return 0;
}

int demux_sputext_seek(demux_sputext_t *this, media_time_t time){

   subtitle_t *subt;
   
   this->cur = 0;
   subt = &this->subtitles[this->cur];
   while(subt->start < time)
      subt = &this->subtitles[this->cur++];

   return OS_OK;
}

demux_sputext_t *demux_sputext_new (FILE * file) {

  demux_sputext_t       *this;

  this = malloc (sizeof (demux_sputext_t));
  memset(this, 0, sizeof (demux_sputext_t));
  this->buflen = 0;

  this->subtitles = sub_read_file (this, file);

  if (this->subtitles) {

     sputext_log (("demux_sputext: subtitle format %s time.\n", this->uses_time?"uses":"doesn't use"));
     sputext_log (("demux_sputext: read %i subtitles, %i errors.\n", this->num, this->errs));
  }

  return this;
}
