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

#include "plugin.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsISupportsUtils.h"	// this is where some useful macros defined
#include <errno.h>

extern int errno;

// service manager which will give the access to all public browser services
// we will use memory service as an illustration
nsIServiceManager *gServiceManager = NULL;

// Unix needs this
#ifdef XP_UNIX

char *NPP_GetMIMEDescription(void)
{
    return GetMIMEDescription();
}

// get values per plugin
NPError NS_PluginGetValue(NPPVariable aVariable, void *aValue)
{
    return GetValue(aVariable, aValue);
}
#endif				//XP_UNIX

//////////////////////////////////////
//
// general initialization and shutdown
//
NPError NS_PluginInitialize()
{
    // this is probably a good place to get the service manager
    // note that Mozilla will add reference, so do not forget to release
    nsISupports *sm = NULL;


    NPN_GetValue(NULL, NPNVserviceManager, &sm);

    // Mozilla returns nsIServiceManager so we can use it directly; doing QI on
    // nsISupports here can still be more appropriate in case something is changed
    // in the future so we don't need to do casting of any sort.
    if (sm) {
	sm->QueryInterface(NS_GET_IID(nsIServiceManager),
			   (void **) &gServiceManager);
	NS_RELEASE(sm);
    }

    return NPERR_NO_ERROR;
}

void NS_PluginShutdown()
{
    // we should release the service manager
    NS_IF_RELEASE(gServiceManager);
    gServiceManager = NULL;
}

/////////////////////////////////////////////////////////////
//
// construction and destruction of our plugin instance object
//
nsPluginInstanceBase *NS_NewPluginInstance(nsPluginCreateData *aCreateDataStruct)
{
    if(!aCreateDataStruct)
		return NULL;

    nsPluginInstance *plugin = new nsPluginInstance(aCreateDataStruct->instance);

    New(plugin, aCreateDataStruct);

    return plugin;
}

void NS_DestroyPluginInstance(nsPluginInstanceBase * aPlugin)
{
   if (aPlugin)
		delete(nsPluginInstance *) aPlugin;
}

////////////////////////////////////////
//
// nsPluginInstance class implementation
//
nsPluginInstance::nsPluginInstance(NPP aInstance) : nsPluginInstanceBase(),
													mInstance(aInstance),
													mInitialized(FALSE),
													mScriptablePeer(NULL),
													mControlsScriptablePeer(NULL)
{
  mString[0] = '\0';
}

nsPluginInstance::~nsPluginInstance()
{
  // mScriptablePeer may be also held by the browser
  // so releasing it here does not guarantee that it is over
  // we should take precaution in case it will be called later
  // and zero its mPlugin member
  mScriptablePeer->SetInstance(NULL);
  NS_IF_RELEASE(mScriptablePeer);
}

NPBool nsPluginInstance::init(NPWindow * aWindow)
{
    if (aWindow == NULL)
	return FALSE;

    mInitialized = TRUE;
    return TRUE;
}

void nsPluginInstance::shut()
{
  mInitialized = FALSE;
}

NPBool nsPluginInstance::isInitialized()
{
    return mInitialized;
}

NPError nsPluginInstance::SetWindow(NPWindow * aWindow)
{
    return NPERR_NO_ERROR;
}

NPError nsPluginInstance::NewStream(NPMIMEType type, NPStream * stream,
				    NPBool seekable, uint16 * stype)
{
    return NPERR_NO_ERROR;
}


NPError nsPluginInstance::DestroyStream(NPStream * stream, NPError reason)
{
    return NPERR_NO_ERROR;
}

int32 nsPluginInstance::WriteReady(NPStream * stream)
{
	 return -1;
}

int32 nsPluginInstance::Write(NPStream * stream, int32 offset, int32 len,
			      void *buffer)
{
	 return -1;
}

// methods called from nsScriptablePeer
//
// the following method will be callable from JavaScript
//
void nsPluginInstance::getVersion(char* *aVersion)
{
	const char *ua = NPN_UserAgent(mInstance);
	char*& version = *aVersion;

	// although we can use NPAPI NPN_MemAlloc call to allocate memory:
	//    version = (char*)NPN_MemAlloc(strlen(ua) + 1);
	// for illustration purposed we use the service manager to access
	// the memory service provided by Mozilla
	nsIMemory * nsMemoryService = NULL;

	if (gServiceManager){

		// get service using its contract id and use it to allocate the memory
		gServiceManager->GetServiceByContractID("@mozilla.org/xpcom/memory-service;1",
																NS_GET_IID(nsIMemory), (void **)&nsMemoryService);

		if(nsMemoryService)
			version = (char *)nsMemoryService->Alloc(strlen(ua) + 1);
	}

	if(version)
		strcpy(version, ua);

	// release service
	NS_IF_RELEASE(nsMemoryService);
}

