/***************************************************************************
                          ogmp_client.c - ogmp client starter
                             -------------------
    begin                : Tue Jul 20 2004
    copyright            : (C) 2004 by Heming
    email                : heming@bigfoot.com; heming@timedia.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 
#include "ogmp.h"

/* For standalone sipua */
int main(int argc, char** argv)
{
    sipua_t* sipua = NULL;
	sipua_uas_t* uas = NULL;
	module_catalog_t *mod_cata = NULL;

	int sip_port;

	if(argc < 2)
	{
		sip_port = 5060;
	}
	else
	{
		sip_port = strtol(argv[1], NULL, 10);
	}

	printf("main: sip port is %d\n", sip_port);

	printf("main: modules in dir:'%s'\n", MOD_DIR);

	mod_cata = catalog_new( "mediaformat" );

	catalog_scan_modules ( mod_cata, OGMP_VERSION, MOD_DIR );

	uas = client_new_uas(mod_cata, "eXosipua");
    if(!uas)
        printf("main: fail to create sipua server!\n");

	if(uas && uas->init(uas, sip_port, "IN", "IP4", NULL, NULL) >= UA_OK)
	{
		sipua = client_new("cursesui", uas, mod_cata, 64*1024);

        if(sipua)
            client_start(sipua);
        else
            printf("main: fail to create sipua!\n");
	}

	return 0;
}
