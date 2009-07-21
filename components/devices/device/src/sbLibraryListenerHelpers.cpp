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

#include "sbLibraryListenerHelpers.h"

#include <pratom.h>
#include <prlog.h>

#include "sbIMediaItem.h"
#include "sbIMediaList.h"
#include "sbIOrderableMediaList.h"
#include "sbBaseDevice.h"
#include "sbLibraryUtils.h"

#include <nsIURI.h>

//
// To log this module, set the following environment variable:
//   NSPR_LOG_MODULES=sbLibraryListenerHelpers:5
//

#ifdef PR_LOGGING
  static PRLogModuleInfo* gLibraryListenerHelpersLog = nsnull;
# define TRACE(args) PR_LOG(gLibraryListenerHelpersLog, PR_LOG_DEBUG, args)
# define LOG(args)   PR_LOG(gLibraryListenerHelpersLog, PR_LOG_WARN, args)
# ifdef __GNUC__
#   define __FUNCTION__ __PRETTY_FUNCTION__
# endif /* __GNUC__ */
#else
# define TRACE(args) /* nothing */
# define LOG(args)   /* nothing */
#endif

nsresult 
sbBaseIgnore::SetIgnoreListener(PRBool aIgnoreListener) {
  if (aIgnoreListener) {
    PR_AtomicIncrement(&mIgnoreListenerCounter);
  } else {
    PRInt32 result = PR_AtomicDecrement(&mIgnoreListenerCounter);
    NS_ASSERTION(result >= 0, "invalid device library ignore listener counter");
  }
  return NS_OK;
}

nsresult sbBaseIgnore::IgnoreMediaItem(sbIMediaItem * aItem) {
  NS_ENSURE_ARG_POINTER(aItem);
  
  nsString guid;
  nsresult rv = aItem->GetGuid(guid);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsAutoLock lock(mLock);
  
  PRInt32 itemCount = 0;
  // We don't care if this fails, itemCount is zero in that case which is fine
  // We have to assume failure is always due to "not found"
  mIgnored.Get(guid, &itemCount);
  if (!mIgnored.Put(guid, ++itemCount))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

/**
 * Returns PR_TRUE if the item is currently being ignored
 */
PRBool sbBaseIgnore::MediaItemIgnored(sbIMediaItem * aItem) {
  NS_ENSURE_ARG_POINTER(aItem);
  
  nsString guid;
  // If ignoring all or ignoring this specific item return PR_TRUE
  if (mIgnoreListenerCounter > 0)
    return PR_TRUE;
  nsAutoLock lock(mLock);
  nsresult rv = aItem->GetGuid(guid);
  
  // If the guid was valid and it's in our ignore list then it's ignored
  return (NS_SUCCEEDED(rv) && mIgnored.Get(guid, nsnull)) ? PR_TRUE : 
                                                            PR_FALSE;
}

nsresult sbBaseIgnore::UnignoreMediaItem(sbIMediaItem * aItem) {
  nsString guid;
  nsresult rv = aItem->GetGuid(guid);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsAutoLock lock(mLock);
  PRInt32 itemCount = 0;
  if (!mIgnored.Get(guid, &itemCount)) {
    // We're out of balance at this point
    return NS_ERROR_FAILURE;
  }
  // If the item count is less than zero then remove the guid else just decrement it
  if (--itemCount == 0) {
    mIgnored.Remove(guid);
  }
  else
    mIgnored.Put(guid, itemCount);
  return NS_OK;
}

//sbBaseDeviceLibraryListener class.
NS_IMPL_THREADSAFE_ISUPPORTS2(sbBaseDeviceLibraryListener, 
                              sbIDeviceLibraryListener,
                              nsISupportsWeakReference);

sbBaseDeviceLibraryListener::sbBaseDeviceLibraryListener() 
: mDevice(nsnull)
{
#ifdef PR_LOGGING
  if (!gLibraryListenerHelpersLog)
    gLibraryListenerHelpersLog = PR_NewLogModule("sbLibraryListenerHelpers");
#endif
}

sbBaseDeviceLibraryListener::~sbBaseDeviceLibraryListener()
{
}

nsresult
sbBaseDeviceLibraryListener::Init(sbBaseDevice* aDevice)
{
  NS_ENSURE_ARG_POINTER(aDevice);

  mDevice = aDevice;

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceLibraryListener::OnBatchBegin(sbIMediaList *aMediaList)
{
  return mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_BATCH_BEGIN,
                              nsnull, aMediaList);
}

NS_IMETHODIMP 
sbBaseDeviceLibraryListener::OnBatchEnd(sbIMediaList *aMediaList)
{
  return mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_BATCH_END,
                              nsnull, aMediaList);
}