void nsPluginInstance::func_one()
{
	/* display on status bar */
	NPN_Status(mInstance, "func_one invoked");
}

void nsPluginInstance::func_two()
{
	/* return to javascript */
	NPN_GetURL(mInstance, "javascript:func_two_return('sipua: func_two invoked');", NULL);
}

#if 0
void nsPluginInstance::Play()
{
    Node *n;

    if (autostart == 0 && threadsignaled == 0) {
	signalPlayerThread(this);
	threadsignaled = 1;
    }

    if (threadlaunched == 0)
	return;

    pthread_mutex_lock(&control_mutex);
    if (paused == 1) {
	sendCommand(this, "pause\n");
	paused = 0;
	js_state = JS_STATE_PLAYING;
    }

    if (js_state == JS_STATE_UNDEFINED) {
	//reset the playlist
	pthread_mutex_lock(&playlist_mutex);
	n = list;
	while (n != NULL) {
	    if (n->play)
		n->played = 0;
	    n = n->next;
	}
	pthread_mutex_unlock(&playlist_mutex);
    }

    if (threadsetup == 0 && controlwindow == 0) {
	state = STATE_GETTING_PLAYLIST;
	pthread_mutex_unlock(&control_mutex);
	SetupPlayer(this, NULL);
	pthread_mutex_lock(&control_mutex);
    }

    if (threadsignaled == 1 && js_state == JS_STATE_UNDEFINED) {
	state = STATE_NEWINSTANCE;
	launchPlayerThread(this);
	pthread_mutex_unlock(&control_mutex);
	// recommended slight pause
	usleep(1);
	//signal player thread
	signalPlayerThread(this);
	threadsignaled = 1;

    } else if ((autostart == 0) && (threadsignaled == 0)) {
	pthread_mutex_unlock(&control_mutex);

	signalPlayerThread(this);
	threadsignaled = 1;
    } else {
	pthread_mutex_unlock(&control_mutex);
    }
}

void nsPluginInstance::Pause()
{
    if (threadlaunched == 0)
	return;
    pthread_mutex_lock(&control_mutex);
    if (paused == 0) {
	sendCommand(this, "pause\n");
	paused = 1;
	js_state = JS_STATE_PAUSED;
    }
    pthread_mutex_unlock(&control_mutex);
}

void nsPluginInstance::Stop()
{
    if (threadlaunched == 0)
	return;
    pthread_mutex_lock(&control_mutex);
    if (paused == 1)
	sendCommand(this, "pause\n");
    sendCommand(this, "seek 0 2\npause\n");
    paused = 1;
    js_state = JS_STATE_STOPPED;
    pthread_mutex_unlock(&control_mutex);
}

void nsPluginInstance::Quit()
{
    if (threadlaunched == 0)
	return;
    pthread_mutex_lock(&control_mutex);
    if (paused == 1)
	sendCommand(this, "pause\n");
    sendCommand(this, "quit\n");
    paused = 0;
    threadsetup = 0;
    threadsignaled = 0;
    pthread_mutex_unlock(&control_mutex);
}

void nsPluginInstance::FastForward()
{
    if (threadlaunched == 0)
	return;
    pthread_mutex_lock(&control_mutex);
    js_state = JS_STATE_SCANFORWARD;
    if (paused == 1)
	sendCommand(this, "pause\n");
    sendCommand(this, "seek +10 0\n");
    if (paused == 1)
	sendCommand(this, "pause\n");
    pthread_mutex_unlock(&control_mutex);
}

void nsPluginInstance::FastReverse()
{
    if (threadlaunched == 0)
	return;
    pthread_mutex_lock(&control_mutex);
    js_state = JS_STATE_SCANREVERSE;
    if (paused == 1)
	sendCommand(this, "pause\n");
    sendCommand(this, "seek -10 0\n");
    if (paused == 1)
	sendCommand(this, "pause\n");
    pthread_mutex_unlock(&control_mutex);
}

void nsPluginInstance::Seek(double counter)
{
    char command[32];

    if (threadlaunched == 0)
	return;
    pthread_mutex_lock(&control_mutex);
    if (paused == 1)
	sendCommand(this, "pause\n");
    snprintf(command, 32, "seek %5.0f 2\n", counter);
    sendCommand(this, command);
    if (paused == 1)
	sendCommand(this, "pause\n");
    pthread_mutex_unlock(&control_mutex);

}

void nsPluginInstance::GetPlayState(PRInt32 * playstate)
{
    pthread_mutex_lock(&control_mutex);
    *playstate = js_state;
    pthread_mutex_unlock(&control_mutex);
}

void nsPluginInstance::GetTime(double *_retval)
{
    *_retval = (double) mediaTime;
}

void nsPluginInstance::GetDuration(double *_retval)
{
    *_retval = (double) mediaLength;
}

