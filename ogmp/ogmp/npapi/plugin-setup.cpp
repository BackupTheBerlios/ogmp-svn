#include "plugin.h"

#define MIME_TYPES_HANDLED  "application/x-vnd-realmtel-sipua"
#define PLUGIN_NAME         "RealmTel SIP UA for Mozilla"
#define MIME_TYPES_DESCRIPTION  MIME_TYPES_HANDLED"::"PLUGIN_NAME
#define PLUGIN_DESCRIPTION  PLUGIN_NAME " (RealmTel Mozilla Phone)"

char *GetMIMEDescription()
{
    return(MIME_TYPES_DESCRIPTION);
}

NPError GetValue(NPPVariable aVariable, void *aValue)
{
	NPError err = NPERR_NO_ERROR;

	switch (aVariable)
	{
		case NPPVpluginNameString:
			*((char **)aValue) = PLUGIN_NAME;
			break;
		case NPPVpluginDescriptionString:
			*((char **)aValue) = PLUGIN_DESCRIPTION;
			break;
		default:
			err = NPERR_INVALID_PARAM;
			break;
	}

	return err;
}

void New(nsPluginInstance * instance, nsPluginCreateData * parameters)
{}

void LoadConfigFile(nsPluginInstance * instance)
{}
