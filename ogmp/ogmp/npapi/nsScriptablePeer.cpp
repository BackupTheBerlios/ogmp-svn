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
   return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt) nsScriptablePeer::Release()
{
   --mRefCnt;
   if (mRefCnt == 0)
	{
		delete this;
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
		*aInstancePtr = NS_STATIC_CAST(nsIScriptableOgmpPlugin *, this);
		AddRef();
		return NS_OK;
   }

   if (aIID.Equals(kIClassInfoIID))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIClassInfo *, this);
		AddRef();
		return NS_OK;
   }

   if (aIID.Equals(kISupportsIID))
	{
		*aInstancePtr = NS_STATIC_CAST(nsISupports *, (NS_STATIC_CAST(nsIScriptableOgmpPlugin *, this)));

		AddRef();
		return NS_OK;
   }

   return NS_NOINTERFACE;
}

void nsScriptablePeer::SetInstance(nsPluginInstance * plugin)
{
   mPlugin = plugin;
}

//
// the following method will be callable from JavaScript
//
NS_IMETHODIMP nsScriptablePeer::GetVersion(char* *aVersion)
{
	printf("JS getVersion issued\n");

	if (mPlugin)
		mPlugin->getVersion(aVersion);

	return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Func_one(PRInt32 value)
{
    printf("JS Func_one issued\n");
    mPlugin->func_one(value);

    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Func_two(const char *str)
{
    printf("JS Func_two issued\n");
    mPlugin->func_two(str);

    return NS_OK;
}

/* void load_user (in string fullname, in long fnsz, in string router, in string regid, in long second); */
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

#if 0
NS_IMETHODIMP nsScriptablePeer::Play(void)
{
    printf("JS Play issued\n");
    mPlugin->Play();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Pause(void)
{
    printf("JS Pause issued\n");
    mPlugin->Pause();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Stop(void)
{
    printf("JS Stop issued\n");
    mPlugin->Stop();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Quit(void)
{
    printf("JS Quit issued\n");
    mPlugin->Quit();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::DoPlay(void)
{
    printf("JS DoPlay issued\n");
    mPlugin->Play();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::DoPause(void)
{
    printf("JS DoPause issued\n");
    mPlugin->Pause();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::FastForward(void)
{
    printf("JS FastForward issued\n");
    mPlugin->FastForward();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::FastReverse(void)
{
    printf("JS FastReverse issued\n");
    mPlugin->FastReverse();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Ff(void)
{
    printf("JS ff issued\n");
    mPlugin->FastForward();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Rew(void)
{
    printf("JS rew issued\n");
    mPlugin->FastReverse();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Rewind(void)
{
    printf("JS Quit issued\n");
    mPlugin->FastReverse();
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Seek(double counter)
{
    printf("JS Seek issued\n");
    mPlugin->Seek(counter);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetPlayState(PRInt32 * aPlayState)
{
    printf("JS playState issued\n");
    mPlugin->GetPlayState(aPlayState);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetTime(double *_retval)
{
    printf("JS getTime issued\n");
    mPlugin->GetTime(_retval);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetDuration(double *_retval)
{
    printf("JS getDuration issued\n");
    mPlugin->GetDuration(_retval);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetPercent(double *_retval)
{
    printf("JS getPercent issued\n");
    mPlugin->GetPercent(_retval);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetFilename(char **aFilename)
{
    printf("JS filename issued\n");
    mPlugin->GetFilename(aFilename);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::SetFilename(const char *aFilename)
{
    printf("JS filename issued\n");
    mPlugin->SetFilename(aFilename);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::Open(const char *filename)
{
    printf("JS filename issued\n");
    mPlugin->SetFilename(filename);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetShowControls(PRBool * aShowControls)
{
    printf("JS GetShowControls issued\n");
    mPlugin->GetShowControls(aShowControls);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::SetShowControls(PRBool aShowControls)
{
    printf("JS SetShowControls issued\n");
    mPlugin->SetShowControls(aShowControls);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetFullscreen(PRBool * aFullscreen)
{
    printf("JS GetFullscreen issued\n");
    mPlugin->GetFullscreen(aFullscreen);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::SetFullscreen(PRBool aFullscreen)
{
    printf("JS SetFullscreen issued\n");
    mPlugin->SetFullscreen(aFullscreen);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetShowlogo(PRBool * aShowlogo)
{
    printf("JS GetFullscreen issued\n");
    mPlugin->GetShowlogo(aShowlogo);
    return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::SetShowlogo(PRBool aShowlogo)
{
    printf("JS SetFullscreen issued\n");
    mPlugin->SetShowlogo(aShowlogo);
    return NS_OK;
}
#endif

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
