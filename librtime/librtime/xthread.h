/***************************************************************************
                          xthread.h  -  Thread for xrtp
                             -------------------
    begin                : Wed Jun 25 2003
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

#ifndef XTHREAD_H

#define XTHREAD_H

#include <timedia/os.h>

#define XTHREAD_NONEFLAGS 0 
 
/* Need to change name, remove xrtp prefix */
typedef struct xrtp_thread_s xrtp_thread_t;
typedef struct xrtp_lock_s xthr_lock_t;
typedef struct xrtp_cond_s xthr_cond_t;

typedef struct xrtp_thread_s xthread_t;

extern DECLSPEC  xthread_t * xthr_new(int (*fn)(void *), void *, int flag);

extern DECLSPEC  int xthr_wait(xrtp_thread_t * anoth, void *ret_anoth);

extern DECLSPEC  int xthr_cancel(xrtp_thread_t * anoth);

extern DECLSPEC  int xthr_done(xrtp_thread_t * thr, void* *ret_th);

extern DECLSPEC  int xthr_notify(xrtp_thread_t * anoth, int signo);

extern DECLSPEC  xthr_lock_t * xthr_new_lock(void);

extern DECLSPEC  int xthr_done_lock(xthr_lock_t * lock);

extern DECLSPEC  int xthr_lock(xthr_lock_t * lock);

extern DECLSPEC  int xthr_unlock(xthr_lock_t * lock);

extern DECLSPEC  xthr_cond_t * xthr_new_cond(int flag);

extern DECLSPEC  int xthr_done_cond(xthr_cond_t * cond);

extern DECLSPEC  int xthr_cond_signal(xthr_cond_t * cond);

extern DECLSPEC  int xthr_cond_broadcast(xthr_cond_t * cond);

extern DECLSPEC  int xthr_cond_wait(xthr_cond_t * cond, xthr_lock_t * lock);

extern DECLSPEC  int xthr_cond_wait_timeout(xthr_cond_t * cond, xthr_lock_t * lock, uint32 time);

#endif