void nsPluginInstance::GetPercent(double *_retval)
{
    *_retval = (double) mediaPercent;
}

void nsPluginInstance::GetFilename(char **filename)
{
    if (href != NULL)
	*filename = strdup(href);
    if (fname != NULL)
	*filename = strdup(fname);
    if (url != NULL)
	*filename = strdup(url);
}

void nsPluginInstance::SetFilename(const char *filename)
{
    char localurl[1024];

    killmplayer(this);
    // reset some vars
    paused = 0;
    threadsetup = 0;
    threadsignaled = 0;
    // reset the list
    pthread_mutex_lock(&playlist_mutex);
    deleteList(list);
    list = newNode();
    td->list = NULL;

    // need to convert to Fully Qualified URL here
    fullyQualifyURL(this, (char *) filename, localurl);

    if (href != NULL) {
	free(href);
	href = NULL;
    }

    if (fname != NULL) {
	free(fname);
	fname = NULL;
    }

    if (url != NULL) {
	free(url);
	url = NULL;
    }

    url = strdup(localurl);
    cancelled = 0;
    if (!isMms(localurl))
	NPN_GetURL(mInstance, localurl, NULL);

    pthread_mutex_unlock(&playlist_mutex);
}

void nsPluginInstance::GetShowControls(PRBool * _retval)
{
    *_retval = (PRBool) controlsvisible;
}

void nsPluginInstance::SetShowControls(PRBool value)
{
}

void nsPluginInstance::GetFullscreen(PRBool * _retval)
{
    *_retval = (PRBool) fullscreen;
}

void nsPluginInstance::SetFullscreen(PRBool value)
{
    int win_height, win_width;

    if (threadlaunched == 0)
	return;

    if (mode == NP_EMBED) {
	win_height = embed_height;
	win_width = embed_width;
    } else {
	win_height = window_height;
	win_width = window_width;
    }

    if (win_height == 0 || win_width == 0 || hidden == 1)
	return;

    if (fullscreen) {
	if (value) {
	    // do nothing
	    fullscreen = 1;
	} else {
	    fullscreen = 0;
	}
    } else {
	if (value) {
	    fullscreen = 1;
	} else {
	    // do nothing
	    fullscreen = 0;
	}
    }
}

void nsPluginInstance::GetShowlogo(PRBool * _retval)
{
    *_retval = (PRBool) showlogo;
}

void nsPluginInstance::SetShowlogo(PRBool value)
{
    showlogo = (int) value;
}
#endif

// ==============================
// ! Scriptability related code !
// ==============================
//
// here the plugin is asked by Mozilla to tell if it is scriptable
// we should return a valid interface id and a pointer to
// nsScriptablePeer interface which we should have implemented
// and which should be defined in the corressponding *.xpt file
// in the bin/components folder
NPError nsPluginInstance::GetValue(NPPVariable aVariable, void *aValue)
{
    NPError rv = NPERR_NO_ERROR;

    switch (aVariable) {

    	case NPPVpluginScriptableInstance:{
		
	    	// addref happens in getter, so we don't addref here
	    	nsIScriptableOgmpPlugin *scriptablePeer =
			getScriptablePeer();
	    	if (scriptablePeer) {
				*(nsISupports **) aValue = scriptablePeer;
	    	} else
				rv = NPERR_OUT_OF_MEMORY_ERROR;
		}
		break;

    	case NPPVpluginScriptableIID:{

	    	static nsIID scriptableIID = NS_ISCRIPTABLEOGMPPLUGIN_IID;
	    	nsIID *ptr = (nsIID *) NPN_MemAlloc(sizeof(nsIID));
	    	if (ptr) {
				*ptr = scriptableIID;
				*(nsIID **) aValue = ptr;
	    	} else
				rv = NPERR_OUT_OF_MEMORY_ERROR;
		}
		break;

    	default:
			break;
	}

	return rv;
}

// ==============================
// ! Scriptability related code !
// ==============================
//
// this method will return the scriptable object (and create it if necessary)
nsScriptablePeer *nsPluginInstance::getScriptablePeer()
{
   if (!mScriptablePeer) {
		mScriptablePeer = new nsScriptablePeer(this);
		if (!mScriptablePeer)
			return NULL;

		NS_ADDREF(mScriptablePeer);
   }

	// add reference for the caller requesting the object
   NS_ADDREF(mScriptablePeer);
   return mScriptablePeer;
}

nsControlsScriptablePeer *nsPluginInstance::getControlsScriptablePeer()
{
	if (!mControlsScriptablePeer) {

		mControlsScriptablePeer = new nsControlsScriptablePeer(this);
		if (!mControlsScriptablePeer)
			return NULL;

		NS_ADDREF(mControlsScriptablePeer);
   }

	// add reference for the caller requesting the object
   NS_ADDREF(mControlsScriptablePeer);

   return mControlsScriptablePeer;
}
