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

#include "sbMediaExportPrefController.h"

#include "sbMediaExportDefines.h"
#include <nsIPrefBranch2.h>
#include <nsIObserverService.h>
#include <nsServiceManagerUtils.h>
#include <nsCOMPtr.h>
#include <nsStringAPI.h>


NS_IMPL_ISUPPORTS1(sbMediaExportPrefController, nsIObserver)

sbMediaExportPrefController::sbMediaExportPrefController()
  : mShouldProcessOnShutdown(PR_FALSE)
  , mShouldProcessOnStartup(PR_FALSE)
  , mShouldExportTracks(PR_FALSE)
  , mShouldExportPlaylists(PR_FALSE)
  , mShouldExportSmartPlaylists(PR_FALSE)
{
}

sbMediaExportPrefController::~sbMediaExportPrefController()
{
}

nsresult
sbMediaExportPrefController::Init()
{
  TRACE(("%s: Initializing the mediaexport pref controller", __FUNCTION__));

  nsresult rv;
  nsCOMPtr<nsIPrefBranch2> prefBranch =
    do_GetService("@mozilla.org/preferences-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Adding observer values for each pref will load the value in |Observe|.
  rv = prefBranch->AddObserver(PREF_IMPORTEXPORT_ONSHUTDOWN,
                               this,
                               PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefBranch->AddObserver(PREF_IMPORTEXPORT_ONSTARTUP,
                               this,
                               PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefBranch->AddObserver(PREF_EXPORT_TRACKS,
                               this,
                               PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefBranch->AddObserver(PREF_EXPORT_PLAYLISTS,
                               this,
                               PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefBranch->AddObserver(PREF_EXPORT_SMARTPLAYLISTS,
                               this,
                               PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
sbMediaExportPrefController::Shutdown()
{
  LOG(("%s: Shutting down the mediaexport pref controller", __FUNCTION__));

  nsresult rv;
  nsCOMPtr<nsIPrefBranch2> prefBranch =
    do_GetService("@mozilla.org/preferences-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Cleanup the pref observers 
  rv = prefBranch->RemoveObserver(PREF_IMPORTEXPORT_ONSHUTDOWN, this);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefBranch->RemoveObserver(PREF_IMPORTEXPORT_ONSTARTUP, this);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefBranch->RemoveObserver(PREF_EXPORT_TRACKS, this);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefBranch->RemoveObserver(PREF_EXPORT_PLAYLISTS, this);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefBranch->RemoveObserver(PREF_EXPORT_SMARTPLAYLISTS, this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbMediaExportPrefController::Observe(nsISupports *aSubject,
                                     const char *aTopic,
                                     const PRUnichar *aData)
{
  if (strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) != 0) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIPrefBranch2> prefBranch = do_QueryInterface(aSubject, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString modifiedPref(aData);
  
  PRBool modifiedValue = PR_FALSE;
  rv = prefBranch->GetBoolPref(NS_ConvertUTF16toUTF8(modifiedPref).get(),
                               &modifiedValue);
  NS_ENSURE_SUCCESS(rv, rv);
 
  LOG(("%s: %s pref changed to %s",
        __FUNCTION__,
        NS_ConvertUTF16toUTF8(modifiedPref).get(),
        (modifiedValue ? "true" : "false")));

  if (modifiedPref.EqualsLiteral(PREF_IMPORTEXPORT_ONSHUTDOWN)) {
    mShouldProcessOnShutdown = modifiedValue;
  }
  else if (modifiedPref.EqualsLiteral(PREF_IMPORTEXPORT_ONSTARTUP)) {
    mShouldProcessOnStartup = modifiedValue;
  }
  else if (modifiedPref.EqualsLiteral(PREF_EXPORT_TRACKS)) {
    mShouldExportTracks = modifiedValue;
  }
  else if (modifiedPref.EqualsLiteral(PREF_EXPORT_PLAYLISTS)) {
    mShouldExportPlaylists = modifiedValue;
  }
  else if (modifiedPref.EqualsLiteral(PREF_EXPORT_SMARTPLAYLISTS)) {
    mShouldExportSmartPlaylists = modifiedValue;
  }

  return NS_OK;
}

PRBool 
sbMediaExportPrefController::GetShouldProcessOnShutdown()
{
  return mShouldProcessOnShutdown; 
}

PRBool
sbMediaExportPrefController::GetShouldPorcessOnStartup()
{
  return mShouldProcessOnStartup;
}

PRBool
sbMediaExportPrefController::GetShouldExportTracks()
{
  return mShouldExportTracks;
}

PRBool
sbMediaExportPrefController::GetShouldExportPlaylists()
{
  return mShouldExportPlaylists;
}

PRBool
sbMediaExportPrefController::GetShouldExportSmartPlaylists()
{
  return mShouldExportSmartPlaylists;
}
