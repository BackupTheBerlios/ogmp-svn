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

// ==============================
// ! Scriptability related code !
// ==============================

/////////////////////////////////////////////////////
//
// This file implements the nsScriptablePeer object
// The native methods of this class are supposed to
// be callable from JavaScript
//
#include "plugin.h"

static NS_DEFINE_IID(kIScriptableOgmpPluginIID,
		     NS_ISCRIPTABLEOGMPPLUGIN_IID);
static NS_DEFINE_IID(kIScriptableWMPPluginIID,
		     NS_ISCRIPTABLEWMPPLUGIN_IID);
static NS_DEFINE_IID(kIClassInfoIID, NS_ICLASSINFO_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsScriptablePeer::nsScriptablePeer(nsPluginInstance * aPlugin)
{
    mPlugin = aPlugin;
    mRefCnt = 0;
    mControls = NULL;
}

nsScriptablePeer::~nsScriptablePeer()
{
}

void nsScriptablePeer::InitControls(nsControlsScriptablePeer * aControls)
{
    mControls = aControls;
}

// AddRef, Release and QueryInterface are common methods and must
// be implemented for any interface
NS_IMETHODIMP_(nsrefcnt) nsScriptablePeer::AddRef()
{
   ++mRefCnt;
   
   printf("nsScriptablePeer::AddRef: refno[%d]\n", mRefCnt);

   return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt) nsScriptablePeer::Release()
{
   --mRefCnt;

   printf("nsScriptablePeer::Release: refno[%d]\n", mRefCnt);

   if (mRefCnt == 0)
   {
		delete this;
   
		printf("nsScriptablePeer::Release: deleted\n");

		return 0;
   }

   return mRefCnt;
}

// here nsScriptablePeer should return three interfaces it can be asked for by their iid's
// static casts are necessary to ensure that correct pointer is returned
NS_IMETHODIMP nsScriptablePeer::QueryInterface(const nsIID & aIID,
					       void **aInstancePtr)
{
   if (!aInstancePtr)
	return NS_ERROR_NULL_POINTER;

   if (aIID.Equals(kIScriptableOgmpPluginIID))
   {
   		printf("nsScriptablePeer::QueryInterface: nsIScriptableOgmpPlugin\n");

		*aInstancePtr = NS_STATIC_CAST(nsIScriptableOgmpPlugin *, this);
		AddRef();
		return NS_OK;
   }

   if (aIID.Equals(kIClassInfoIID))
   {
   		printf("nsScriptablePeer::QueryInterface: nsIClassInfo\n");

		*aInstancePtr = NS_STATIC_CAST(nsIClassInfo *, this);
		AddRef();
		return NS_OK;
   }

   if (aIID.Equals(kISupportsIID))
   {
   		printf("nsScriptablePeer::QueryInterface: nsISupports\n");

		*aInstancePtr = NS_STATIC_CAST(nsISupports *, (NS_STATIC_CAST(nsIScriptableOgmpPlugin *, this)));

		AddRef();
		return NS_OK;
   }

   printf("nsScriptablePeer::QueryInterface: No interface\n");

   return NS_NOINTERFACE;
}

void nsScriptablePeer::SetInstance(nsPluginInstance * plugin)
{
   mPlugin = plugin;
}

//
// the following method will be callable from JavaScript
//

/* readonly attribute string Useragent; */
NS_IMETHODIMP nsScriptablePeer::GetUseragent(char * *aUseragent)
{
	if (mPlugin)
		mPlugin->getUseragent(aUseragent);

	return NS_OK;
}

/* readonly attribute string Licence; */
NS_IMETHODIMP nsScriptablePeer::GetLicence(char * *aLicence)
{
	if (mPlugin)
		mPlugin->getLicence(aLicence);

	return NS_OK;
}

/* readonly attribute string Version; */
NS_IMETHODIMP nsScriptablePeer::GetVersion(char* *aVersion)
{
	if (mPlugin)
		mPlugin->getVersion(aVersion);

	return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetNetaddr(char* *addr)
{
	if (mPlugin)
		mPlugin->getNetaddr(addr);

	return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Get_ip()
{
    mPlugin->get_ip();

    return NS_OK;
}

/* void load_user () */
NS_IMETHODIMP nsScriptablePeer::Load_user(const char *uid, PRInt32 uidsz)
{
    mPlugin->load_user(uid, uidsz);

    return NS_OK;
}

/* void regist (); */
NS_IMETHODIMP nsScriptablePeer::Regist(const char *fullname, PRInt32 fnsz, const char *home_router, const char *user_at_domain, PRInt32 seconds)
{
    mPlugin->regist(fullname, fnsz, home_router, user_at_domain, seconds);

    return NS_OK;
}

/* void unrigst (); */
NS_IMETHODIMP nsScriptablePeer::Unregist(const char *home_router, const char *user_at_domain)
{
    mPlugin->unregist(home_router, user_at_domain);

    return NS_OK;
}

/* void new_call (); */
NS_IMETHODIMP nsScriptablePeer::New_call(const char *subject, const char *info)
{
    mPlugin->new_call(subject, info);

    return NS_OK;
}

/* void call (); */
NS_IMETHODIMP nsScriptablePeer::Call(const char *callee_at_domain)
{
    mPlugin->call(callee_at_domain);

    return NS_OK;
}

/* void answer (); */
NS_IMETHODIMP nsScriptablePeer::Answer(PRInt32 lineno)
{
    mPlugin->answer(lineno);

    return NS_OK;
}

/* void bye (); */
NS_IMETHODIMP nsScriptablePeer::Bye(PRInt32 lineno)
{
    mPlugin->bye(lineno);

    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetControls(nsIScriptableWMPPlugin *
					    *aControls)
{
    *aControls = mControls;

    if (mControls == NULL)
		return NS_ERROR_NULL_POINTER;
    else
		return NS_OK;
}


nsControlsScriptablePeer::nsControlsScriptablePeer(nsPluginInstance *
						   aPlugin)
{
    mPlugin = aPlugin;
    mRefCnt = 0;
}

nsControlsScriptablePeer::~nsControlsScriptablePeer()
{
    //printf("~nsScriptablePeer called\n");
}

// AddRef, Release and QueryInterface are common methods and must
// be implemented for any interface
NS_IMETHODIMP_(nsrefcnt) nsControlsScriptablePeer::AddRef()
{
    ++mRefCnt;
    return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt) nsControlsScriptablePeer::Release()
{
    --mRefCnt;
    if (mRefCnt == 0) {
	//delete this;
	return 0;
    }
    return mRefCnt;
}

// here nsScriptablePeer should return three interfaces it can be asked for by their iid's
// static casts are necessary to ensure that correct pointer is returned
NS_IMETHODIMP nsControlsScriptablePeer::QueryInterface(const nsIID & aIID,
						       void **aInstancePtr)
{
   if (!aInstancePtr)
		return NS_ERROR_NULL_POINTER;

   if (aIID.Equals(kIScriptableWMPPluginIID)) {

		*aInstancePtr = NS_STATIC_CAST(nsIScriptableWMPPlugin *, this);
		AddRef();
		return NS_OK;
   }

   if (aIID.Equals(kIClassInfoIID)) {

		*aInstancePtr = NS_STATIC_CAST(nsIClassInfo *, this);
		AddRef();
		return NS_OK;
   }

   if (aIID.Equals(kISupportsIID)) {

		*aInstancePtr = NS_STATIC_CAST(nsISupports *, (NS_STATIC_CAST(nsIScriptableWMPPlugin *, this)));
		AddRef();
		return NS_OK;
   }

   return NS_NOINTERFACE;
}

void nsControlsScriptablePeer::SetInstance(nsPluginInstance * plugin)
{
    mPlugin = plugin;
}

//
// the following method will be callable from JavaScript
//
NS_IMETHODIMP nsControlsScriptablePeer::Play(void)
{
    printf("JS Play issued\n");
    //mPlugin->Play();
    return NS_OK;
}

NS_IMETHODIMP nsControlsScriptablePeer::Pause(void)
{
    printf("JS Pause issued\n");
    //mPlugin->Pause();
    return NS_OK;
}

NS_IMETHODIMP nsControlsScriptablePeer::Stop(void)
{
    printf("JS Stop issued\n");
    //mPlugin->Stop();
    return NS_OK;
}
