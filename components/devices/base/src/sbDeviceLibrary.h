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

#ifndef __SBDEVICELIBRARY_H__
#define __SBDEVICELIBRARY_H__

#include <nsAutoPtr.h>
#include <nsCOMPtr.h>
#include <nsInterfaceHashtable.h>
#include <nsIClassInfo.h>

#include <sbIDeviceEventListener.h>
#include <sbIDeviceLibrary.h>
#include <sbILibrary.h>
#include <sbIMediaListListener.h>
#include <sbILocalDatabaseSimpleMediaList.h>
#include <sbIPropertyArray.h>

#include <pref/nsIPrefBranch.h>
#include <prlock.h>

#include "sbDeviceLibraryHelpers.h"

// These are the methods from sbLibrary that we're going to
// override in sbDeviceLibrary.
#define SB_DECL_SBILIBRARY_OVERRIDES  \
  NS_IMETHOD CreateMediaItem(nsIURI *aContentUri, sbIPropertyArray *aProperties, PRBool aAllowDuplicates, sbIMediaItem **_retval); \
  NS_IMETHOD CreateMediaItemIfNotExist(nsIURI *aContentUri, sbIPropertyArray *aProperties, sbIMediaItem **aResultItem, PRBool *_retval); \
  NS_IMETHOD CreateMediaList(const nsAString & aType, sbIPropertyArray *aProperties, sbIMediaList **_retval);  \
  NS_IMETHOD GetDevice(sbIDevice * *aDevice); \

// Use this macro to declare functions that forward the behavior of this
// interface to another object in a safe way.
#define SB_FORWARD_SAFE_SBILIBRARY(_to) \
  NS_IMETHOD GetSupportsForeignMediaItems(PRBool *aSupportsForeignMediaItems) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetSupportsForeignMediaItems(aSupportsForeignMediaItems); } \
  NS_IMETHOD GetCreationParameters(nsIPropertyBag2 * *aCreationParameters) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetCreationParameters(aCreationParameters); } \
  NS_IMETHOD GetFactory(sbILibraryFactory * *aFactory) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetFactory(aFactory); } \
  NS_IMETHOD Resolve(nsIURI *aUri, nsIChannel **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->Resolve(aUri, _retval); } \
  NS_IMETHOD CopyMediaList(const nsAString & aType, sbIMediaList *aSource, sbIMediaList **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->CopyMediaList(aType, aSource, _retval); } \
  NS_IMETHOD GetMediaItem(const nsAString & aGuid, sbIMediaItem **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetMediaItem(aGuid, _retval); } \
  NS_IMETHOD GetMediaListTypes(nsIStringEnumerator * *aMediaListTypes) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetMediaListTypes(aMediaListTypes); } \
  NS_IMETHOD RegisterMediaListFactory(sbIMediaListFactory *aFactory) { return !_to ? NS_ERROR_NULL_POINTER : _to->RegisterMediaListFactory(aFactory); } \
  NS_IMETHOD Optimize(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->Optimize(); } \
  NS_IMETHOD Flush(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->Flush(); } \
  NS_IMETHOD BatchCreateMediaItems(nsIArray *aURIArray, nsIArray *aPropertyArrayArray, PRBool aAllowDuplicates, nsIArray **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->BatchCreateMediaItems(aURIArray, aPropertyArrayArray, aAllowDuplicates, _retval); } \
  NS_IMETHOD BatchCreateMediaItemsAsync(sbIBatchCreateMediaItemsListener *aListener, nsIArray *aURIArray, nsIArray *aPropertyArrayArray, PRBool aAllowDuplicates) { return !_to ? NS_ERROR_NULL_POINTER : _to->BatchCreateMediaItemsAsync(aListener, aURIArray, aPropertyArrayArray, aAllowDuplicates); } 

// These are the methods from sbLibrary that we're going to
// override in sbDeviceLibrary.
#define SB_DECL_SBIMEDIALIST_OVERRIDES \
  NS_IMETHOD Add(sbIMediaItem *aMediaItem); \
  NS_IMETHOD AddAll(sbIMediaList *aMediaList); \
  NS_IMETHOD AddSome(nsISimpleEnumerator *aMediaItems); \
  NS_IMETHOD Clear(void);

