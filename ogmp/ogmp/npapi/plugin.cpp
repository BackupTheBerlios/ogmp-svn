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

#define CLIE_LOG
#define CLIE_DEBUG

#ifdef CLIE_LOG
 #define clie_log(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define clie_log(fmtargs)
#endif

#ifdef CLIE_DEBUG
 #define clie_debug(fmtargs)  do{printf fmtargs;}while(0)
#else
 #define clie_debug(fmtargs)
#endif

#define USERAGENT "RealmTel Mozilla Phone"
#define LICENCE   "GPL"

#define OGMP_VERSION  1

#define SIPUA_MAX_RING 6
#define MAX_CALL_BANDWIDTH  5000  /* in Bytes */

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
    clie_log(("NS_PluginInitialize: begin\n"));

    // this is probably a good place to get the service manager
    // note that Mozilla will add reference, so do not forget to release
    nsISupports *sm = NULL;

    NPN_GetValue(NULL, NPNVserviceManager, &sm);

    // Mozilla returns nsIServiceManager so we can use it directly; doing QI on
    // nsISupports here can still be more appropriate in case something is changed
    // in the future so we don't need to do casting of any sort.
    if (sm)
    {
        sm->QueryInterface(NS_GET_IID(nsIServiceManager),
                            (void **) &gServiceManager);
        NS_RELEASE(sm);
    }

    clie_log(("NS_PluginInitialize: end\n"));

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
    clie_log(("NS_NewPluginInstance: begin\n"));

    if(!aCreateDataStruct)
		return NULL;

    nsPluginInstance *plugin = new nsPluginInstance(aCreateDataStruct->instance);

    New(plugin, aCreateDataStruct);

    clie_log(("NS_NewPluginInstance: end\n"));

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
    //mString[0] = '\0';
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
    /*
    if (aWindow == NULL)
        return FALSE;
    */
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
// the following attritudes accessed by JavaScript
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

	if (gServiceManager)
    {
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

void nsPluginInstance::getUseragent(char * *aUseragent)
{
	// although we can use NPAPI NPN_MemAlloc call to allocate memory:
	//    version = (char*)NPN_MemAlloc(strlen(ua) + 1);
	// for illustration purposed we use the service manager to access
	// the memory service provided by Mozilla
	nsIMemory * nsMemoryService = NULL;

	if (gServiceManager)
    {
		// get service using its contract id and use it to allocate the memory
		gServiceManager->GetServiceByContractID("@mozilla.org/xpcom/memory-service;1",
												NS_GET_IID(nsIMemory), (void **)&nsMemoryService);
		if(nsMemoryService)
			*aUseragent = (char *)nsMemoryService->Alloc(strlen(USERAGENT) + 1);
	}

	if(*aUseragent)
		strcpy(*aUseragent, USERAGENT);

	// release service
	NS_IF_RELEASE(nsMemoryService);

    clie_log (("nsPluginInstance::getUseragent: %s\n", *aUseragent));
}

void nsPluginInstance::getLicence(char * *aLicence)
{
	// although we can use NPAPI NPN_MemAlloc call to allocate memory:
	//    version = (char*)NPN_MemAlloc(strlen(ua) + 1);
	// for illustration purposed we use the service manager to access
	// the memory service provided by Mozilla
	nsIMemory * nsMemoryService = NULL;

	if (gServiceManager)
    {
		// get service using its contract id and use it to allocate the memory
		gServiceManager->GetServiceByContractID("@mozilla.org/xpcom/memory-service;1",
												NS_GET_IID(nsIMemory), (void **)&nsMemoryService);
		if(nsMemoryService)
			*aLicence = (char *)nsMemoryService->Alloc(strlen(LICENCE) + 1);
	}

	if(*aLicence)
		strcpy(*aLicence, LICENCE);

	// release service
	NS_IF_RELEASE(nsMemoryService);
}

void nsPluginInstance::getNetaddr(char* *addr)
{
    char *nettype, *addrtype, *netaddr;
    
	this->sipua->uas->address(this->sipua->uas, &nettype, &addrtype, &netaddr);

	// although we can use NPAPI NPN_MemAlloc call to allocate memory:
	//    version = (char*)NPN_MemAlloc(strlen(ua) + 1);
	// for illustration purposed we use the service manager to access
	// the memory service provided by Mozilla
	nsIMemory * nsMemoryService = NULL;

	if (gServiceManager)
    {
		// get service using its contract id and use it to allocate the memory
		gServiceManager->GetServiceByContractID("@mozilla.org/xpcom/memory-service;1",
												NS_GET_IID(nsIMemory), (void **)&nsMemoryService);
		if(nsMemoryService)
			*addr = (char *)nsMemoryService->Alloc(strlen(netaddr) + 1);
	}

	if(*addr)
		strcpy(*addr, netaddr);

	// release service
	NS_IF_RELEASE(nsMemoryService);
}

// methods called from nsScriptablePeer
//
// the following methods called by JavaScript
//
void nsPluginInstance::get_ip()
{
	/* return to javascript */
    char javascript[512], *nettype, *addrtype, *netaddr;
    
	this->sipua->uas->address(this->sipua->uas, &nettype, &addrtype, &netaddr);

    sprintf(javascript, "javascript:get_ip_return('%s');", netaddr);
    
	NPN_GetURL(mInstance, javascript, "mainframe");
}

int nsPluginInstance::callback_on_register(void *user_on_register, int result, char *reason)
{
    char javascript[512];
    
    nsPluginInstance *plugin = static_cast<nsPluginInstance*>(user_on_register);
    
    if(result == SIPUA_STATUS_REG_OK)
        sprintf(javascript, "javascript:regist_return('reg_ok', '%s');", reason);
        
    if(result == SIPUA_STATUS_NORMAL)
        sprintf(javascript, "javascript:regist_return('unreg_ok', '%s');", reason);
        
    if(result == SIPUA_STATUS_REG_FAIL)
        sprintf(javascript, "javascript:regist_return('reg_fail', '%s');", reason);
        
    if(result == SIPUA_STATUS_UNREG_FAIL)
        sprintf(javascript, "javascript:regist_return('unreg_fail', '%s');", reason);

    NPN_GetURL(plugin->mInstance, javascript, "mainframe");
    
    return NPERR_NO_ERROR;
}

int nsPluginInstance::callback_on_newcall(void *user_on_newcall, int lineno, char *caller, char *subject, char *info)
{
    char javascript[512];

    nsPluginInstance *plugin = static_cast<nsPluginInstance*>(user_on_newcall);

    sprintf(javascript, "javascript:regist_return(%d, %s, %s, %s);", lineno, caller, subject, info);

	NPN_GetURL(plugin->mInstance, javascript, NULL);

    return NPERR_NO_ERROR;
}

int nsPluginInstance::callback_on_conversation_start(void *user_on_newcall, int lineno)
{
    char javascript[512];

    nsPluginInstance *plugin = static_cast<nsPluginInstance*>(user_on_newcall);

    sprintf(javascript, "javascript:regist_return(%d);", lineno);

	NPN_GetURL(plugin->mInstance, javascript, "mainframe");

    return NPERR_NO_ERROR;
}

int nsPluginInstance::callback_on_conversation_end(void *user_on_newcall, int lineno)
{
    char javascript[512];

    nsPluginInstance *plugin = static_cast<nsPluginInstance*>(user_on_newcall);

    sprintf(javascript, "javascript:regist_return(%d);", lineno);

	NPN_GetURL(plugin->mInstance, javascript, "mainframe");

    return NPERR_NO_ERROR;
}

int nsPluginInstance::callback_on_bye(void *user_on_newcall, int lineno, char *caller, char *reason)
{
    char javascript[512];

    nsPluginInstance *plugin = static_cast<nsPluginInstance*>(user_on_newcall);

    sprintf(javascript, "javascript:regist_return(%d, %s, %s);", lineno, caller, reason);

	NPN_GetURL(plugin->mInstance, javascript, "mainframe");

    return NPERR_NO_ERROR;
}

NPError nsPluginInstance::sipua_init()
{
    int sip_port = 5070;
    
    this->sipua = NULL;
    this->uas = NULL;
    this->user = NULL;
    this->mod_cata = NULL;

    clie_log (("plugin.sipua_init: sip port is %d\n", sip_port));
    clie_log (("plugin.sipua_init: modules in dir:'%s'\n", MOD_DIR));

    mod_cata = catalog_new( "mediaformat" );

    catalog_scan_modules( this->mod_cata, OGMP_VERSION, MOD_DIR );

    this->uas = client_new_uas(this->mod_cata, "eXosipua");
    if(!this->uas)
    {
		clie_log (("main: fail to create sipua server!\n"));
		return NPERR_MODULE_LOAD_FAILED_ERROR;
    }

	if(this->uas->init(this->uas, sip_port, "IN", "IP4", NULL, NULL) < UA_OK)
    {
		clie_log (("plugin.sipua_init: fail to initialize sipua server!\n"));
		return NPERR_MODULE_LOAD_FAILED_ERROR;
    }
    
    this->sipua = client_new("dummyui", this->uas, this->mod_cata, 64*1024);

    if(!this->sipua)
    {
        clie_log(("plugin.sipua_init: fail to create sipua!\n"));
        return NPERR_MODULE_LOAD_FAILED_ERROR;
    }

    this->sipua->set_register_callback(this->sipua, nsPluginInstance::callback_on_register, this);
    this->sipua->set_newcall_callback(this->sipua, nsPluginInstance::callback_on_newcall, this);
    this->sipua->set_conversation_start_callback(this->sipua, nsPluginInstance::callback_on_conversation_start, this);
    this->sipua->set_conversation_end_callback(this->sipua, nsPluginInstance::callback_on_conversation_end, this);
    this->sipua->set_bye_callback(this->sipua, nsPluginInstance::callback_on_bye, this);
    
    client_start(this->sipua);

    clie_log(("plugin.sipua_init: ok!\n"));
    
    return NPERR_NO_ERROR;
}

void nsPluginInstance::load_user(const char *uid, PRInt32 uidsz)
{
    if(!this->user)
        this->user = user_new((char*)uid, uidsz);

    if(!this->user)
    {
        NPN_GetURL(mInstance, "javascript:load_user_return('loaduser_fail');", NULL);
        return;
    }

    /* locate user when plugin initialized */
    this->sipua->locate_user(this->sipua, this->user);
    
    NPN_GetURL(mInstance, "javascript:load_user_return('loaduser_ok');", "mainframe");
}

void nsPluginInstance::regist(const char *fullname, PRInt32 fnsz, const char *home_router, const char *user_at_domain, PRInt32 seconds)
{
    char sip_router[256];
    char sip_user[256];

    user_profile_t* prof;

    if(!this->user)
    {
        clie_log (("nsPluginInstance::regist: Must load user first!\n"));
        return;
    }
    
    sprintf(sip_router, "sip:%s", home_router);
    sprintf(sip_user, "sip:%s", user_at_domain);

    prof = user_add_profile(this->user, (char*)fullname, fnsz, NULL/*char* book_loc*/, (char*)sip_router, (char*)sip_user, seconds);
    if(!prof)
    {
        NPN_GetURL(mInstance, "javascript:regist_return('reg_fail');", "mainframe");

        return;
    }

    this->sipua->regist(this->sipua, prof, this->user->userloc);
    
    NPN_GetURL(mInstance, "javascript:regist_return('reg_ing');", "mainframe");
    
    return;
}

void nsPluginInstance::unregist(const char *home_router, const char *user_at_domain)
{}
void nsPluginInstance::new_call(const char *subject, const char *info)
{}
void nsPluginInstance::call(const char *callee_at_domain)
{}
void nsPluginInstance::answer(PRInt32 lineno)
{}
void nsPluginInstance::bye(PRInt32 lineno)
{}

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

    	switch (aVariable)
    	{
    		case NPPVpluginScriptableInstance:
        	{
			clie_log(("nsPluginInstance::GetValue: NPPVpluginScriptableInstance\n"));
	    		// addref happens in getter, so we don't addref here
	    		nsIScriptableOgmpPlugin *scriptablePeer = getScriptablePeer();

	    		if (scriptablePeer)
            		{
				clie_log(("nsPluginInstance::GetValue: NPPVpluginScriptableInstance[@%x]\n", (int)scriptablePeer));

				*(nsISupports **) aValue = scriptablePeer;
	    		} else
				rv = NPERR_OUT_OF_MEMORY_ERROR;
		}
			break;

    		case NPPVpluginScriptableIID:
        	{
			clie_log(("nsPluginInstance::GetValue: NPPVpluginScriptableIID = %s\n",
 					NS_ISCRIPTABLEOGMPPLUGIN_IID_STR));

	    		static nsIID scriptableIID = NS_ISCRIPTABLEOGMPPLUGIN_IID;

	    		nsIID *ptr = (nsIID *) NPN_MemAlloc(sizeof(nsIID));
	    		if (ptr)
            		{
				*ptr = scriptableIID;
				*(nsIID **) aValue = ptr;
	    		} else
				rv = NPERR_OUT_OF_MEMORY_ERROR;
		}
			break;

    		case NPPVpluginNameString:
			clie_log(("nsPluginInstance::GetValue: NPPVpluginNameString\n")); 
			break;
     		case NPPVpluginDescriptionString:
			clie_log(("nsPluginInstance::GetValue: NPPVpluginDescriptionString\n"));
			break;
     		case NPPVpluginWindowBool:
			clie_log(("nsPluginInstance::GetValue: NPPVpluginWindowBool\n"));
			break;
     		case NPPVpluginTransparentBool:
			clie_log(("nsPluginInstance::GetValue: NPPVpluginTransparentBool\n"));
			break;
     		case NPPVjavaClass: /* Not implemented in Mozilla 1.0 */
			clie_log(("nsPluginInstance::GetValue: NPPVjavaClass\n"));
			break;
     		case NPPVpluginWindowSize:
 			clie_log(("nsPluginInstance::GetValue: NPPVpluginWindowSize\n"));
			break;
    		case NPPVpluginTimerInterval:
			clie_log(("nsPluginInstance::GetValue: NPPVpluginTimerInterval\n"));
			break;

   		/* following are available on Mozilla builds starting with 0.9.9 */

    		case NPPVjavascriptPushCallerBool:
			clie_log(("nsPluginInstance::GetValue: NPPVjavascriptPushCallerBool\n"));
			break;
     		case NPPVpluginKeepLibraryInMemory: /* available in Mozilla 1.0 */
			clie_log(("nsPluginInstance::GetValue: NPPVpluginKeepLibraryInMemory\n"));
			break;
     		case NPPVpluginNeedsXEmbed:
			clie_log(("nsPluginInstance::GetValue: NPPVpluginNeedsXEmbed\n"));
			break;
    		case NPPVpluginScriptableNPObject: /* Get the NPObject for scripting the plugin. */
			clie_log(("nsPluginInstance::GetValue: NPPVpluginScriptableNPObject\n"));
			break;
     		default:
			clie_log(("nsPluginInstance::GetValue: Unknown\n"));
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
    if (!mScriptablePeer)
    {
        mScriptablePeer = new nsScriptablePeer(this);
        
        if (!mScriptablePeer)
            return NULL;

        /* Initialize sipua when it is created */
        if(this->sipua_init() != NPERR_NO_ERROR)
        {
            delete(nsScriptablePeer *) mScriptablePeer;
            return NULL;
        }

        NS_ADDREF(mScriptablePeer);

        clie_log(("nsPluginInstance::getScriptablePeer: init ok!\n"));
    }

    // add reference for the caller requesting the object
    NS_ADDREF(mScriptablePeer);
    
    return mScriptablePeer;
}

nsControlsScriptablePeer *nsPluginInstance::getControlsScriptablePeer()
{
    if (!mControlsScriptablePeer)
    {
        mControlsScriptablePeer = new nsControlsScriptablePeer(this);
        if (!mControlsScriptablePeer)
            return NULL;

        NS_ADDREF(mControlsScriptablePeer);
    }

    // add reference for the caller requesting the object
    NS_ADDREF(mControlsScriptablePeer);

    return mControlsScriptablePeer;
}
