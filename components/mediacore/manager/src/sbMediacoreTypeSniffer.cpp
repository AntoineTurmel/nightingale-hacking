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

#include "sbMediacoreTypeSniffer.h"

#include <nsIURI.h>
#include <nsIURL.h>

#include <nsAutoLock.h>
#include <nsArrayUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsMemory.h>
#include <nsServiceManagerUtils.h>
#include <nsStringGlue.h>
#include <nsTArray.h>

#include <sbIMediacoreCapabilities.h>
#include <sbIMediacoreFactory.h>
#include <sbIPlaylistReader.h>

#include <sbTArrayStringEnumerator.h>

const char *gBannedWebExtensions[] = {"htm", "html", "php", "php3"};
const PRUint16 gBannedWebExtensionsSize = 
  sizeof(gBannedWebExtensions) / sizeof(gBannedWebExtensions[0]);

NS_IMPL_THREADSAFE_ISUPPORTS1(sbMediacoreTypeSniffer, 
                              sbIMediacoreTypeSniffer)

sbMediacoreTypeSniffer::sbMediacoreTypeSniffer()
: mMonitor(nsnull)
{
}

sbMediacoreTypeSniffer::~sbMediacoreTypeSniffer()
{
  if(mMonitor) {
    nsAutoMonitor::DestroyMonitor(mMonitor);
  }
}

