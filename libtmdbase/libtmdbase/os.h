/***************************************************************************
                          const.h.h  -  description
                             -------------------
    begin                : Sat Mar 1 2003
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

#define OS_H

#define OS_ENABLE 1
#define OS_DISABLE 0

#define OS_YES 1
#define OS_NO 0

#define OS_OK 0

#define OS_CALL_UNSUPPORTED 0

#define OS_EPARAM -1
#define OS_EMEM   -2
#define OS_EFULL  -3
#define OS_EPERMIT -4
#define OS_EBUSY   -5
#define OS_EUNSUP  -6
#define OS_NULLPOINT -7
#define OS_EREFUSE   -8
#define OS_EFORMAT   -9
#define OS_EOS       -10
#define OS_INVALID   -11
#define OS_FAIL      -12
#define OS_ERANGE    -13
#define OS_EAGAIN    -14
#define OS_EINTR    -15     /* Operation interrupted */
#define OS_ETIMEOUT  -16    /* Timeout error */

#define LITTLEEND_ORDER 0
#define BIGEND_ORDER 1

#define HOST_BYTEORDER LITTLEEND_ORDER
#define NET_BYTEORDER BIGEND_ORDER


#ifdef _MSC_VER
 typedef unsigned __int64  uint64;
 typedef __int64           int64;
 typedef long              off_t;
#else
 typedef unsigned long long  uint64;
 typedef long long int64;
#endif

#ifdef __MINGW32__
 typedef long              off_t;
#endif

typedef unsigned int   uint32;
typedef unsigned short uint16;
typedef unsigned char  uint8;

typedef unsigned int   uint;

typedef int   int32;
typedef short int16;
typedef char  int8;

#endif
