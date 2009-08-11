/*
//
// BEGIN SONGBIRD GPL
// 
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2009 POTI, Inc.
// http://songbirdnest.com
// 
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the "GPL").
// 
// Software distributed under the License is distributed 
// on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either 
// express or implied. See the GPL for the specific language 
// governing rights and limitations.
//
// You should have received a copy of the GPL along with this 
// program. If not, go to http://www.gnu.org/licenses/gpl.html
// or write to the Free Software Foundation, Inc., 
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
// 
// END SONGBIRD GPL
//
*/

#include "sbBaseDeviceFirmwareHandler.h"

#include <nsIIOService.h>
#include <nsIScriptSecurityManager.h>

#include <nsAutoLock.h>
#include <nsServiceManagerUtils.h>
#include <nsThreadUtils.h>
#include <prlog.h>

#include <sbIDeviceEventTarget.h>

#include <sbProxiedComponentManager.h>

/**
 * To log this module, set the following environment variable:
 *   NSPR_LOG_MODULES=sbBaseDeviceFirmwareHandler:5
 */
#ifdef PR_LOGGING
  static PRLogModuleInfo* gBaseDeviceFirmwareHandlerLog = nsnull;
# define TRACE(args) PR_LOG(gBaseDeviceFirmwareHandlerLog, PR_LOG_DEBUG, args)
# define LOG(args)   PR_LOG(gBaseDeviceFirmwareHandlerLog, PR_LOG_WARN, args)
# ifdef __GNUC__
#   define __FUNCTION__ __PRETTY_FUNCTION__
# endif /* __GNUC__ */
#else
# define TRACE(args) /* nothing */
# define LOG(args)   /* nothing */
#endif

static const PRInt32 HTTP_STATE_UNINITIALIZED = 0;
static const PRInt32 HTTP_STATE_LOADING       = 1;
static const PRInt32 HTTP_STATE_LOADED        = 2;
static const PRInt32 HTTP_STATE_INTERACTIVE   = 3;
static const PRInt32 HTTP_STATE_COMPLETED     = 4;

NS_IMPL_THREADSAFE_ISUPPORTS2(sbBaseDeviceFirmwareHandler,
                              sbIDeviceFirmwareHandler,
                              nsITimerCallback)

sbBaseDeviceFirmwareHandler::sbBaseDeviceFirmwareHandler()
: mMonitor(nsnull)
, mHandlerState(HANDLER_IDLE)
, mFirmwareVersion(0)
, mNeedsRecoveryMode(PR_FALSE)
{
#ifdef PR_LOGGING
  if(!gBaseDeviceFirmwareHandlerLog) {
    gBaseDeviceFirmwareHandlerLog =
      PR_NewLogModule("sbBaseDeviceFirmwareHandler");
  }
#endif
}

sbBaseDeviceFirmwareHandler::~sbBaseDeviceFirmwareHandler()
{
  if(mMonitor) {
    nsAutoMonitor::DestroyMonitor(mMonitor);
  }
}