NS_IMETHODIMP
sbBaseDeviceLibraryListener::OnItemAdded(sbIMediaList *aMediaList,
                                         sbIMediaItem *aMediaItem,
                                         PRUint32 aIndex,
                                         PRBool *aNoMoreForBatch)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(aMediaItem);
  NS_ENSURE_ARG_POINTER(aNoMoreForBatch);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  *aNoMoreForBatch = PR_FALSE;

  nsresult rv;

  // Always listen to all added lists.
  nsCOMPtr<sbIMediaList> list = do_QueryInterface(aMediaItem);
  if (list) {
    rv = mDevice->ListenToList(list);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }

  //XXXAus: Before adding to queue, make sure it doesn't come from
  //another device. Ask DeviceManager for the device library
  //containing this item.
  if (list) {
    // new playlist
    rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_NEW_PLAYLIST,
                              aMediaItem, aMediaList, aIndex);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Hide the item. It is the responsibility of the device to make the item
    // visible when the transfer is successful.
    rv = aMediaItem->SetProperty(NS_LITERAL_STRING(SB_PROPERTY_HIDDEN), 
                                 NS_LITERAL_STRING("1"));

    rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_WRITE,
                              aMediaItem, aMediaList, aIndex);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceLibraryListener::OnBeforeItemRemoved(sbIMediaList *aMediaList,
                                                 sbIMediaItem *aMediaItem,
                                                 PRUint32 aIndex,
                                                 PRBool *aNoMoreForBatch)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(aMediaItem);
  NS_ENSURE_ARG_POINTER(aNoMoreForBatch);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  *aNoMoreForBatch = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceLibraryListener::OnAfterItemRemoved(sbIMediaList *aMediaList, 
                                                sbIMediaItem *aMediaItem,
                                                PRUint32 aIndex,
                                                PRBool *aNoMoreForBatch)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(aMediaItem);
  NS_ENSURE_ARG_POINTER(aNoMoreForBatch);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  *aNoMoreForBatch = PR_FALSE;

  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }
  
  nsresult rv;
  
  rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_DELETE,
                            aMediaItem, aMediaList, aIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceLibraryListener::OnListCleared(sbIMediaList *aMediaList,
                                           PRBool* aNoMoreForBatch)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(aNoMoreForBatch);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  *aNoMoreForBatch = PR_FALSE;
  
  /* yay, we're going to wipe the device! */

  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }
  
  nsresult rv;
  rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_WIPE,
                            aMediaList);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP 
