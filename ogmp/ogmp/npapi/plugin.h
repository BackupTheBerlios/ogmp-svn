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

#define OS_IGNORE_INTEGER_TYPE

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

extern "C"
{
#include "ogmp.h"
}

class nsPluginInstance:public nsPluginInstanceBase
{
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
    
  private:
  
    user_t *user;
    sipua_t *sipua;
    sipua_uas_t *uas;
    module_catalog_t *mod_cata;
    
  public:
  
    /*********************/
    /* SIP UA Attritudes */
    /*********************/
    void getUseragent(char * *aUseragent);
    void getLicence(char * *aLicence);
    void getNetaddr(char* *addr);
    
    /********************/
    /* SIP UA Methods */
    /********************/
    
    /* Test only */
    void get_ip();

    NPError sipua_init();
    
    void load_user(const char *uid, PRInt32 uidsz);
    void regist(const char *fullname, PRInt32 fnsz, const char *home_router, const char *user_at_domain, PRInt32 seconds);
    void unregist(const char *home_router, const char *user_at_domain);
    void new_call(const char *subject, const char *info);
    void call(const char *callee_at_domain);
    void answer(PRInt32 lineno);
    void bye(PRInt32 lineno);
    
    static int callback_on_register (void *user_on_register, int result, char *reason);
    static int callback_on_newcall (void *user_on_newcall, int lineno, char *caller, char *subject, char *info);
    static int callback_on_conversation_start (void *user_on_conversation_start, int lineno);
    static int callback_on_conversation_end (void *user_on_conversation_end, int lineno);
    static int callback_on_bye (void *user_on_bye, int lineno, char *caller, char *reason);

    // we need to provide implementation of this method as it will be
    // used by Mozilla to retrive the scriptable peer
    // and couple of other things on Unix
    NPError GetValue(NPPVariable variable, void *value);

    nsScriptablePeer *getScriptablePeer();
    nsControlsScriptablePeer *getControlsScriptablePeer();
    
  public:
  
    NPBool mInitialized;
    
    NPP mInstance;
    nsScriptablePeer *mScriptablePeer;
    nsControlsScriptablePeer *mControlsScriptablePeer;
};

#endif  // __PLUGIN_H__
