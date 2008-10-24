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

#ifndef __SBLOCALDATABASEPROPERTYCACHE_H__
#define __SBLOCALDATABASEPROPERTYCACHE_H__

#include <nsIStringEnumerator.h>
#include <sbILocalDatabasePropertyCache.h>

#include <nsCOMArray.h>
#include <nsCOMPtr.h>
#include <nsIClassInfo.h>
#include <nsDataHashtable.h>
#include <nsInterfaceHashtable.h>
#include <nsIObserver.h>
#include <nsIThread.h>
#include <nsStringGlue.h>
#include <nsTArray.h>
#include <nsTHashtable.h>
#include <nsIRunnable.h> 

#include <sbIJobProgress.h>
#include <sbIMediaListListener.h>
#include <nsITimer.h>

#include "sbLocalDatabaseResourcePropertyBag.h"
#include "sbFixedInterfaceCache.h"
#include "sbLocalDatabaseSQL.h"



struct PRLock;
struct PRMonitor;

class nsIURI;
class sbIDatabaseQuery;
class sbLocalDatabaseLibrary;
class sbIPropertyManager;
class sbISQLBuilderCriterionIn;
class sbISQLInsertBuilder;
class sbISQLSelectBuilder;
class sbISQLUpdateBuilder;
class sbISQLDeleteBuilder;
class sbLocalDatabaseSortInvalidateJob;

class sbLocalDatabasePropertyCache: public sbILocalDatabasePropertyCache,
    public nsIObserver
{
public:
  friend class sbLocalDatabaseResourcePropertyBag;
  /**
   * The size of our property bag cache
   */
  static PRUint32 const CACHE_SIZE = 1024;
  /**
   * The number of bags to read at a time
   */
  static PRUint32 const BATCH_READ_SIZE = 128;

  NS_DECL_ISUPPORTS
  NS_DECL_SBILOCALDATABASEPROPERTYCACHE
  NS_DECL_NSIOBSERVER

  typedef sbFixedInterfaceCache<nsStringHashKey,
                                sbLocalDatabaseResourcePropertyBag> InterfaceCache;

  sbLocalDatabasePropertyCache();

  // This is public so that it can be used with nsAutoPtr and friends.
  ~sbLocalDatabasePropertyCache();

  nsresult Init(sbLocalDatabaseLibrary* aLibrary,
      const nsAString& aLibraryResourceGUID);

  PRBool GetPropertyID(PRUint32 aPropertyDBID, nsAString& aPropertyID);

  void GetColumnForPropertyID(PRUint32 aPropertyID, nsAString &aColumn);

  // Called when mSortInvalidateJob completes
  nsresult InvalidateSortDataComplete();
  
  // Determine the pre-baked secondary sort string for a property
  // in a given bag
  nsresult CreateSecondarySortValue(sbILocalDatabaseResourcePropertyBag* aBag,
                                    PRUint32 aPropertyDBID, 
                                    nsAString& _retval);
private:
  nsresult Shutdown();

  nsresult MakeQuery(const nsAString& aSql, sbIDatabaseQuery** _retval);
  nsresult LoadProperties();

  nsresult AddDirty(const nsAString &aGuid,
      sbLocalDatabaseResourcePropertyBag * aBag);

  PRUint32 GetPropertyDBIDInternal(const nsAString& aPropertyID);

  nsresult InsertPropertyIDInLibrary(const nsAString& aPropertyID,
      PRUint32 *aPropertyDBID);
  
  // Used to persist invalid sorting state in case mSortInvalidateJob 
  // is interrupted.
  nsresult GetSetInvalidSortDataPref(PRBool aWrite, PRBool& aValue);

  typedef nsInterfaceHashtable<nsUint32HashKey, sbLocalDatabaseResourcePropertyBag> IDToBagMap;

  nsresult RetrieveSecondaryProperties(nsAString const & aSQLStatement,
      IDToBagMap const & bags);

  nsresult RetrieveLibraryProperties(nsAString const & aSQLStatement,
      sbLocalDatabaseResourcePropertyBag * aBag);

  /**
   * This retrieves a collection of property bags for the list of guids passed
   * to aGUIDs.
   * \param aGUIDs this is the collection of guids. NOTE: this array may be
   *               modified if it contains a library guid. In that case the
   *               library guid will be moved to the end of the array.
   * \param aBags This is the collection of property bags returned. It is
   *              returned in the same order as aGUIDs as it is returned.
   */
  template <class T>
  nsresult RetrieveProperties(T & aGUIDs,
      nsCOMArray<sbLocalDatabaseResourcePropertyBag> & aBags);

  /**
   * Creates property bags containing the primary properties. This also
   * creates a list of ID's that were retrieved. This is primarily an
   * optimization issue so we don't have to go back through and enumerate
   * the bags or ID to Bag Map to get an array.
   * \param aSQLStatement an SQL statement that will retrieve media item
   *                      primary properties
   * \param aGuids The list of guids of the media items to retrieve the
   *               properties
   * \param aIDToBagMap Will hold the map of item ID's to property bags
   * \param aBags The collection of bags containing the primary properties
   * \param aItemIDs the collection of item ID's for the property bags
   *                 being retrieved
   */
  template <class T>
  nsresult
  RetrievePrimaryProperties(nsAString const & aSQLStatement,
      T const & aGuids,
      IDToBagMap & aIDToBagMap,
      nsCOMArray<sbLocalDatabaseResourcePropertyBag> & aBags,
      nsTArray<PRUint32> & aItemIDs);

  // Pending write count.
  PRUint32 mWritePendingCount;

  // Database GUID
  nsString mDatabaseGUID;

  // Database Location
  nsCOMPtr<nsIURI> mDatabaseLocation;

  // Cache the property name list
  nsDataHashtableMT<nsUint32HashKey, nsString> mPropertyDBIDToID;
  nsDataHashtableMT<nsStringHashKey, PRUint32> mPropertyIDToDBID;

  // Media items fts delete query
  // XXXAus: resource_properties_fts is disabled. See bug 9488 and bug 9617
  //         for more details.
  //nsCOMPtr<sbISQLDeleteBuilder> mMediaItemsFtsDelete;
  //sbISQLBuilderCriterionIn* mMediaItemsFtsDeleteInCriterion;

  // Media items fts insert query
  // XXXAus: resource_properties_fts is disabled. See bug 9488 and bug 9617
  //         for more details.
  //nsCOMPtr<sbISQLInsertBuilder> mMediaItemsFtsInsert;
  //sbISQLBuilderCriterionIn* mMediaItemsFtsInsertInCriterion;

  // Used to protect mCache and mDirty
  PRMonitor* mCacheMonitor;

  // Cache for GUID -> property bag
  InterfaceCache mCache;

  // Dirty GUIDs
  nsInterfaceHashtable<nsStringHashKey, sbLocalDatabaseResourcePropertyBag> mDirty;

  // flushing on a background thread
  struct FlushQueryData {
    nsCOMPtr<sbIDatabaseQuery> query;
    PRUint32 dirtyGuidCount;
  };
  void RunFlushThread();

  static
  nsresult ProcessQueries(nsTArray<FlushQueryData> & aQueries);

  // The GUID of the library resource
  nsString mLibraryResourceGUID;

  PRBool mIsShuttingDown;
  nsCOMPtr<nsIThread> mFlushThread;
  PRMonitor* mFlushThreadMonitor;

  // Backstage pass to our parent library. Can't use an nsRefPtr because the
  // library owns us and that would create a cycle.
  sbLocalDatabaseLibrary* mLibrary;

  nsCOMPtr<sbIPropertyManager> mPropertyManager;

  // When invalidating sort data this keeps the job alive, and
  // is used to cancel if shutdown occurs. 
  nsRefPtr<sbLocalDatabaseSortInvalidateJob> mSortInvalidateJob;

  sbLocalDatabaseSQL mSQLStrings;
};