sbBaseDeviceLibraryListener::OnItemUpdated(sbIMediaList *aMediaList,
                                           sbIMediaItem *aMediaItem,
                                           sbIPropertyArray* aProperties,
                                           PRBool* aNoMoreForBatch)
{
  NS_ENSURE_ARG_POINTER(aMediaItem);
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(aProperties);
  NS_ENSURE_ARG_POINTER(aNoMoreForBatch);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  *aNoMoreForBatch = PR_FALSE;

  if(MediaItemIgnored(aMediaItem)) {
    return NS_OK;
  }

  nsresult rv;
  rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_UPDATE,
                            aMediaItem, aMediaList);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceLibraryListener::OnItemMoved(sbIMediaList *aMediaList,
                                         PRUint32 aFromIndex,
                                         PRUint32 aToIndex,
                                         PRBool *aNoMoreForBatch)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(aNoMoreForBatch);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);
  
  *aNoMoreForBatch = PR_FALSE;
  
  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }

  nsresult rv;
  rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_MOVE,
                            nsnull, aMediaList, aFromIndex, aToIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceLibraryListener::OnItemCopied(sbIMediaItem *aSourceItem,
                                          sbIMediaItem *aDestItem)
{
  NS_ENSURE_ARG_POINTER(aSourceItem);
  NS_ENSURE_ARG_POINTER(aDestItem);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);
  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceLibraryListener::OnBeforeCreateMediaItem(nsIURI *aContentUri,
                                                     sbIPropertyArray *aProperties,
                                                     PRBool aAllowDuplicates,
                                                     PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aContentUri);
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);
  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceLibraryListener::OnBeforeCreateMediaList(const nsAString & aType,
                                                     sbIPropertyArray *aProperties,
                                                     PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);
  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceLibraryListener::OnBeforeAdd(sbIMediaItem *aMediaItem,
                                         PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aMediaItem);
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);
  return NS_OK;
}

NS_IMETHODIMP sbBaseDeviceLibraryListener::OnBeforeAddAll(sbIMediaList *aMediaList,
                                                          PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);
  return NS_OK;
}

NS_IMETHODIMP sbBaseDeviceLibraryListener::OnBeforeAddSome(nsISimpleEnumerator *aMediaItems,
                                                           PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aMediaItems);
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);
  return NS_OK;
}

NS_IMETHODIMP sbBaseDeviceLibraryListener::OnBeforeClear(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);
  return NS_OK;
}


//sbILocalDatabaseMediaListCopyListener
NS_IMPL_THREADSAFE_ISUPPORTS1(sbDeviceBaseLibraryCopyListener, 
                              sbILocalDatabaseMediaListCopyListener);

sbDeviceBaseLibraryCopyListener::sbDeviceBaseLibraryCopyListener()
: mDevice(nsnull)
{
#ifdef PR_LOGGING
  if (!gLibraryListenerHelpersLog)
    gLibraryListenerHelpersLog = PR_NewLogModule("sbLibraryListenerHelpers");
#endif
}

sbDeviceBaseLibraryCopyListener::~sbDeviceBaseLibraryCopyListener()
{

}

nsresult
sbDeviceBaseLibraryCopyListener::Init(sbBaseDevice* aDevice)
{
  NS_ENSURE_ARG_POINTER(aDevice);

  mDevice = aDevice;

  return NS_OK;
}

