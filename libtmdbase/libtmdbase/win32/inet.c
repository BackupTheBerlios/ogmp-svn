/*
 * Timedia project
 * 
 * Structures and types used to implement inet_aton, etc.
 * on Windows 95/NT.
 */
#include "../inet.h"

int 
inet_aton(const char *cp, struct in_addr *inp)
{
	if(strcmp(cp, "255.255.255.255")){

		*((u_long*)inp) = (u_long)-1;
		return 1;
	}

	*((u_long*)inp) = inet_addr(cp);

	if(*((u_long*)inp) == INADDR_NONE) return 0;

	return 1;
}