/***************************************************************************
                          buffer.c  -  byte buffer module
                             -------------------
    begin                : Tue Apr 22 2003
    copyright            : (C) 2003 by Heming Ling
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

 #include "buffer.h"
 #include "byteorder.h"
 
 #include <stdlib.h>
 #include <string.h>

 #include <stdio.h>
 
 #ifdef BUFFER_LOG
   const int buffer_log = 1;
 #else
   const int buffer_log = 0;
 #endif
 #define buffer_log(fmtargs)  do{if(buffer_log) printf fmtargs;}while(0)

 xrtp_buffer_t * buffer_new(uint size, enum byte_order_e order){

    xrtp_buffer_t * buf = (xrtp_buffer_t *)malloc(sizeof(struct xrtp_buffer_s) + sizeof(char) * size);

    if(!buf){

       return NULL;
    }

    buf->byte_order = order;
    buf->len = size;
    buf->pos = 0;
    buf->len_data = 0;
    buf->data = (char *)(&(buf->data) + 1);
    buf->mounted = 0;

    buffer_log(("buffer_new: %d bytes buffer@(%d)\n", buf->len, (int)buf));
    buffer_log(("buffer_new: buffer.data@(%d)\n\n", (int)(buf->data)));

    return buf;
 }

 int buffer_done(xrtp_buffer_t * buf){

    if(buf->mounted)
        free(buf->data);
            
    free(buf);

    return OS_OK;
 }

 int buffer_mount(xrtp_buffer_t * buf, char *data, int len){

    buf->len = buf->len_data = len;
    buf->data = data;
    buf->mounted = 1;
    
    buffer_log(("buffer_mount: %d bytes data[@%d] mounted\n", buf->len, (int)(buf->data)));
    
    return OS_OK;
 }

 char * buffer_umount(xrtp_buffer_t * buf, int *r_len){

    buf->len = buf->len_data = buf->pos = 0;
    buf->mounted = 0;
    
    *r_len = buf->len;
    
    return buf->data;
 }

 xrtp_buffer_t * buffer_clone(xrtp_buffer_t * buf){

    xrtp_buffer_t * newbuf = (xrtp_buffer_t *)malloc(sizeof(struct xrtp_buffer_s) + sizeof(char) * buf->len);
    if(!newbuf){

       return NULL;
    }
    
    newbuf->byte_order = buf->byte_order;
    newbuf->len = buf->len;
    newbuf->pos = 0;
    newbuf->mounted = 0;
    
    newbuf->len_data = buf->len_data;
    newbuf->data = (char *)(&(newbuf->data) + 1);

    memcpy(newbuf->data, buf->data, buf->len_data);
    buffer_log(("buffer_clone: new buffer[%u]\n", (int)newbuf));
    return newbuf;
 }

 xrtp_buffer_t * buffer_newsize(xrtp_buffer_t * buf, uint size){

    xrtp_buffer_t * newbuf = (xrtp_buffer_t *)malloc(sizeof(struct xrtp_buffer_s) + sizeof(char) * size);
    if(!newbuf){

       return NULL;
    }

    newbuf->byte_order = buf->byte_order;
    newbuf->len = size;
    newbuf->pos = 0;
    newbuf->len_data = 0;
    newbuf->data = (char *)(&(newbuf->data) + 1);

    buffer_log(("buffer_newsize: %d bytes buffer@(%d)\n", buf->len, (int)buf));
    buffer_log(("buffer_newsize: buffer.data@(%d)\n\n", (int)(buf->data)));

    return newbuf;
 }

 char * buffer_data(xrtp_buffer_t * buf){

    return buf->data;
 }

 uint buffer_maxlen(xrtp_buffer_t * buf){

    return buf->len;
 }

 uint buffer_datalen(xrtp_buffer_t * buf){

    return buf->len_data;
 }

 int buffer_copy(xrtp_buffer_t * src, uint pos1, uint len1, xrtp_buffer_t * des, uint pos2){

    if((pos1 + len1) > src->len || (pos2 + len1) > des->len){

       return OS_EREFUSE;
    }

    memcpy(&(des->data[pos2]), &(src->data[pos1]), len1);
    des->len_data = des->len_data > (pos2 + len1) ? des->len_data : (pos2 + len1);

    return OS_OK;
 }

 int buffer_add_data(xrtp_buffer_t * buf, char * data, uint len){

    if(buf->len_data + len > buf->len){

       return OS_EREFUSE;
    }

    memcpy((buf->data + buf->len_data), data, len);
    
    buffer_log(("buffer_add_data: Add %d bytes data to buf[%d-%d]\n", len, buf->len_data, (buf->len_data + len - 1)));

    buf->len_data += len;
    buf->pos += len;

    return OS_OK;
 }

 int buffer_fill_value(xrtp_buffer_t * buf, char val, uint run){

    if(buf->len_data + run > buf->len){

       return OS_EREFUSE;
    }

    memset((buf->data + buf->len_data), val, run);

    buffer_log(("buffer_fill_value: Fill %d '%d's data to buf[%d-%d]\n", run, val, buf->len_data, (buf->len_data + run - 1)));

    buf->len_data += run;
    buf->pos += run;

    return OS_OK;
 }

 int buffer_fill_data(xrtp_buffer_t * buf, char *data, uint len){

    if(!len) return OS_OK;

    if(buf->len_data + len > buf->len){

       return OS_EREFUSE;
    }

    memcpy((buf->data + buf->len_data), data, len);

    buf->len_data += len;
    buf->pos += len;

    return OS_OK;   
 }

 int buffer_add_uint8(xrtp_buffer_t * buf, uint8 byte){

    if(buf->len_data + sizeof(uint8) > buf->len){

       return OS_EREFUSE;
    }

    buf->data[buf->len_data] = byte;
    
    buffer_log(("buffer_add_uint8: Add (%d) to buf[%d]\n", byte, buf->pos));

    buf->len_data += sizeof(uint8);
    buf->pos += sizeof(uint8);

    return OS_OK;
 }

 int buffer_add_uint16(xrtp_buffer_t * buf, uint16 word){

    if(buf->len_data + sizeof(uint16) > buf->len){

       return OS_EREFUSE;
    }

    if(buf->byte_order == BIGEND_ORDER)
       RSSVAL(buf->data, buf->len_data, word);
    else
       SSVAL(buf->data, buf->len_data, word);
       
    buffer_log(("buffer_add_uint16: Add (%d) to buf[%d]@%d\n", word, buf->pos, (int)&(buf->data[buf->len_data])));

    buf->len_data += sizeof(uint16);
    buf->pos += sizeof(uint16);
    
    return OS_OK;
 }

 int buffer_add_uint32(xrtp_buffer_t * buf, uint32 dword){

    if(buf->len_data + sizeof(uint32) > buf->len){

       buffer_log(("buffer_add_uint32(): buffer.len(%d) full! Can't add (%d)", buf->len, dword));

       return OS_EREFUSE;
    }

    if(buf->byte_order == BIGEND_ORDER){
       
       buffer_log(("Report :)\tbuffer_add_uint32()\t{ Add Big end uint32 }\n"));
       RSIVAL(buf->data, buf->len_data, dword);
    }
    else{
       
       buffer_log(("buffer_add_uint32: Add Little end uint32\n"));
       SIVAL(buf->data, buf->len_data, dword);
    }
       
    buffer_log(("buffer_add_uint32: Add (%d) to buf[%d]@%d\n", dword, buf->pos, (int)&(buf->data[buf->len_data])));

    buf->len_data += sizeof(uint32);
    buf->pos += sizeof(uint32);

    return OS_OK;
 }

 int buffer_add_int8(xrtp_buffer_t * buf, int8 byte){

    if(buf->len_data + sizeof(int8) > buf->len){

       return OS_EREFUSE;
    }

    buf->data[buf->len_data] = byte;

    buffer_log(("buffer_add_int8: Add (%d) to buf[%d]\n", byte, buf->pos));

    buf->len_data += sizeof(int8);
    buf->pos += sizeof(int8);

    return OS_OK;
 }

 int buffer_add_int16(xrtp_buffer_t * buf, int16 word){

    if(buf->len_data + sizeof(int16) > buf->len){

       return OS_EREFUSE;
    }

    if(buf->byte_order == BIGEND_ORDER)
       RSSVALS(buf->data, buf->len_data, word);
    else
       SSVALS(buf->data, buf->len_data, word);

    buffer_log(("buffer_add_int16: Add (%d) to buf[%d]\n", word, buf->pos));

    buf->len_data += sizeof(int16);
    buf->pos += sizeof(int16);

    return OS_OK;
 }

 int buffer_add_int32(xrtp_buffer_t * buf, int32 dword){

    if(buf->len_data + sizeof(int32) > buf->len){

       return OS_EREFUSE;
    }

    if(buf->byte_order == BIGEND_ORDER)
       RSIVALS(buf->data, buf->len_data, dword);
    else
       SIVALS(buf->data, buf->len_data, dword);

    buffer_log(("buffer_add_int32: Add (%d) to buf[%d]\n", dword, buf->pos));

    buf->len_data += sizeof(int32);
    buf->pos += sizeof(int32);

    return OS_OK;
 }

 int buffer_seek(xrtp_buffer_t * buf, uint pos){

    buf->pos = pos;

    buffer_log(("buffer_seek: Buffer offset is %d\n", pos));

    return OS_OK;
 }

 int buffer_skip(xrtp_buffer_t * buf, int step){

    int pos = buf->pos + step;
    
    if(pos < 0 || pos > buf->len_data || pos >= buf->len){

       return 0;
    }

    buf->pos = pos;
    
    return step;
 }

 void * buffer_address(xrtp_buffer_t * buf){

    return buf->data + buf->pos;
 }

 uint buffer_position(xrtp_buffer_t * buf){

    return buf->pos;
 }

 int buffer_next_uint8(xrtp_buffer_t * buf, uint8 * ret){

    if(buf->pos + sizeof(uint8) > buf->len){

       return 0;
    }

    *ret = (uint8)buf->data[buf->pos];

    buffer_log(("buffer_next_uint8: (%d) at buf[%d]\n", *ret, buf->pos));

    buf->pos += sizeof(uint8);

    return sizeof(uint8);
 }

 int buffer_next_uint16(xrtp_buffer_t * buf, uint16 * ret){

    if(buf->pos + sizeof(uint16) > buf->len){

       return 0;
    }

    if(HOST_BYTEORDER == BIGEND_ORDER){

       *ret = RSVAL(buf->data, buf->pos);

       buffer_log(("buffer_next_uint16: bigend (%d) at buf[%d]@%d\n", *ret, buf->pos, (int)(buf->data+buf->pos)));
    }
    else{
       
       *ret = SVAL(buf->data, buf->pos);
       
       buffer_log(("buffer_next_uint16: smallend (%d) at buf[%d]@%d\n", *ret, buf->pos, (int)(buf->data+buf->pos)));
    }
    
    if(HOST_BYTEORDER != buf->byte_order){

       _buffer_swap16(*ret);

       buffer_log(("buffer_next_uint16: hostend (%d) at buf[%d]@%d\n", *ret, buf->pos, (int)(buf->data+buf->pos)));
    }
    
    buffer_log(("buffer_next_uint16: (%d) at buf[%d]@%d\n", *ret, buf->pos, (int)(buf->data+buf->pos)));
    
    buf->pos += sizeof(uint16);

    return sizeof(uint16);
 }

 int buffer_next_uint32(xrtp_buffer_t * buf, uint32 * ret){

    if(buf->pos + sizeof(uint32) > buf->len){

       return 0;
    }

    if(HOST_ORDER == BIGEND_ORDER){
       
       buffer_log(("buffer_next_uint32: Get Big end uint32\n"));
       *ret = RIVAL(buf->data, buf->pos);
    }
    else{
       
       buffer_log(("buffer_next_uint32: Get Little end uint32\n"));
       *ret = IVAL(buf->data, buf->pos);
    }
       
    if(HOST_ORDER != buf->byte_order){
       
       buffer_log(("buffer_next_uint32: Convert to host endian uint32\n"));
       _buffer_swap32(*ret);
    }

    buffer_log(("buffer_next_uint32: (%d) at buf[%d]@%d\n", *ret, buf->pos, (int)(buf->data+buf->pos)));

    buf->pos += sizeof(uint32);

    return sizeof(uint32);
 }

 int buffer_next_int8(xrtp_buffer_t * buf, int8 * ret){

    if(buf->pos + sizeof(int8) > buf->len){

       return 0;
    }

    *ret = (int8)buf->data[buf->pos];

    buffer_log(("buffer_next_int8: (%d) at buf[%d]\n", *ret, buf->pos));

    buf->pos += sizeof(int8);

    return sizeof(int8);
 }

 int buffer_next_int16(xrtp_buffer_t * buf, int16 * ret){

    if(buf->pos + sizeof(int16) > buf->len){

       return 0;
    }

    if(HOST_BYTEORDER == BIGEND_ORDER)
       *ret = RSVALS(&(buf->data), buf->pos);
    else
       *ret = SVALS(&(buf->data), buf->pos);

    buffer_log(("buffer_next_int16: (%d) at buf[%d]\n", *ret, buf->pos));

    buf->pos += sizeof(int16);

    return sizeof(int16);
 }

 int buffer_next_int32(xrtp_buffer_t * buf, int32 * ret){

    if(buf->pos + sizeof(int32) > buf->len){

       return 0;
    }

    if(HOST_BYTEORDER == BIGEND_ORDER)
       *ret = RIVALS(&(buf->data), buf->pos);
    else
       *ret = IVALS(&(buf->data), buf->pos);

    buffer_log(("buffer_next_int32: (%d) at buf[%d]\n", *ret, buf->pos));

    buf->pos += sizeof(int32);

    return sizeof(int32);
 }

 int buffer_next_data(xrtp_buffer_t * buf, char * data, uint len){

    if(buf->pos + len > buf->len){

       return buf->pos;
    }

    memcpy(data, buf->data + buf->pos, len);
    
    buffer_log(("buffer_next_data: (data) at buf[%d-%d]\n", buf->pos, (buf->pos + len - 1)));

    buf->pos += len;

    return len;
 }
