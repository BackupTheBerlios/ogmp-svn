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

#define MIME_TYPES_HANDLED  "application/simple-plugin"
#define PLUGIN_NAME         "Simple Plugin Example for Mozilla"
#define MIME_TYPES_DESCRIPTION  MIME_TYPES_HANDLED"::"PLUGIN_NAME
#define PLUGIN_DESCRIPTION  PLUGIN_NAME " (Plug-ins SDK sample)"

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
#if 0
    int i;
    char parse[1000];
    char *cp;

    if (DEBUG)
		printf("mimetype: %s\n", parameters->type);

    instance->mode = parameters->mode;
	 
    instance->mInstance = parameters->instance;
    instance->mimetype = strdup(parameters->type);

    if (parameters->mode == NP_EMBED) {

		if (DEBUG)
	    	printf("Embedded mode\n");

		for (i = 0; i < parameters->argc; i++) {

	   	if (DEBUG) {
				printf("Argument Name: %s\n", parameters->argn[i]);
				printf("Argument Value: %s\n", parameters->argv[i]);
	   	}

	   	if (strncasecmp(parameters->argn[i], "src", 3) == 0) {

				instance->url = strdup(parameters->argv[i]);

				if (strncasecmp(parameters->argv[i], "file://", 7) == 0)
		    		fullyQualifyURL(instance, parameters->argv[i], instance->url);

				instance->state = STATE_HAVEURL;
	   	}

	   	if (strncasecmp(parameters->argn[i], "type", 4) == 0) {

				if (instance->mimetype != NULL)
		    		free(instance->mimetype);

				instance->mimetype = strdup(parameters->argv[i]);
	   	}

	   	if ((strncasecmp(parameters->argn[i], "filename", 8) == 0)
				|| (strncasecmp(parameters->argn[i], "url", 3) == 0)
				|| (strncasecmp(parameters->argn[i], "location", 8) == 0)) {

				instance->fname = strdup(parameters->argv[i]);

				if (strncasecmp(parameters->argv[i], "file://", 7) == 0)
		    		fullyQualifyURL(instance, parameters->argv[i], instance->fname);

				instance->state = STATE_HAVEURL;
			}

			if ((strncasecmp(parameters->argn[i], "href", 4) == 0)
				|| (strncasecmp(parameters->argn[i], "qtsrc", 5) == 0)) {

				instance->href = strdup(parameters->argv[i]);

				if (strncasecmp(parameters->argv[i], "file://", 7) == 0)
		    		fullyQualifyURL(instance, parameters->argv[i], instance->href);

				instance->state = STATE_HAVEURL;
			}

			if (strncasecmp(parameters->argn[i], "height", 6) == 0) {

				sscanf(parameters->argv[i], "%i", &instance->embed_height);
	   	}

			if (strncasecmp(parameters->argn[i], "width", 5) == 0) {

				sscanf(parameters->argv[i], "%i", &instance->embed_width);
			}

			if (strncasecmp(parameters->argn[i], "hidden", 6) == 0) {

				lowercase(parameters->argv[i]);

				if (DEBUG)
		    		printf("argv[i]=%s\n", parameters->argv[i]);

				if (strstr(parameters->argv[i], "true")
					|| strstr(parameters->argv[i], "yes")
					|| strstr(parameters->argv[i], "1")) {

					instance->hidden = 1;

				} else {

					instance->hidden = 0;
				}

				if (DEBUG)
					printf("hidden=%i\n", instance->hidden);
			}

			if (strncasecmp(parameters->argn[i], "target", 6) == 0) {

				if (strncasecmp(parameters->argv[i], "quicktimeplayer", 15) == 0) {

					nstance->noembed = 1;
				}
			}

			/* handle 'scale' attribute used by QT instance */
			if (strncasecmp(parameters->argn[i], "scale", 5) == 0) {

				if (strncasecmp(parameters->argv[i], "aspect", 6) == 0) {

					instance->maintain_aspect = 1;
				}
			}

			if ((strncasecmp(parameters->argn[i], "loop", 4) == 0)
				|| (strncasecmp(parameters->argn[i], "autorewind", 10) == 0)) {

				lowercase(parameters->argv[i]);

				if (DEBUG)
					printf("argv[i]=%s\n", parameters->argv[i]);

				if (strstr(parameters->argv[i], "true")
					|| strstr(parameters->argv[i], "yes")) {

					instance->loop = 1;

				} else {

					instance->loop = 0;
				}

				if (DEBUG)
					printf("loop=%i\n", instance->loop);
			}

			if ((strncasecmp(parameters->argn[i], "autostart", 9) == 0)
				|| (strncasecmp(parameters->argn[i], "autoplay", 8) == 0)) {

				lowercase(parameters->argv[i]);

				if (DEBUG)
					printf("argv[i]=%s\n", parameters->argv[i]);

				if (strstr(parameters->argv[i], "true")
					|| strstr(parameters->argv[i], "yes")
					|| strstr(parameters->argv[i], "1")) {

					instance->autostart = 1;

				} else {

					instance->autostart = 0;
				}

				if (DEBUG)
					printf("autostart=%i\n", instance->autostart);
			}

			if ((strncasecmp(parameters->argn[i], "showcontrols", 12) == 0)
				|| ((strncasecmp(parameters->argn[i], "controls", 8) == 0)
				&& (strstr(instance->mimetype, "quicktime") != NULL))) {

				lowercase(parameters->argv[i]);

				if (DEBUG)
					printf("argv[i]=%s\n", parameters->argv[i]);

				if (strstr(parameters->argv[i], "true")
					|| strstr(parameters->argv[i], "yes")
					|| strstr(parameters->argv[i], "1")) {

					instance->showcontrols = 1;

				} else {

					instance->showcontrols = 0;
				}

				if (DEBUG)
					printf("showcontrols=%i\n", instance->showcontrols);
			}

			if (strncasecmp(parameters->argn[i], "showtracker", 11) == 0) {

				lowercase(parameters->argv[i]);

				if (DEBUG)
					printf("argv[i]=%s\n", parameters->argv[i]);

				if (strstr(parameters->argv[i], "true")
					|| strstr(parameters->argv[i], "yes")
					|| strstr(parameters->argv[i], "1")) {

					instance->showtracker = 1;

				} else {

					instance->showtracker = 0;
				}

				if (DEBUG)
					printf("showtracker=%i\n", instance->showtracker);
	   	}

			if (strncasecmp(parameters->argn[i], "showbuttons", 11) == 0) {

				lowercase(parameters->argv[i]);

				if (DEBUG)
					printf("argv[i]=%s\n", parameters->argv[i]);

				if (strstr(parameters->argv[i], "true")
					|| strstr(parameters->argv[i], "yes")
					|| strstr(parameters->argv[i], "1")) {

					instance->showbuttons = 1;

				} else {

					instance->showbuttons = 0;
				}

				if (DEBUG)
					printf("showbuttons=%i\n", instance->showbuttons);
	   	}

			if (strncasecmp(parameters->argn[i], "showfsbutton", 12) == 0) {

				lowercase(parameters->argv[i]);

				if (DEBUG)
					intf("argv[i]=%s\n", parameters->argv[i]);

				if (strstr(parameters->argv[i], "true")
					|| strstr(parameters->argv[i], "yes")
					|| strstr(parameters->argv[i], "1")) {

					instance->showfsbutton = 1;

				} else {

					instance->showfsbutton = 0;
				}

				if (DEBUG)
					printf("showfsbutton=%i\n", instance->showfsbutton);
			}

	   	if (strncasecmp(parameters->argn[i], "showlogo", 8) == 0) {

				lowercase(parameters->argv[i]);

				if (DEBUG)
					printf("argv[i]=%s\n", parameters->argv[i]);

				if (strstr(parameters->argv[i], "false")
					|| strstr(parameters->argv[i], "no")
					|| strstr(parameters->argv[i], "0")) {

					instance->showlogo = 0;

				} else {

					instance->showlogo = 1;
				}

				if (DEBUG)
					printf("showlogo=%i\n", instance->showlogo);
			}

			if ((strncasecmp(parameters->argn[i], "controls", 8) == 0)
				&& (strstr(instance->mimetype, "quicktime") == NULL)) {

				lowercase(parameters->argv[i]);

				if (DEBUG)
					printf("argv[i]=%s\n", parameters->argv[i]);

				if (strstr(parameters->argv[i], "true")
					|| strstr(parameters->argv[i], "yes")
					|| strstr(parameters->argv[i], "1")
					|| strstr(parameters->argv[i], "all")
					|| strstr(parameters->argv[i], "statusfield")
					|| strstr(parameters->argv[i], "statusbar")
					|| strstr(parameters->argv[i], "statuspanel")
					|| strstr(parameters->argv[i], "playbutton")
					|| strstr(parameters->argv[i], "volumeslider")
					|| strstr(parameters->argv[i], "stopbutton")
					|| strstr(parameters->argv[i], "positionslider")
					|| strstr(parameters->argv[i], "controlpanel")) {

						instance->controlwindow = 1;

				} else {

					instance->controlwindow = 0;
				}

				if (strstr(parameters->argv[i], "imagewindow")) {

					instance->controlwindow = 0;
				}

				if (DEBUG)
		   		printf("controlwindow=%i\n", instance->controlwindow);
			}

			if ((strncasecmp(parameters->argn[i], "onmediacomplete", 15) == 0)
				|| (strncasecmp(parameters->argn[i], "onendofstream", 13) == 0)) {

				instance->mediaCompleteCallback = (char *) NPN_MemAlloc(strlen(parameters->argv[i]) + 12);

				if (strncasecmp(parameters->argv[i], "javascript:", 11) == 0) {

					snprintf(instance->mediaCompleteCallback, strlen(parameters->argv[i]),
									"%s", parameters->argv[i]);

				} else {

					snprintf(instance->mediaCompleteCallback,
			     			strlen(parameters->argv[i]) + 12,
			     			"javascript:%s", parameters->argv[i]);
				}

				if (DEBUG)
		    		printf("mediaCompleteCallback=%s\n", instance->mediaCompleteCallback);
	   	}

			if (instance->nQtNext < 256
				&& (strncasecmp(parameters->argn[i], "qtnext", 6) == 0)
				&& parameters->argv[i][0] == '<') {

				snprintf(parse, 1000, "%s", strtok(&parameters->argv[i][1], ">"));

				if ((cp = strchr(parse, ' ')) && strlen(parse) == (unsigned int) (cp - parse + 1))
					*cp = (char) NULL;

				instance->qtNext[instance->nQtNext++] = strdup(parse);
				snprintf(parse, 1000, "%s", strtok(NULL, "<"));

				if (strcmp(parse, "T")) {

					if (DEBUG)
						printf("qtNext%i expected \"T\" found \"%s\"\n",

					instance->nQtNext, parse);
					instance->nQtNext--;

				} else {

					snprintf(parse, 1000, "%s", strtok(NULL, ">\n"));

					if (strcmp(parse, "myself")) {

						if (DEBUG)
			    			printf("qtNext%i expected \"myself\" found \"%s\"\n",

						instance->nQtNext, parse);
						instance->nQtNext--;

		   		} else if (DEBUG)
						printf("qtNext%i=%s\n",instance->nQtNext,

					instance->qtNext[instance->nQtNext - 1]);
				}
			}
		}

		if (instance->controlwindow == 0) {

			if ((instance->fname != NULL) && (!isMms(instance->fname))) {

				NPN_GetURL(parameters->instance, instance->fname, NULL);
	   	}
		}
	} else {

		if (DEBUG)
	   	printf("New, full mode %i\n", instance->mode);
   }

   return;
