/***************************************************************************
                          buffer.h  -  byte buffer module
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

 #ifndef OS_H
 #include "os.h"
 #endif
  
 enum byte_order_e{

    BIGEND = BIGEND_ORDER,
    LITTLEEND = LITTLEEND_ORDER,
    HOST_ORDER = HOST_BYTEORDER,
    NET_ORDER = NET_BYTEORDER
 };

 #define _buffer_swap16(x) \
        ((x) = ((x)<<8 | (x)>>8));

 #define _buffer_swap32(x) \
        ((x) = ((x)<<24) | (((x)<<8) & 0x00FF0000) | (((x)>>8) & 0x0000FF00) | ((x)>>24));

 typedef struct xrtp_buffer_s xrtp_buffer_t;
 struct xrtp_buffer_s{

    enum byte_order_e byte_order;
    int mounted;

    uint len;
    uint pos;
    uint len_data;
    
    char * data;
 };

 xrtp_buffer_t * buffer_new(uint size, enum byte_order_e order);

 int buffer_done(xrtp_buffer_t * buf);

 int buffer_mount(xrtp_buffer_t * buf, char * data, int len);

 char * buffer_umount(xrtp_buffer_t * buf, int *r_len);

 xrtp_buffer_t * buffer_clone(xrtp_buffer_t * buf);

 xrtp_buffer_t * buffer_newsize(xrtp_buffer_t * buf, uint size);

 char * buffer_data(xrtp_buffer_t * buf);

 uint buffer_maxlen(xrtp_buffer_t * buf);

 uint buffer_datalen(xrtp_buffer_t * buf);

 int buffer_copy(xrtp_buffer_t * src, uint pos1, uint len1, xrtp_buffer_t * des, uint pos2);

 int buffer_fill_value(xrtp_buffer_t * buf, char val, uint run);

 int buffer_fill_data(xrtp_buffer_t * buf, char *val, uint run);
 
 int buffer_add_data(xrtp_buffer_t * buf, char * data, uint len);

 int buffer_add_uint8(xrtp_buffer_t * buf, uint8 byte);

 int buffer_add_uint16(xrtp_buffer_t * buf, uint16 word);

 int buffer_add_uint32(xrtp_buffer_t * buf, uint32 dword);

 int buffer_add_int8(xrtp_buffer_t * buf, int8 byte);

 int buffer_add_int16(xrtp_buffer_t * buf, int16 word);

 int buffer_add_int32(xrtp_buffer_t * buf, int32 dword);

 int buffer_seek(xrtp_buffer_t * buf, uint pos);

 int buffer_skip(xrtp_buffer_t * buf, int step);

 void * buffer_address(xrtp_buffer_t * buf);
 
 uint buffer_position(xrtp_buffer_t * buf);
 
 int buffer_next_uint8(xrtp_buffer_t * buf, uint8 * ret);

 int buffer_next_uint16(xrtp_buffer_t * buf, uint16 * ret);

 int buffer_next_uint32(xrtp_buffer_t * buf, uint32 * ret);

 int buffer_next_int8(xrtp_buffer_t * buf, int8 * ret);

 int buffer_next_int16(xrtp_buffer_t * buf, int16 * ret);

 int buffer_next_int32(xrtp_buffer_t * buf, int32 * ret);

 int buffer_next_data(xrtp_buffer_t * buf, char * data, uint len);
