/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __PLUGIN_H__
#define __PLUGIN_H__
#ifdef BSD
#include <cmath>
#endif
#define _XOPEN_SOURCE 500

#ifndef BSD
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif
#ifdef GECKOSDK_ENABLED
#include "mozilla-config.h"
#endif
#include "pluginbase.h"
#include "nsScriptablePeer.h"
#include "plugin-setup.h"
/*
#include <pthread.h>
*/
#include <sys/types.h>
#include <signal.h>
#include <string.h>

class nsPluginInstance:public nsPluginInstanceBase {
  public:
    nsPluginInstance(NPP aInstance);
    virtual ~ nsPluginInstance();

    NPBool init(NPWindow * aWindow);
    void shut();
    NPBool isInitialized();
    NPError NewStream(NPMIMEType type, NPStream * stream, NPBool seekable,
		      uint16 * stype);
    NPError SetWindow(NPWindow * aWindow);
    NPError DestroyStream(NPStream * stream, NPError reason);
    int32 WriteReady(NPStream * stream);
    int32 Write(NPStream * stream, int32 offset, int32 len, void *buffer);

    // JS Methods
  	 void getVersion(char* *aVersion);
	 
  	 void func_one();
  	 void func_two();

	 /*
    void Play();
    void Pause();
    void Stop();
    void Quit();
    void FastForward();
    void FastReverse();
    void Seek(double counter);
    void GetPlayState(PRInt32 * playstate);
    void GetTime(double *_retval);
    void GetDuration(double *_retval);
    void GetPercent(double *_retval);
    void GetFilename(char * *filename);
    void SetFilename(const char *filename);
    void GetShowControls(PRBool *_retval);
    void SetShowControls(PRBool value);
    void GetFullscreen(PRBool *_retval);
    void SetFullscreen(PRBool value);
    void GetShowlogo(PRBool *_retval);
    void SetShowlogo(PRBool value);
	 */

    // we need to provide implementation of this method as it will be
    // used by Mozilla to retrive the scriptable peer
    // and couple of other things on Unix
    NPError GetValue(NPPVariable variable, void *value);

    nsScriptablePeer *getScriptablePeer();
    nsControlsScriptablePeer *getControlsScriptablePeer();
    
  public:
     NPP mInstance;
    NPBool mInitialized;
    nsScriptablePeer *mScriptablePeer;
    nsControlsScriptablePeer *mControlsScriptablePeer;
    
    char mString[128];
    // put member data here

    char *mimetype;
    int state;
    char *url;
    char *fname;
    char *href;
    char *lastmessage;
    uint16 mode;
    uint32 window_width;
    uint32 window_height;
    uint32 embed_width;
    uint32 embed_height;
    uint32 movie_width;
    uint32 movie_height;
    int setwindow;
    char *baseurl;
    char *hostname;
    int control;
    FILE *player;
    /*pid_t pid;*/
    int noredraw;
    int hrefrequested;
    int threadsetup;
    int threadlaunched;
    int threadsignaled;
    int cancelled;
    int autostart;
    int controlwindow;
    int showcontrols;
    int showtracker;
    int showbuttons;
    int showfsbutton;
    int mmsstream;
	/*
    Node *list;
    Node *currentnode;
    ThreadData *td;
    Window window;
    Window player_window;
    Display *display;
    Widget widget;
	*/
    uint32 nQtNext;
    char *qtNext[256];
    int panel_height;
    int panel_drawn;
    float percent;
    char *mediaCompleteCallback;
    float mediaLength;			// length of media in seconds
    int mediaPercent;			// percentage of media played
    float mediaTime; 			// time in seconds
    int nomediacache;
    int controlsvisible;
    int fullscreen;
    int showlogo;
    int DPMSEnabled;
    int hidden;
    int black_background;
    /*
    pthread_t player_thread;
    pthread_attr_t thread_attr;
    pthread_cond_t playlist_complete_cond;
    pthread_mutex_t playlist_mutex;
    pthread_mutex_t playlist_cond_mutex;
    pthread_mutex_t control_mutex;
	*/
    // options
    char *vo;
    char *vop;
    int novop;
    int noembed;
    char *ao;
    int loop;
    int rtsp_use_tcp;
    int keep_download;
    int maintain_aspect;
    int qt_speed;
    char *download_dir;
    int cachesize;
    char *output_display;
    int osdlevel;
    int cache_percent;
    int toolkitok;
    int moz_toolkit;
    int plug_toolkit;
    int framedrop;
    int autosync;
    int mc;
    
    // JavaScript State
    int paused;
    int js_state;
};

#endif				// __PLUGIN_H__