// ----------------------------------------------------------------------------
// sbBaseDeviceFirmwareHandler
// ----------------------------------------------------------------------------
nsresult
sbBaseDeviceFirmwareHandler::Init()
{
  TRACE(("[%s]", __FUNCTION__));
  mMonitor = nsAutoMonitor::NewMonitor("sbBaseDeviceFirmwareHandler::mMonitor");
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_ERROR_UNEXPECTED;
  mXMLHttpRequest = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptSecurityManager> ssm = 
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> principal;
  rv = ssm->GetSystemPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mXMLHttpRequest->Init(principal, nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mXMLHttpRequest->SetMozBackgroundRequest(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = OnInit();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDeviceFirmwareHandler::CreateProxiedURI(const nsACString &aURISpec,
                                              nsIURI **aURI)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aURI);

  nsresult rv = NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIIOService> ioService = 
    do_ProxiedGetService("@mozilla.org/network/io-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("[%s] Creating proxied URI for '%s'", aURISpec.BeginReading()));

  nsCOMPtr<nsIURI> uri;
  rv = ioService->NewURI(aURISpec, 
                         nsnull, 
                         nsnull, 
                         getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIThread> mainThread;
  rv = NS_GetMainThread(getter_AddRefs(mainThread));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = do_GetProxyForObject(mainThread,
                            NS_GET_IID(nsIURI),
                            uri, 
                            NS_PROXY_ALWAYS | NS_PROXY_SYNC,
                            (void **) aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult 
sbBaseDeviceFirmwareHandler::SendHttpRequest(const nsACString &aMethod, 
                                             const nsACString &aUrl,
                                             const nsAString &aUsername /*= EmptyString()*/,
                                             const nsAString &aPassword /*= EmptyString()*/,
                                             const nsACString &aContentType /*= EmptyCString()*/,
                                             nsIVariant *aRequestBody /*= nsnull*/)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_STATE(mXMLHttpRequest);

  NS_ENSURE_TRUE(!aMethod.IsEmpty(), NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(!aUrl.IsEmpty(), NS_ERROR_INVALID_ARG);

  PRInt32 state = 0;
  nsresult rv = mXMLHttpRequest->GetReadyState(&state);
  NS_ENSURE_SUCCESS(rv, rv);

  // Only one request at a time.
  if(state != HTTP_STATE_UNINITIALIZED && 
     state != HTTP_STATE_COMPLETED) {
    TRACE(("[%s] - can't do parallel requests (in state %08x)", __FUNCTION__, state));
    return NS_ERROR_ABORT;
  }
  
  TRACE(("[%s] - sending %s request to %s", __FUNCTION__,
         nsCString(aMethod).get(), nsCString(aUrl).get()));

  rv = mXMLHttpRequest->OpenRequest(aMethod, aUrl, PR_TRUE, 
                                    aUsername, aPassword);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aContentType.IsEmpty()) {
    rv = mXMLHttpRequest->SetRequestHeader(NS_LITERAL_CSTRING("Content-Type"),
                                           aContentType);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if(!mXMLHttpRequestTimer) {
    mXMLHttpRequestTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Send the request asynchronously. aRequestBody may be null here.
  rv = mXMLHttpRequest->Send(aRequestBody);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITimerCallback> callback =
    do_QueryInterface(NS_ISUPPORTS_CAST(nsITimerCallback*, this), &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "sbBaseDeviceFirmwareHandler doesn't implement nsITimerCallback!");
  rv = mXMLHttpRequestTimer->InitWithCallback(callback,
                                              100, 
                                              nsITimer::TYPE_REPEATING_SLACK);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

nsresult
sbBaseDeviceFirmwareHandler::AbortHttpRequest()
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_STATE(mXMLHttpRequest);

  PRInt32 state = 0;
  nsresult rv = mXMLHttpRequest->GetReadyState(&state);
  NS_ENSURE_SUCCESS(rv, rv);

  if(state != HTTP_STATE_UNINITIALIZED &&
     state != HTTP_STATE_COMPLETED) {
    rv = mXMLHttpRequest->Abort();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if(mXMLHttpRequestTimer) {
    rv = mXMLHttpRequestTimer->Cancel();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult 
sbBaseDeviceFirmwareHandler::CreateDeviceEvent(PRUint32 aType,
                                               nsIVariant *aData, 
                                               sbIDeviceEvent **aEvent)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aEvent);

  nsAutoMonitor mon(mMonitor);
  NS_ENSURE_STATE(mDevice);
  nsCOMPtr<sbIDevice> device = mDevice;
  mon.Exit();

  nsresult rv = NS_ERROR_UNEXPECTED;

  nsCOMPtr<sbIDeviceManager2> deviceManager = 
    do_GetService("@songbirdnest.com/Songbird/DeviceManager;2", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = deviceManager->CreateEvent(aType, aData, device, aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDeviceFirmwareHandler::SendDeviceEvent(sbIDeviceEvent *aEvent, 
                                             PRBool aAsync /*= PR_TRUE*/)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aEvent);

  nsresult rv = NS_ERROR_UNEXPECTED;

  nsAutoMonitor mon(mMonitor);

  nsCOMPtr<sbIDeviceEventListener> listener = mListener;

  NS_ENSURE_STATE(mDevice);
  nsCOMPtr<sbIDeviceEventTarget> target = do_QueryInterface(mDevice, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mon.Exit();

  PRBool dispatched = PR_FALSE;
  rv = target->DispatchEvent(aEvent, aAsync, &dispatched);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_WARN_IF_FALSE(dispatched, "Event not dispatched");

  if(listener) {
    rv = listener->OnDeviceEvent(aEvent);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Error while calling listener.");
  }

  return NS_OK;
}

nsresult 
sbBaseDeviceFirmwareHandler::SendDeviceEvent(PRUint32 aType, 
                                             nsIVariant *aData,
                                             PRBool aAsync /*= PR_TRUE*/)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  
  nsCOMPtr<sbIDeviceEvent> deviceEvent;
  nsresult rv = CreateDeviceEvent(aType, aData, getter_AddRefs(deviceEvent));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SendDeviceEvent(deviceEvent, aAsync);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

sbBaseDeviceFirmwareHandler::handlerstate_t
sbBaseDeviceFirmwareHandler::GetState()
{
  TRACE(("[%s]", __FUNCTION__));
  nsAutoMonitor mon(mMonitor);
  return mHandlerState;
}

nsresult 
sbBaseDeviceFirmwareHandler::SetState(handlerstate_t aState)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_ARG_RANGE(aState, HANDLER_IDLE, HANDLER_X);

  nsAutoMonitor mon(mMonitor);
  mHandlerState = aState;

  return NS_OK;
}

nsresult 
sbBaseDeviceFirmwareHandler::CheckForError(const nsresult &aResult, 
                                           PRUint32 aEventType,
                                           nsIVariant *aData /*= nsnull*/)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);

  if(NS_FAILED(aResult)) {
    nsresult rv = SendDeviceEvent(aEventType, aData);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return aResult;
}

// ----------------------------------------------------------------------------
// Overridable methods for users of this base class.
// ----------------------------------------------------------------------------

/*virtual*/ nsresult 
sbBaseDeviceFirmwareHandler::OnInit()
{
  TRACE(("[%s]", __FUNCTION__));
  /**
   * Here is where you will want to initialize yourself for the first time.
   * This should include setting the following member variables to the
   * the values you need: mContractId.
   *
   * You should end up with a string that looks something like this:
   * "@yourdomain.com/FirmwareHandler/AcmePortablePlayer900x;1"
   * 
   * The ';1' at the end of the contract id is the implementation version
   * of the object. If you ever update your object and break or modify
   * behavior, make sure to increment that number to ensure no one
   * gets nasty surprises when creating your handler.
   *
   * The other values only need to be set when OnRefreshInfo is called.
   * These values are: mFirmwareVersion, mReadableFirmwareVersion, mFirmwareLocation
   * mResetInstructionsLocation and mReleaseNotesLocation.
   *
   * Events must be sent to both the device and the listener (if it is specified 
   * during the call).
   */

  return NS_ERROR_NOT_IMPLEMENTED;
}

/*virtual*/ nsresult 
sbBaseDeviceFirmwareHandler::OnCanHandleDevice(sbIDevice *aDevice, 
                                               PRBool *_retval)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(_retval);
  /**
   * Here is where you will want to verify the incoming sbIDevice object
   * to determine if your handler can support it in some way. _retval should be
   * set to either PR_TRUE (yes, use this handler for the device) or PR_FALSE
   * (let some other handler try).
   */

  *_retval = PR_FALSE;
  return NS_OK;
}

/*virtual*/ nsresult 
sbBaseDeviceFirmwareHandler::OnCanUpdate(sbIDevice *aDevice, 
                                         PRBool *_retval)
{
  TRACE(("[%s]", __FUNCTION__));
  /**
   * Here is where you will want to verify the incoming sbIDevice object
   * to determine if your handler can support updating the firmware on
   * the device. _retval should be set to either PR_TRUE (yes, can update) 
   * or PR_FALSE (no, cannot update).
   */

  return NS_ERROR_NOT_IMPLEMENTED;
}

/*virtual*/ nsresult
sbBaseDeviceFirmwareHandler::OnCancel()
{
  TRACE(("[%s]", __FUNCTION__));
  /**
   * Cancel whatever operation is happening. This is a synchronous call.
   * After this call returns, your object will be destroyed.
   */

  return NS_ERROR_NOT_IMPLEMENTED;
}

/*virtual*/ nsresult
sbBaseDeviceFirmwareHandler::OnRefreshInfo()
{
  TRACE(("[%s]", __FUNCTION__));
  /**
   * Here is where you will want to refresh the info for your handler. 
   * This includes the latest firmware version, firmware location, reset instructions
   * and release notes locations.
   *
   * Always use CreateProxiedURI when creating the nsIURIs for the firmware, 
   * reset instructions and release notes location. This will ensure
   * that the object is thread-safe and it is created in a thread-safe manner.
   *
   * This method must be asynchronous and should not block the main thread. 
   * Progress for this operation is also expected. The flow of expected events 
   * is as follows: firmware refresh info start, N * firmware refresh info progress,
   * firmware refresh info end. See sbIDeviceEvent for more information about
   * event payload.
   *
   * Events must be sent to both the device and the listener (if it is specified 
   * during the call).
   */

  return NS_ERROR_NOT_IMPLEMENTED;
}

/*virtual*/ nsresult
sbBaseDeviceFirmwareHandler::OnUpdate(sbIDeviceFirmwareUpdate *aFirmwareUpdate)
{
  TRACE(("[%s]", __FUNCTION__));
  /**
   * Here is where you will want to actually perform the firmware update
   * on the device. The firmware update object will contain the local 
   * location for the firmware image. It also contains the version of the 
   * firmware image. 
   *
   * The implementation of this method must be asynchronous and not block
   * the main thread. The flow of expected events is as follows:
   * firmware update start, firmware write start, firmware write progress, 
   * firmware write end, firmware verify start, firmware verify progress, 
   * firmware verify end, firmware update end.
   *
   * See sbIDeviceEvent for more infomation about event payload.
   *
   * Events must be sent to both the device and the listener (if it is specified 
   * during the call).
   */

  return NS_ERROR_NOT_IMPLEMENTED;
}

/*virtual*/ nsresult
sbBaseDeviceFirmwareHandler::OnVerifyDevice()
{
  TRACE(("[%s]", __FUNCTION__));
  /**
   * Here is where you will want to verify the firmware on the device itself
   * to ensure that it is not corrupt. Whichever method you use will most likely
   * be device specific.
   * 
   * The implementation of this method must be asynchronous and not block
   * the main thread. The flow of expected events is as follows:
   * firmware verify start, firmware verify progress, firmware verify end.
   *
   * If any firmware verify error events are sent during the process
   * the firmware is considered corrupted.
   *
   * See sbIDeviceEvent for more infomation about event payload.
   *
   * Events must be sent to both the device and the listener (if it is specified 
   * during the call).
   */

  return NS_ERROR_NOT_IMPLEMENTED;
}

/*virtual*/ nsresult
sbBaseDeviceFirmwareHandler::OnVerifyUpdate(sbIDeviceFirmwareUpdate *aFirmwareUpdate)
{
  TRACE(("[%s]", __FUNCTION__));
  /**
   * Here is where you should provide a way to verify the firmware update
   * image itself to make sure that it is not corrupt in any way.
   *
   * The implementation of this method must be asynchronous and not block
   * the main thread. The flow of expected events is as follows:
   * firmware image verify start, firmware image verify progress, firmware 
   * image verify end.
   *
   * If any firmware image verify error events are sent during the process
   * the firmware image is considered corrupted.
   *
   * See sbIDeviceEvent for more infomation about event payload. 
   *
   * Events must be sent to both the device and the listener (if it is specified 
   * during the call).
   */

  return NS_ERROR_NOT_IMPLEMENTED;
}

/*virtual*/ nsresult
sbBaseDeviceFirmwareHandler::OnHttpRequestCompleted()
{
  TRACE(("[%s]", __FUNCTION__));
  return NS_ERROR_NOT_IMPLEMENTED;
}

// ----------------------------------------------------------------------------
// sbIDeviceFirmwareHandler
// ----------------------------------------------------------------------------

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::GetContractId(nsAString & aContractId)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);

  nsAutoMonitor mon(mMonitor);
  aContractId = mContractId;

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::GetLatestFirmwareLocation(nsIURI * *aLatestFirmwareLocation)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aLatestFirmwareLocation);

  *aLatestFirmwareLocation = nsnull;

  nsAutoMonitor mon(mMonitor);
  
  if(!mFirmwareLocation) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = mFirmwareLocation->Clone(aLatestFirmwareLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::GetLatestFirmwareVersion(PRUint32 *aLatestFirmwareVersion)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aLatestFirmwareVersion);

  nsAutoMonitor mon(mMonitor);
  *aLatestFirmwareVersion = mFirmwareVersion;
  
  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::GetLatestFirmwareReadableVersion(nsAString & aLatestFirmwareReadableVersion)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);

  nsAutoMonitor mon(mMonitor);
  aLatestFirmwareReadableVersion = mReadableFirmwareVersion;
  
  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::GetReleaseNotesLocation(nsIURI * *aReleaseNotesLocation)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aReleaseNotesLocation);

  *aReleaseNotesLocation = nsnull;

  nsAutoMonitor mon(mMonitor);
  
  if(!mReleaseNotesLocation) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = mReleaseNotesLocation->Clone(aReleaseNotesLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::GetResetInstructionsLocation(nsIURI * *aResetInstructionsLocation)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aResetInstructionsLocation);

  *aResetInstructionsLocation = nsnull;

  nsAutoMonitor mon(mMonitor);
  
  if(!mResetInstructionsLocation) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = mResetInstructionsLocation->Clone(aResetInstructionsLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::GetCustomerSupportLocation(nsIURI * *aSupportLocation)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aSupportLocation);
  
  *aSupportLocation = nsnull;

  nsAutoMonitor mon(mMonitor);
  
  if(!mSupportLocation) {
    return NS_OK;
  }

  nsresult rv = mSupportLocation->Clone(aSupportLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::GetRegisterLocation(nsIURI * *aRegisterLocation)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aRegisterLocation);

  *aRegisterLocation = nsnull;

  nsAutoMonitor mon(mMonitor);
  
  if(!mRegisterLocation) {
    return NS_OK;
  }

  nsresult rv = mRegisterLocation->Clone(aRegisterLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceFirmwareHandler::GetNeedsRecoveryMode(PRBool *aNeedsRecoveryMode)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aNeedsRecoveryMode);

  nsAutoMonitor mon(mMonitor);
  *aNeedsRecoveryMode = mNeedsRecoveryMode;

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::CanHandleDevice(sbIDevice *aDevice, 
                                               PRBool *_retval)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aDevice);
  NS_ENSURE_ARG_POINTER(_retval);

  nsAutoMonitor mon(mMonitor);

  nsresult rv = OnCanHandleDevice(aDevice, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::CanUpdate(sbIDevice *aDevice, 
                                       PRBool *_retval)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aDevice);
  NS_ENSURE_ARG_POINTER(_retval);

  nsAutoMonitor mon(mMonitor);

  nsresult rv = OnCanUpdate(aDevice, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceFirmwareHandler::Bind(sbIDevice *aDevice,
                                  sbIDeviceEventListener *aListener)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aDevice);

  nsAutoMonitor mon(mMonitor);

  NS_ENSURE_FALSE(mDevice, NS_ERROR_ALREADY_INITIALIZED);
  NS_ENSURE_FALSE(mListener, NS_ERROR_ALREADY_INITIALIZED);

  mDevice = aDevice;
  
  if(aListener) {
    nsCOMPtr<nsIThread> target = do_GetMainThread();
    nsresult rv = do_GetProxyForObject(target, 
      NS_GET_IID(sbIDeviceEventListener), 
      aListener, 
      NS_PROXY_ALWAYS | NS_PROXY_ASYNC, 
      getter_AddRefs(mListener));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceFirmwareHandler::Unbind()
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  
  nsAutoMonitor mon(mMonitor);

  mDevice = nsnull;
  mListener = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceFirmwareHandler::Cancel()
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = OnCancel();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::RefreshInfo()
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);

  nsAutoMonitor mon(mMonitor);

  nsresult rv = OnRefreshInfo();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::Update(sbIDeviceFirmwareUpdate *aFirmwareUpdate)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aFirmwareUpdate);

  nsAutoMonitor mon(mMonitor);

  nsresult rv = OnUpdate(aFirmwareUpdate);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::VerifyDevice()
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);

  nsAutoMonitor mon(mMonitor);

  nsresult rv = OnVerifyDevice();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceFirmwareHandler::VerifyUpdate(sbIDeviceFirmwareUpdate *aFirmwareUpdate)
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_NOT_INITIALIZED);

  nsAutoMonitor mon(mMonitor);

  nsresult rv = OnVerifyUpdate(aFirmwareUpdate);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// ----------------------------------------------------------------------------
// nsITimerCallback
// ----------------------------------------------------------------------------

NS_IMETHODIMP
sbBaseDeviceFirmwareHandler::Notify(nsITimer *aTimer) 
{
  TRACE(("[%s]", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aTimer);
  nsresult rv = NS_ERROR_UNEXPECTED;

  if(aTimer == mXMLHttpRequestTimer) {
    NS_ENSURE_STATE(mXMLHttpRequest);

    PRInt32 state = 0;
    rv = mXMLHttpRequest->GetReadyState(&state);
    NS_ENSURE_SUCCESS(rv, rv);

    if(state == HTTP_STATE_COMPLETED) {
      rv = OnHttpRequestCompleted();
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "OnHttpRequestCompleted failed");

      rv = mXMLHttpRequestTimer->Cancel();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}
