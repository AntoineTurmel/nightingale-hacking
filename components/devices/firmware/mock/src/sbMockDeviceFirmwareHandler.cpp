/* vim: set sw=2 :miv */
/*
//
// BEGIN SONGBIRD GPL
// 
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2008 POTI, Inc.
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

#include "sbMockDeviceFirmwareHandler.h"

#include <nsIDOMDocument.h>
#include <nsIDOMElement.h>
#include <nsIDOMNode.h>
#include <nsIDOMNodeList.h>
#include <nsIInputStream.h>
#include <nsIInputStreamPump.h>
#include <nsIPrefBranch.h>
#include <nsIPrefService.h>
#include <nsISupportsUtils.h>
#include <nsIVariant.h>
#include <nsIWritablePropertyBag2.h>

#include <nsAutoLock.h>
#include <nsCOMPtr.h>
#include <nsComponentManagerUtils.h>
#include <nsNetUtil.h>
#include <nsServiceManagerUtils.h>
#include <nsXPCOMCIDInternal.h>

#include <sbIDevice.h>
#include <sbIDeviceFirmwareUpdate.h>
#include <sbIDeviceProperties.h>

#include <sbVariantUtils.h>

#define SB_MOCK_DEVICE_FIRMWARE_URL \
  "http://dingo.songbirdnest.com/~aus/firmware/firmware.xml"
#define SB_MOCK_DEVICE_RESET_URL \
  "http://dingo.songbirdnest.com/~aus/firmware/reset.html"
#define SB_MOCK_DEVICE_RELEASE_NOTES_URL \
  "http://dingo.songbirdnest.com/~aus/firmware/release_notes.html"
#define SB_MOCK_DEVICE_SUPPORT_URL \
  "http://dingo.songbirdnest.com/~aus/firmware/support.html"
#define SB_MOCK_DEVICE_REGISTER_URL \
  "http://dingo.songbirdnest.com/~aus/firmware/register.html"

NS_IMPL_ISUPPORTS_INHERITED1(sbMockDeviceFirmwareHandler, 
                             sbBaseDeviceFirmwareHandler,
                             nsIStreamListener)

SB_DEVICE_FIRMWARE_HANLDER_REGISTERSELF_IMPL(sbMockDeviceFirmwareHandler,
                                             "Songbird Device Firmware Tester - Mock Device Firmware Handler")

sbMockDeviceFirmwareHandler::sbMockDeviceFirmwareHandler()
{
}

sbMockDeviceFirmwareHandler::~sbMockDeviceFirmwareHandler()
{
}

/*virtual*/ nsresult 
sbMockDeviceFirmwareHandler::OnInit()
{
  mContractId = 
    NS_LITERAL_STRING("@songbirdnest.com/Songbird/Device/Firmware/Handler/MockDevice;1");

  nsCOMPtr<nsIURI> uri;
  nsresult rv = CreateProxiedURI(nsDependentCString(SB_MOCK_DEVICE_RESET_URL),
                                 getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  uri.swap(mResetInstructionsLocation);

  rv = CreateProxiedURI(nsDependentCString(SB_MOCK_DEVICE_RELEASE_NOTES_URL),
                        getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  uri.swap(mReleaseNotesLocation);

  return NS_OK;
}

/*virtual*/ nsresult 
sbMockDeviceFirmwareHandler::OnCanHandleDevice(sbIDevice *aDevice, 
                                              PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;

  nsCOMPtr<sbIDeviceProperties> properties;
  nsresult rv = aDevice->GetProperties(getter_AddRefs(properties));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString vendorName;
  rv = properties->GetVendorName(vendorName);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXXAus: Other firmware handlers will probably want to be a 
  //         little bit more stringent.
  if(!vendorName.EqualsLiteral("ACME Inc.")) {
    return NS_OK;
  }

  // Yep, supported!
  *_retval = PR_TRUE;

  return NS_OK;
}

/*virtual*/ nsresult 
sbMockDeviceFirmwareHandler::OnCanUpdate(sbIDevice *aDevice, 
                                         PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  
  // we can update all devices we support
  nsresult rv = OnCanHandleDevice(aDevice, _retval);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

/*virtual*/ nsresult
sbMockDeviceFirmwareHandler::OnCancel()
{
  return NS_OK;
}

/*virtual*/ nsresult
sbMockDeviceFirmwareHandler::OnRefreshInfo()
{
  nsresult rv = SendHttpRequest(NS_LITERAL_CSTRING("GET"), 
                                NS_LITERAL_CSTRING(SB_MOCK_DEVICE_FIRMWARE_URL));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetState(HANDLER_REFRESHING_INFO);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SendDeviceEvent(sbIDeviceEvent::EVENT_FIRMWARE_CFU_START, 
                       nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/*virtual*/ nsresult
sbMockDeviceFirmwareHandler::OnUpdate(sbIDeviceFirmwareUpdate *aFirmwareUpdate)
{
  {
    nsCOMPtr<nsIVariant> shouldFailVariant;
    nsresult rv = 
      mDevice->GetPreference(NS_LITERAL_STRING("testing.firmware.update.fail"), 
                             getter_AddRefs(shouldFailVariant));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint16 dataType = 0;
    rv = shouldFailVariant->GetDataType(&dataType);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool shouldFail = PR_FALSE;
    if(dataType == nsIDataType::VTYPE_BOOL) {
      rv = shouldFailVariant->GetAsBool(&shouldFail);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if(shouldFail) {
      rv = CheckForError(NS_ERROR_FAILURE, 
                         sbIDeviceEvent::EVENT_FIRMWARE_UPDATE_ERROR);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }
  }

  nsCOMPtr<nsIFile> firmwareFile;
  nsresult rv = 
    aFirmwareUpdate->GetFirmwareImageFile(getter_AddRefs(firmwareFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> inputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), firmwareFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStreamPump> inputStreamPump;
  rv = NS_NewInputStreamPump(getter_AddRefs(inputStreamPump), 
                             inputStream, -1, -1, 0, 0, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SendDeviceEvent(sbIDeviceEvent::EVENT_FIRMWARE_UPDATE_START, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = inputStreamPump->AsyncRead(this, firmwareFile);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/*virtual*/ nsresult
sbMockDeviceFirmwareHandler::OnVerifyDevice()
{
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
sbMockDeviceFirmwareHandler::OnVerifyUpdate(sbIDeviceFirmwareUpdate *aFirmwareUpdate)
{
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
sbMockDeviceFirmwareHandler::OnHttpRequestCompleted()
{
  nsresult rv = NS_ERROR_UNEXPECTED;
  handlerstate_t state = GetState();

  switch(state) {
    case HANDLER_REFRESHING_INFO: {
      rv = HandleRefreshInfoRequest();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    break;

    default:
      NS_WARNING("No code!");
  }

  return NS_OK;
}

nsresult 
sbMockDeviceFirmwareHandler::HandleRefreshInfoRequest()
{
  PRUint32 status = 0;
  
  // XXXAus: Check device pref to see if we should simulate a failure!

  nsresult rv = mXMLHttpRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> document;
  rv = mXMLHttpRequest->GetResponseXML(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNodeList> rootNodeList;
  rv = document->GetElementsByTagName(NS_LITERAL_STRING("firmware"),
                                      getter_AddRefs(rootNodeList));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 rootNodeListLength = 0;
  rv = rootNodeList->GetLength(&rootNodeListLength);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXXAus: Only one 'firmware' node is allowed.
  NS_ENSURE_TRUE(rootNodeListLength == 1, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMNode> rootNode;
  rv = rootNodeList->Item(0, getter_AddRefs(rootNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNodeList> childNodes;
  rv = rootNode->GetChildNodes(getter_AddRefs(childNodes));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childNodeListLength = 0;
  rv = childNodes->GetLength(&childNodeListLength);
  NS_ENSURE_SUCCESS(rv, rv);

  for(PRUint32 i = 0; i < childNodeListLength; ++i) {
    nsCOMPtr<nsIDOMNode> domNode;
    rv = childNodes->Item(i, getter_AddRefs(domNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(domNode, &rv);
    if(NS_FAILED(rv)) {
      continue;
    }

    nsString tagName;
    rv = element->GetTagName(tagName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString value;
    rv = element->GetAttribute(NS_LITERAL_STRING("value"), value);
    if(NS_FAILED(rv)) {
      continue;
    }

    // XXXAus: We only support 'version' and 'location' nodes
    //         and they must have a 'value' attribute.
    if(tagName.EqualsLiteral("version")) {
      nsAutoMonitor mon(mMonitor);
      mReadableFirmwareVersion = value;
      mFirmwareVersion = 0x01000001;
    }
    else if(tagName.EqualsLiteral("location")) {
      nsCOMPtr<nsIURI> uri;
      rv = CreateProxiedURI(NS_ConvertUTF16toUTF8(value), 
                            getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoMonitor mon(mMonitor);
      mFirmwareLocation = uri;
    }
  }

  // XXXAus: Populate the fake support and register locations.
  {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = CreateProxiedURI(nsDependentCString(SB_MOCK_DEVICE_SUPPORT_URL),
                                   getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoMonitor mon(mMonitor);
    uri.swap(mSupportLocation);
    mon.Exit();

    rv = CreateProxiedURI(nsDependentCString(SB_MOCK_DEVICE_REGISTER_URL),
                          getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    mon.Enter();
    uri.swap(mRegisterLocation);
    mon.Exit();

    rv = CreateProxiedURI(nsDependentCString(SB_MOCK_DEVICE_RESET_URL),
                          getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    mon.Enter();
    uri.swap(mResetInstructionsLocation);
    mon.Exit();

    nsCOMPtr<nsIVariant> needsRecoveryModeVariant;
    rv = mDevice->GetPreference(NS_LITERAL_STRING("testing.firmware.needRecoveryMode"),
                                getter_AddRefs(needsRecoveryModeVariant));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint16 dataType = 0;
    rv = needsRecoveryModeVariant->GetDataType(&dataType);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool needsRecoveryMode = PR_FALSE;
    if(dataType == nsIDataType::VTYPE_BOOL) {
      rv = needsRecoveryModeVariant->GetAsBool(&needsRecoveryMode);
      NS_ENSURE_SUCCESS(rv, rv);

      mon.Enter();
      mNeedsRecoveryMode = needsRecoveryMode;
      mon.Exit();
    }
  }

  {
    nsCOMPtr<nsIVariant> shouldFailVariant;
    rv = mDevice->GetPreference(NS_LITERAL_STRING("testing.firmware.cfu.fail"),
                                getter_AddRefs(shouldFailVariant));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint16 dataType = 0;
    rv = shouldFailVariant->GetDataType(&dataType);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool shouldFail = PR_FALSE;
    if(dataType == nsIDataType::VTYPE_BOOL) {
      rv = shouldFailVariant->GetAsBool(&shouldFail);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if(shouldFail) {
      rv = CheckForError(NS_ERROR_FAILURE, 
                         sbIDeviceEvent::EVENT_FIRMWARE_CFU_ERROR);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_ERROR_FAILURE;
    }
  }

  // XXXAus: Looks like we have an update location and version.
  //         Figure out if we should say there is an update available
  //         or not by comparing current device firmware version
  //         and the one we got from the web service.

  // XXXAus: For now just pretend like it's always new.
  nsCOMPtr<nsIVariant> data = 
    sbNewVariant(PR_TRUE, nsIDataType::VTYPE_BOOL).get();

  rv = SendDeviceEvent(sbIDeviceEvent::EVENT_FIRMWARE_CFU_END, 
                       data);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// -----------------------------------------------------------------------------
// nsIStreamListener
// -----------------------------------------------------------------------------
NS_IMETHODIMP
sbMockDeviceFirmwareHandler::OnStartRequest(nsIRequest *aRequest, 
                                            nsISupports *aContext)
{
  nsresult rv = 
    SendDeviceEvent(sbIDeviceEvent::EVENT_FIRMWARE_WRITE_START, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbMockDeviceFirmwareHandler::OnDataAvailable(nsIRequest *aRequest,
                                             nsISupports *aContext,
                                             nsIInputStream *aStream,
                                             PRUint32 aOffset,
                                             PRUint32 aCount)
{
  nsresult rv = NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIFile> firmwareFile = do_QueryInterface(aContext, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 availableBytes = 0;
  rv = aStream->Available(&availableBytes);
  NS_ENSURE_SUCCESS(rv, rv);

  char *buffer = 
    reinterpret_cast<char *>(NS_Alloc(availableBytes * sizeof(char)));
  NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 readBytes = 0;
  rv = aStream->Read(buffer, availableBytes, &readBytes);
  
  NS_Free(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 fileSize = 0;
  rv = firmwareFile->GetFileSize(&fileSize);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 progress = static_cast<PRUint32>(aOffset * 100 / fileSize);

  nsCOMPtr<nsIVariant> progressVariant = sbNewVariant(progress).get();
  rv = SendDeviceEvent(sbIDeviceEvent::EVENT_FIRMWARE_WRITE_PROGRESS, progressVariant);
  NS_ENSURE_SUCCESS(rv, rv);

  PR_Sleep(PR_MillisecondsToInterval(50));

  {
    nsCOMPtr<nsIVariant> shouldFailVariant;
    nsresult rv = 
      mDevice->GetPreference(NS_LITERAL_STRING("testing.firmware.write.fail"),
                             getter_AddRefs(shouldFailVariant));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint16 dataType = 0;
    rv = shouldFailVariant->GetDataType(&dataType);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool shouldFail = PR_FALSE;
    if(dataType == nsIDataType::VTYPE_BOOL) {
      rv = shouldFailVariant->GetAsBool(&shouldFail);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if(shouldFail) {
      rv = CheckForError(NS_ERROR_FAILURE, 
                         sbIDeviceEvent::EVENT_FIRMWARE_WRITE_ERROR);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
sbMockDeviceFirmwareHandler::OnStopRequest(nsIRequest *aRequest,
                                           nsISupports *aContext,
                                           nsresult aResultCode)
{
  nsresult rv = 
    CheckForError(aResultCode, sbIDeviceEvent::EVENT_FIRMWARE_WRITE_ERROR);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SendDeviceEvent(sbIDeviceEvent::EVENT_FIRMWARE_WRITE_END, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SendDeviceEvent(sbIDeviceEvent::EVENT_FIRMWARE_UPDATE_END, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