nsresult
sbMediacoreTypeSniffer::Init() 
{
  mMonitor = nsAutoMonitor::NewMonitor("sbMediacoreTypeSniffer::mMonitor");
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_ERROR_UNEXPECTED;

  nsCOMPtr<sbIMediacoreFactoryRegistrar> factoryRegistrar = 
    do_GetService(SB_MEDIACOREMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  factoryRegistrar.swap(mFactoryRegistrar);

  PRBool success = mAudioExtensions.Init();
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mVideoExtensions.Init();
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mImageExtensions.Init();
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mPlaylistExtensions.Init();
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mBannedWebExtensions.Init();
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mAllExtensions.Init();
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIArray> factories;
  rv = mFactoryRegistrar->GetFactories(getter_AddRefs(factories));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 length = 0;
  rv = factories->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for(PRUint32 current = 0; current < length; ++current) {
    nsCOMPtr<sbIMediacoreFactory> factory = do_QueryElementAt(factories,
                                                              current,
                                                              &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<sbIMediacoreCapabilities> caps;
    rv = factory->GetCapabilities(getter_AddRefs(caps));
    NS_ENSURE_SUCCESS(rv, rv);

    // Audio extensions
    nsCOMPtr<nsIStringEnumerator> extensions;
    rv = caps->GetAudioExtensions(getter_AddRefs(extensions));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore = PR_FALSE;
    while(NS_SUCCEEDED(extensions->HasMore(&hasMore)) &&
          hasMore) {
      nsString extension;
      
      rv = extensions->GetNext(extension);
      NS_ENSURE_SUCCESS(rv, rv);

      NS_ConvertUTF16toUTF8 theExtension(extension);
      nsCStringHashKey *hashKey = mAudioExtensions.PutEntry(theExtension);
      NS_ENSURE_TRUE(hashKey, NS_ERROR_OUT_OF_MEMORY);

      hashKey = mAllExtensions.PutEntry(theExtension);
      NS_ENSURE_TRUE(hashKey, NS_ERROR_OUT_OF_MEMORY);
    }

    // Video extensions
    rv = caps->GetVideoExtensions(getter_AddRefs(extensions));
    NS_ENSURE_SUCCESS(rv, rv);

    hasMore = PR_FALSE;
    while(NS_SUCCEEDED(extensions->HasMore(&hasMore)) &&
          hasMore) {
        nsString extension;

        rv = extensions->GetNext(extension);
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ConvertUTF16toUTF8 theExtension(extension);
        nsCStringHashKey *hashKey = mVideoExtensions.PutEntry(theExtension);
        NS_ENSURE_TRUE(hashKey, NS_ERROR_OUT_OF_MEMORY);

        hashKey = mAllExtensions.PutEntry(theExtension);
        NS_ENSURE_TRUE(hashKey, NS_ERROR_OUT_OF_MEMORY);
    }

    // Image extensions
    rv = caps->GetImageExtensions(getter_AddRefs(extensions));
    NS_ENSURE_SUCCESS(rv, rv);

    hasMore = PR_FALSE;
    while(NS_SUCCEEDED(extensions->HasMore(&hasMore)) &&
          hasMore) {
        nsString extension;

        rv = extensions->GetNext(extension);
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ConvertUTF16toUTF8 theExtension(extension);
        nsCStringHashKey *hashKey = mImageExtensions.PutEntry(theExtension);
        NS_ENSURE_TRUE(hashKey, NS_ERROR_OUT_OF_MEMORY);

        hashKey = mAllExtensions.PutEntry(theExtension);
        NS_ENSURE_TRUE(hashKey, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  // Get the playlist extensions from the playlist reader manager
  nsCOMPtr<sbIPlaylistReaderManager> readerManager = 
    do_GetService("@songbirdnest.com/Songbird/PlaylistReaderManager;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  length = 0;
  PRUnichar **extensions = nsnull;

  rv = readerManager->SupportedFileExtensions(&length, &extensions);
  NS_ENSURE_SUCCESS(rv, rv);

  for(PRUint32 current = 0; current < length; ++current) {
    NS_ConvertUTF16toUTF8 theExtension(extensions[current]);

    mPlaylistExtensions.PutEntry(theExtension);
    mAllExtensions.PutEntry(theExtension);
  }

  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(length, extensions);
  NS_ENSURE_TRUE(length == mPlaylistExtensions.Count(), NS_ERROR_UNEXPECTED);

  for(PRUint16 current = 0; current < gBannedWebExtensionsSize; ++current) {
    mBannedWebExtensions.PutEntry(nsDependentCString(gBannedWebExtensions[current]));
  }

  return NS_OK;
}


nsresult
sbMediacoreTypeSniffer::GetFileExtensionFromURI(nsIURI* aURI,
                                                nsACString& _retval)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsCAutoString strExtension;

  nsresult rv;
  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI, &rv);
  if (NS_SUCCEEDED(rv)) {
    // Try the easy way...
    rv = url->GetFileExtension(strExtension);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Try the hard way...
    nsCAutoString spec;
    rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 index = spec.RFindChar('.');
    if (index >= 0) {
      strExtension.Assign(StringTail(spec, spec.Length() -1 - index));
    }
  }

  if (strExtension.IsEmpty()) {
    _retval.Truncate();
    return NS_OK;
  }

  // Strip '.' from the beginning and end, if it exists
  strExtension.Trim(".");

  _retval.Assign(strExtension);
  return NS_OK;
}

NS_IMETHODIMP 
sbMediacoreTypeSniffer::IsValidMediaURL(nsIURI *aURL, 
                                        PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aURL);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = PR_TRUE;

  nsCString fileExtension;
  nsresult rv = GetFileExtensionFromURI(aURL, fileExtension);
  NS_ENSURE_SUCCESS(rv, rv);

  // The quick and easy way. Verify against file extension if available.
  if(!fileExtension.IsEmpty()) {

    nsAutoMonitor mon(mMonitor);

    if(mAudioExtensions.GetEntry(fileExtension)) {
      return NS_OK;
    }

    if(mVideoExtensions.GetEntry(fileExtension)) {
      return NS_OK;
    }

    if(mImageExtensions.GetEntry(fileExtension)) {
      return NS_OK;
    }

    // Exhausted all possibilities.
    *_retval = PR_FALSE;

    return NS_OK;
  }

  // XXXAus: For the time being, if there's no extension we'll pretend like it 
  //         is not media.
  *_retval = PR_FALSE;

  // XXXAus: Below are comments for implementing further functionality.
  // Looks like we'll have to crack open the file or connect to the server
  // to have a look at the content type.

  return NS_OK;
}

NS_IMETHODIMP 
sbMediacoreTypeSniffer::IsValidVideoURL(nsIURI *aURL, 
                                        PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aURL);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = PR_FALSE;

  nsCString fileExtension;
  nsresult rv = GetFileExtensionFromURI(aURL, fileExtension);
  NS_ENSURE_SUCCESS(rv, rv);

  // The quick and easy way. Verify against file extension if available.
  if(!fileExtension.IsEmpty()) {
    
    nsAutoMonitor mon(mMonitor);

    if(mVideoExtensions.GetEntry(fileExtension)) {
      return NS_OK;
    }

    *_retval = PR_FALSE;

    return NS_OK;
  }

  // XXXAus: For the time being, if there's no extension we'll pretend like there
  //         is no video.
  *_retval = PR_FALSE;

  // XXXAus: Below are comments for implementing further functionality.
  // Looks like we'll have to crack open the file or connect to the server
  // to have a look at the content type.

  return NS_OK;
}

NS_IMETHODIMP 
sbMediacoreTypeSniffer::IsValidPlaylistURL(nsIURI *aURL, 
                                           PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aURL);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = PR_TRUE;

  nsCString fileExtension;
  nsresult rv = GetFileExtensionFromURI(aURL, fileExtension);
  NS_ENSURE_SUCCESS(rv, rv);

  // The quick and easy way. Verify against file extension if available.
  if(!fileExtension.IsEmpty()) {
    
    nsAutoMonitor mon(mMonitor);

    if(mPlaylistExtensions.GetEntry(fileExtension)) {
      return NS_OK;
    }

    *_retval = PR_FALSE;

    return NS_OK;
  }

  // XXXAus: For the time being, if there's no extension we'll pretend like this
  //         is _not_ a playlist.
  *_retval = PR_FALSE;

  // XXXAus: Below are comments for implementing further functionality.
  // Looks like we'll have to crack open the file or connect to the server
  // to have a look at the content type.

  return NS_OK;
}

NS_IMETHODIMP
sbMediacoreTypeSniffer::IsValidWebSafePlaylistURL(nsIURI *aURL,
                                                  PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aURL);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = PR_FALSE;

  nsCString fileExtension;
  nsresult rv = GetFileExtensionFromURI(aURL, fileExtension);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoMonitor mon(mMonitor);

  if(!fileExtension.IsEmpty() &&
     !mBannedWebExtensions.GetEntry(fileExtension)) {
    
    if(mPlaylistExtensions.GetEntry(fileExtension)) {
      *_retval = PR_TRUE;
    }
  }

  return NS_OK;
}