NS_IMETHODIMP
sbDeviceBaseLibraryCopyListener::OnItemCopied(sbIMediaItem *aSourceItem, 
                                              sbIMediaItem *aDestItem)
{
  NS_ENSURE_ARG_POINTER(aSourceItem);
  NS_ENSURE_ARG_POINTER(aDestItem);

  nsresult rv;

  #if PR_LOGGING
  nsCOMPtr<nsIURI> srcURI, destURI;
  nsCString srcSpec, destSpec;
  nsCOMPtr<sbILibrary> srcLib, destLib;
  nsString srcLibId, destLibId;
  rv = aSourceItem->GetContentSrc(getter_AddRefs(srcURI));
  if (NS_SUCCEEDED(rv)) {
    rv = srcURI->GetSpec(srcSpec);
  }
  rv = aSourceItem->GetLibrary(getter_AddRefs(srcLib));
  if (NS_SUCCEEDED(rv)) {
    rv = srcLib->GetGuid(srcLibId);
  }
  rv = aDestItem->GetContentSrc(getter_AddRefs(destURI));
  if (NS_SUCCEEDED(rv)) {
    rv = destURI->GetSpec(destSpec);
  }
  rv = aDestItem->GetLibrary(getter_AddRefs(destLib));
  if (NS_SUCCEEDED(rv)) {
    rv = destLib->GetGuid(destLibId);
  }
  TRACE(("%s: %s::%s -> %s::%s",
         __FUNCTION__,
         NS_ConvertUTF16toUTF8(srcLibId).get(), srcSpec.get(),
         NS_ConvertUTF16toUTF8(destLibId).get(), destSpec.get()));
  #endif

  rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_READ,
                            aSourceItem, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(sbBaseDeviceMediaListListener,
                              sbIMediaListListener)

sbBaseDeviceMediaListListener::sbBaseDeviceMediaListListener()
: mDevice(nsnull)
{
  
}

sbBaseDeviceMediaListListener::~sbBaseDeviceMediaListListener()
{
  
}

nsresult
sbBaseDeviceMediaListListener::Init(sbBaseDevice* aDevice)
{
  NS_ENSURE_ARG_POINTER(aDevice);
  NS_ENSURE_FALSE(mDevice, NS_ERROR_ALREADY_INITIALIZED);
  mDevice = aDevice;
  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceMediaListListener::OnItemAdded(sbIMediaList *aMediaList,
                                           sbIMediaItem *aMediaItem,
                                           PRUint32 aIndex,
                                           PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(aMediaItem);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }

  nsresult rv;

  nsCOMPtr<sbILibrary> lib = do_QueryInterface(aMediaList);
  if (lib) {
    // umm, why are we listening to a library adding an item?
    *_retval = PR_FALSE;
    return NS_OK;
  }

  nsCOMPtr<sbIMediaList> list = do_QueryInterface(aMediaItem);
  if (list) {
    // a list being added to a list? we don't care, I think?
  } else {
    rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_WRITE,
                              aMediaItem, aMediaList, aIndex);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  if (_retval) {
    *_retval = PR_FALSE; /* don't stop */
  }

  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceMediaListListener::OnBeforeItemRemoved(sbIMediaList *aMediaList,
                                                   sbIMediaItem *aMediaItem,
                                                   PRUint32 aIndex,
                                                   PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceMediaListListener::OnAfterItemRemoved(sbIMediaList *aMediaList,
                                                  sbIMediaItem *aMediaItem,
                                                  PRUint32 aIndex,
                                                  PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(aMediaItem);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }

  nsresult rv;
  
  rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_DELETE,
                            aMediaItem, aMediaList, aIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  if (_retval) {
    *_retval = PR_FALSE; /* don't stop */
  }

  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceMediaListListener::OnItemUpdated(sbIMediaList *aMediaList,
                                             sbIMediaItem *aMediaItem,
                                             sbIPropertyArray *aProperties,
                                             PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceMediaListListener::OnItemMoved(sbIMediaList *aMediaList,
                                           PRUint32 aFromIndex,
                                           PRUint32 aToIndex,
                                           PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }

  nsresult rv;
  rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_MOVE,
                            nsnull, aMediaList, aFromIndex, aToIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (_retval) {
    *_retval = PR_FALSE; /* don't stop */
  }

  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceMediaListListener::OnListCleared(sbIMediaList *aMediaList,
                                             PRBool * /* aNoMoreForBatch */)
{
  NS_ENSURE_ARG_POINTER(aMediaList);
  
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  // Check if we're ignoring then do nothing

  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }
  
  // Send the wipe request
  nsresult rv;
  rv = mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_WIPE,
                            aMediaList);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbBaseDeviceMediaListListener::OnBatchBegin(sbIMediaList *aMediaList)
{
  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }
  return mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_BATCH_BEGIN,
                              nsnull, aMediaList);
}

NS_IMETHODIMP
sbBaseDeviceMediaListListener::OnBatchEnd(sbIMediaList *aMediaList)
{
  if(MediaItemIgnored(aMediaList)) {
    return NS_OK;
  }
  return mDevice->PushRequest(sbBaseDevice::TransferRequest::REQUEST_BATCH_END,
                              nsnull, aMediaList);
}