/**
 * \class sbLocalDatabaseSortInvalidateJob
 * Used by sbLocalDatabasePropertyCache rebuild the database sortable 
 * data for every item in the associated media library. Implements
 * sbIJobProgress so that it is easy to present progress to the user.
 */
class sbLocalDatabaseSortInvalidateJob : public sbIJobProgress,
                                         public nsIClassInfo,
                                         public nsIRunnable,
                                         public sbIMediaListEnumerationListener,
                                         public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_SBIJOBPROGRESS
  NS_DECL_NSIRUNNABLE
  NS_DECL_SBIMEDIALISTENUMERATIONLISTENER
  NS_DECL_NSIOBSERVER

  sbLocalDatabaseSortInvalidateJob();

  virtual ~sbLocalDatabaseSortInvalidateJob();

  nsresult Init(sbLocalDatabasePropertyCache* aPropCache,
                sbLocalDatabaseLibrary* aLibrary);
  
  nsresult Shutdown();  

private:

  /**
   * sbMediaListBatchCallbackFunc used to perform
   * batch sbIMediaItem setProperties calls.
   * Begins an EnumerateAllItems call on the background thread.
   */
  static nsresult RunLibraryBatch(nsISupports* aUserData);
  
  PRPackedBool                             mShouldShutdown;
  nsCOMPtr<nsIThread>                      mThread;

  sbLocalDatabaseLibrary*                  mLibrary;  
  sbLocalDatabasePropertyCache*            mPropCache;
  
  // Timer used to send job progress notifications()
  nsCOMPtr<nsITimer>                       mNotificationTimer;
  
  // sbIJobProgress variables
  PRUint16                                 mStatus;
  PRUint32                                 mCompletedItemCount;
  PRUint32                                 mTotalItemCount;
  nsString                                 mTitleText;
  nsString                                 mStatusText;
  nsString                                 mFailedText;
  nsCOMArray<sbIJobProgressListener>       mListeners;
};

#endif /* __SBLOCALDATABASEPROPERTYCACHE_H__ */
