/* vim: set sw=2 :miv */
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

#include "sbBaseDevice.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <vector>

#include <prtime.h>

#include <nsIDOMParser.h>
#include <nsIFileURL.h>
#include <nsIPropertyBag2.h>
#include <nsITimer.h>
#include <nsIURI.h>
#include <nsIVariant.h>
#include <nsIPrefService.h>
#include <nsIPrefBranch.h>
#include <nsIProxyObjectManager.h>

#include <nsArrayUtils.h>
#include <nsAutoLock.h>
#include <nsAutoPtr.h>
#include <nsComponentManagerUtils.h>
#include <nsCRT.h>
#include <nsMemory.h>
#include <nsNetUtil.h>
#include <nsServiceManagerUtils.h>
#include <nsThreadUtils.h>
#include <nsIDOMDocument.h>
#include <nsIDOMWindow.h>
#include <nsIPromptService.h>
#include <nsIScriptSecurityManager.h>
#include <nsISupportsPrimitives.h>
#include <nsIWritablePropertyBag.h>
#include <nsIWritablePropertyBag2.h>
#include <nsIXMLHttpRequest.h>

#include <sbIDeviceContent.h>
#include <sbIDeviceCapabilities.h>
#include <sbIDeviceCapabilitiesRegistrar.h>
#include <sbIDeviceEvent.h>
#include <sbIDeviceHelper.h>
#include <sbIDeviceManager.h>
#include <sbIDeviceProperties.h>
#include <sbILibrary.h>
#include <sbILibraryDiffingService.h>
#include <sbIMediaItem.h>
#include <sbIMediaList.h>
#include <sbIMediaFileManager.h>
#include <sbIOrderableMediaList.h>
#include <sbIPrompter.h>
#include <sbITranscodeManager.h>
#include <sbITranscodeAlbumArt.h>
#include <sbArray.h>
#include <sbLocalDatabaseCID.h>
#include <sbPrefBranch.h>
#include <sbMemoryUtils.h>
#include <sbProxiedComponentManager.h>
#include <sbStringBundle.h>
#include <sbStringUtils.h>
#include <sbVariantUtils.h>

#include "sbDeviceEnsureSpaceForWrite.h"
#include "sbDeviceLibrary.h"
#include "sbDeviceStatus.h"
#include "sbDeviceUtils.h"
#include "sbDeviceXMLCapabilities.h"
#include "sbLibraryListenerHelpers.h"
#include "sbLibraryUtils.h"
#include "sbProxyUtils.h"
#include "sbStandardDeviceProperties.h"
#include "sbStandardProperties.h"
#include "sbVariantUtils.h"

/*
 * To log this module, set the following environment variable:
 *   NSPR_LOG_MODULES=sbBaseDevice:5
 */
#undef LOG
#undef TRACE
#ifdef PR_LOGGING
PRLogModuleInfo* gBaseDeviceLog = nsnull;
#define LOG(args)   PR_LOG(gBaseDeviceLog, PR_LOG_WARN,  args)
#define TRACE(args) PR_LOG(gBaseDeviceLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)  do{ } while(0)
#define TRACE(args) do { } while(0)
#endif

#define DEFAULT_COLUMNSPEC_DEVICE_LIBRARY SB_PROPERTY_TRACKNAME " 265 "     \
                                          SB_PROPERTY_DURATION " 43 "       \
                                          SB_PROPERTY_ARTISTNAME " 177 a "  \
                                          SB_PROPERTY_ALBUMNAME " 159 "     \
                                          SB_PROPERTY_GENRE " 53 "          \
                                          SB_PROPERTY_RATING   " 80"        \

// preference names
#define PREF_DEVICE_PREFERENCES_BRANCH "songbird.device."
#define PREF_DEVICE_LIBRARY_BASE "library."
#define PREF_WARNING "warning."
#define PREF_ORGANIZE_PREFIX "media_management.library."
#define PREF_ORGANIZE_ENABLED "media_management.library.enabled"
#define PREF_ORGANIZE_DIR_FORMAT "media_management.library.format.dir"
#define PREF_ORGANIZE_FILE_FORMAT "media_management.library.format.file"

#define BATCH_TIMEOUT 200 /* number of milliseconds to wait for batching */

NS_IMPL_THREADSAFE_ISUPPORTS0(sbBaseDevice::TransferRequest)

class MediaListListenerAttachingEnumerator : public sbIMediaListEnumerationListener
{
public:
  MediaListListenerAttachingEnumerator(sbBaseDevice* aDevice)
   : mDevice(aDevice)
   {}
  NS_DECL_ISUPPORTS
  NS_DECL_SBIMEDIALISTENUMERATIONLISTENER
private:
  sbBaseDevice* mDevice;
};

NS_IMPL_ISUPPORTS1(MediaListListenerAttachingEnumerator,
                   sbIMediaListEnumerationListener)

NS_IMETHODIMP MediaListListenerAttachingEnumerator::OnEnumerationBegin(sbIMediaList*,
                                                                       PRUint16 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = sbIMediaListEnumerationListener::CONTINUE;
  return NS_OK;
}

