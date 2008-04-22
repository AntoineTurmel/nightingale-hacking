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

#ifndef __SBLIBRARYUTILS_H__
#define __SBLIBRARYUTILS_H__

#include <nsCOMPtr.h>
#include <pratom.h>
#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>

#include <sbILibrary.h>
#include <sbILibraryManager.h>
#include <nsStringAPI.h>

class sbIMediaItem;
class sbILibrary;

class sbLibraryBatchHelper
{
public:
  sbLibraryBatchHelper() :
    mDepth(0)
  {
    MOZ_COUNT_CTOR(sbLibraryBatchHelper);
  }

  ~sbLibraryBatchHelper()
  {
    MOZ_COUNT_DTOR(sbLibraryBatchHelper);
  }

  void Begin()
  {
    NS_ASSERTION(mDepth >= 0, "Illegal batch depth, mismatched calls!");
    PR_AtomicIncrement(&mDepth);
  }

  void End()
  {
    PRInt32 depth = PR_AtomicDecrement(&mDepth);
    NS_ASSERTION(depth >= 0, "Illegal batch depth, mismatched calls!");
  }

  PRUint32 Depth()
  {
    return (PRUint32) mDepth;
  }

  PRBool IsActive()
  {
    return mDepth > 0;
  }

private:
  PRInt32 mDepth;
};

class sbLibraryUtils
{
public:
  /**
   * Given an item and a library, attempt to locate a matching item in the
   * library.  Returns null (and success) if not found.
   *
   * \param aItem    The item to find; not necessarily owned by aLibrary
   * \param aLibrary The library to look in
   * \return         The found item, or null
   */
  static nsresult GetItemInLibrary(/* in */  sbIMediaItem * aItem,
                                   /* in */  sbILibrary   * aLibrary,
                                   /* out */ sbIMediaItem **_retval);
  
  /**
   * Attempt to get the content length of the media item
   *
   * \param aItem    The item to look up
   * \retur          The content length of the item (or throw an exception)
   */
  static nsresult GetContentLength(/* in */  sbIMediaItem * aItem,
                                   /* out */ PRInt64      * _retval);
};

/**
 * Retrieves the main library
 */
inline
nsresult GetMainLibrary(sbILibrary ** aMainLibrary) {
  nsresult rv;
  nsCOMPtr<sbILibraryManager> libManager =
      do_GetService("@songbirdnest.com/Songbird/library/Manager;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<sbILibrary> mainLibrary;
  return libManager->GetMainLibrary(aMainLibrary);
}

/**
 * Retrieves the ID of the main library
 */
inline
nsresult GetMainLibraryId(nsAString & aLibraryId) {

  nsCOMPtr<sbILibrary> mainLibrary;
  nsresult rv = GetMainLibrary(getter_AddRefs(mainLibrary));
  NS_ENSURE_SUCCESS(rv, rv);
  
  return mainLibrary->GetGuid(aLibraryId);
}

#endif // __SBLIBRARYUTILS_H__

