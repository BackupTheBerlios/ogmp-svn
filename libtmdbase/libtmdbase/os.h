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

/* Make sure the correct platform symbols are defined */
#if !defined(WIN32) && defined(_WIN32)
#define WIN32
#endif /* Windows */

#ifdef _MSC_VER
 typedef unsigned __int64  uint64;
 typedef __int64           int64;
 typedef long              off_t;
 typedef unsigned int   uint;
#else
 #include <sys/types.h>
 typedef unsigned long long  uint64;
 typedef long long int64;
#endif

#ifdef __MINGW32__
 typedef long              off_t;
#endif

typedef unsigned int   uint32;
typedef unsigned short uint16;
typedef unsigned char  uint8;

typedef int   int32;
typedef short int16;
typedef char  int8;

/* Some compilers use a special export keyword 
 * Copy from SDL (www.libsdl.org)
 */
#ifndef DECLSPEC
# ifdef __BEOS__
#  if defined(__GNUC__)
#   define DECLSPEC	__declspec(dllexport)
#  else
#   define DECLSPEC	__declspec(export)
#  endif
# else
# ifdef WIN32
#  ifdef __BORLANDC__
#   ifdef BUILD_SDL
#    define DECLSPEC 
#   else
#    define DECLSPEC __declspec(dllimport)
#   endif
#  else
#   define DECLSPEC	__declspec(dllexport)
#  endif
# else
#  define DECLSPEC
# endif
# endif
#endif

/* By default SDL uses the C calling convention */
#ifndef SDLCALL
#ifdef WIN32
#define SDLCALL __cdecl
#else
#define SDLCALL
#endif
#endif /* SDLCALL */

/* Removed DECLSPEC on Symbian OS because SDL cannot be a DLL in EPOC */
#ifdef __SYMBIAN32__ 
#undef DECLSPEC
#define DECLSPEC
#endif /* __SYMBIAN32__ */

#endif
