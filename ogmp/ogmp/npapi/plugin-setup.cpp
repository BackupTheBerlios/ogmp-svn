#include "plugin.h"

#ifndef STATICDECLS
#define STATICDECLS
int DEBUG = 0;
int enable_real;
int enable_qt;
int enable_wm;
int enable_mpeg;
int enable_ogg;
int enable_smil;

#define MAX_BUF_LEN 255
#define STATE_RESET 0
#define STATE_NEW 1
#define STATE_HAVEURL 3
#define STATE_WINDOWSET 4
#define STATE_READY 5
#define STATE_QUEUED 6
#define STATE_DOWNLOADING 7
#define STATE_DOWNLOADED_ENOUGH 8

#define STATE_CANCELLED 11

#define STATE_NEWINSTANCE 100
#define STATE_GETTING_PLAYLIST 110
#define STATE_STARTED_PLAYER 115
#define STATE_PLAYLIST_COMPLETE 120
#define STATE_PLAYLIST_NEXT 125
#define STATE_PLAYING 130
#define STATE_PLAY_COMPLETE 140
#define STATE_PLAY_CANCELLED 150

// speed options
#define SPEED_LOW 1
#define SPEED_MED 2
#define SPEED_HIGH 3

#endif

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
  switch (aVariable) {
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
{
}

void LoadConfigFile(nsPluginInstance * instance)
{
}