#define SB_FORWARD_SAFE_SBIMEDIALIST(_to) \
  NS_IMETHOD GetName(nsAString & aName) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetName(aName); } \
  NS_IMETHOD SetName(const nsAString & aName) { return !_to ? NS_ERROR_NULL_POINTER : _to->SetName(aName); } \
  NS_IMETHOD GetType(nsAString & aType) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetType(aType); } \
  NS_IMETHOD GetLength(PRUint32 *aLength) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetLength(aLength); } \
  NS_IMETHOD GetIsEmpty(PRBool *aIsEmpty) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetIsEmpty(aIsEmpty); } \
  NS_IMETHOD GetUserEditableContent(PRBool *aUserEditableContent) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetUserEditableContent(aUserEditableContent); } \
  NS_IMETHOD GetItemByGuid(const nsAString & aGuid, sbIMediaItem **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetItemByGuid(aGuid, _retval); } \
  NS_IMETHOD GetItemByIndex(PRUint32 aIndex, sbIMediaItem **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetItemByIndex(aIndex, _retval); } \
  NS_IMETHOD EnumerateAllItems(sbIMediaListEnumerationListener *aEnumerationListener, PRUint16 aEnumerationType) { return !_to ? NS_ERROR_NULL_POINTER : _to->EnumerateAllItems(aEnumerationListener, aEnumerationType); } \
  NS_IMETHOD EnumerateItemsByProperty(const nsAString & aPropertyID, const nsAString & aPropertyValue, sbIMediaListEnumerationListener *aEnumerationListener, PRUint16 aEnumerationType) { return !_to ? NS_ERROR_NULL_POINTER : _to->EnumerateItemsByProperty(aPropertyID, aPropertyValue, aEnumerationListener, aEnumerationType); } \
  NS_IMETHOD EnumerateItemsByProperties(sbIPropertyArray *aProperties, sbIMediaListEnumerationListener *aEnumerationListener, PRUint16 aEnumerationType) { return !_to ? NS_ERROR_NULL_POINTER : _to->EnumerateItemsByProperties(aProperties, aEnumerationListener, aEnumerationType); } \
  NS_IMETHOD GetItemsByProperty(const nsAString & aPropertyID, const nsAString & aPropertyValue, nsIArray **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetItemsByProperty(aPropertyID, aPropertyValue, _retval); } \
  NS_IMETHOD GetItemsByProperties(sbIPropertyArray *aProperties, nsIArray **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetItemsByProperties(aProperties, _retval); } \
  NS_IMETHOD IndexOf(sbIMediaItem *aMediaItem, PRUint32 aStartFrom, PRUint32 *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->IndexOf(aMediaItem, aStartFrom, _retval); } \
  NS_IMETHOD LastIndexOf(sbIMediaItem *aMediaItem, PRUint32 aStartFrom, PRUint32 *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->LastIndexOf(aMediaItem, aStartFrom, _retval); } \
  NS_IMETHOD Contains(sbIMediaItem *aMediaItem, PRBool *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->Contains(aMediaItem, _retval); } \
  NS_IMETHOD Remove(sbIMediaItem *aMediaItem) { return !_to ? NS_ERROR_NULL_POINTER : _to->Remove(aMediaItem); } \
  NS_IMETHOD RemoveByIndex(PRUint32 aIndex) { return !_to ? NS_ERROR_NULL_POINTER : _to->RemoveByIndex(aIndex); } \
  NS_IMETHOD RemoveSome(nsISimpleEnumerator *aMediaItems) { return !_to ? NS_ERROR_NULL_POINTER : _to->RemoveSome(aMediaItems); } \
  NS_IMETHOD AddListener(sbIMediaListListener *aListener, PRBool aOwnsWeak, PRUint32 aFlags, sbIPropertyArray *aPropertyFilter) { return !_to ? NS_ERROR_NULL_POINTER : _to->AddListener(aListener, aOwnsWeak, aFlags, aPropertyFilter); } \
  NS_IMETHOD RemoveListener(sbIMediaListListener *aListener) { return !_to ? NS_ERROR_NULL_POINTER : _to->RemoveListener(aListener); } \
  NS_IMETHOD CreateView(sbIMediaListViewState *aState, sbIMediaListView **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->CreateView(aState, _retval); } \
  NS_IMETHOD RunInBatchMode(sbIMediaListBatchCallback *aCallback, nsISupports *aUserData) { return !_to ? NS_ERROR_NULL_POINTER : _to->RunInBatchMode(aCallback, aUserData); } \
  NS_IMETHOD GetDistinctValuesForProperty(const nsAString & aPropertyID, nsIStringEnumerator **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetDistinctValuesForProperty(aPropertyID, _retval); } 