#endif
}

void LoadConfigFile(nsPluginInstance * instance)
{
#if 0
    FILE *config;
    char buffer[1000];
    char parse[1000];

    // load config file

    config = NULL;

    if (config == NULL) {
	snprintf(buffer, 1000, "%s", getenv("HOME"));
	strlcat(buffer, "/.mplayer/mplayerplug-in.conf", 1000);
	config = fopen(buffer, "r");
    }

    if (config == NULL) {
	snprintf(buffer, 1000, "%s", getenv("HOME"));
	strlcat(buffer, "/.mozilla/mplayerplug-in.conf", 1000);
	config = fopen(buffer, "r");
    }

    if (config == NULL) {
	config = fopen("/etc/mplayerplug-in.conf", "r");
    }

    if (config == NULL) {
	// no config file
    } else {
	while (fgets(buffer, sizeof(buffer), config) != NULL) {
	    if ((strncasecmp(buffer, "cachesize", 9) == 0)
		|| (strncasecmp(buffer, "cachemin", 8) == 0)) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->cachesize);
		if (instance->cachesize < 0)
		    instance->cachesize = 0;
		if (instance->cachesize > 65535)
		    instance->cachesize = 65535;
		continue;
	    }

	    if (strncasecmp(buffer, "debug", 5) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &DEBUG);
//              if (DEBUG != 0)
//                  DEBUG = 1;
		continue;
	    }

	    if (strncasecmp(buffer, "novop", 5) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->novop);
		if (instance->novop != 0)
		    instance->novop = 1;
		continue;
	    }

	    if (strncasecmp(buffer, "noembed", 7) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->noembed);
		if (instance->noembed != 0)
		    instance->noembed = 1;
		continue;
	    }

	    if (strncasecmp(buffer, "nomediacache", 12) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->nomediacache);
		if (instance->nomediacache != 0)
		    instance->nomediacache = 1;
		continue;
	    }


	    if (strncasecmp(buffer, "vopopt", 6) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "\n"));
		instance->novop = 0;
		instance->vop = strdup(parse);
		continue;
	    }

	    if (strncasecmp(buffer, "prefer-aspect", 13) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->maintain_aspect);
		if (instance->maintain_aspect != 0)
		    instance->maintain_aspect = 1;
		continue;
	    }

	    if (strncasecmp(buffer, "rtsp-use-tcp", 12) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->rtsp_use_tcp);
		if (instance->rtsp_use_tcp != 0)
		    instance->rtsp_use_tcp = 1;
		continue;
	    }

	    if (strncasecmp(buffer, "qt-speed", 8) == 0) {
		sprintf(parse, "%s", strtok(buffer, "="));
		sprintf(parse, "%s", strtok(NULL, "="));
		if (strncasecmp(parse, "low", 3) == 0)
		    instance->qt_speed = SPEED_LOW;
		if (strncasecmp(parse, "medium", 6) == 0)
		    instance->qt_speed = SPEED_MED;
		if (strncasecmp(parse, "high", 4) == 0)
		    instance->qt_speed = SPEED_HIGH;
		if (DEBUG)
		    printf("QT Speed: %i\n", instance->qt_speed);
		continue;
	    }

	    if (strncasecmp(buffer, "vo", 2) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "=\n"));
		instance->vo = strdup(parse);
		continue;
	    }

	    if (strncasecmp(buffer, "ao", 2) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "\n"));
		instance->ao = strdup(parse);
		continue;
	    }

	    if (strncasecmp(buffer, "display", 7) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "=\n"));
		instance->output_display = strdup(parse);
		continue;
	    }

	    if (strncasecmp(buffer, "dload-dir", 9) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "=\n"));
		if (strstr(parse, "$HOME") != NULL) {
		    snprintf(buffer, sizeof(buffer), "%s%s",
			     getenv("HOME"), parse + 5);
		    strlcpy(parse, buffer, sizeof(parse));
		}
		if (instance->download_dir != NULL)
		    free(instance->download_dir);
		instance->download_dir = strdup(parse);
		continue;
	    }

	    if (strncasecmp(buffer, "keep-download", 13) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->keep_download);
		if (instance->keep_download != 0)
		    instance->keep_download = 1;
		continue;
	    }

	    if (strncasecmp(buffer, "framedrop", 9) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->framedrop);
		if (instance->framedrop != 0)
		    instance->framedrop = 1;
		continue;
	    }

	    if (strncasecmp(buffer, "autosync", 8) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->autosync);
		if (instance->autosync < 0)
		    instance->autosync = 0;
		continue;
	    }

	    if (strncasecmp(buffer, "mc", 2) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->mc);
		if (instance->mc < 0)
		    instance->mc = 0;
		continue;
	    }

	    if (strncasecmp(buffer, "black-background", 16) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->black_background);
		if (instance->black_background != 0)
		    instance->black_background = 1;
		continue;
	    }

	    if (strncasecmp(buffer, "osdlevel", 8) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->osdlevel);
		if (instance->osdlevel < 0)
		    instance->osdlevel = 0;
		if (instance->osdlevel > 3)
		    instance->osdlevel = 3;
	    }

	    if (strncasecmp(buffer, "cache-percent", 13) == 0) {
		snprintf(parse, 1000, "%s", strtok(buffer, "="));
		snprintf(parse, 1000, "%s", strtok(NULL, "="));
		sscanf(parse, "%i", &instance->cache_percent);
		if (instance->cache_percent < 0)
		    instance->cache_percent = 0;
		if (instance->cache_percent > 100)
		    instance->cache_percent = 100;
	    }




	}
    }

    if (instance->download_dir == NULL && instance->keep_download == 1)
	instance->download_dir = strdup(getenv("HOME"));
#endif
}
