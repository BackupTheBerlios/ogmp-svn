/***************************************************************************
                          xthread_sdl.c  -  SDL implementation
                             -------------------
    begin                : Tue Oct 28 2003
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
 
#include "../xthread.h"

#include <SDL/SDL_thread.h>
 
#ifdef XTHREAD_SDL_LOG
 #define xthr_log(fmtargs)  do{printf fmtargs;}while(0)
#else          
 #define xthr_log(fmtargs)  
#endif

struct xrtp_thread_s{
    SDL_Thread  *th;
};

struct xrtp_lock_s{
    SDL_mutex  *mutex;
};
 
struct  xthr_cond_s{
    SDL_cond *cond;
};

xrtp_thread_t * xthr_new(int (*fn)(void *), void *data, int flag){

    xrtp_thread_t *th = (xrtp_thread_t*)SDL_CreateThread(fn, data);

	return th;
}

int xthr_wait(xrtp_thread_t * anoth, void * ret_anoth){

    SDL_WaitThread((SDL_Thread*)anoth, (int*)ret_anoth);

    return OS_OK;
}

int xthr_cancel(xrtp_thread_t * th){

    SDL_KillThread((SDL_Thread*)th);

    return OS_OK;
}

int xthr_done(xrtp_thread_t * thr, void* *ret_th){

    SDL_KillThread((SDL_Thread*)thr);

    return OS_OK;
}

int xthr_notify(xrtp_thread_t * anoth, int signo){

    return OS_EUNSUP;
}

xthr_lock_t * xthr_new_lock(void){

    return (xthr_lock_t*)SDL_CreateMutex();
}

int xthr_done_lock(xthr_lock_t * lock){

    SDL_DestroyMutex((SDL_mutex*)lock);

    return OS_OK;
}

int xthr_lock(xthr_lock_t * lock){

    int r = SDL_LockMutex((SDL_mutex*)lock);
    
    if(r == 0) return OS_OK;
    
    return OS_FAIL;
}

int xthr_unlock(xthr_lock_t * lock){

    int r = SDL_UnlockMutex((SDL_mutex*)lock);

    if (r == 0) return OS_OK;
    
    return OS_FAIL;
}

xthr_cond_t * xthr_new_cond(int flag){

    return (xthr_cond_t*)SDL_CreateCond();
}

int xthr_done_cond(xthr_cond_t * cond){

    SDL_DestroyCond((SDL_cond*)cond);

    return OS_OK;
}
 
int xthr_cond_signal(xthr_cond_t * cond){

    if (SDL_CondSignal((SDL_cond*)cond) == -1) {
		
		xthr_log("xth_wait: SDL_CondSignal error\n");
		return OS_FAIL;
	}

	return OS_OK;
}

int xthr_cond_broadcast(xthr_cond_t * cond){

    if (SDL_CondBroadcast((SDL_cond*)cond) == -1) {
		
		xthr_log("xth_wait: SDL_CondBroadcast error\n");
		return OS_FAIL;
	}

	return OS_OK;
}

int xthr_cond_wait(xthr_cond_t * cond, xthr_lock_t * lock){

    if (SDL_CondWait((SDL_cond*)cond, (SDL_mutex*)lock) == -1) {
		
		xthr_log("xth_wait: SDL_CondWait error\n");
		return OS_FAIL;
	}

	return OS_OK;
}

/* The time unit is millisecond */
int xthr_cond_wait_millisec(xthr_cond_t * cond, xthr_lock_t * lock, uint32 millisec){

    return SDL_CondWaitTimeout((SDL_cond*)cond, (SDL_mutex*)lock, millisec);
}