class sbDeviceLibrary : public sbIDeviceLibrary,
                        public sbIMediaListListener,
                        public sbILocalDatabaseMediaListCopyListener,
                        public sbIDeviceEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO

  NS_DECL_SBIDEVICELIBRARY
  NS_DECL_SBIMEDIALISTLISTENER
  NS_DECL_SBILOCALDATABASEMEDIALISTCOPYLISTENER
  NS_DECL_SBIDEVICEEVENTLISTENER

  sbDeviceLibrary(sbIDevice* aDevice);
  virtual ~sbDeviceLibrary();

  NS_FORWARD_SAFE_SBILIBRARYRESOURCE(mDeviceLibrary)
  NS_FORWARD_SAFE_SBIMEDIAITEM(mDeviceLibrary)
  SB_FORWARD_SAFE_SBIMEDIALIST(mDeviceLibrary)
  SB_FORWARD_SAFE_SBILIBRARY(mDeviceLibrary)

  SB_DECL_SBILIBRARY_OVERRIDES
  SB_DECL_SBIMEDIALIST_OVERRIDES

private:

  /**
   * \brief This callback adds the enumerated listeners to an nsCOMArray.
   *
   * \param aKey      - An nsISupport pointer to a listener for the key.
   * \param aEntry    - An sbIDeviceLibrary entry.
   * \param aUserData - An nsCOMArray to hold the enumerated pointers.
   *
   * \return PL_DHASH_NEXT on success
   * \return PL_DHASH_STOP on failure
   */
  static PLDHashOperator PR_CALLBACK
    AddListenersToCOMArrayCallback(nsISupportsHashKey::KeyType aKey,
                                   sbIDeviceLibraryListener* aEntry,
                                   void* aUserData);

  /**
   * \breif Gets the pref branch for the library sync preferences.
   */
  nsresult GetSyncPrefBranch(nsIPrefBranch** _retval);

  /**
   * \brief Create a library for a device instance.
   *
   * Creating a library provides you with storage for all data relating
   * to media items present on the device. After creating a library you will
   * typically want to register it so that it may be shown to the user.
   * 
   * When a library is created, two listeners are added to it. One listener
   * takes care of advising the sbIDeviceLibrary interface instance when items
   * need to be transferred to it, deleted from it or updated because data
   * relating to those items have changed.
   *
   * The second listener is responsible for detecting when items are transferred
   * from the devices library to another library.
   * 
   * \param aDeviceDatabaseURI Optional URI for the database.
   * \sa RemoveDeviceLibrary, RegisterDeviceLibrary, UnregisterDeviceLibrary
   */
  nsresult CreateDeviceLibrary(const nsAString &aDeviceIdentifier,
                               nsIURI *aDeviceDatabaseURI);

  /**
   * \brief Register the device library with the library manager.
   *
   * Registering the device library with the library manager enables the user
   * to view the library. It becomes accessible to others programmatically as
   * well through the library manager.
   *
   * \param aDeviceLibrary The library instance to register.
   */
  nsresult RegisterDeviceLibrary(sbILibrary* aDeviceLibrary);

  /**
   * \brief Unregister a device library with the library manager.
   *
   * Unregister a device library with the library manager will make the library
   * vanish from the list of libraries and block out access to it programatically 
   * as well.
   * 
   * A device should always unregister the device library when the application
   * shuts down, when the device is disconnected suddenly and when the user
   * ejects the device.
   *
   * \param aDeviceLibrary The library instance to unregister.
   */
  nsresult UnregisterDeviceLibrary(sbILibrary* aDeviceLibrary);

  /**
   * \brief Update the listeners for the main library to account for changes to
   *        the sync settings.
   */
  nsresult UpdateMainLibraryListeners();

  /**
   * \brief Return true in aIsSyncedLocally if the device is configured to sync
   *        to the local library.
   *
   * \param aIsSyncedLocally Set to true if the device is configured to sync to
   *                         the local library.
   */
  nsresult GetIsSyncedLocally(PRBool* aIsSyncedLocally);

  /**
   * \brief library for this device, location is specified by the
   *        aDeviceDatabaseURI param for CreateDeviceLibrary or the default
   *        location under the users profile.
   */
  nsCOMPtr<sbILibrary> mDeviceLibrary;

  /**
   * \brief the device this library belongs to
   */
  nsCOMPtr<sbIDevice> mDevice;
  
  /**
   * \brief the main library updating listener
   */
  nsRefPtr<sbLibraryUpdateListener> mMainLibraryListener;

  /**
   * \brief the main library updating listener property filter
   */
  nsCOMPtr<sbIMutablePropertyArray> mMainLibraryListenerFilter;

  /**
   * \brief A list of listeners.
   */
  nsInterfaceHashtable<nsISupportsHashKey, sbIDeviceLibraryListener> mListeners;

  /**
   * \brief Last is synced locally state.  Used to detect changes to the state.
   */
  PRBool mLastIsSyncedLocally;

  PRLock* mLock;
};

#endif /* __SBDEVICELIBRARY_H__ */