template<class EntryType>
PLDHashOperator PR_CALLBACK EnumerateAllExtensions(EntryType* aEntry, 
                                                   void *aUserArg)
{
  NS_ENSURE_TRUE(aEntry, PL_DHASH_STOP);
  NS_ENSURE_TRUE(aUserArg, PL_DHASH_STOP);

  nsTArray<nsString> *aArray = reinterpret_cast<nsTArray<nsString> *>(aUserArg);
  

  nsString *elem = 
    aArray->AppendElement(NS_ConvertUTF8toUTF16(aEntry->GetKey()));
  NS_ENSURE_TRUE(elem, PL_DHASH_STOP);

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP 
sbMediacoreTypeSniffer::GetAudioFileExtensions(nsIStringEnumerator **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsTArray<nsString> allExtensions;

  nsAutoMonitor mon(mMonitor);
  PRUint32 count = mAudioExtensions.EnumerateEntries(EnumerateAllExtensions<nsCStringHashKey>, 
                                                     &allExtensions);
  NS_ENSURE_TRUE(count == mAudioExtensions.Count(), NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIStringEnumerator> allExtensionsEnum = 
    new sbTArrayStringEnumerator(&allExtensions);
  NS_ENSURE_TRUE(allExtensionsEnum, NS_ERROR_OUT_OF_MEMORY);

  allExtensionsEnum.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP 
sbMediacoreTypeSniffer::GetVideoFileExtensions(nsIStringEnumerator **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsTArray<nsString> allExtensions;

  PRUint32 count = mVideoExtensions.EnumerateEntries(EnumerateAllExtensions<nsCStringHashKey>, 
                                                     &allExtensions);
  NS_ENSURE_TRUE(count == mVideoExtensions.Count(), NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIStringEnumerator> allExtensionsEnum = 
    new sbTArrayStringEnumerator(&allExtensions);
  NS_ENSURE_TRUE(allExtensionsEnum, NS_ERROR_OUT_OF_MEMORY);

  allExtensionsEnum.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP 
sbMediacoreTypeSniffer::GetPlaylistFileExtensions(nsIStringEnumerator **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsTArray<nsString> allExtensions;

  nsAutoMonitor mon(mMonitor);
  PRUint32 count = mPlaylistExtensions.EnumerateEntries(EnumerateAllExtensions<nsCStringHashKey>, 
                                                        &allExtensions);
  NS_ENSURE_TRUE(count == mPlaylistExtensions.Count(), NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIStringEnumerator> allExtensionsEnum = 
    new sbTArrayStringEnumerator(&allExtensions);
  NS_ENSURE_TRUE(allExtensionsEnum, NS_ERROR_OUT_OF_MEMORY);

  allExtensionsEnum.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP 
sbMediacoreTypeSniffer::GetMediaFileExtensions(nsIStringEnumerator **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsTArray<nsString> allExtensions;
  
  nsAutoMonitor mon(mMonitor);
  PRUint32 count = mAudioExtensions.EnumerateEntries(EnumerateAllExtensions<nsCStringHashKey>, 
                                                     &allExtensions);
  NS_ENSURE_TRUE(count == mAudioExtensions.Count(), NS_ERROR_UNEXPECTED);

  count = mVideoExtensions.EnumerateEntries(EnumerateAllExtensions<nsCStringHashKey>, 
                                            &allExtensions);
  NS_ENSURE_TRUE(count == mVideoExtensions.Count(), NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIStringEnumerator> allExtensionsEnum = 
    new sbTArrayStringEnumerator(&allExtensions);
  NS_ENSURE_TRUE(allExtensionsEnum, NS_ERROR_OUT_OF_MEMORY);

  allExtensionsEnum.forget(_retval);

  return NS_OK;
}
