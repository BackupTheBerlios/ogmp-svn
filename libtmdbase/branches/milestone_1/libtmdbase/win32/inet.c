/*
 * Timedia project
 * 
 * Structures and types used to implement inet_aton, etc.
 * on Windows 95/NT.
 */
#include "../inet.h"
#include "../socket.h"
#include <stdio.h>
/*
#define WINSOCK_LOG
#define WINSOCK_DEBUG
*/
#ifdef WINSOCK_LOG
 #define wsock_log(fmtargs) do{printf fmtargs;}while(0)
#else 
 #define wsock_log(fmtargs)
#endif

#ifdef WINSOCK_DEBUG
 #define wsock_debug(fmtargs) do{printf fmtargs;}while(0)
#else 
 #define wsock_debug(fmtargs)
#endif


int 
inet_aton(const char *cp, struct in_addr *inp)
{
	if(strcmp(cp, "255.255.255.255") == 0){

		wsock_log(("inet_aton: WARNING - you ask for a broadcast address!\n"));

		*((u_long*)inp) = (u_long)-1;

		return 1;
	}

	*((u_long*)inp) = inet_addr(cp);

	if(*((u_long*)inp) == INADDR_NONE){

		wsock_debug(("inet_aton: \"%s\" is not available\n"));
		return 0;
	}

	wsock_log(("inet_aton: \"%s\"=net[%d]\n", cp, *((u_long*)inp) ));

	return 1;
}

int 
socket_init()
{
	WORD wVersionRequested;
	WSADATA wsaData;

	int err;
 
	wVersionRequested = MAKEWORD( 1, 1 );
 
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {

		/* Tell the user that we could not find a usable */
		/* WinSock DLL. */
		wsock_debug(("socket_init: Winsock Fail!\n"));
		return OS_FAIL;
	}

	return OS_OK;
}