NS_IMETHODIMP MediaListListenerAttachingEnumerator::OnEnumeratedItem(sbIMediaList*,
                                                                     sbIMediaItem* aItem,
                                                                     PRUint16 *_retval)
{
  NS_ENSURE_ARG_POINTER(aItem);
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_TRUE(mDevice, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;

  nsCOMPtr<sbIMediaList> list(do_QueryInterface(aItem, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDevice->ListenToList(list);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = sbIMediaListEnumerationListener::CONTINUE;

  return NS_OK;
}

NS_IMETHODIMP MediaListListenerAttachingEnumerator::OnEnumerationEnd(sbIMediaList*,
                                                                     nsresult)
{
  return NS_OK;
}

class ShowMediaListEnumerator : public sbIMediaListEnumerationListener
{
public:
  explicit ShowMediaListEnumerator(PRBool aHideMediaLists);
  NS_DECL_ISUPPORTS
  NS_DECL_SBIMEDIALISTENUMERATIONLISTENER
private:
  PRBool    mHideMediaLists;
  nsString  mHideMediaListsStringValue;
};

NS_IMPL_ISUPPORTS1(ShowMediaListEnumerator ,
                   sbIMediaListEnumerationListener)


ShowMediaListEnumerator::ShowMediaListEnumerator(PRBool aHideMediaLists)
: mHideMediaLists(aHideMediaLists)
{
  mHideMediaListsStringValue = (mHideMediaLists == PR_TRUE) ?
                               NS_LITERAL_STRING("1") :
                               NS_LITERAL_STRING("0");
}

NS_IMETHODIMP ShowMediaListEnumerator::OnEnumerationBegin(sbIMediaList*,
                                                          PRUint16 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = sbIMediaListEnumerationListener::CONTINUE;
  return NS_OK;
}

NS_IMETHODIMP ShowMediaListEnumerator::OnEnumeratedItem(sbIMediaList*,
                                                        sbIMediaItem* aItem,
                                                        PRUint16 *_retval)
{
  NS_ENSURE_ARG_POINTER(aItem);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv = aItem->SetProperty(NS_LITERAL_STRING(SB_PROPERTY_HIDDEN),
                          mHideMediaListsStringValue);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = sbIMediaListEnumerationListener::CONTINUE;

  return NS_OK;
}

NS_IMETHODIMP ShowMediaListEnumerator::OnEnumerationEnd(sbIMediaList*,
                                                        nsresult)
{
  return NS_OK;
}

sbBaseDevice::TransferRequest * sbBaseDevice::TransferRequest::New()
{
  return new TransferRequest();
}

PRBool sbBaseDevice::TransferRequest::IsPlaylist() const
{
  if (!list)
    return PR_FALSE;
  // Is this a playlist
  nsCOMPtr<sbILibrary> libTest = do_QueryInterface(list);
  return libTest ? PR_FALSE : PR_TRUE;
}

PRBool sbBaseDevice::TransferRequest::IsCountable() const
{
  return !IsPlaylist() &&
         type != sbIDevice::REQUEST_UPDATE &&
         (type & REQUEST_FLAG_USER) == 0;
}

void sbBaseDevice::TransferRequest::SetTranscodeProfile(sbITranscodeProfile * aProfile)
{
  // Addref then release on the odd chance we're getting reassigned the same
  // value
  NS_IF_ADDREF(aProfile);
  NS_IF_RELEASE(transcodeProfile);
  transcodeProfile = aProfile;
}

sbBaseDevice::TransferRequest::TransferRequest() :
  transcodeProfile(nsnull),
  contentSrcSet(PR_FALSE),
  destinationMediaPresent(PR_FALSE),
  needsTranscoding(PR_FALSE)
{
}

sbBaseDevice::TransferRequest::~TransferRequest()
{
  NS_IF_RELEASE(transcodeProfile);
}

/**
 * Utility function to check a transfer request queue for proper batching
 */
#if DEBUG
static void CheckRequestBatch(sbBaseDevice::TransferRequestQueue::const_iterator aBegin,
                              sbBaseDevice::TransferRequestQueue::const_iterator aEnd)
{
  PRUint32 lastIndex = 0, lastCount = 0, lastBatchID = 0;
  int lastType = 0;

  for (;aBegin != aEnd; ++aBegin) {
    if (!(*aBegin)->IsCountable()) {
      // we don't care about any non-countable things
      continue;
    }
    if (lastType == 0 || lastType != (*aBegin)->type) {
      // type change
      if (lastIndex != lastCount) {
        printf("Type change with missing items lastBatchID=%ud newBatchID=%ud\n",
               lastBatchID, (*aBegin)->batchID);
        NS_WARNING("Type change with missing items");
      }
      if ((lastType != 0) && ((*aBegin)->batchIndex != 1)) {
        printf ("batch does not start with 1 lastBatchID=%ud newBatchID=%ud\n",
                lastBatchID, (*aBegin)->batchID);
        NS_WARNING("batch does not start with 1");
      }
      if ((*aBegin)->batchCount == 0) {
        printf("empty batch lastBatchID=%ud newBatchID=%ud\n",
               lastBatchID, (*aBegin)->batchID);
        NS_WARNING("empty batch;");
      }
      lastType = (*aBegin)->type;
      lastCount = (*aBegin)->batchCount;
      lastIndex = (*aBegin)->batchIndex;
      lastBatchID = (*aBegin)->batchID;
    } else {
      // continue batch
      if (lastCount != (*aBegin)->batchCount) {
        printf("mismatched batch count "
               "batchID=%ud, batchCount=%ud, this Count=%ud, index=%ud\n",
               lastBatchID, lastCount, (*aBegin)->batchCount, (*aBegin)->batchIndex);
        NS_WARNING("mismatched batch count");
      }
      if (lastIndex + 1 != (*aBegin)->batchIndex) {
        printf("unexpected index batchID=%ud, lastIndex=%ud, index=%ud",
               lastBatchID, lastIndex, (*aBegin)->batchIndex);
        NS_WARNING("unexpected index");
      }
      lastIndex = (*aBegin)->batchIndex;
    }
  }

  // check end of queue too
  NS_ASSERTION(lastIndex == lastCount, "end of queue with missing items");
}
#endif /* DEBUG */

sbBaseDevice::sbBaseDevice() :
  mNextBatchID(1),
  mBatchDepth(0),
  mLastTransferID(0),
  mLastRequestPriority(PR_INT32_MIN),
  mAbortCurrentRequest(PR_FALSE),
  mIgnoreMediaListCount(0),
  mPerTrackOverhead(DEFAULT_PER_TRACK_OVERHEAD),
  mCapabilitiesRegistrarType(sbIDeviceCapabilitiesRegistrar::NONE),
  mPreferenceLock(nsnull),
  mMusicLimitPercent(100)
{
#ifdef PR_LOGGING
  if (!gBaseDeviceLog) {
    gBaseDeviceLog = PR_NewLogModule( "sbBaseDevice" );
  }
#endif /* PR_LOGGING */

  mStateLock = nsAutoLock::NewLock(__FILE__ "::mStateLock");
  NS_ASSERTION(mStateLock, "Failed to allocate state lock");

  mPreviousStateLock = nsAutoLock::NewLock(__FILE__ "::mPreviousStateLock");
  NS_ASSERTION(mPreviousStateLock, "Failed to allocate state lock");

  mRequestMonitor = nsAutoMonitor::NewMonitor(__FILE__ "::mRequestMonitor");
  NS_ASSERTION(mRequestMonitor, "Failed to allocate request monitor");

  mPreferenceLock = nsAutoLock::NewLock(__FILE__ "::mPreferenceLock");
  NS_ASSERTION(mPreferenceLock, "Failed to allocate preference lock");

  // the typical case is 1 library per device
  PRBool success = mOrganizeLibraryPrefs.Init(1);
  NS_ASSERTION(success, "Failed to initialize organize prefs hashtable");
}

sbBaseDevice::~sbBaseDevice()
{
  NS_WARN_IF_FALSE(mBatchDepth != 0,
                   "Device destructed with batches remaining");
  if (mPreferenceLock)
    nsAutoLock::DestroyLock(mPreferenceLock);

  if (mRequestMonitor)
    nsAutoMonitor::DestroyMonitor(mRequestMonitor);

  if (mStateLock)
    nsAutoLock::DestroyLock(mStateLock);

  if (mPreviousStateLock)
    nsAutoLock::DestroyLock(mPreviousStateLock);
}

NS_IMETHODIMP sbBaseDevice::SyncLibraries()
{
  nsresult rv;

  nsCOMPtr<sbIDeviceContent> content;
  rv = GetContent(getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIArray> libraries;
  rv = content->GetLibraries(getter_AddRefs(libraries));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 libraryCount;
  rv = libraries->GetLength(&libraryCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 index = 0; index < libraryCount; ++index) {
    nsCOMPtr<sbIDeviceLibrary> deviceLib =
      do_QueryElementAt(libraries, index, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = deviceLib->Sync();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult sbBaseDevice::PushRequest(const int aType,
                                   sbIMediaItem* aItem,
                                   sbIMediaList* aList,
                                   PRUint32 aIndex,
                                   PRUint32 aOtherIndex,
                                   PRInt32 aPriority)
{
  NS_ENSURE_TRUE(mRequestMonitor, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(aType != 0, NS_ERROR_INVALID_ARG);

  nsRefPtr<TransferRequest> req = TransferRequest::New();
  NS_ENSURE_TRUE(req, NS_ERROR_OUT_OF_MEMORY);
  req->type = aType;
  req->item = aItem;
  req->list = aList;
  req->index = aIndex;
  req->otherIndex = aOtherIndex;
  req->priority = aPriority;

  return PushRequest(req);
}

/**
 * Finds the last countable item in the queue. If not found it returns
 * end
 */
template <class T>
inline
T SBFindLastCountable(T begin, T end)
{
  T const theEnd = end;

  if (begin != end) {
    // Start at the last item not "end" and find the first countable item
    for (--end; end != begin && !(*end)->IsCountable(); --end) ;
    // We'll either be at begin or some point between begin and end
    // If we're at the beginning then we may or may not have found a
    // countable item. So we have to test it
    return (*end)->IsCountable() ? end : theEnd;
  }
  // There are no items, so just return the "end"
  return theEnd;
}

/**
 * Updates the batch count for the request items associated with this batch
 */

template <class T>
void SBUpdateBatchCounts(T        batchEnd,
                         T        queueBegin,
                         PRUint32 aBatchCount,
                         PRUint32 aBatchID)
{
  // Reverse iterator from the end of the batch to the beginning and
  // bump the batch count, skipping the non-countable stuff
  for (;(!(*batchEnd)->IsCountable() || (*batchEnd)->batchID == aBatchID);
       --batchEnd) {
    if ((*batchEnd)->IsCountable()) {
      (*batchEnd)->batchCount = aBatchCount;
    }
    // Bail at beginning
    // Can't test in for statement since we'd miss the last one
    if (batchEnd == queueBegin) {
      break;
    }
  }
}

void SBUpdateBatchCounts(sbBaseDevice::Batch& aBatch)
{
  // Get the batch end.  Do nothing if batch is empty.
  sbBaseDevice::Batch::iterator batchEnd = aBatch.end();
  if (batchEnd == aBatch.begin())
    return;

  // Update the batch counts.
  SBUpdateBatchCounts(--batchEnd,
                      aBatch.begin(),
                      aBatch.size(),
                      (*(aBatch.begin()))->batchID);
}

nsresult sbBaseDevice::PushRequest(TransferRequest *aRequest)
{
  NS_ENSURE_ARG_POINTER(aRequest);

  #if DEBUG
    // XXX Mook: if writing, make sure that we're writing from a file
    if (aRequest->type == TransferRequest::REQUEST_WRITE) {
      NS_ASSERTION(aRequest->item, "writing with no item");
      nsCOMPtr<nsIURI> aSourceURI;
      aRequest->item->GetContentSrc(getter_AddRefs(aSourceURI));
      PRBool schemeIs = PR_FALSE;
      aSourceURI->SchemeIs("file", &schemeIs);
      NS_ASSERTION(schemeIs, "writing from device, but not from file!");
    }
  #endif

  { /* scope for request lock */
    nsAutoMonitor requestMon(mRequestMonitor);

    // If we're aborting don't accept any more requests
    if (mAbortCurrentRequest)
    {
      return NS_ERROR_ABORT;
    }
    /* decide where this request will be inserted */
    // figure out which queue we're looking at
    PRInt32 priority = aRequest->priority;
    TransferRequestQueue& queue = mRequests[priority];

    #if DEBUG
      CheckRequestBatch(queue.begin(), queue.end());
    #endif /* DEBUG */

    // initialize some properties of the request
    aRequest->itemTransferID = mLastTransferID++;
    aRequest->batchIndex = 1;
    aRequest->batchCount = 1;
    aRequest->batchID = 0;
    aRequest->timeStamp = PR_Now();

    // If this request isn't countable there's nothing to be done
    if (aRequest->IsCountable())
    {
      // Calculate the batch count
      TransferRequestQueue::iterator const begin = queue.begin();
      TransferRequestQueue::iterator const end = queue.end();
      TransferRequestQueue::iterator lastCountable = SBFindLastCountable(begin, end);

      // If there was a countable request found and it's the same type
      if (lastCountable != end && (*lastCountable)->type == aRequest->type) {
        nsRefPtr<TransferRequest> last = *(lastCountable);
        // batch them
        aRequest->batchCount += last->batchCount;
        aRequest->batchIndex = aRequest->batchCount;
        aRequest->batchID = last->batchID;

        SBUpdateBatchCounts(lastCountable, begin, aRequest->batchCount, aRequest->batchID);
      } else {
        // start of a new batch.  allocate a new batch ID atomically, ensuring
        // its value is never 0 (assuming it's not incremented 2^32-1 times
        // between the calls to PR_AtomicIncrement and that it's OK to sometimes
        // increment a few times too often)
        PRInt32 batchID = PR_AtomicIncrement(&mNextBatchID);
        if (!batchID)
          batchID = PR_AtomicIncrement(&mNextBatchID);
        aRequest->batchID = batchID;
      }
    }

    queue.push_back(aRequest);

    #if DEBUG
      CheckRequestBatch(queue.begin(), queue.end());
    #endif /* DEBUG */

  } /* end scope for request lock */

  // Defer process request until end of batch
  PRInt32 batchDepth;
  switch (aRequest->type) {
    case TransferRequest::REQUEST_BATCH_BEGIN:
      batchDepth = PR_AtomicIncrement(&mBatchDepth);
      // Incrementing, no need to go further
      return NS_OK;
    case TransferRequest::REQUEST_BATCH_END:
      batchDepth = PR_AtomicDecrement(&mBatchDepth);
      break;
    default:
      batchDepth = mBatchDepth;
      break;
  }
  NS_ASSERTION(batchDepth >= 0,
               "Batch depth out of whack in sbBaseDevice::PushRequest");
  if (batchDepth == 0) {
    nsresult rv = ProcessRequest();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult sbBaseDevice::FindFirstRequest(
  TransferRequestQueueMap::iterator & aMapIter,
  TransferRequestQueue::iterator & aQueueIter,
  bool aRemove)
{
  // Note: we shouldn't remove any empty queues from the map either, if we're
  // not going to pop the request.

  // try to find the last peeked request
  aMapIter =
    mRequests.find(mLastRequestPriority);
  if (aMapIter == mRequests.end()) {
    aMapIter = mRequests.begin();
  }

  while (aMapIter != mRequests.end()) {
    // always pop the request from the first queue

    TransferRequestQueue& queue = aMapIter->second;

    #if DEBUG
      CheckRequestBatch(queue.begin(), queue.end());
    #endif

    if (queue.empty()) {
      if (aRemove) {
        // this queue is empty, remove it
        mRequests.erase(aMapIter);
        aMapIter = mRequests.begin();
      } else {
        // go to the next queue
        ++aMapIter;
      }
      continue;
    }

    aQueueIter = queue.begin();

    mLastRequestPriority = aMapIter->first;

    return NS_OK;
  }
  // there are no queues left
  mLastRequestPriority = PR_INT32_MIN;
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult sbBaseDevice::GetFirstRequest(bool aRemove,
                                       sbBaseDevice::TransferRequest** retval)
{
  NS_ENSURE_ARG_POINTER(retval);
  NS_ENSURE_TRUE(mRequestMonitor, NS_ERROR_NOT_INITIALIZED);

  nsAutoMonitor reqMon(mRequestMonitor);

  TransferRequestQueueMap::iterator mapIter;
  TransferRequestQueue::iterator queueIter;
  nsresult rv = FindFirstRequest(mapIter, queueIter, aRemove);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    // there are no queues left
    mLastRequestPriority = PR_INT32_MIN;
    *retval = nsnull;
    return NS_OK;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  *retval = queueIter->get();
  NS_ADDREF(*retval);

  if (aRemove) {
    mapIter->second.erase(queueIter);
    mLastRequestPriority = PR_INT32_MIN;
  }

  return NS_OK;
}

nsresult sbBaseDevice::PopRequest(sbBaseDevice::TransferRequest** _retval)
{
  return GetFirstRequest(true, _retval);
}

nsresult sbBaseDevice::PopRequest(sbBaseDevice::Batch & aBatch) {

  aBatch.clear();
  nsAutoMonitor reqMon(mRequestMonitor);

  TransferRequestQueueMap::iterator mapIter;
  TransferRequestQueue::iterator queueIter;

  nsresult rv = FindFirstRequest(mapIter,
                                 queueIter,
                                 false);
  // If nothing was found then just return with an empty batch
  if (rv == NS_ERROR_NOT_AVAILABLE || queueIter == mapIter->second.end()) {
    LOG(("No requests found\n"));
    return NS_OK;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  TransferRequestQueue & queue = mapIter->second;

  nsRefPtr<TransferRequest> & request = *queueIter;
  // If this isn't countable or isn't part of  batch then just return it
  if (!request->IsCountable() || request->batchID == 0) {
    LOG(("Single non-batch request found\n"));
    aBatch.push_back(*queueIter);
    queue.erase(queueIter);
    return NS_OK;
  }

  typedef std::vector<TransferRequestQueue::iterator> BatchIters;
  BatchIters batchIters;
  batchIters.reserve(queue.size());

  // find the end of the batch and keep track of the matching batch entries
  TransferRequestQueue::iterator const queueEnd = queue.end();
  PRUint32 batchID = (*queueIter)->batchID;
  for (;queueIter != queueEnd; ++queueIter) {
    // skip requests that are not a part of a batch
    if ((*queueIter)->batchID == batchID) {
      batchIters.push_back(queueIter);
    }
  }

  // See if we found anything
  if (!batchIters.empty()) {
    BatchIters::iterator begin = batchIters.begin();
    PRTime const now = PR_Now();
    // Is the batch complete (batch timeout expired)
    if (now - (**begin)->timeStamp < BATCH_TIMEOUT * PR_USEC_PER_MSEC) {
      // Update the last batch timestamp
      LOG(("Waiting on batch complete\n"));
      // Leave the request monitor else we'll deadlock waiting
      reqMon.Exit();
      rv = WaitForBatchEnd();
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }
  }

  BatchIters::iterator const batchItersEnd = batchIters.end();
  for (BatchIters::iterator iter = batchIters.begin();
       iter != batchItersEnd;
       ++iter) {
    TransferRequestQueue::iterator const requestQueueIter = *iter;
    aBatch.push_back(*requestQueueIter);
    queue.erase(requestQueueIter);
  }
  return NS_OK;
}

nsresult sbBaseDevice::PeekRequest(sbBaseDevice::TransferRequest** _retval)
{
  return GetFirstRequest(false, _retval);
}

template <class T>
NS_HIDDEN_(PRBool) Compare(T* left, nsCOMPtr<T> right)
{
  nsresult rv = NS_OK;
  PRBool isEqual = (left == right) ? PR_TRUE : PR_FALSE;
  if (!isEqual && left != nsnull && right != nsnull)
    rv = left->Equals(right, &isEqual);
  return NS_SUCCEEDED(rv) && isEqual;
}

nsresult sbBaseDevice::RemoveRequest(const int aType,
                                     sbIMediaItem* aItem,
                                     sbIMediaList* aList)
{
  NS_ENSURE_TRUE(mRequestMonitor, NS_ERROR_NOT_INITIALIZED);

  nsAutoMonitor reqMon(mRequestMonitor);

  // always pop the request from the first queue
  // can't just test for empty because we may have junk requests
  TransferRequestQueueMap::iterator mapIt = mRequests.begin(),
                                    mapEnd = mRequests.end();

  for (; mapIt != mapEnd; ++mapIt) {
    TransferRequestQueue& queue = mapIt->second;

    // more possibly dummy item accounting
    TransferRequestQueue::iterator queueIt = queue.begin(),
                                   queueEnd = queue.end();

    #if DEBUG
      CheckRequestBatch(queueIt, queueEnd);
    #endif

    for (; queueIt != queueEnd; ++queueIt) {
      nsRefPtr<TransferRequest> request = *queueIt;

      if (request->type == aType &&
          Compare(aItem, request->item) && Compare(aList, request->list))
      {
        // found; remove
        queue.erase(queueIt);

        #if DEBUG
          CheckRequestBatch(queue.begin(), queue.end());
        #endif /* DEBUG */

        return NS_OK;
      }
    }

    #if DEBUG
      CheckRequestBatch(queue.begin(), queue.end());
    #endif /* DEBUG */

  }

  // there are no queues left
  return NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA;
}

typedef std::vector<nsRefPtr<sbBaseDevice::TransferRequest> > sbBaseDeviceTransferRequests;

nsresult sbBaseDevice::ClearRequests(const nsAString &aDeviceID)
{
  nsresult rv;
  sbBaseDeviceTransferRequests requests;

  nsRefPtr<TransferRequest> request;
  PeekRequest(getter_AddRefs(request));

  NS_ENSURE_TRUE(mRequestMonitor, NS_ERROR_NOT_INITIALIZED);
  {
    nsAutoMonitor reqMon(mRequestMonitor);

    if(!mRequests.empty()) {
      rv = SetState(STATE_CANCEL);
      NS_ENSURE_SUCCESS(rv, rv);

      Batch::iterator const batchEnd = mCurrentBatch.end();
      for (Batch::iterator batchIter = mCurrentBatch.begin();
           batchIter != batchEnd;
           ++batchIter) {
        requests.push_back(*batchIter);
      }
      // Save off the library items that are pending to avoid any
      // potential reenterancy issues when deleting them.
      TransferRequestQueueMap::const_iterator mapIt = mRequests.begin(),
                                              mapEnd = mRequests.end();
      for (; mapIt != mapEnd; ++mapIt) {
        const TransferRequestQueue& queue = mapIt->second;
        TransferRequestQueue::const_iterator queueIt = queue.begin(),
                                             queueEnd = queue.end();

        #if DEBUG
          CheckRequestBatch(queueIt, queueEnd);
        #endif

        for (; queueIt != queueEnd; ++queueIt) {
          if ((*queueIt)->type == sbBaseDevice::TransferRequest::REQUEST_WRITE) {
            requests.push_back(*queueIt);
          }
        }

        #if DEBUG
          CheckRequestBatch(queue.begin(), queue.end());
        #endif
      }

      mAbortCurrentRequest = PR_TRUE;
      mRequests.clear();

      rv = SetState(STATE_IDLE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // must not hold onto the monitor while we create sbDeviceStatus objects
  // because that involves proxying to the main thread

  if (!requests.empty()) {
    rv = RemoveLibraryItems(requests.begin(), requests.end());
    NS_ENSURE_SUCCESS(rv, rv);

    if (request) {
      nsCOMPtr<nsIWritableVariant> var =
        do_CreateInstance("@songbirdnest.com/Songbird/Variant;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = var->SetAsISupports(request->item);
      NS_ENSURE_SUCCESS(rv, rv);

      CreateAndDispatchEvent(sbIDeviceEvent::EVENT_DEVICE_TRANSFER_END,
                             var,
                             PR_TRUE);
    }
  }

  return NS_OK;
}

nsresult sbBaseDevice::BatchGetRequestType(sbBaseDevice::Batch& aBatch,
                                           int*                 aRequestType)
{
  // Validate arguments.
  NS_ENSURE_ARG(!aBatch.empty());
  NS_ENSURE_ARG_POINTER(aRequestType);

  // Use the type of the first batch request as the batch request type.
  *aRequestType = aBatch.front()->type;

  return NS_OK;
}

nsresult sbBaseDevice::GetPreferenceInternal(nsIPrefBranch *aPrefBranch,
                                             const nsAString & aPrefName,
                                             nsIVariant **_retval)
{
  NS_ENSURE_ARG_POINTER(aPrefBranch);
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_FALSE(aPrefName.IsEmpty(), NS_ERROR_INVALID_ARG);
  nsresult rv;

  NS_ConvertUTF16toUTF8 prefNameUTF8(aPrefName);

  // get tht type of the pref
  PRInt32 prefType;
  rv = aPrefBranch->GetPrefType(prefNameUTF8.get(), &prefType);
  NS_ENSURE_SUCCESS(rv, rv);

  // create a writable variant
  nsCOMPtr<nsIWritableVariant> writableVariant =
    do_CreateInstance("@songbirdnest.com/Songbird/Variant;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the value of our pref
  switch (prefType) {
    case nsIPrefBranch::PREF_INVALID: {
      rv = writableVariant->SetAsEmpty();
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    case nsIPrefBranch::PREF_STRING: {
      char* _value = NULL;
      rv = aPrefBranch->GetCharPref(prefNameUTF8.get(), &_value);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCString value;
      value.Adopt(_value);

      // set the value of the variant to the value of the pref
      rv = writableVariant->SetAsACString(value);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    case nsIPrefBranch::PREF_INT: {
      PRInt32 value;
      rv = aPrefBranch->GetIntPref(prefNameUTF8.get(), &value);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = writableVariant->SetAsInt32(value);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    case nsIPrefBranch::PREF_BOOL: {
      PRBool value;
      rv = aPrefBranch->GetBoolPref(prefNameUTF8.get(), &value);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = writableVariant->SetAsBool(value);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
  }

  return CallQueryInterface(writableVariant, _retval);
}

/* nsIVariant getPreference (in AString aPrefName); */
NS_IMETHODIMP sbBaseDevice::GetPreference(const nsAString & aPrefName, nsIVariant **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_FALSE(aPrefName.IsEmpty(), NS_ERROR_INVALID_ARG);
  nsresult rv;

  // special case device capabilities preferences
  if (aPrefName.Equals(NS_LITERAL_STRING("capabilities"))) {
    return GetCapabilitiesPreference(_retval);
  }

  // get the pref branch for this device
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = GetPrefBranch(getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  return GetPreferenceInternal(prefBranch, aPrefName, _retval);
}

/* void setPreference (in AString aPrefName, in nsIVariant aPrefValue); */
NS_IMETHODIMP sbBaseDevice::SetPreference(const nsAString & aPrefName, nsIVariant *aPrefValue)
{
  NS_ENSURE_ARG_POINTER(aPrefValue);
  NS_ENSURE_FALSE(aPrefName.IsEmpty(), NS_ERROR_INVALID_ARG);
  nsresult rv;

  // get the pref branch for this device
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = GetPrefBranch(getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetPreferenceInternal(prefBranch, aPrefName, aPrefValue);
}

nsresult sbBaseDevice::SetPreferenceInternal(nsIPrefBranch *aPrefBranch,
                                             const nsAString & aPrefName,
                                             nsIVariant *aPrefValue)
{
  NS_ENSURE_ARG_POINTER(aPrefValue);
  NS_ENSURE_FALSE(aPrefName.IsEmpty(), NS_ERROR_INVALID_ARG);
  nsresult rv;

  PRBool hasChanged = PR_FALSE;
  rv = SetPreferenceInternal(aPrefBranch, aPrefName, aPrefValue, &hasChanged);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasChanged) {
    // apply the preference
    ApplyPreference(aPrefName, aPrefValue);

    // fire the pref change event
    nsCOMPtr<sbIDeviceManager2> devMgr =
      do_GetService("@songbirdnest.com/Songbird/DeviceManager;2", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CreateAndDispatchEvent(sbIDeviceEvent::EVENT_DEVICE_PREFS_CHANGED,
                                sbNewVariant(aPrefName),
                                PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult sbBaseDevice::SetPreferenceInternalNoNotify(
                         const nsAString& aPrefName,
                         nsIVariant*      aPrefValue,
                         PRBool*          aHasChanged)
{
  // get the pref branch for this device
  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsresult rv = GetPrefBranch(getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetPreferenceInternal(prefBranch, aPrefName, aPrefValue, aHasChanged);
}

nsresult sbBaseDevice::SetPreferenceInternal(nsIPrefBranch*   aPrefBranch,
                                             const nsAString& aPrefName,
                                             nsIVariant*      aPrefValue,
                                             PRBool*          aHasChanged)
{
  NS_ENSURE_ARG_POINTER(aPrefValue);
  NS_ENSURE_FALSE(aPrefName.IsEmpty(), NS_ERROR_INVALID_ARG);
  nsresult rv;

  NS_ConvertUTF16toUTF8 prefNameUTF8(aPrefName);

  // figure out what sort of variant we have
  PRUint16 dataType;
  rv = aPrefValue->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  // figure out what sort of data we used to have
  PRInt32 prefType;
  rv = aPrefBranch->GetPrefType(prefNameUTF8.get(), &prefType);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasChanged = PR_FALSE;

  switch (dataType) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_UINT64:
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE:
    {
      // some sort of number
      PRInt32 oldValue, value;
      rv = aPrefValue->GetAsInt32(&value);
      NS_ENSURE_SUCCESS(rv, rv);

      if (prefType != nsIPrefBranch::PREF_INT) {
        hasChanged = PR_TRUE;
      } else {
        rv = aPrefBranch->GetIntPref(prefNameUTF8.get(), &oldValue);
        if (NS_SUCCEEDED(rv) && oldValue != value) {
          hasChanged = PR_TRUE;
        }
      }

      rv = aPrefBranch->SetIntPref(prefNameUTF8.get(), value);
      NS_ENSURE_SUCCESS(rv, rv);

      break;
    }

    case nsIDataType::VTYPE_BOOL:
    {
      // a bool pref
      PRBool oldValue, value;
      rv = aPrefValue->GetAsBool(&value);
      NS_ENSURE_SUCCESS(rv, rv);

      if (prefType != nsIPrefBranch::PREF_BOOL) {
        hasChanged = PR_TRUE;
      } else {
        rv = aPrefBranch->GetBoolPref(prefNameUTF8.get(), &oldValue);
        if (NS_SUCCEEDED(rv) && oldValue != value) {
          hasChanged = PR_TRUE;
        }
      }

      rv = aPrefBranch->SetBoolPref(prefNameUTF8.get(), value);
      NS_ENSURE_SUCCESS(rv, rv);

      break;
    }

    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
    {
      // unset the pref
      if (prefType != nsIPrefBranch::PREF_INVALID) {
        rv = aPrefBranch->ClearUserPref(prefNameUTF8.get());
        NS_ENSURE_SUCCESS(rv, rv);
        hasChanged = PR_TRUE;
      }

      break;
    }

    default:
    {
      // assume a string
      nsCString value;
      rv = aPrefValue->GetAsACString(value);
      NS_ENSURE_SUCCESS(rv, rv);

      if (prefType != nsIPrefBranch::PREF_STRING) {
        hasChanged = PR_TRUE;
      } else {
        char* oldValue;
        rv = aPrefBranch->GetCharPref(prefNameUTF8.get(), &oldValue);
        if (NS_SUCCEEDED(rv)) {
          if (!(value.Equals(oldValue))) {
            hasChanged = PR_TRUE;
          }
          NS_Free(oldValue);
        }
      }

      rv = aPrefBranch->SetCharPref(prefNameUTF8.get(), value.get());
      NS_ENSURE_SUCCESS(rv, rv);

      break;
    }
  }

  // return has changed status
  if (aHasChanged)
    *aHasChanged = hasChanged;

  return NS_OK;
}

/* readonly attribute boolean isBusy; */
NS_IMETHODIMP sbBaseDevice::GetIsBusy(PRBool *aIsBusy)
{
  NS_ENSURE_ARG_POINTER(aIsBusy);
  NS_ENSURE_TRUE(mStateLock, NS_ERROR_NOT_INITIALIZED);
  nsAutoLock lock(mStateLock);
  switch (mState) {
    case STATE_IDLE:
    case STATE_DOWNLOAD_PAUSED:
    case STATE_UPLOAD_PAUSED:
      *aIsBusy = PR_FALSE;
    break;

    default:
      *aIsBusy = PR_TRUE;
    break;
  }
  return NS_OK;
}

/* readonly attribute boolean canDisconnect; */
NS_IMETHODIMP sbBaseDevice::GetCanDisconnect(PRBool *aCanDisconnect)
{
  NS_ENSURE_ARG_POINTER(aCanDisconnect);
  NS_ENSURE_TRUE(mStateLock, NS_ERROR_NOT_INITIALIZED);
  nsAutoLock lock(mStateLock);
  switch(mState) {
    case STATE_IDLE:
    case STATE_MOUNTING:
    case STATE_DISCONNECTED:
    case STATE_DOWNLOAD_PAUSED:
    case STATE_UPLOAD_PAUSED:
      *aCanDisconnect = PR_TRUE;
    break;

    default:
      *aCanDisconnect = PR_FALSE;
    break;
  }
  return NS_OK;
}

/* readonly attribute unsigned long state; */
NS_IMETHODIMP sbBaseDevice::GetPreviousState(PRUint32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  NS_ENSURE_TRUE(mPreviousStateLock, NS_ERROR_NOT_INITIALIZED);
  nsAutoLock lock(mPreviousStateLock);
  *aState = mPreviousState;
  return NS_OK;
}

nsresult sbBaseDevice::SetPreviousState(PRUint32 aState)
{
  // set state, checking if it changed
  NS_ENSURE_TRUE(mPreviousStateLock, NS_ERROR_NOT_INITIALIZED);
  nsAutoLock lock(mPreviousStateLock);
  if (mPreviousState != aState) {
    mPreviousState = aState;
  }
  return NS_OK;
}

/* readonly attribute unsigned long state; */
NS_IMETHODIMP sbBaseDevice::GetState(PRUint32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  NS_ENSURE_TRUE(mStateLock, NS_ERROR_NOT_INITIALIZED);
  nsAutoLock lock(mStateLock);
  *aState = mState;
  return NS_OK;
}

nsresult sbBaseDevice::SetState(PRUint32 aState)
{

  nsresult rv;
  PRBool stateChanged = PR_FALSE;
  PRUint32 prevState;

  // set state, checking if it changed
  {
    NS_ENSURE_TRUE(mStateLock, NS_ERROR_NOT_INITIALIZED);
    nsAutoLock lock(mStateLock);
    prevState = mState;
    if (mState != aState) {
      mState = aState;
      stateChanged = PR_TRUE;
    }
    // Update the previous state - we set it outside of the if loop, so
    // even if the current state isn't changing, the previous state still
    // gets updated to the correct previous state.
    SetPreviousState(prevState);
  }

  // send state changed event.  do it outside of lock in case event handler gets
  // called immediately and tries to read the state
  if (stateChanged) {
    nsCOMPtr<nsIWritableVariant> var = do_CreateInstance("@songbirdnest.com/Songbird/Variant;1",
                                                         &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = var->SetAsUint32(aState);
    NS_ENSURE_SUCCESS(rv, rv);
    CreateAndDispatchEvent(sbIDeviceEvent::EVENT_DEVICE_STATE_CHANGED, var);
  }

  return NS_OK;
}

nsresult sbBaseDevice::CreateDeviceLibrary(const nsAString& aId,
                                           nsIURI* aLibraryLocation,
                                           sbIDeviceLibrary** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsRefPtr<sbDeviceLibrary> devLib = new sbDeviceLibrary(this);
  NS_ENSURE_TRUE(devLib, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = InitializeDeviceLibrary(devLib, aId, aLibraryLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CallQueryInterface(devLib.get(), _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult sbBaseDevice::InitializeDeviceLibrary
                         (sbDeviceLibrary* aDevLib,
                          const nsAString& aId,
                          nsIURI*          aLibraryLocation)
{
  NS_ENSURE_ARG_POINTER(aDevLib);

  if (!mMediaListListeners.IsInitialized()) {
    // we expect to be unintialized, but just in case...
    if (!mMediaListListeners.Init()) {
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv = aDevLib->Initialize(aId);
  NS_ENSURE_SUCCESS(rv, rv);

  // Hide the library on creation. The device is responsible
  // for showing it is done mounting.
  rv = aDevLib->SetProperty(NS_LITERAL_STRING(SB_PROPERTY_HIDDEN),
                            NS_LITERAL_STRING("1"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDevLib->SetProperty(NS_LITERAL_STRING(SB_PROPERTY_ISSORTABLE),
                            NS_LITERAL_STRING("1"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDevLib->SetProperty(NS_LITERAL_STRING(SB_PROPERTY_DEFAULTCOLUMNSPEC),
                            NS_ConvertASCIItoUTF16(NS_LITERAL_CSTRING(DEFAULT_COLUMNSPEC_DEVICE_LIBRARY)));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<sbBaseDeviceLibraryListener> libListener = new sbBaseDeviceLibraryListener();
  NS_ENSURE_TRUE(libListener, NS_ERROR_OUT_OF_MEMORY);

  rv = libListener->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDevLib->AddDeviceLibraryListener(libListener);
  NS_ENSURE_SUCCESS(rv, rv);

  // hook up the media list listeners to the existing lists
  nsRefPtr<MediaListListenerAttachingEnumerator> enumerator =
    new MediaListListenerAttachingEnumerator(this);
  NS_ENSURE_TRUE(enumerator, NS_ERROR_OUT_OF_MEMORY);

  rv = aDevLib->EnumerateItemsByProperty(NS_LITERAL_STRING(SB_PROPERTY_ISLIST),
                                         NS_LITERAL_STRING("1"),
                                         enumerator,
                                         sbIMediaList::ENUMERATIONTYPE_SNAPSHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  libListener.swap(mLibraryListener);

  return NS_OK;
}

void sbBaseDevice::FinalizeDeviceLibrary(sbIDeviceLibrary* aDevLib)
{
  // Finalize and clear the media list listeners.
  EnumerateFinalizeMediaListListenersInfo enumerateInfo;
  enumerateInfo.device = this;
  enumerateInfo.library = aDevLib;
  mMediaListListeners.Enumerate
                        (sbBaseDevice::EnumerateFinalizeMediaListListeners,
                         &enumerateInfo);

  // Finalize the device library.
  aDevLib->Finalize();
}

nsresult sbBaseDevice::AddLibrary(sbIDeviceLibrary* aDevLib)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aDevLib);

  // Function variables.
  nsresult rv;

  // Check library access.
  rv = CheckAccess(aDevLib);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the device content.
  nsCOMPtr<sbIDeviceContent> content;
  rv = GetContent(getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  // Add the library to the device content.
  rv = content->AddLibrary(aDevLib);
  NS_ENSURE_SUCCESS(rv, rv);

  // Send a device library added event.
  CreateAndDispatchEvent(sbIDeviceEvent::EVENT_DEVICE_LIBRARY_ADDED,
                         sbNewVariant(aDevLib));

  // Apply the library preferences.
  rv = ApplyLibraryPreference(aDevLib, SBVoidString(), nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult sbBaseDevice::CheckAccess(sbIDeviceLibrary* aDevLib)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aDevLib);

  // Function variables.
  nsresult rv;

  // Get the device properties.
  nsCOMPtr<sbIDeviceProperties> baseDeviceProperties;
  nsCOMPtr<nsIPropertyBag2>     deviceProperties;
  rv = GetProperties(getter_AddRefs(baseDeviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = baseDeviceProperties->GetProperties(getter_AddRefs(deviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the access compatibility.
  nsAutoString accessCompatibility;
  rv = deviceProperties->GetPropertyAsAString
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_ACCESS_COMPATIBILITY),
          accessCompatibility);
  if (NS_FAILED(rv))
    accessCompatibility.Truncate();

  // Do nothing more if access is not read-only.
  if (!accessCompatibility.Equals(NS_LITERAL_STRING("ro")))
    return NS_OK;

  // Prompt user.
  // Get a prompter.
  nsCOMPtr<sbIPrompter>
    prompter = do_CreateInstance(SONGBIRD_PROMPTER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Determine whether the access can be changed.
  PRBool canChangeAccess = PR_FALSE;
  rv = deviceProperties->GetPropertyAsBool
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_ACCESS_COMPATIBILITY_MUTABLE),
          &canChangeAccess);
  if (NS_FAILED(rv))
    canChangeAccess = PR_FALSE;

  // Get the device name.
  nsAutoString deviceName;
  rv = GetName(deviceName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the prompt title.
  nsAutoString title = SBLocalizedString
                         ("device.dialog.read_only_device.title");

  // Get the prompt message.
  nsAutoString       msg;
  nsTArray<nsString> params;
  params.AppendElement(deviceName);
  if (canChangeAccess) {
    msg = SBLocalizedString("device.dialog.read_only_device.can_change.msg",
                            params);
  } else {
    msg = SBLocalizedString("device.dialog.read_only_device.cannot_change.msg",
                            params);
  }

  // Configure the buttons.
  PRUint32 buttonFlags = 0;
  PRInt32 changeAccessButtonIndex = -1;
  if (canChangeAccess) {
    changeAccessButtonIndex = 0;
    buttonFlags += nsIPromptService::BUTTON_POS_0 *
                   nsIPromptService::BUTTON_TITLE_IS_STRING;
    buttonFlags += nsIPromptService::BUTTON_POS_1 *
                   nsIPromptService::BUTTON_TITLE_IS_STRING;
  } else {
    buttonFlags += nsIPromptService::BUTTON_POS_0 *
                   nsIPromptService::BUTTON_TITLE_OK;
  }

  // Get the button labels.
  nsAutoString buttonLabel0 =
    SBLocalizedString("device.dialog.read_only_device.change");
  nsAutoString buttonLabel1 =
    SBLocalizedString("device.dialog.read_only_device.dont_change");

  // Prompt user.
  PRInt32 buttonPressed;
  rv = prompter->ConfirmEx(nsnull,             // Parent window.
                           title.get(),
                           msg.get(),
                           buttonFlags,
                           buttonLabel0.get(),
                           buttonLabel1.get(),
                           nsnull,             // Button 2 label.
                           nsnull,             // Check message.
                           nsnull,             // Check result.
                           &buttonPressed);
  NS_ENSURE_SUCCESS(rv, rv);

  // Change access if user selected to do so.
  if (canChangeAccess && (buttonPressed == changeAccessButtonIndex)) {
    // Set the access compatibility property to read-write.
    nsCOMPtr<nsIWritablePropertyBag>
      writeDeviceProperties = do_QueryInterface(deviceProperties, &rv);
    accessCompatibility = NS_LITERAL_STRING("rw");
    NS_ENSURE_SUCCESS(rv, rv);
    writeDeviceProperties->SetProperty
      (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_ACCESS_COMPATIBILITY),
       sbNewVariant(accessCompatibility));
  }

  return NS_OK;
}

nsresult sbBaseDevice::RemoveLibrary(sbIDeviceLibrary* aDevLib)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aDevLib);

  // Function variables.
  nsresult rv;

  // Send a device library removed event.
  nsAutoString guid;
  rv = aDevLib->GetGuid(guid);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to get device library.");
  CreateAndDispatchEvent(sbIDeviceEvent::EVENT_DEVICE_LIBRARY_REMOVED,
                         sbNewVariant(guid));

  // Get the device content.
  nsCOMPtr<sbIDeviceContent> content;
  rv = GetContent(getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove the device library from the device content.
  rv = content->RemoveLibrary(aDevLib);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult sbBaseDevice::ListenToList(sbIMediaList* aList)
{
  NS_ENSURE_ARG_POINTER(aList);

  NS_ASSERTION(mMediaListListeners.IsInitialized(),
               "sbBaseDevice::ListenToList called before listener hash is initialized!");

  nsresult rv;

  #if DEBUG
    // check to make sure we're not listening to a library
    nsCOMPtr<sbILibrary> library = do_QueryInterface(aList);
    NS_ASSERTION(!library,
                 "Should not call sbBaseDevice::ListenToList on a library!");
  #endif

  // the extra QI to make sure we're at the canonical pointer
  // and not some derived interface
  nsCOMPtr<sbIMediaList> list = do_QueryInterface(aList, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // check for an existing listener
  if (mMediaListListeners.Get(list, nsnull)) {
    // we are already listening to the media list, don't re-add
    return NS_OK;
  }

  nsRefPtr<sbBaseDeviceMediaListListener> listener =
    new sbBaseDeviceMediaListListener();
  NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);

  rv = listener->Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = list->AddListener(listener,
                         PR_FALSE, /* weak */
                         0, /* all */
                         nsnull /* filter */);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're currently ignoring listeners then we need to ignore this one too
  // else we'll get out of balance
  if (mIgnoreMediaListCount > 0)
    listener->SetIgnoreListener(PR_TRUE);
  mMediaListListeners.Put(list, listener);

  return NS_OK;
}

PLDHashOperator sbBaseDevice::EnumerateFinalizeMediaListListeners
                                (nsISupports* aKey,
                                 nsRefPtr<sbBaseDeviceMediaListListener>& aData,
                                 void* aClosure)
{
  nsresult rv;

  // Get the device and library for which to finalize media list listeners.
  EnumerateFinalizeMediaListListenersInfo*
    enumerateInfo =
      static_cast<EnumerateFinalizeMediaListListenersInfo*>(aClosure);
  nsCOMPtr<sbILibrary> library = enumerateInfo->library;

  // Get the listener media list.
  nsCOMPtr<sbIMediaList> mediaList = do_QueryInterface(aKey, &rv);
  NS_ENSURE_SUCCESS(rv, PL_DHASH_STOP);

  // Do nothing if media list is contained in another library.
  nsCOMPtr<sbILibrary> mediaListLibrary;
  PRBool               equals;
  rv = mediaList->GetLibrary(getter_AddRefs(mediaListLibrary));
  NS_ENSURE_SUCCESS(rv, PL_DHASH_STOP);
  rv = mediaListLibrary->Equals(library, &equals);
  NS_ENSURE_SUCCESS(rv, PL_DHASH_STOP);
  if (!equals)
    return PL_DHASH_NEXT;

  // Remove the media list listener.
  mediaList->RemoveListener(aData);

  return PL_DHASH_REMOVE;
}

PLDHashOperator sbBaseDevice::EnumerateIgnoreMediaListListeners(nsISupports* aKey,
                                                                nsRefPtr<sbBaseDeviceMediaListListener> aData,
                                                                void* aClosure)
{
  nsresult rv;
  PRBool *ignore = static_cast<PRBool *>(aClosure);

  rv = aData->SetIgnoreListener(*ignore);
  NS_ENSURE_SUCCESS(rv, PL_DHASH_STOP);

  return PL_DHASH_NEXT;
}

nsresult
sbBaseDevice::SetIgnoreMediaListListeners(PRBool aIgnoreListener)
{
  if (aIgnoreListener)
    PR_AtomicIncrement(&mIgnoreMediaListCount);
  else
    PR_AtomicDecrement(&mIgnoreMediaListCount);

  mMediaListListeners.EnumerateRead(sbBaseDevice::EnumerateIgnoreMediaListListeners,
                                    &aIgnoreListener);
  return NS_OK;
}

nsresult
sbBaseDevice::SetIgnoreLibraryListener(PRBool aIgnoreListener)
{
  return mLibraryListener->SetIgnoreListener(aIgnoreListener);
}

nsresult
sbBaseDevice::SetMediaListsHidden(sbIMediaList *aLibrary, PRBool aHidden)
{
  NS_ENSURE_ARG_POINTER(aLibrary);

  nsRefPtr<ShowMediaListEnumerator> enumerator = new ShowMediaListEnumerator(aHidden);
  NS_ENSURE_TRUE(enumerator, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = aLibrary->EnumerateItemsByProperty(NS_LITERAL_STRING(SB_PROPERTY_ISLIST),
                                                   NS_LITERAL_STRING("1"),
                                                   enumerator,
                                                   sbIMediaList::ENUMERATIONTYPE_SNAPSHOT);
  return rv;
}

nsresult sbBaseDevice::IgnoreMediaItem(sbIMediaItem * aItem) {
  return mLibraryListener->IgnoreMediaItem(aItem);
}

nsresult sbBaseDevice::UnignoreMediaItem(sbIMediaItem * aItem) {
  return mLibraryListener->UnignoreMediaItem(aItem);
}

nsresult
sbBaseDevice::DeleteItem(sbIMediaList *aLibrary, sbIMediaItem *aItem)
{
  NS_ENSURE_ARG_POINTER(aLibrary);
  NS_ENSURE_ARG_POINTER(aItem);

  NS_ENSURE_STATE(mLibraryListener);

  SetIgnoreMediaListListeners(PR_TRUE);
  mLibraryListener->SetIgnoreListener(PR_TRUE);

  nsresult rv = aLibrary->Remove(aItem);

  SetIgnoreMediaListListeners(PR_FALSE);
  mLibraryListener->SetIgnoreListener(PR_FALSE);

  return rv;
}

nsresult
sbBaseDevice::GetItemContentType(sbIMediaItem* aMediaItem,
                                 PRUint32*     aContentType)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aMediaItem);
  NS_ENSURE_ARG_POINTER(aContentType);

  // Return results.  Assume all media items are audio.
  *aContentType = sbIDeviceCapabilities::CONTENT_AUDIO;

  return NS_OK;
}

nsresult
sbBaseDevice::CreateTransferRequest(PRUint32 aRequest,
                                    nsIPropertyBag2 *aRequestParameters,
                                    TransferRequest **aTransferRequest)
{
  NS_ENSURE_TRUE( ((aRequest >= REQUEST_MOUNT && aRequest <= REQUEST_FORMAT) ||
                   (aRequest & REQUEST_FLAG_USER)),
                  NS_ERROR_ILLEGAL_VALUE);
  NS_ENSURE_ARG_POINTER(aRequestParameters);
  NS_ENSURE_ARG_POINTER(aTransferRequest);

  TransferRequest* req = TransferRequest::New();
  NS_ENSURE_TRUE(req, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;

  nsCOMPtr<sbIMediaItem> item;
  nsCOMPtr<sbIMediaList> list;
  nsCOMPtr<nsISupports>  data;

  PRUint32 index = PR_UINT32_MAX;
  PRUint32 otherIndex = PR_UINT32_MAX;
  PRInt32 priority = TransferRequest::PRIORITY_DEFAULT;

  rv = aRequestParameters->GetPropertyAsInterface(NS_LITERAL_STRING("item"),
                                                  NS_GET_IID(sbIMediaItem),
                                                  getter_AddRefs(item));
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "No item present in request parameters.");

  rv = aRequestParameters->GetPropertyAsInterface(NS_LITERAL_STRING("list"),
                                                  NS_GET_IID(sbIMediaList),
                                                  getter_AddRefs(list));
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "No list present in request parameters.");

  rv = aRequestParameters->GetPropertyAsInterface(NS_LITERAL_STRING("data"),
                                                  NS_GET_IID(nsISupports),
                                                  getter_AddRefs(data));
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "No data present in request parameters.");

  NS_WARN_IF_FALSE(item || list || data, "No data of any kind given in request. This request will most likely fail.");

  rv = aRequestParameters->GetPropertyAsUint32(NS_LITERAL_STRING("index"),
                                               &index);
  if(NS_FAILED(rv)) {
    index = PR_UINT32_MAX;
  }

  rv = aRequestParameters->GetPropertyAsUint32(NS_LITERAL_STRING("otherIndex"),
                                               &otherIndex);
  if(NS_FAILED(rv)) {
    otherIndex = PR_UINT32_MAX;
  }

  rv = aRequestParameters->GetPropertyAsInt32(NS_LITERAL_STRING("priority"),
                                               &priority);
  if(NS_FAILED(rv)) {
    priority = TransferRequest::PRIORITY_DEFAULT;
  }

  req->type = aRequest;
  req->item = item;
  req->list = list;
  req->data = data;
  req->index = index;
  req->otherIndex = otherIndex;
  req->priority = priority;

  NS_ADDREF(*aTransferRequest = req);

  return NS_OK;
}

nsresult sbBaseDevice::CreateAndDispatchEvent(PRUint32 aType,
                                              nsIVariant *aData,
                                              PRBool aAsync /*= PR_TRUE*/)
{
  nsresult rv;

  nsCOMPtr<sbIDeviceManager2> manager =
    do_GetService("@songbirdnest.com/Songbird/DeviceManager;2", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIDeviceEvent> deviceEvent;
  rv = manager->CreateEvent(aType, aData, static_cast<sbIDevice*>(this),
                            getter_AddRefs(deviceEvent));
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchEvent(deviceEvent, aAsync, nsnull);
}

template <class T>
inline
T find_iterator(T start, T end, T target)
{
  while (start != target && start != end) {
    ++start;
  }
  return start;
}

nsresult sbBaseDevice::EnsureSpaceForWrite(Batch & aBatch)
{
  LOG(("                        sbBaseDevice::EnsureSpaceForWrite++\n"));

  nsresult rv;

  sbDeviceEnsureSpaceForWrite esfw(this, aBatch);

  rv = esfw.EnsureSpace();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::WaitForBatchEnd()
{
  nsresult rv;

  // wait for the complete batch to be pushed into the queue
  rv = mBatchEndTimer->InitWithFuncCallback(WaitForBatchEndCallback,
                                            this,
                                            BATCH_TIMEOUT,
                                            nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/* static */
void sbBaseDevice::WaitForBatchEndCallback(nsITimer* aTimer,
                                           void* aClosure)
{
  // dispatch callback into non-static method
  sbBaseDevice* self = reinterpret_cast<sbBaseDevice*>(aClosure);
  NS_ASSERTION(self, "self is null");
  self->WaitForBatchEndCallback();
}

void
sbBaseDevice::WaitForBatchEndCallback()
{
  // start processing requests now that the complete batch is available
  ProcessRequest();
}

/* a helper class to proxy sbBaseDevice::Init onto the main thread
 * needed because sbBaseDevice multiply-inherits from nsISupports, so
 * AddRef gets confused
 */
class sbBaseDeviceInitHelper : public nsRunnable
{
public:
  sbBaseDeviceInitHelper(sbBaseDevice* aDevice)
    : mDevice(aDevice) {
      NS_ADDREF(NS_ISUPPORTS_CAST(sbIDevice*, mDevice));
    }

  NS_IMETHOD Run() {
    mDevice->Init();
    return NS_OK;
  }

private:
  ~sbBaseDeviceInitHelper() {
    NS_ISUPPORTS_CAST(sbIDevice*, mDevice)->Release();
  }
  sbBaseDevice* mDevice;
};

nsresult sbBaseDevice::Init()
{
  NS_ASSERTION(NS_IsMainThread(),
               "base device init not on main thread, implement proxying");
  if (!NS_IsMainThread()) {
    // we need to get the weak reference on the main thread because it is not
    // threadsafe, but we only ever use it from the main thread
    nsCOMPtr<nsIRunnable> event = new sbBaseDeviceInitHelper(this);
    return NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
  }

  nsresult rv;
  // get a weak ref of the device manager
  nsCOMPtr<nsISupportsWeakReference> manager =
    do_GetService("@songbirdnest.com/Songbird/DeviceManager;2", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = manager->GetWeakReference(getter_AddRefs(mParentEventTarget));
  if (NS_FAILED(rv)) {
    mParentEventTarget = nsnull;
    return rv;
  }

  // create a timer used to wait for complete batches in queue
  nsCOMPtr<nsITimer> batchEndTimer = do_CreateInstance(NS_TIMER_CONTRACTID,
                                                       &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SB_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                            NS_GET_IID(nsITimer),
                            batchEndTimer,
                            nsIProxyObjectManager::INVOKE_SYNC |
                            nsIProxyObjectManager::FORCE_PROXY_CREATION,
                            getter_AddRefs(mBatchEndTimer));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get a device statistics instance.
  rv = sbDeviceStatistics::New(this, getter_AddRefs(mDeviceStatistics));
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the device properties.
  rv = InitializeProperties();
  NS_ENSURE_SUCCESS(rv, rv);

  // Perform derived class intialization
  rv = InitDevice();
  NS_ENSURE_SUCCESS(rv, rv);

  // Perform initial properties update.
  UpdateProperties();

  return NS_OK;
}

nsresult
sbBaseDevice::GetMusicFreeSpace(sbILibrary* aLibrary,
                                PRInt64*    aFreeMusicSpace)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aFreeMusicSpace);

  // Function variables.
  nsresult rv;

  // Get the available music space.
  PRInt64 musicAvailableSpace;
  rv = GetMusicAvailableSpace(aLibrary, &musicAvailableSpace);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the device properties.
  nsCOMPtr<sbIDeviceProperties> baseDeviceProperties;
  nsCOMPtr<nsIPropertyBag2>     deviceProperties;
  rv = GetProperties(getter_AddRefs(baseDeviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = baseDeviceProperties->GetProperties(getter_AddRefs(deviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the music used space.
  PRInt64 musicUsedSpace;
  rv = deviceProperties->GetPropertyAsInt64
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_MUSIC_USED_SPACE),
          &musicUsedSpace);
  NS_ENSURE_SUCCESS(rv, rv);

  // Return result.
  if (musicAvailableSpace >= musicUsedSpace)
    *aFreeMusicSpace = musicAvailableSpace - musicUsedSpace;
  else
    *aFreeMusicSpace = 0;

  return NS_OK;
}

nsresult
sbBaseDevice::GetMusicAvailableSpace(sbILibrary* aLibrary,
                                     PRInt64*    aMusicAvailableSpace)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aMusicAvailableSpace);

  // Function variables.
  nsresult rv;

  // Get the device properties.
  nsCOMPtr<sbIDeviceProperties> baseDeviceProperties;
  nsCOMPtr<nsIPropertyBag2>     deviceProperties;
  rv = GetProperties(getter_AddRefs(baseDeviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = baseDeviceProperties->GetProperties(getter_AddRefs(deviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the total capacity.
  PRInt64 capacity;
  rv = deviceProperties->GetPropertyAsInt64
                           (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_CAPACITY),
                            &capacity);
  NS_ENSURE_SUCCESS(rv, rv);

  // Compute the amount of available music space.
  PRInt64 musicAvailableSpace;
  if (mMusicLimitPercent < 100) {
    musicAvailableSpace = (capacity * mMusicLimitPercent) /
                          static_cast<PRInt64>(100);
  } else {
    musicAvailableSpace = capacity;
  }

  // Return results.
  *aMusicAvailableSpace = musicAvailableSpace;

  return NS_OK;
}

nsresult
sbBaseDevice::GetDeviceSettingsDocument
                (nsIDOMDocument** aDeviceSettingsDocument)
{
  // No device settings document.
  NS_ENSURE_ARG_POINTER(aDeviceSettingsDocument);
  *aDeviceSettingsDocument = nsnull;
  return NS_OK;
}

nsresult
sbBaseDevice::GetDeviceSettingsDocument
                (nsIFile*         aDeviceSettingsFile,
                 nsIDOMDocument** aDeviceSettingsDocument)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aDeviceSettingsFile);
  NS_ENSURE_ARG_POINTER(aDeviceSettingsDocument);

  // Function variables.
  nsresult rv;

  // If the device settings file does not exist, just return null.
  PRBool exists;
  rv = aDeviceSettingsFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    *aDeviceSettingsDocument = nsnull;
    return NS_OK;
  }

  // Get the device settings file URI spec.
  nsCAutoString    deviceSettingsURISpec;
  nsCOMPtr<nsIURI> deviceSettingsURI;
  rv = NS_NewFileURI(getter_AddRefs(deviceSettingsURI), aDeviceSettingsFile);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deviceSettingsURI->GetSpec(deviceSettingsURISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create an XMLHttpRequest object.
  nsCOMPtr<nsIXMLHttpRequest>
    xmlHttpRequest = do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIScriptSecurityManager> ssm =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> principal;
  rv = ssm->GetSystemPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = xmlHttpRequest->Init(principal, nsnull, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the device settings file document.
  rv = xmlHttpRequest->OpenRequest(NS_LITERAL_CSTRING("GET"),
                                   deviceSettingsURISpec,
                                   PR_FALSE,                  // async
                                   SBVoidString(),            // user
                                   SBVoidString());           // password
  NS_ENSURE_SUCCESS(rv, rv);
  rv = xmlHttpRequest->Send(nsnull);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = xmlHttpRequest->GetResponseXML(aDeviceSettingsDocument);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::GetDeviceSettingsDocument
                (nsTArray<PRUint8>& aDeviceSettingsContent,
                 nsIDOMDocument**   aDeviceSettingsDocument)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aDeviceSettingsDocument);

  // Function variables.
  nsresult rv;

  // Parse the device settings document from the content.
  nsCOMPtr<nsIDOMParser> domParser = do_CreateInstance(NS_DOMPARSER_CONTRACTID,
                                                       &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = domParser->ParseFromBuffer(aDeviceSettingsContent.Elements(),
                                  aDeviceSettingsContent.Length(),
                                  "text/xml",
                                  aDeviceSettingsDocument);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

//------------------------------------------------------------------------------
//
// Device properties services.
//
//------------------------------------------------------------------------------

nsresult
sbBaseDevice::InitializeProperties()
{
  return NS_OK;
}


/**
 * Update the device properties.
 */

nsresult
sbBaseDevice::UpdateProperties()
{
  nsresult rv;

  // Update the device statistics properties.
  rv = UpdateStatisticsProperties();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/**
 * Update the device property specified by aName.
 *
 * \param aName                 Name of property to update.
 */

nsresult
sbBaseDevice::UpdateProperty(const nsAString& aName)
{
  return NS_OK;
}


/**
 * Update the device statistics properties.
 */

nsresult
sbBaseDevice::UpdateStatisticsProperties()
{
  nsresult rv;

  // Get the device properties.
  nsCOMPtr<sbIDeviceProperties>    baseDeviceProperties;
  nsCOMPtr<nsIPropertyBag2>        roDeviceProperties;
  nsCOMPtr<nsIWritablePropertyBag> deviceProperties;
  rv = GetProperties(getter_AddRefs(baseDeviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = baseDeviceProperties->GetProperties(getter_AddRefs(roDeviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);
  deviceProperties = do_QueryInterface(roDeviceProperties, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Update the statistics properties.
  //XXXeps should use base properties class and use SetPropertyInternal
  rv = deviceProperties->SetProperty
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_MUSIC_ITEM_COUNT),
          sbNewVariant(mDeviceStatistics->AudioCount()));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deviceProperties->SetProperty
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_MUSIC_USED_SPACE),
          sbNewVariant(mDeviceStatistics->AudioUsed()));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deviceProperties->SetProperty
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_MUSIC_TOTAL_PLAY_TIME),
          sbNewVariant(mDeviceStatistics->AudioPlayTime()));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deviceProperties->SetProperty
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_VIDEO_ITEM_COUNT),
          sbNewVariant(mDeviceStatistics->VideoCount()));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deviceProperties->SetProperty
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_VIDEO_USED_SPACE),
          sbNewVariant(mDeviceStatistics->VideoUsed()));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deviceProperties->SetProperty
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_VIDEO_TOTAL_PLAY_TIME),
          sbNewVariant(mDeviceStatistics->VideoPlayTime()));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


//------------------------------------------------------------------------------
//
// Device preference services.
//
//------------------------------------------------------------------------------

NS_IMETHODIMP sbBaseDevice::SetWarningDialogEnabled(const nsAString & aWarning, PRBool aEnabled)
{
  nsresult rv;

  // get the key for this warning
  nsString prefKey(NS_LITERAL_STRING(PREF_WARNING));
  prefKey.Append(aWarning);

  // have a variant to set
  nsCOMPtr<nsIWritableVariant> var =
    do_CreateInstance("@songbirdnest.com/Songbird/Variant;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = var->SetAsBool(aEnabled);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetPreference(prefKey, var);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP sbBaseDevice::GetWarningDialogEnabled(const nsAString & aWarning, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv;

  // get the key for this warning
  nsString prefKey(NS_LITERAL_STRING(PREF_WARNING));
  prefKey.Append(aWarning);

  nsCOMPtr<nsIVariant> var;
  rv = GetPreference(prefKey, getter_AddRefs(var));
  NS_ENSURE_SUCCESS(rv, rv);

  // does the pref exist?
  PRUint16 dataType;
  rv = var->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dataType == nsIDataType::VTYPE_EMPTY ||
      dataType == nsIDataType::VTYPE_VOID)
  {
    // by default warnings are enabled
    *_retval = PR_TRUE;
  } else {
    rv = var->GetAsBool(_retval);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP sbBaseDevice::ResetWarningDialogs()
{
  nsresult rv;

  // get the pref branch for this device
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = GetPrefBranch(getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  // the key for all warnings
  nsString prefKey(NS_LITERAL_STRING(PREF_WARNING));

  // clear the prefs
  rv = prefBranch->DeleteBranch(NS_ConvertUTF16toUTF8(prefKey).get());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult sbBaseDevice::GetPrefBranch(const char *aPrefBranchName,
                                     nsIPrefBranch** aPrefBranch)
{
  NS_ENSURE_ARG_POINTER(aPrefBranch);
  nsresult rv;

  // get the prefs service
  nsCOMPtr<nsIPrefService> prefService =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're not on the main thread proxy the service
  PRBool const isMainThread = NS_IsMainThread();
  if (!isMainThread) {
    nsCOMPtr<nsIPrefService> proxy;
    rv = SB_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                              NS_GET_IID(nsIPrefService),
                              prefService,
                              nsIProxyObjectManager::INVOKE_SYNC |
                              nsIProxyObjectManager::FORCE_PROXY_CREATION,
                              getter_AddRefs(proxy));
    NS_ENSURE_SUCCESS(rv, rv);
    prefService.swap(proxy);
  }

  // get the pref branch
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch(aPrefBranchName, getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're not on the main thread proxy the pref branch
  if (!isMainThread) {
    nsCOMPtr<nsIPrefBranch> proxy;
    rv = SB_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                              NS_GET_IID(nsIPrefBranch),
                              prefBranch,
                              nsIProxyObjectManager::INVOKE_SYNC |
                              nsIProxyObjectManager::FORCE_PROXY_CREATION,
                              getter_AddRefs(proxy));
    NS_ENSURE_SUCCESS(rv, rv);
    prefBranch.swap(proxy);
  }

  prefBranch.forget(aPrefBranch);

  return rv;
}

nsresult sbBaseDevice::GetPrefBranch(nsIPrefBranch** aPrefBranch)
{
  NS_ENSURE_ARG_POINTER(aPrefBranch);
  nsresult rv;

  // get id of this device
  nsID* id;
  rv = GetId(&id);
  NS_ENSURE_SUCCESS(rv, rv);

  // get that as a string
  char idString[NSID_LENGTH];
  id->ToProvidedString(idString);
  NS_Free(id);

  // create the pref key
  nsCString prefKey(PREF_DEVICE_PREFERENCES_BRANCH);
  prefKey.Append(idString);
  prefKey.AppendLiteral(".preferences.");

  return GetPrefBranch(prefKey.get(), aPrefBranch);
}

nsresult sbBaseDevice::ApplyPreference(const nsAString& aPrefName,
                                       nsIVariant*      aPrefValue)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aPrefValue);

  // Function variables.
  nsresult rv;

  // Check if it's a library preference.
  PRBool isLibraryPreference = GetIsLibraryPreference(aPrefName);

  // Apply preference.
  if (isLibraryPreference) {
    // Get the library preference info.
    nsCOMPtr<sbIDeviceLibrary> library;
    nsAutoString               libraryPrefBase;
    nsAutoString               libraryPrefName;
    rv = GetPreferenceLibrary(aPrefName,
                              getter_AddRefs(library),
                              libraryPrefBase);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = GetLibraryPreferenceName(aPrefName, libraryPrefBase, libraryPrefName);
    NS_ENSURE_SUCCESS(rv, rv);

    // Apply the library preference.
    rv = ApplyLibraryPreference(library, libraryPrefName, aPrefValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

PRBool sbBaseDevice::GetIsLibraryPreference(const nsAString& aPrefName)
{
  return StringBeginsWith(aPrefName,
                          NS_LITERAL_STRING(PREF_DEVICE_LIBRARY_BASE));
}

nsresult sbBaseDevice::GetPreferenceLibrary(const nsAString&   aPrefName,
                                            sbIDeviceLibrary** aLibrary,
                                            nsAString&         aLibraryPrefBase)
{
  // Function variables.
  nsresult rv;

  // Get the device content.
  nsCOMPtr<sbIDeviceContent> content;
  rv = GetContent(getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIArray> libraryList;
  rv = content->GetLibraries(getter_AddRefs(libraryList));
  NS_ENSURE_SUCCESS(rv, rv);

  // Search the libraries for a match.  Return results if found.
  PRUint32 libraryCount;
  rv = libraryList->GetLength(&libraryCount);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < libraryCount; i++) {
    // Get the next library.
    nsCOMPtr<sbIDeviceLibrary> library = do_QueryElementAt(libraryList, i, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the library info.
    nsAutoString guid;
    rv = library->GetGuid(guid);
    NS_ENSURE_SUCCESS(rv, rv);

    // Return library if it matches pref.
    nsAutoString libraryPrefBase;
    rv = GetLibraryPreferenceBase(library, libraryPrefBase);
    NS_ENSURE_SUCCESS(rv, rv);
    if (StringBeginsWith(aPrefName, libraryPrefBase)) {
      if (aLibrary)
        library.forget(aLibrary);
      aLibraryPrefBase.Assign(libraryPrefBase);
      return NS_OK;
    }
  }

  // Library not found.
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult sbBaseDevice::GetLibraryPreference(sbIDeviceLibrary* aLibrary,
                                            const nsAString&  aLibraryPrefName,
                                            nsIVariant**      aPrefValue)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aLibrary);

  // Function variables.
  nsresult rv;

  // Get the library preference base.
  nsAutoString libraryPrefBase;
  rv = GetLibraryPreferenceBase(aLibrary, libraryPrefBase);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetLibraryPreference(libraryPrefBase, aLibraryPrefName, aPrefValue);
}

nsresult sbBaseDevice::GetLibraryPreference(const nsAString& aLibraryPrefBase,
                                            const nsAString& aLibraryPrefName,
                                            nsIVariant**     aPrefValue)
{
  // Get the full preference name.
  nsAutoString prefName(aLibraryPrefBase);
  prefName.Append(aLibraryPrefName);

  return GetPreference(prefName, aPrefValue);
}

nsresult sbBaseDevice::ApplyLibraryPreference
                         (sbIDeviceLibrary* aLibrary,
                          const nsAString&  aLibraryPrefName,
                          nsIVariant*       aPrefValue)
{
  nsresult rv;

  // Operate under the preference lock.
  nsAutoLock preferenceLock(mPreferenceLock);

  // Get the library pref base.
  nsAutoString prefBase;
  rv = GetLibraryPreferenceBase(aLibrary, prefBase);
  NS_ENSURE_SUCCESS(rv, rv);

  // If no preference name is specified, read and apply all library preferences.
  PRBool applyAll = PR_FALSE;
  if (aLibraryPrefName.IsEmpty())
    applyAll = PR_TRUE;

  // Apply music limit preference.
  if (applyAll ||
      aLibraryPrefName.EqualsLiteral("music_limit_percent") ||
      aLibraryPrefName.EqualsLiteral("use_music_limit_percent"))
  {
    // First ensure that the music limit percentage pref is enabled.
    PRBool shouldLimitMusicSpace = PR_FALSE;
    rv = GetShouldLimitMusicSpace(prefBase, &shouldLimitMusicSpace);
    if (NS_SUCCEEDED(rv) && shouldLimitMusicSpace) {
      PRUint32 musicLimitPercent = 100;
      rv = GetMusicLimitSpacePercent(prefBase, &musicLimitPercent);
      if (NS_SUCCEEDED(rv)) {
        // Finally, apply the preference value.
        mMusicLimitPercent = musicLimitPercent;
      }
    }
    else {
      // Music space limiting is disabled, set the limit percent to 100%.
      mMusicLimitPercent = 100;
    }
  }

  return ApplyLibraryOrganizePreference(aLibrary,
                                        aLibraryPrefName,
                                        prefBase,
                                        aPrefValue);
}

nsresult sbBaseDevice::ApplyLibraryOrganizePreference
                         (sbIDeviceLibrary* aLibrary,
                          const nsAString&  aLibraryPrefName,
                          const nsAString&  aLibraryPrefBase,
                          nsIVariant*       aPrefValue)
{
  nsresult rv;
  PRBool applyAll = aLibraryPrefName.IsEmpty();

  if (!applyAll && !StringBeginsWith(aLibraryPrefName,
                                     NS_LITERAL_STRING(PREF_ORGANIZE_PREFIX)))
  {
    return NS_OK;
  }

  nsString prefBase(aLibraryPrefBase);
  if (prefBase.IsEmpty()) {
    // Get the library pref base.
    rv = GetLibraryPreferenceBase(aLibrary, prefBase);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsString guidString;
  rv = aLibrary->GetGuid(guidString);
  NS_ENSURE_SUCCESS(rv, rv);
  nsID libraryGuid;
  PRBool success =
    libraryGuid.Parse(NS_LossyConvertUTF16toASCII(guidString).get());
  NS_ENSURE_TRUE(success, NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA);

  nsAutoPtr<OrganizeData> libraryDataReleaser;
  OrganizeData* libraryData;
  PRBool found = mOrganizeLibraryPrefs.Get(libraryGuid, &libraryData);
  if (!found) {
    libraryData = new OrganizeData;
    libraryDataReleaser = libraryData;
  }
  NS_ENSURE_TRUE(libraryData, NS_ERROR_OUT_OF_MEMORY);

  // Get the preference value.
  nsCOMPtr<nsIVariant> prefValue = aPrefValue;

  if (applyAll ||
      aLibraryPrefName.EqualsLiteral(PREF_ORGANIZE_ENABLED))
  {
    if (applyAll || !prefValue) {
      rv = GetLibraryPreference(prefBase,
                                NS_LITERAL_STRING(PREF_ORGANIZE_ENABLED),
                                getter_AddRefs(prefValue));
      if (NS_FAILED(rv))
        prefValue = nsnull;
    }
    if (prefValue) {
      PRUint16 dataType;
      rv = prefValue->GetDataType(&dataType);
      if (NS_SUCCEEDED(rv) && dataType == nsIDataType::VTYPE_BOOL) {
        rv = prefValue->GetAsBool(&libraryData->organizeEnabled);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  if (applyAll ||
      aLibraryPrefName.EqualsLiteral(PREF_ORGANIZE_DIR_FORMAT))
  {
    if (applyAll || !prefValue) {
      rv = GetLibraryPreference(prefBase,
                                NS_LITERAL_STRING(PREF_ORGANIZE_DIR_FORMAT),
                                getter_AddRefs(prefValue));
      if (NS_FAILED(rv))
        prefValue = nsnull;
    }
    if (prefValue) {
      PRUint16 dataType;
      rv = prefValue->GetDataType(&dataType);
      if (NS_SUCCEEDED(rv) && dataType != nsIDataType::VTYPE_EMPTY) {
        rv = prefValue->GetAsACString(libraryData->dirFormat);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  if (applyAll ||
      aLibraryPrefName.EqualsLiteral(PREF_ORGANIZE_FILE_FORMAT))
  {
    if (applyAll || !prefValue) {
      rv = GetLibraryPreference(prefBase,
                                NS_LITERAL_STRING(PREF_ORGANIZE_FILE_FORMAT),
                                getter_AddRefs(prefValue));
      if (NS_FAILED(rv))
        prefValue = nsnull;
    }
    if (prefValue) {
      PRUint16 dataType;
      rv = prefValue->GetDataType(&dataType);
      if (NS_SUCCEEDED(rv) && dataType != nsIDataType::VTYPE_EMPTY) {
        rv = prefValue->GetAsACString(libraryData->fileFormat);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  if (!found) {
    success = mOrganizeLibraryPrefs.Put(libraryGuid, libraryData);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
    libraryDataReleaser.forget();
  }

  return NS_OK;
}

nsresult sbBaseDevice::GetLibraryPreferenceName
                         (const nsAString& aPrefName,
                          nsAString&       aLibraryPrefName)
{
  nsresult rv;

  // Get the preference library preference base.
  nsAutoString               libraryPrefBase;
  rv = GetPreferenceLibrary(aPrefName, nsnull, libraryPrefBase);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetLibraryPreferenceName(aPrefName, libraryPrefBase, aLibraryPrefName);
}

nsresult sbBaseDevice::GetLibraryPreferenceName
                         (const nsAString&  aPrefName,
                          const nsAString&  aLibraryPrefBase,
                          nsAString&        aLibraryPrefName)
{
  // Validate pref name.
  NS_ENSURE_TRUE(StringBeginsWith(aPrefName, aLibraryPrefBase),
                 NS_ERROR_ILLEGAL_VALUE);

  // Extract the library preference name.
  aLibraryPrefName.Assign(Substring(aPrefName, aLibraryPrefBase.Length()));

  return NS_OK;
}

nsresult sbBaseDevice::GetLibraryPreferenceBase(sbIDeviceLibrary* aLibrary,
                                                nsAString&        aPrefBase)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aLibrary);

  // Function variables.
  nsresult rv;

  // Get the library info.
  nsAutoString guid;
  rv = aLibrary->GetGuid(guid);
  NS_ENSURE_SUCCESS(rv, rv);

  // Produce the library preference base.
  aPrefBase.Assign(NS_LITERAL_STRING(PREF_DEVICE_LIBRARY_BASE));
  aPrefBase.Append(guid);
  aPrefBase.AppendLiteral(".");

  return NS_OK;
}

nsresult
sbBaseDevice::GetCapabilitiesPreference(nsIVariant** aCapabilities)
{
  NS_ENSURE_ARG_POINTER(aCapabilities);

  nsresult rv;

  // Get the device settings document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  rv = GetDeviceSettingsDocument(getter_AddRefs(domDocument));
  NS_ENSURE_SUCCESS(rv, rv);

  // Read the device capabilities from the device settings document.
  if (domDocument) {
    nsCOMPtr<sbIDeviceCapabilities> deviceCapabilities =
      do_CreateInstance(SONGBIRD_DEVICECAPABILITIES_CONTRACTID);
    sbDeviceXMLCapabilities xmlCapabilities(domDocument);
    rv = xmlCapabilities.Read(deviceCapabilities);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = deviceCapabilities->InitDone();
    NS_ENSURE_SUCCESS(rv, rv);

    // Return any device capabilities from the device settings document.
    if (xmlCapabilities.HasCapabilities()) {
      sbNewVariant capabilitiesVariant(deviceCapabilities);
      NS_ENSURE_TRUE(capabilitiesVariant.get(), NS_ERROR_FAILURE);
      NS_ADDREF(*aCapabilities = capabilitiesVariant.get());
      return NS_OK;
    }
  }

  // Return no capabilities.
  sbNewVariant capabilitiesVariant;
  NS_ENSURE_TRUE(capabilitiesVariant.get(), NS_ERROR_FAILURE);
  NS_ADDREF(*aCapabilities = capabilitiesVariant.get());

  return NS_OK;
}

//------------------------------------------------------------------------------
//
// Device sync services.
//
//------------------------------------------------------------------------------

nsresult
sbBaseDevice::HandleSyncRequest(TransferRequest* aRequest)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aRequest);

  // Function variables.
  nsresult rv;

  // Cancel operation if device is not linked to the local sync partner.
  PRBool isLinkedLocally;
  rv = sbDeviceUtils::SyncCheckLinkedPartner(this, PR_TRUE, &isLinkedLocally);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isLinkedLocally) {
    rv = SetState(STATE_CANCEL);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  // Ensure enough space is available for operation.  Cancel operation if not.
  PRBool abort;
  rv = EnsureSpaceForSync(aRequest, &abort);
  NS_ENSURE_SUCCESS(rv, rv);
  if (abort) {
    rv = SetState(STATE_CANCEL);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  // Produce the sync change set.
  nsCOMPtr<sbILibraryChangeset> changeset;
  rv = SyncProduceChangeset(aRequest, getter_AddRefs(changeset));
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsRequestAbortedOrDeviceDisconnected()) {
    return NS_ERROR_ABORT;
  }

  // Apply changes to the destination library.
  nsCOMPtr<sbIDeviceLibrary> dstLib = do_QueryInterface(aRequest->list, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetState(STATE_SYNCING);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIDeviceStatus> status;
  rv = GetCurrentStatus(getter_AddRefs(status));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = status->SetCurrentState(STATE_SYNCING);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = status->SetCurrentSubState(STATE_SYNCING);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SyncApplyChanges(dstLib, changeset);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::EnsureSpaceForSync(TransferRequest* aRequest,
                                 PRBool*          aAbort)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aRequest);
  NS_ENSURE_ARG_POINTER(aAbort);

  // Function variables.
  PRBool   success;
  nsresult rv;

  // Get the sync source and destination libraries.
  nsCOMPtr<sbILibrary> srcLib = do_QueryInterface(aRequest->item, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<sbIDeviceLibrary> dstLib = do_QueryInterface(aRequest->list, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Allocate a list of sync items and a hash table of their sizes.
  nsCOMArray<sbIMediaItem>                     syncItemList;
  nsDataHashtable<nsISupportsHashKey, PRInt64> syncItemSizeMap;
  success = syncItemSizeMap.Init();
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  // Get the sync item sizes and total size.
  PRInt64 totalSyncSize;
  rv = SyncGetSyncItemSizes(srcLib,
                            dstLib,
                            syncItemList,
                            syncItemSizeMap,
                            &totalSyncSize);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the space available for syncing.
  PRInt64 availableSpace;
  rv = SyncGetSyncAvailableSpace(dstLib, &availableSpace);
  NS_ENSURE_SUCCESS(rv, rv);

  // If not enough space is available, ask the user what action to take.  If the
  // user does not abort the operation, create and sync to a sync media list
  // that will fit in the available space.  Otherwise, set the management type
  // to manual and return with abort.
  if (availableSpace < totalSyncSize) {
    // Ask the user what action to take.
    PRBool abort;
    rv = sbDeviceUtils::QueryUserSpaceExceeded(this,
                                               dstLib,
                                               totalSyncSize,
                                               availableSpace,
                                               &abort);
    NS_ENSURE_SUCCESS(rv, rv);
    if (abort) {
      rv = dstLib->SetMgmtType(sbIDeviceLibrary::MGMT_TYPE_MANUAL);
      NS_ENSURE_SUCCESS(rv, rv);
      *aAbort = PR_TRUE;
      return NS_OK;
    }

    // Create a sync media list and sync to it.
    rv = SyncCreateAndSyncToList(srcLib,
                                 dstLib,
                                 syncItemList,
                                 syncItemSizeMap,
                                 availableSpace);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Don't abort.
  *aAbort = PR_FALSE;

  return NS_OK;
}

nsresult
sbBaseDevice::SyncCreateAndSyncToList
  (sbILibrary*                                   aSrcLib,
   sbIDeviceLibrary*                             aDstLib,
   nsCOMArray<sbIMediaItem>&                     aSyncItemList,
   nsDataHashtable<nsISupportsHashKey, PRInt64>& aSyncItemSizeMap,
   PRInt64                                       aAvailableSpace)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aSrcLib);
  NS_ENSURE_ARG_POINTER(aDstLib);

  // Function variables.
  nsresult rv;

  // Set to sync to an empty list of playlists to prevent syncing while the sync
  // playlist is created.
  nsCOMPtr<nsIMutableArray> emptySyncPlaylistList =
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDstLib->SetSyncPlaylistList(emptySyncPlaylistList);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDstLib->SetMgmtType(sbIDeviceLibrary::MGMT_TYPE_SYNC_PLAYLISTS);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a shuffled sync item list that will fit in the available space.
  nsCOMPtr<nsIArray> syncItemList;
  rv = SyncShuffleSyncItemList(aSyncItemList,
                               aSyncItemSizeMap,
                               aAvailableSpace,
                               getter_AddRefs(syncItemList));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a new source sync media list.
  nsCOMPtr<sbIMediaList> syncMediaList;
  rv = SyncCreateSyncMediaList(aSrcLib,
                               syncItemList,
                               getter_AddRefs(syncMediaList));
  NS_ENSURE_SUCCESS(rv, rv);

  // Sync to the sync media list.
  rv = SyncToMediaList(aDstLib, syncMediaList);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::SyncShuffleSyncItemList
  (nsCOMArray<sbIMediaItem>&                     aSyncItemList,
   nsDataHashtable<nsISupportsHashKey, PRInt64>& aSyncItemSizeMap,
   PRInt64                                       aAvailableSpace,
   nsIArray**                                    aShuffleSyncItemList)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aShuffleSyncItemList);

  // Function variables.
  PRBool   success;
  nsresult rv;

  // Get the sync item list info.
  PRInt32 itemCount = aSyncItemList.Count();

  // Seed the random number generator for shuffling.
  srand((unsigned int) (PR_Now() & 0xFFFFFFFF));

  // Copy the sync item list to a vector and shuffle it.
  std::vector<sbIMediaItem*> randomItemList;
  for (PRInt32 i = 0; i < itemCount; i++) {
    randomItemList.push_back(aSyncItemList[i]);
  }
  std::random_shuffle(randomItemList.begin(), randomItemList.end());

  // Create a shuffled sync item list that will fill the available space.
  nsCOMPtr<nsIMutableArray> shuffleSyncItemList =
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add sync items from the shuffled list to fill the available space,
  // reserving some margin for error.
  PRInt64 remainingSpace = aAvailableSpace;
  remainingSpace -= (aAvailableSpace * SYNC_PLAYLIST_MARGIN_PCT) / 100;
  for (PRInt32 i = 0; i < itemCount; i++) {
    // Get the next item and its info.
    nsCOMPtr<sbIMediaItem> syncItem = randomItemList[i];
    PRInt64                itemSize;
    success = aSyncItemSizeMap.Get(syncItem, &itemSize);
    NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

    // Add item if it fits in remaining space.
    if (remainingSpace >= itemSize) {
      rv = shuffleSyncItemList->AppendElement(syncItem, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
      remainingSpace -= itemSize;
    }
  }

  // Return results.
  NS_ADDREF(*aShuffleSyncItemList = shuffleSyncItemList);

  return NS_OK;
}

nsresult
sbBaseDevice::SyncCreateSyncMediaList(sbILibrary*    aSrcLib,
                                      nsIArray*      aSyncItemList,
                                      sbIMediaList** aSyncMediaList)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aSrcLib);
  NS_ENSURE_ARG_POINTER(aSyncItemList);
  NS_ENSURE_ARG_POINTER(aSyncMediaList);

  // Function variables.
  nsresult rv;

  // Create a new source sync media list.
  nsCOMPtr<sbIMediaList> syncMediaList;
  rv = aSrcLib->CreateMediaList(NS_LITERAL_STRING("simple"),
                                nsnull,
                                getter_AddRefs(syncMediaList));
  NS_ENSURE_SUCCESS(rv, rv);

  // Add the sync item list to the sync media list.
  nsCOMPtr<nsISimpleEnumerator> syncItemListEnum;
  rv = aSyncItemList->Enumerate(getter_AddRefs(syncItemListEnum));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = syncMediaList->AddSome(syncItemListEnum);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the device name.
  nsAutoString deviceName;
  rv = GetName(deviceName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Produce the sync media list name.
  nsString listName = SBLocalizedString
                        ("device.error.not_enough_freespace.playlist_name");

  // Set the sync media list name.
  rv = syncMediaList->SetName(listName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Return results.
  *aSyncMediaList = nsnull;
  syncMediaList.swap(*aSyncMediaList);

  return NS_OK;
}

nsresult
sbBaseDevice::SyncToMediaList(sbIDeviceLibrary* aDstLib,
                              sbIMediaList*     aMediaList)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aDstLib);
  NS_ENSURE_ARG_POINTER(aMediaList);

  // Function variables.
  nsresult rv;

  // Create a sync playlist list array with the sync media list.
  nsCOMPtr<nsIMutableArray> syncPlaylistList =
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = syncPlaylistList->AppendElement(aMediaList, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set to sync to the sync media list.
  rv = aDstLib->SetSyncPlaylistList(syncPlaylistList);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDstLib->SetMgmtType(sbIDeviceLibrary::MGMT_TYPE_SYNC_PLAYLISTS);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::SyncGetSyncItemSizes
  (sbILibrary*                                   aSrcLib,
   sbIDeviceLibrary*                             aDstLib,
   nsCOMArray<sbIMediaItem>&                     aSyncItemList,
   nsDataHashtable<nsISupportsHashKey, PRInt64>& aSyncItemSizeMap,
   PRInt64*                                      aTotalSyncSize)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aSrcLib);
  NS_ENSURE_ARG_POINTER(aDstLib);
  NS_ENSURE_ARG_POINTER(aTotalSyncSize);

  // Function variables.
  nsresult rv;

  // Get the list of sync media lists.
  nsCOMPtr<nsIArray> syncList;
  rv = SyncGetSyncList(aSrcLib, aDstLib, getter_AddRefs(syncList));
  NS_ENSURE_SUCCESS(rv, rv);

  // Fill in the sync item size table and calculate the total sync size.
  PRInt64 totalSyncSize = 0;
  PRUint32 syncListLength;
  rv = syncList->GetLength(&syncListLength);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < syncListLength; i++) {
    // Get the next sync media list.
    nsCOMPtr<sbIMediaList> syncML = do_QueryElementAt(syncList, i, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the sizes of the sync media items.
    rv = SyncGetSyncItemSizes(syncML,
                              aSyncItemList,
                              aSyncItemSizeMap,
                              &totalSyncSize);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Return results.
  *aTotalSyncSize = totalSyncSize;

  return NS_OK;
}

nsresult
sbBaseDevice::SyncGetSyncItemSizes
  (sbIMediaList*                                 aSyncML,
   nsCOMArray<sbIMediaItem>&                     aSyncItemList,
   nsDataHashtable<nsISupportsHashKey, PRInt64>& aSyncItemSizeMap,
   PRInt64*                                      aTotalSyncSize)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aSyncML);
  NS_ENSURE_ARG_POINTER(aTotalSyncSize);

  // Function variables.
  PRBool   success;
  nsresult rv;

  // Accumulate the sizes of all sync items.  Use GetItemByIndex like the
  // diffing service does.
  PRInt64  totalSyncSize = *aTotalSyncSize;
  PRUint32 itemCount;
  rv = aSyncML->GetLength(&itemCount);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < itemCount; i++) {
    // Get the sync item.
    nsCOMPtr<sbIMediaItem> mediaItem;
    rv = aSyncML->GetItemByIndex(i, getter_AddRefs(mediaItem));
    NS_ENSURE_SUCCESS(rv, rv);

    // Ignore media lists.
    nsCOMPtr<sbIMediaList> mediaList = do_QueryInterface(mediaItem, &rv);
    if (NS_SUCCEEDED(rv))
      continue;

    // Do nothing more if item is already in list.
    if (aSyncItemSizeMap.Get(mediaItem, nsnull))
      continue;

    // Get the item size adding in the per track overhead.  Assume a length of
    // 0 on error.
    PRInt64 contentLength;
    rv = sbLibraryUtils::GetContentLength(mediaItem, &contentLength);
    if (NS_FAILED(rv))
      contentLength = 0;
    contentLength += mPerTrackOverhead;

    // Add item.
    success = aSyncItemList.AppendObject(mediaItem);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
    success = aSyncItemSizeMap.Put(mediaItem, contentLength);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
    totalSyncSize += contentLength;
  }

  // Return results.
  *aTotalSyncSize = totalSyncSize;

  return NS_OK;
}

nsresult
sbBaseDevice::SyncGetSyncList(sbILibrary*       aSrcLib,
                              sbIDeviceLibrary* aDstLib,
                              nsIArray**        aSyncList)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aSrcLib);
  NS_ENSURE_ARG_POINTER(aDstLib);
  NS_ENSURE_ARG_POINTER(aSyncList);

  // Function variables.
  nsresult rv;

  // Get the list of sync media lists.
  nsCOMPtr<nsIArray> syncList;
  PRUint32           syncMode;
  rv = aDstLib->GetMgmtType(&syncMode);
  NS_ENSURE_SUCCESS(rv, rv);
  switch (syncMode)
  {
    case sbIDeviceLibrary::MGMT_TYPE_SYNC_ALL:
    {
      // Create a sync all array containing the entire source library.
      nsCOMPtr<nsIMutableArray>
        syncAllList =
          do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1",
                            &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<sbIMediaList> srcLibML = do_QueryInterface(aSrcLib, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = syncAllList->AppendElement(srcLibML, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);

      // Set the sync list.
      syncList = do_QueryInterface(syncAllList, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    } break;

    case sbIDeviceLibrary::MGMT_TYPE_SYNC_PLAYLISTS:
      rv = aDstLib->GetSyncPlaylistList(getter_AddRefs(syncList));
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    default:
      NS_ENSURE_SUCCESS(NS_ERROR_ILLEGAL_VALUE, NS_ERROR_ILLEGAL_VALUE);
      break;
  }

  // Return results.
  syncList.forget(aSyncList);

  return NS_OK;
}

nsresult
sbBaseDevice::SyncGetSyncAvailableSpace(sbILibrary* aLibrary,
                                        PRInt64*    aAvailableSpace)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aLibrary);
  NS_ENSURE_ARG_POINTER(aAvailableSpace);

  // Function variables.
  nsresult rv;

  // Get the device properties.
  nsCOMPtr<sbIDeviceProperties> baseDeviceProperties;
  nsCOMPtr<nsIPropertyBag2>     deviceProperties;
  rv = GetProperties(getter_AddRefs(baseDeviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = baseDeviceProperties->GetProperties(getter_AddRefs(deviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the free space and the music used space.
  PRInt64 freeSpace;
  PRInt64 musicUsedSpace;
  rv = deviceProperties->GetPropertyAsInt64
                           (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_FREE_SPACE),
                            &freeSpace);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deviceProperties->GetPropertyAsInt64
         (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_MUSIC_USED_SPACE),
          &musicUsedSpace);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add track overhead to the music used space.
  PRUint32 trackCount;
  rv = aLibrary->GetLength(&trackCount);
  NS_ENSURE_SUCCESS(rv, rv);
  musicUsedSpace += trackCount * mPerTrackOverhead;

  // Determine the total available space for syncing as the free space plus the
  // space used for music.
  PRInt64 availableSpace = freeSpace + musicUsedSpace;

  // Apply limit to the total space available for music.
  PRInt64 musicAvailableSpace;
  rv = GetMusicAvailableSpace(aLibrary, &musicAvailableSpace);
  NS_ENSURE_SUCCESS(rv, rv);
  if (availableSpace >= musicAvailableSpace)
    availableSpace = musicAvailableSpace;

  // Return results.
  *aAvailableSpace = availableSpace;

  return NS_OK;
}

nsresult
sbBaseDevice::SyncProduceChangeset(TransferRequest*      aRequest,
                                   sbILibraryChangeset** aChangeset)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aRequest);
  NS_ENSURE_ARG_POINTER(aChangeset);

  // Function variables.
  nsresult rv;

  // Get the sync source and destination libraries.
  nsCOMPtr<sbILibrary> srcLib = do_QueryInterface(aRequest->item, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<sbIDeviceLibrary> dstLib = do_QueryInterface(aRequest->list, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set up to force a diff on all media lists so that all source and
  // destination media lists can be compared with each other.
  rv = SyncForceDiffMediaLists(dstLib);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the list of sync media lists.
  nsCOMPtr<nsIArray> syncList;
  PRUint32           syncMode;
  rv = dstLib->GetMgmtType(&syncMode);
  NS_ENSURE_SUCCESS(rv, rv);
  switch (syncMode)
  {
    case sbIDeviceLibrary::MGMT_TYPE_SYNC_ALL:
    {
      // Create a sync all array containing the entire source library.
      nsCOMPtr<nsIMutableArray>
        syncAllList =
          do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1",
                            &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = syncAllList->AppendElement(srcLib, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);

      // Set the sync list.
      syncList = do_QueryInterface(syncAllList, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    } break;

    case sbIDeviceLibrary::MGMT_TYPE_SYNC_PLAYLISTS:
      rv = dstLib->GetSyncPlaylistList(getter_AddRefs(syncList));
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    default:
      NS_ENSURE_SUCCESS(NS_ERROR_ILLEGAL_VALUE, NS_ERROR_ILLEGAL_VALUE);
      break;
  }

  // Get the diffing service.
  nsCOMPtr<sbILibraryDiffingService>
    diffService = do_GetService(SB_LOCALDATABASE_DIFFINGSERVICE_CONTRACTID,
                                &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Produce the sync changeset.
  nsCOMPtr<sbILibraryChangeset> changeset;
  rv = diffService->CreateMultiChangeset(syncList,
                                         dstLib,
                                         getter_AddRefs(changeset));
  NS_ENSURE_SUCCESS(rv, rv);

  // Return results.
  NS_ADDREF(*aChangeset = changeset);

  return NS_OK;
}

nsresult
sbBaseDevice::SyncApplyChanges(sbIDeviceLibrary*    aDstLibrary,
                               sbILibraryChangeset* aChangeset)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aDstLibrary);
  NS_ENSURE_ARG_POINTER(aChangeset);

  // Function variables.
  PRBool   success;
  nsresult rv;

  // Create some change list arrays.
  nsCOMArray<sbIMediaList>     addMediaListList;
  nsCOMArray<sbILibraryChange> mediaListChangeList;
  nsCOMPtr<nsIMutableArray>    addItemList =
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIMutableArray>    deleteItemList =
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  bool const playlistsSupported = sbDeviceUtils::ArePlaylistsSupported(this);

  // Get the list of all changes.
  nsCOMPtr<nsIArray> changeList;
  PRUint32           changeCount;
  rv = aChangeset->GetChanges(getter_AddRefs(changeList));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = changeList->GetLength(&changeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // Group changes for later processing but apply property updates immediately.
  for (PRUint32 i = 0; i < changeCount; i++) {
    if (IsRequestAbortedOrDeviceDisconnected()) {
      return NS_ERROR_ABORT;
    }
    // Get the next change.
    nsCOMPtr<sbILibraryChange> change = do_QueryElementAt(changeList, i, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Put the change into the appropriate list.
    PRUint32 operation;
    rv = change->GetOperation(&operation);
    NS_ENSURE_SUCCESS(rv, rv);

    // Add item to add media list list or add item list.
    PRBool itemIsList;
    rv = change->GetItemIsList(&itemIsList);
    NS_ENSURE_SUCCESS(rv, rv);

    // if this is a playlist and they're not supported ignore the change
    if (itemIsList && !playlistsSupported) {
      continue;
    }

    switch (operation)
    {
      case sbIChangeOperation::DELETED:
        {
          // Add the destination item to the delete list.
          nsCOMPtr<sbIMediaItem> mediaItem;
          rv = change->GetDestinationItem(getter_AddRefs(mediaItem));
          NS_ENSURE_SUCCESS(rv, rv);
          rv = deleteItemList->AppendElement(mediaItem, PR_FALSE);
          NS_ENSURE_SUCCESS(rv, rv);
        } break;

      case sbIChangeOperation::ADDED:
        {
          // Get the source item to add.
          nsCOMPtr<sbIMediaItem> mediaItem;
          rv = change->GetSourceItem(getter_AddRefs(mediaItem));
          NS_ENSURE_SUCCESS(rv, rv);

          if (itemIsList) {
            nsCOMPtr<sbIMediaList> mediaList = do_QueryInterface(mediaItem,
                                                                 &rv);
            NS_ENSURE_SUCCESS(rv, rv);
            rv = addMediaListList.AppendObject(mediaList);
            NS_ENSURE_SUCCESS(rv, rv);
          } else {
            rv = addItemList->AppendElement(mediaItem, PR_FALSE);
            NS_ENSURE_SUCCESS(rv, rv);
          }
        } break;

      case sbIChangeOperation::MODIFIED:
        {
          // If the change is to a media list, add it to the media list change
          // list.
          if (itemIsList) {
            success = mediaListChangeList.AppendObject(change);
            NS_ENSURE_SUCCESS(success, NS_ERROR_FAILURE);
          }

          // Update the item properties.
          rv = SyncUpdateProperties(change);
          NS_ENSURE_SUCCESS(rv, rv);
        } break;

      default:
        break;
    }
  }

  if (IsRequestAbortedOrDeviceDisconnected()) {
    return NS_ERROR_ABORT;
  }

  // Delete items.
  nsCOMPtr<nsISimpleEnumerator> deleteItemEnum;
  rv = deleteItemList->Enumerate(getter_AddRefs(deleteItemEnum));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDstLibrary->RemoveSome(deleteItemEnum);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsRequestAbortedOrDeviceDisconnected()) {
    return NS_ERROR_ABORT;
  }

  // Add items.
  nsCOMPtr<nsISimpleEnumerator> addItemEnum;
  rv = addItemList->Enumerate(getter_AddRefs(addItemEnum));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDstLibrary->AddSome(addItemEnum);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsRequestAbortedOrDeviceDisconnected()) {
    rv = sbDeviceUtils::DeleteByProperty(aDstLibrary,
                                         NS_LITERAL_STRING(SB_PROPERTY_HIDDEN),
                                         NS_LITERAL_STRING("1"));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to remove partial added items");
    return NS_ERROR_ABORT;
  }

  // Add media lists.
  PRInt32 count = addMediaListList.Count();
  for (PRInt32 i = 0; i < count; i++) {
    if (IsRequestAbortedOrDeviceDisconnected()) {
      return NS_ERROR_ABORT;
    }
    rv = SyncAddMediaList(aDstLibrary, addMediaListList[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Sync the media lists.
  rv = SyncMediaLists(mediaListChangeList);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::SyncAddMediaList(sbIDeviceLibrary* aDstLibrary,
                               sbIMediaList*     aMediaList)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aDstLibrary);
  NS_ENSURE_ARG_POINTER(aMediaList);

  // Function variables.
  nsresult rv;

  // Don't sync media list if it shouldn't be synced.
  PRBool shouldSync = PR_FALSE;
  rv = ShouldSyncMediaList(aMediaList, &shouldSync);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!shouldSync)
    return NS_OK;

  // Create a main thread destination library proxy to add the media lists.
  // This ensures that the library notification for the added media list is
  // delivered and processed before media list items are added.  This ensures
  // that a device listener is added for the added media list in time to get
  // notifications for the added media list items.
  nsCOMPtr<sbIDeviceLibrary> proxyDstLibrary;
  rv = SB_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                            NS_GET_IID(sbIDeviceLibrary),
                            aDstLibrary,
                            nsIProxyObjectManager::INVOKE_SYNC |
                            nsIProxyObjectManager::FORCE_PROXY_CREATION,
                            getter_AddRefs(proxyDstLibrary));
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the media list.  Don't use the add method because it disables
  // notifications until after the media list and its items are added.
  nsCOMPtr<sbIMediaList> mediaList;
  rv = proxyDstLibrary->CopyMediaList(NS_LITERAL_STRING("simple"),
                                      aMediaList,
                                      getter_AddRefs(mediaList));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::SyncMediaLists(nsCOMArray<sbILibraryChange>& aMediaListChangeList)
{
  nsresult rv;

  // Just replace the destination media lists with the source media lists.
  // TODO: just apply changes between the source and destination lists.

  // Sync each media list.
  PRInt32 count = aMediaListChangeList.Count();
  for (PRInt32 i = 0; i < count; i++) {
    // Get the media list change.
    nsCOMPtr<sbILibraryChange> change = aMediaListChangeList[i];

    // Get the destination media list item and library.
    nsCOMPtr<sbIMediaItem>     dstMediaListItem;
    nsCOMPtr<sbILibrary>       dstLibrary;
    nsCOMPtr<sbIDeviceLibrary> dstDeviceLibrary;
    rv = change->GetDestinationItem(getter_AddRefs(dstMediaListItem));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = dstMediaListItem->GetLibrary(getter_AddRefs(dstLibrary));
    NS_ENSURE_SUCCESS(rv, rv);
    dstDeviceLibrary = do_QueryInterface(dstLibrary, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Remove the destination media list item.
    rv = dstLibrary->Remove(dstMediaListItem);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the source media list.
    nsCOMPtr<sbIMediaItem> srcMediaListItem;
    nsCOMPtr<sbIMediaList> srcMediaList;
    rv = change->GetSourceItem(getter_AddRefs(srcMediaListItem));
    NS_ENSURE_SUCCESS(rv, rv);
    srcMediaList = do_QueryInterface(srcMediaListItem, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Add the source media list to the destination library.
    rv = SyncAddMediaList(dstDeviceLibrary, srcMediaList);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
sbBaseDevice::SyncUpdateProperties(sbILibraryChange* aChange)
{
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aChange);

  // Function variables.
  nsresult rv;

  // Get the item to update.
  nsCOMPtr<sbIMediaItem> mediaItem;
  rv = aChange->GetDestinationItem(getter_AddRefs(mediaItem));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the list of properties to update.
  nsCOMPtr<nsIArray> propertyList;
  PRUint32           propertyCount;
  rv = aChange->GetProperties(getter_AddRefs(propertyList));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = propertyList->GetLength(&propertyCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // Update properties.
  for (PRUint32 i = 0; i < propertyCount; i++) {
    // Get the next property to update.
    nsCOMPtr<sbIPropertyChange>
      property = do_QueryElementAt(propertyList, i, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the property info.
    nsAutoString propertyID;
    nsAutoString propertyValue;
    nsAutoString oldPropertyValue;
    rv = property->GetId(propertyID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = property->GetNewValue(propertyValue);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = property->GetOldValue(oldPropertyValue);
    NS_ENSURE_SUCCESS(rv, rv);

    // Don't sync properties set by the device.
    if (propertyID.EqualsLiteral(SB_PROPERTY_CONTENTURL) ||
        propertyID.EqualsLiteral(SB_PROPERTY_DEVICE_PERSISTENT_ID) ||
        propertyID.EqualsLiteral(SB_PROPERTY_LAST_SYNC_PLAYCOUNT) ||
        propertyID.EqualsLiteral(SB_PROPERTY_LAST_SYNC_SKIPCOUNT)) {
      continue;
    }

    // Merge the property, ignoring errors.
    SyncMergeProperty(mediaItem, propertyID, propertyValue, oldPropertyValue);
  }

  return NS_OK;
}

/**
 * Merges the property value based on what property we're dealing with
 */
nsresult
sbBaseDevice::SyncMergeProperty(sbIMediaItem * aItem,
                                nsAString const & aPropertyId,
                                nsAString const & aNewValue,
                                nsAString const & aOldValue) {
  nsresult rv = NS_OK;

  nsString mergedValue = nsString(aNewValue);
  if (aPropertyId.Equals(NS_LITERAL_STRING(SB_PROPERTY_LASTPLAYTIME))) {
    rv = SyncMergeSetToLatest(aNewValue, aOldValue, mergedValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return aItem->SetProperty(aPropertyId, mergedValue);
}

/**
 * Returns the latest of the date/time. The dates are in milliseconds since
 * the JS Data's epoch date.
 */
nsresult
sbBaseDevice::SyncMergeSetToLatest(nsAString const & aNewValue,
                                   nsAString const & aOldValue,
                                   nsAString & aMergedValue) {
  nsresult rv;
  PRInt64 newDate;
  newDate = nsString_ToInt64(aNewValue, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 oldDate;
  oldDate = nsString_ToInt64(aOldValue, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  aMergedValue = newDate > oldDate ? aNewValue : aOldValue;
  return NS_OK;
}

nsresult
sbBaseDevice::SyncForceDiffMediaLists(sbIMediaList* aMediaList)
{
  TRACE(("%s", __FUNCTION__));
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aMediaList);

  // Function variables.
  nsresult rv;

  // Get the list of media lists.  Do nothing if none available.
  nsCOMPtr<nsIArray> mediaListList;
  rv = aMediaList->GetItemsByProperty(NS_LITERAL_STRING(SB_PROPERTY_ISLIST),
                                      NS_LITERAL_STRING("1"),
                                      getter_AddRefs(mediaListList));
  if (NS_FAILED(rv))
    return NS_OK;

  // Set the force diff property on each media list.
  PRUint32 mediaListCount;
  rv = mediaListList->GetLength(&mediaListCount);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < mediaListCount; i++) {
    // Get the media list.
    nsCOMPtr<sbIMediaList> mediaList = do_QueryElementAt(mediaListList, i, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the force diff property.
    rv = mediaList->SetProperty
                      (NS_LITERAL_STRING(DEVICE_PROPERTY_SYNC_FORCE_DIFF),
                       NS_LITERAL_STRING("1"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
sbBaseDevice::ShouldSyncMediaList(sbIMediaList* aMediaList,
                                  PRBool*       aShouldSync)
{
  TRACE(("%s", __FUNCTION__));
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aMediaList);
  NS_ENSURE_ARG_POINTER(aShouldSync);

  // Function variables.
  nsresult rv;

  // Default to should not sync.
  *aShouldSync = PR_FALSE;

  // Don't sync download media lists.
  nsAutoString customType;
  rv = aMediaList->GetProperty(NS_LITERAL_STRING(SB_PROPERTY_CUSTOMTYPE),
                               customType);
  NS_ENSURE_SUCCESS(rv, rv);
  if (customType.EqualsLiteral("download"))
    return NS_OK;

  // Don't sync hidden lists.
  nsAutoString hidden;
  rv = aMediaList->GetProperty(NS_LITERAL_STRING(SB_PROPERTY_HIDDEN), hidden);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hidden.EqualsLiteral("1"))
    return NS_OK;

  // Don't sync media lists that are storage for other media lists (e.g., simple
  // media lists for smart media lists).
  nsAutoString outerGUID;
  rv = aMediaList->GetProperty(NS_LITERAL_STRING(SB_PROPERTY_OUTERGUID),
                               outerGUID);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!outerGUID.IsEmpty())
    return NS_OK;

  // Media list should be synced.
  *aShouldSync = PR_TRUE;

  return NS_OK;
}

nsresult
sbBaseDevice::PromptForEjectDuringPlayback(PRBool* aEject)
{
  TRACE(("%s", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aEject);

  nsresult rv;

  sbPrefBranch prefBranch("songbird.device.dialog.", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hide_dialog = prefBranch.GetBoolPref("eject_while_playing", PR_FALSE);

  if (hide_dialog) {
    // if the dialog is disabled, continue as if the user had said yes
    *aEject = PR_TRUE;
    return NS_OK;
  }

  // get the prompter service and wait for a window to be available
  nsCOMPtr<sbIPrompter> prompter =
    do_GetService("@songbirdnest.com/Songbird/Prompter;1");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prompter->SetWaitForWindow(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the stringbundle
  sbStringBundle bundle;

  // get the window title
  nsString const& title = bundle.Get("device.dialog.eject_while_playing.title");

  // get the device name
  nsString deviceName;
  rv = GetName(deviceName);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the message, based on the device name
  nsTArray<nsString> formatParams;
  formatParams.AppendElement(deviceName);
  nsString const& message =
    bundle.Format("device.dialog.eject_while_playing.message", formatParams);

  // get the text for the eject button
  nsString const& eject = bundle.Get("device.dialog.eject_while_playing.eject");

  // get the text for the checkbox
  nsString const& check =
    bundle.Get("device.dialog.eject_while_playing.dontask");

  // show the dialog box
  PRBool accept;
  rv = prompter->ConfirmEx(nsnull, title.get(), message.get(),
      (nsIPromptService::BUTTON_POS_0 *
       nsIPromptService::BUTTON_TITLE_IS_STRING) +
      (nsIPromptService::BUTTON_POS_1 *
       nsIPromptService::BUTTON_TITLE_CANCEL), eject.get(), nsnull, nsnull,
      check.get(), &hide_dialog, &accept);
  NS_ENSURE_SUCCESS(rv, rv);

  *aEject = !accept;

  // save the checkbox state
  rv = prefBranch.SetBoolPref("eject_while_playing", hide_dialog);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult sbBaseDevice::GetPrimaryLibrary(sbIDeviceLibrary ** aDeviceLibrary)
{
  TRACE(("%s", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aDeviceLibrary);

  nsCOMPtr<sbIDeviceContent> content;
  nsresult rv = GetContent(getter_AddRefs(content));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIArray> libraries;
  rv = content->GetLibraries(getter_AddRefs(libraries));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 libraryCount;
  rv = libraries->GetLength(&libraryCount);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(libraryCount > 0, NS_ERROR_UNEXPECTED);

  nsCOMPtr<sbIDeviceLibrary> deviceLib =
    do_QueryElementAt(libraries, 0, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  deviceLib.forget(aDeviceLibrary);
  return NS_OK;
}

nsresult sbBaseDevice::GetDeviceWriteContentSrc
                         (sbIMediaItem* aWriteDstItem,
                          nsIURI*       aContentSrcBaseURI,
                          nsIURI*       aWriteSrcURI,
                          nsIURI **     aContentSrc)
{
  TRACE(("%s", __FUNCTION__));
  // Validate arguments.
  NS_ENSURE_ARG_POINTER(aWriteDstItem);
  NS_ENSURE_ARG_POINTER(aContentSrcBaseURI);
  NS_ENSURE_ARG_POINTER(aContentSrc);

  // Function variables.
  nsString         kIllegalChars =
                     NS_ConvertASCIItoUTF16(FILE_ILLEGAL_CHARACTERS);
  nsCOMPtr<nsIURI> writeSrcURI = aWriteSrcURI;
  nsresult         rv;

  // If no write source URI is given, get it from the write source media item.
  if (!writeSrcURI) {
    // Get the origin item for the write destination item.
    nsCOMPtr<sbIMediaItem> writeSrcItem;
    rv = sbLibraryUtils::GetOriginItem(aWriteDstItem,
                                       getter_AddRefs(writeSrcItem));
    if (NS_FAILED(rv)) {
      // If there is not an existing origin for the write item, use the URI
      // of |aWriteDstItem|.
      rv = aWriteDstItem->GetContentSrc(getter_AddRefs(writeSrcURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // Get the write source URI.
      rv = writeSrcItem->GetContentSrc(getter_AddRefs(writeSrcURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Convert our nsIURI to an nsIFileURL
  nsCOMPtr<nsIFileURL> writeSrcFileURL = do_QueryInterface(writeSrcURI, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now get the nsIFile
  nsCOMPtr<nsIFile> writeSrcFile;
  rv = writeSrcFileURL->GetFile(getter_AddRefs(writeSrcFile));
  NS_ENSURE_SUCCESS(rv, rv);

  // Now check to make sure the source file actually exists
  PRBool fileExists = PR_FALSE;
  rv = writeSrcFile->Exists(&fileExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!fileExists) {
    // Create the device error event and dispatch it
    nsCOMPtr<nsIVariant> var = sbNewVariant(aWriteDstItem).get();
    CreateAndDispatchEvent(sbIDeviceEvent::EVENT_DEVICE_FILE_MISSING,
                           var, PR_TRUE);

    // Remove item from library
    nsCOMPtr<sbILibrary> destLibrary;
    rv = aWriteDstItem->GetLibrary(getter_AddRefs(destLibrary));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = DeleteItem(destLibrary, aWriteDstItem);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_ERROR_NOT_AVAILABLE;
  }

  // Check if the item needs to be organized
  nsCOMPtr<sbILibrary> destLibrary;
  rv = aWriteDstItem->GetLibrary(getter_AddRefs(destLibrary));
  NS_ENSURE_SUCCESS(rv, rv);
  nsString destLibGuidStr;
  rv = destLibrary->GetGuid(destLibGuidStr);
  NS_ENSURE_SUCCESS(rv, rv);
  nsID destLibGuid;
  PRBool success =
    destLibGuid.Parse(NS_LossyConvertUTF16toASCII(destLibGuidStr).get());
  OrganizeData* organizeData = nsnull;
  if (success) {
    success = mOrganizeLibraryPrefs.Get(destLibGuid, &organizeData);
  }

  nsCOMPtr<nsIFile> contentSrcFile;
  if (success && organizeData->organizeEnabled) {
    nsCOMPtr<nsIFileURL> baseFileUrl =
      do_QueryInterface(aContentSrcBaseURI, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIFile> baseFile;
    rv = baseFileUrl->GetFile(getter_AddRefs(baseFile));
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the managed path
    nsCOMPtr<sbIMediaFileManager> fileMgr =
      do_CreateInstance(SB_MEDIAFILEMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_NAMED_LITERAL_STRING(KEY_MEDIA_FOLDER, "media-folder");
    NS_NAMED_LITERAL_STRING(KEY_FILE_FORMAT, "file-format");
    NS_NAMED_LITERAL_STRING(KEY_DIR_FORMAT, "dir-format");
    nsCOMPtr<nsIWritablePropertyBag2> writableBag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1");
    NS_ENSURE_TRUE(writableBag, NS_ERROR_OUT_OF_MEMORY);
    rv = writableBag->SetPropertyAsInterface(KEY_MEDIA_FOLDER, baseFile);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = writableBag->SetPropertyAsACString(KEY_FILE_FORMAT, organizeData->fileFormat);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = writableBag->SetPropertyAsACString(KEY_DIR_FORMAT, organizeData->dirFormat);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = fileMgr->Init(writableBag);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = fileMgr->GetManagedPath(aWriteDstItem,
                                 sbIMediaFileManager::MANAGE_COPY |
                                   sbIMediaFileManager::MANAGE_MOVE,
                                 getter_AddRefs(contentSrcFile));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> parentDir;
    rv = contentSrcFile->GetParent(getter_AddRefs(parentDir));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = parentDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (rv != NS_ERROR_FILE_ALREADY_EXISTS) {
      NS_ENSURE_SUCCESS(rv, rv);
    }

  } else {
    // Get the write source file name, unescape it, and replace illegal characters.
    nsCOMPtr<nsIURL> writeSrcURL = do_QueryInterface(writeSrcURI, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString cWriteSrcFileName;
    rv = writeSrcURL->GetFileName(cWriteSrcFileName);
    NS_ENSURE_SUCCESS(rv, rv);

    // First, unescape it
    nsCAutoString cUnescapedWriteSrcFileName;
    nsCOMPtr<nsINetUtil> netUtil =
                       do_GetService("@mozilla.org/network/util;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = netUtil->UnescapeString(cWriteSrcFileName,
                                 nsINetUtil::ESCAPE_ALL,
                                 cUnescapedWriteSrcFileName);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoString writeSrcFileName =
                        NS_ConvertUTF8toUTF16(cUnescapedWriteSrcFileName);

    // replace illegal characters
    nsString_ReplaceChar(writeSrcFileName, kIllegalChars, PRUnichar('_'));

    // Get a file object for the content base.
    nsCOMPtr<nsIFileURL>
      contentSrcBaseFileURL = do_QueryInterface(aContentSrcBaseURI, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIFile> contentSrcBaseFile;
    rv = contentSrcBaseFileURL->GetFile(getter_AddRefs(contentSrcBaseFile));
    NS_ENSURE_SUCCESS(rv, rv);

    // Start the content source at the base.
    rv = contentSrcBaseFile->Clone(getter_AddRefs(contentSrcFile));
    NS_ENSURE_SUCCESS(rv, rv);

    // Append file name of the write source file.
    rv = contentSrcFile->Append(writeSrcFileName);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Check if the content source file already exists.
  PRBool exists;
  rv = contentSrcFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a unique file if content source file already exists.
  if (exists) {
    // Get the permissions of the content source parent.
    PRUint32          permissions;
    nsCOMPtr<nsIFile> parent;
    rv = contentSrcFile->GetParent(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = parent->GetPermissions(&permissions);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create a unique file.
    rv = contentSrcFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, permissions);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get the device content source URI.
  nsCOMPtr<nsIURI> contentSrc;
  rv = sbLibraryUtils::GetFileContentURI(contentSrcFile,
                                         getter_AddRefs(contentSrc));
  NS_ENSURE_SUCCESS(rv, rv);

  contentSrc.forget(aContentSrc);
  return NS_OK;
}

nsresult sbBaseDevice::SetDeviceWriteContentSrc
                         (sbIMediaItem* aWriteDstItem,
                          nsIURI*       aContentSrcBaseURI,
                          nsIURI*       aWriteSrcURI)
{
  TRACE(("%s", __FUNCTION__));
  nsCOMPtr<nsIURI> contentSrc;
  nsresult rv = GetDeviceWriteContentSrc(aWriteDstItem,
                                         aContentSrcBaseURI,
                                         aWriteSrcURI,
                                         getter_AddRefs(contentSrc));
  // Set the write destination item content source.
  rv = aWriteDstItem->SetContentSrc(contentSrc);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult sbBaseDevice::SetupDevice()
{
  TRACE(("%s", __FUNCTION__));
  nsresult rv;

  // Present the setup device dialog.
  nsCOMPtr<sbIPrompter>
    prompter = do_CreateInstance(SONGBIRD_PROMPTER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMWindow> dialogWindow;
  rv = prompter->OpenDialog
         (nsnull,
          NS_LITERAL_STRING
            ("chrome://songbird/content/xul/device/deviceSetupDialog.xul"),
          NS_LITERAL_STRING("DeviceSetup"),
          NS_LITERAL_STRING("chrome,centerscreen,modal=yes,titlebar=no"),
          NS_ISUPPORTS_CAST(sbIDevice*, this),
          getter_AddRefs(dialogWindow));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::ProcessCapabilitiesRegistrars()
{
  TRACE(("%s", __FUNCTION__));
  // If we haven't built the registrars then do so
  if (mCapabilitiesRegistrarType != sbIDeviceCapabilitiesRegistrar::NONE) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMgr =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = catMgr->EnumerateCategory(SB_DEVICE_CAPABILITIES_REGISTRAR_CATEGORY,
                                 getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv, rv);

  // Enumerate the registrars and find the highest scoring one (Greatest type)
  PRBool hasMore;
  rv = enumerator->HasMoreElements(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);

  while(hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = enumerator->GetNext(getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupportsCString> data = do_QueryInterface(supports, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString entryName;
    rv = data->GetData(entryName);
    NS_ENSURE_SUCCESS(rv, rv);

    char * contractId;
    rv = catMgr->GetCategoryEntry(SB_DEVICE_CAPABILITIES_REGISTRAR_CATEGORY,
                                  entryName.get(),
                                  &contractId);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<sbIDeviceCapabilitiesRegistrar> capabilitiesRegistrar =
      do_CreateInstance(contractId, &rv);
    NS_Free(contractId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool interested;
    rv = capabilitiesRegistrar->InterestedInDevice(this, &interested);
    NS_ENSURE_SUCCESS(rv, rv);
    if (interested) {
      PRUint32 type;
      rv = capabilitiesRegistrar->GetType(&type);
      NS_ENSURE_SUCCESS(rv, rv);
      if (type >= mCapabilitiesRegistrarType) {
        mCapabilitiesRegistrar = capabilitiesRegistrar;
        mCapabilitiesRegistrarType = type;
      }
    }

    rv = enumerator->HasMoreElements(&hasMore);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

/**
 * The capabilities cannot be cached because the capabilities are not preserved
 * on all devices.
 */
nsresult
sbBaseDevice::RegisterDeviceCapabilities(sbIDeviceCapabilities * aCapabilities)
{
  TRACE(("%s", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aCapabilities);

  nsresult rv;

  rv = ProcessCapabilitiesRegistrars();
  NS_ENSURE_SUCCESS(rv, rv);

  if (mCapabilitiesRegistrar) {
    rv = mCapabilitiesRegistrar->AddCapabilities(this, aCapabilities);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
sbBaseDevice::FindTranscodeProfile(sbIMediaItem * aMediaItem,
                                   sbITranscodeProfile ** aProfile)
{
  TRACE(("%s", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aMediaItem);
  NS_ENSURE_ARG_POINTER(aProfile);

  nsresult rv;

  nsAutoString isDRMProtected;
  rv = aMediaItem->GetProperty(NS_LITERAL_STRING(SB_PROPERTY_ISDRMPROTECTED),
                               isDRMProtected);
  if (NS_SUCCEEDED(rv) && isDRMProtected.EqualsLiteral("1")) {
    // we can't have any transcoding profiles that support DRM
    nsCOMPtr<nsIWritablePropertyBag2> bag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = bag->SetPropertyAsAString(NS_LITERAL_STRING("message"),
                                   SBLocalizedString("transcode.file.drmprotected"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = bag->SetPropertyAsInterface(NS_LITERAL_STRING("item"),
                                     aMediaItem);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CreateAndDispatchEvent(sbIDeviceEvent::EVENT_DEVICE_TRANSCODE_ERROR,
                                sbNewVariant(NS_GET_IID(nsIPropertyBag2), bag));
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // TODO: In the future, GetFormatTypeForItem should return a big complex
  // object describing everything we need about an object, rather than this
  // local magic lookup table entry.
  sbExtensionToContentFormatEntry_t formatType;
  PRUint32 bitRate = 0;
  PRUint32 sampleRate = 0;
  rv = sbDeviceUtils::GetFormatTypeForItem(aMediaItem,
                                           formatType,
                                           bitRate,
                                           sampleRate);
  // Check for expected error, unable to find format type
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return rv;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  bool needsTranscoding = false;
  rv = sbDeviceUtils::DoesItemNeedTranscoding(formatType,
                                              bitRate,
                                              sampleRate,
                                              this,
                                              needsTranscoding);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!needsTranscoding) {
    *aProfile = nsnull;
    return NS_OK;
  }

  rv = SelectTranscodeProfile(formatType.Type, aProfile);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::GetSupportedTranscodeProfiles(nsIArray **aSupportedProfiles)
{
  nsresult rv;
  if (!mTranscodeProfiles) {
    rv = sbDeviceUtils::GetSupportedTranscodeProfiles(
                          this,
                          getter_AddRefs(mTranscodeProfiles));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aSupportedProfiles = mTranscodeProfiles);

  return NS_OK;
}

nsresult
sbBaseDevice::SelectTranscodeProfile(PRUint32 transcodeType,
                                     sbITranscodeProfile **aProfile)
{
  nsresult rv;

  PRBool hasProfilePref = PR_FALSE;
  // See if we have a preference for the transcoding profile.
  nsCOMPtr<nsIVariant> profileIdVariant;
  nsString prefProfileId;
  rv = GetPreference(NS_LITERAL_STRING("transcode_profile.profile_id"),
                     getter_AddRefs(profileIdVariant));
  if (NS_SUCCEEDED(rv))
  {
    hasProfilePref = PR_TRUE;
    rv = profileIdVariant->GetAsAString(prefProfileId);
    NS_ENSURE_SUCCESS(rv, rv);
    TRACE(("%s: found a profile", __FUNCTION__));
  }

  nsCOMPtr<nsIArray> supportedProfiles;
  rv = GetSupportedTranscodeProfiles(getter_AddRefs(supportedProfiles));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 bestPriority = 0;
  nsCOMPtr<sbITranscodeProfile> bestProfile;
  nsCOMPtr<sbITranscodeProfile> prefProfile;

  PRUint32 length;
  rv = supportedProfiles->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 index = 0; index < length; ++index) {
    nsCOMPtr<sbITranscodeProfile> profile =
        do_QueryElementAt(supportedProfiles, index, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (profile) {
      PRUint32 profileContentType;
      rv = profile->GetType(&profileContentType);
      NS_ENSURE_SUCCESS(rv, rv);

      // If the content types don't match, skip (we don't want to use a video
      // transcoding profile to transcode audio, for example)
      if (profileContentType == transcodeType) {
        if (hasProfilePref) {
          nsString profileId;
          rv = profile->GetId(profileId);
          NS_ENSURE_SUCCESS(rv, rv);

          if (profileId.Equals(prefProfileId))
            prefProfile = profile;
        }

        // Also track the highest-priority profile. This is our default.
        PRUint32 priority;
        rv = profile->GetPriority(&priority);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!bestProfile || priority > bestPriority) {
          bestProfile = profile;
          bestPriority = priority;
        }
      }
      else {
        TRACE(("%s: skipping profile for content type %d",
                __FUNCTION__,
                profileContentType));
      }
    }
  }
  if (prefProfile) {
    // We found the profile selected in the preferences. Apply relevant
    // preferenced properties to it as well...
    nsCOMPtr<nsIArray> audioProperties;
    rv = prefProfile->GetAudioProperties(getter_AddRefs(audioProperties));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = sbDeviceUtils::ApplyPropertyPreferencesToProfile(
            this,
            audioProperties,
            NS_LITERAL_STRING("transcode_profile.audio_properties"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIArray> videoProperties;
    rv = prefProfile->GetVideoProperties(getter_AddRefs(videoProperties));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = sbDeviceUtils::ApplyPropertyPreferencesToProfile(
            this,
            videoProperties,
            NS_LITERAL_STRING("transcode_profile.video_properties"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIArray> containerProperties;
    rv = prefProfile->GetContainerProperties(
            getter_AddRefs(containerProperties));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = sbDeviceUtils::ApplyPropertyPreferencesToProfile(
            this,
            containerProperties,
            NS_LITERAL_STRING("transcode_profile.container_properties"));
    NS_ENSURE_SUCCESS(rv, rv);

    prefProfile.forget(aProfile);
    TRACE(("%s: found pref profile", __FUNCTION__));
    return NS_OK;
  }
  else if (bestProfile) {
    TRACE(("%s: using best-match profile", __FUNCTION__));
    bestProfile.forget(aProfile);
    return NS_OK;
  }

  // Indicate no appropriate transcoding profile available
  TRACE(("%s: no supported profiles available", __FUNCTION__));
  return NS_ERROR_NOT_AVAILABLE;
}

/**
 * Process a batch in preparation for transcoding, figuring out which items
 * need transcoding.
 */

nsresult
sbBaseDevice::PrepareBatchForTranscoding(Batch & aBatch)
{
  TRACE(("%s", __FUNCTION__));
  nsresult rv;

  if (aBatch.empty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIArray> imageFormats;
  rv = GetSupportedAlbumArtFormats(getter_AddRefs(imageFormats));
  // No album art formats isn't fatal.
  if (rv != NS_ERROR_NOT_AVAILABLE) {
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Iterate over the batch getting the transcode profiles if needed.
  Batch::iterator end = aBatch.end();
  Batch::iterator iter = aBatch.begin();
  while (iter != end) {
    // Check for abort.
    if (IsRequestAbortedOrDeviceDisconnected()) {
      return NS_ERROR_ABORT;
    }

    TransferRequest * const request = *iter;
    rv = FindTranscodeProfile(request->item,
                              &request->transcodeProfile);
    // Treat no profiles available as not needing transcoding
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      TRACE(("%s: no transcode profile available", __FUNCTION__));
    } else {
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (request->transcodeProfile) {
      TRACE(("%s: transcoding needed", __FUNCTION__));
      request->needsTranscoding = PR_TRUE;
    }

    request->albumArt = do_CreateInstance(
            SONGBIRD_TRANSCODEALBUMART_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // It's ok for this to fail; album art is optional
    rv = request->albumArt->Init(request->item, imageFormats);
    if (NS_FAILED(rv)) {
      TRACE(("%s: no album art available", __FUNCTION__));
      request->albumArt = nsnull;
    }

    ++iter;
  }

  return NS_OK;
}

static nsresult
AddAlbumArtFormats(sbIDeviceCapabilities *aCapabilities,
                   nsIMutableArray *aArray,
                   PRUint32 numFormats,
                   char **formats)
{
  nsresult rv;

  for (PRUint32 i = 0; i < numFormats; i++) {
    nsCOMPtr<nsISupports> formatType;
    rv = aCapabilities->GetFormatType(NS_ConvertASCIItoUTF16(formats[i]),
            getter_AddRefs(formatType));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<sbIImageFormatType> constraints =
        do_QueryInterface(formatType, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aArray->AppendElement(constraints, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
sbBaseDevice::GetSupportedAlbumArtFormats(nsIArray * *aFormats)
{
  TRACE(("%s", __FUNCTION__));
  nsresult rv;
  nsCOMPtr<nsIMutableArray> formatConstraints =
      do_CreateInstance(SB_THREADSAFE_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIDeviceCapabilities> capabilities;
  rv = GetCapabilities(getter_AddRefs(capabilities));
  NS_ENSURE_SUCCESS(rv, rv);

  char **formats;
  PRUint32 numFormats;
  rv = capabilities->GetSupportedFormats(sbIDeviceCapabilities::CONTENT_IMAGE,
          &numFormats, &formats);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AddAlbumArtFormats(capabilities, formatConstraints, numFormats, formats);
  /* Ensure everything is freed here before potentially returning; no
     magic destructors for this thing */
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(numFormats, formats);
  NS_ENSURE_SUCCESS (rv, rv);

  NS_ADDREF (*aFormats = formatConstraints);
  return NS_OK;
}

nsresult
sbBaseDevice::GetShouldLimitMusicSpace(const nsAString & aPrefBase,
                                       PRBool *aOutShouldLimitSpace)
{
  TRACE(("%s", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aOutShouldLimitSpace);
  *aOutShouldLimitSpace = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsIVariant> shouldEnableVar;
  rv = GetLibraryPreference(aPrefBase,
                            NS_LITERAL_STRING("use_music_limit_percent"),
                            getter_AddRefs(shouldEnableVar));
  NS_ENSURE_SUCCESS(rv, rv);

  return shouldEnableVar->GetAsBool(aOutShouldLimitSpace);
}

nsresult
sbBaseDevice::GetMusicLimitSpacePercent(const nsAString & aPrefBase,
                                        PRUint32 *aOutLimitPercentage)
{
  TRACE(("%s", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aOutLimitPercentage);
  *aOutLimitPercentage = 100;  // always default to 100

  nsresult rv;
  nsCOMPtr<nsIVariant> prefValue;
  rv = GetLibraryPreference(aPrefBase,
                            NS_LITERAL_STRING("music_limit_percent"),
                            getter_AddRefs(prefValue));
  NS_ENSURE_SUCCESS(rv, rv);

  return prefValue->GetAsUint32(aOutLimitPercentage);
}

/* void Format(); */
NS_IMETHODIMP sbBaseDevice::Format()
{
  TRACE(("%s", __FUNCTION__));
  return NS_ERROR_NOT_IMPLEMENTED;
}
/* readonly attribute boolean supportsReformat; */
NS_IMETHODIMP sbBaseDevice::GetSupportsReformat(PRBool *_retval)
{
  TRACE(("%s", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  return NS_OK;
}

static nsresult
GetPropertyBag(sbIDevice * aDevice, nsIPropertyBag2 ** aProperties)
{
  TRACE(("%s", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aDevice);
  NS_ENSURE_ARG_POINTER(aProperties);

  nsCOMPtr<sbIDeviceProperties> deviceProperties;
  nsresult rv = aDevice->GetProperties(getter_AddRefs(deviceProperties));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = deviceProperties->GetProperties(aProperties);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbBaseDevice::GetNameBase(nsAString& aName)
{
  TRACE(("%s", __FUNCTION__));
  PRBool   hasKey;
  nsresult rv;

  nsCOMPtr<nsIPropertyBag2> properties;
  rv = GetPropertyBag(this, getter_AddRefs(properties));
  NS_ENSURE_SUCCESS(rv, rv);

  // Try using the friendly name property and exit if successful.
  rv = properties->HasKey(NS_LITERAL_STRING(SB_DEVICE_PROPERTY_NAME), &hasKey);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasKey) {
    rv = properties->GetPropertyAsAString
                        (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_NAME), aName);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  // Use the product name.
  return GetProductName(aName);
}

nsresult
sbBaseDevice::GetProductNameBase(char const * aDefaultModelNumberString,
                                 nsAString& aProductName)
{
  TRACE(("%s [%s]", __FUNCTION__, aDefaultModelNumberString));
  NS_ENSURE_ARG_POINTER(aDefaultModelNumberString);

  nsAutoString productName;
  PRBool       hasKey;
  nsresult     rv;

  nsCOMPtr<nsIPropertyBag2> properties;
  rv = GetPropertyBag(this, getter_AddRefs(properties));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the vendor name.
  nsAutoString vendorName;
  rv = properties->HasKey(NS_LITERAL_STRING(SB_DEVICE_PROPERTY_MANUFACTURER),
                          &hasKey);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasKey) {
    // Get the vendor name.
    rv = properties->GetPropertyAsAString
                        (NS_LITERAL_STRING(SB_DEVICE_PROPERTY_MANUFACTURER),
                         vendorName);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get the device model number, using a default if one is not available.
  nsAutoString modelNumber;
  rv = properties->HasKey(NS_LITERAL_STRING(SB_DEVICE_PROPERTY_MODEL),
                          &hasKey);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasKey) {
    // Get the model number.
    rv = properties->GetPropertyAsAString(
                              NS_LITERAL_STRING(SB_DEVICE_PROPERTY_MODEL),
                              modelNumber);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (modelNumber.IsEmpty()) {
    // Get the default model number.
    modelNumber = SBLocalizedString(aDefaultModelNumberString);
  }

  // Produce the product name.
  if (!vendorName.IsEmpty()) {
    nsTArray<nsString> params;
    NS_ENSURE_TRUE(params.AppendElement(vendorName), NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_TRUE(params.AppendElement(modelNumber), NS_ERROR_OUT_OF_MEMORY);
    productName.Assign(SBLocalizedString("device.product.name", params));
  } else {
    productName.Assign(modelNumber);
  }

  // Return results.
  aProductName.Assign(productName);

  return NS_OK;
}

//------------------------------------------------------------------------------
//
// CD device request added event nsISupports services.
//
//------------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS1(sbDeviceReqAddedEvent, nsIRunnable)


//------------------------------------------------------------------------------
//
// MSC device request added event nsIRunnable services.
//
//------------------------------------------------------------------------------

/**
 * Run the event.
 */

NS_IMETHODIMP
sbDeviceReqAddedEvent::Run()
{
  // Dispatch to the device object to handle the event.
  mDevice->ReqHandleRequestAdded();

  return NS_OK;
}


/**
 * Create a new sbMSCReqAddedEvent object for the device specified by aDevice
 * and return it in aEvent.
 *
 * \param aDevice               Device for which to create event.
 * \param aEvent                Created event.
 */

/* static */ nsresult
sbDeviceReqAddedEvent::New(sbBaseDevice* aDevice,
                           nsIRunnable**    aEvent)
{
  NS_ENSURE_ARG_POINTER(aDevice);
  NS_ENSURE_ARG_POINTER(aEvent);

  // Create the event object.
  sbDeviceReqAddedEvent* event;
  NS_NEWXPCOM(event, sbDeviceReqAddedEvent);
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  // Set the event parameters.
  event->mDevice = aDevice;
  NS_ADDREF(NS_ISUPPORTS_CAST(sbIDevice*, aDevice));

  // Return results.
  NS_ADDREF(*aEvent = event);

  return NS_OK;
}
