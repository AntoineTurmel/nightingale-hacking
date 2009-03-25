/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef __SB_MEDIAMANAGEMENTJOB_H__
#define __SB_MEDIAMANAGEMENTJOB_H__

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
// Songbird media management job
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/**
 * \file  sbMediaManagementJob.h
 * \brief Songbird Media Management Job Definitions.
 */

//------------------------------------------------------------------------------
//
// Songbird media management job imported services.
//
//------------------------------------------------------------------------------

// Songbird imports.
#include <sbIMediaManagementJob.h>
#include <sbIMediaFileManager.h>
#include <sbIJobCancelable.h>
#include <sbIJobProgress.h>
#include <sbIJobProgressUI.h>
#include <sbIWatchFolderService.h>

// Mozilla imports.
#include <nsCOMArray.h>
#include <nsCOMPtr.h>
#include <nsIClassInfo.h>
#include <nsIFile.h>
#include <nsIStringBundle.h>
#include <nsITimer.h>
#include <nsTArray.h>

//
// Songbird media management job component defs.
//

#define SB_MEDIAMANAGEMENTJOB_CLASSNAME "Songbird Media Management Job"
#define SB_MEDIAMANAGEMENTJOB_CID                                              \
  /* {ed63b0ec-ba51-4678-be7d-619bdda4674b} */                                 \
{                                                                              \
    0xed63b0ec,                                                                \
    0xba51,                                                                    \
    0x4678,                                                                    \
    { 0xbe, 0x7d, 0x61, 0x9b, 0xdd, 0xa4, 0x67, 0x4b }                         \
}

// Default values for the timers (milliseconds)
#define MMJOB_SCANNER_INTERVAL 10

// Preference keys
#define PREF_MMJOB_INTERVAL "songbird.media_management.library.scan.interval"
#define PREF_MMJOB_LOCATION "songbird.media_management.library.folder"
#define PREF_MMJOB_COPYFILES "songbird.media_management.libary.copy"

//------------------------------------------------------------------------------
//
// Songbird media management job defs.
//
//------------------------------------------------------------------------------

class sbMediaManagementJob : public sbIMediaManagementJob,
                             public sbIJobCancelable,
                             public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_SBIMEDIAMANAGEMENTJOB
  NS_DECL_SBIJOBPROGRESS
  NS_DECL_SBIJOBPROGRESSUI
  NS_DECL_SBIJOBCANCELABLE
  NS_DECL_NSITIMERCALLBACK

  sbMediaManagementJob();
  virtual ~sbMediaManagementJob();

private:
  nsresult UpdateProgress();
  
  nsresult ProcessNextItem();
  nsresult ProcessItem(sbIMediaItem* aItem, nsAString& outFilePath);
protected:
  // We need to hold onto the media list so we can get the items
  nsCOMPtr<sbIMediaList>                   mMediaList;
 
  // Where our media folder is located.
  nsCOMPtr<nsIFile>                        mMediaFolder;
  
  // Flag to indicate if we need to copy files into the Media Folder
  PRBool                                   mCopyFilesToMediaFolder;
  
  // Watch Folder information
  PRBool                                   mShouldIgnoreMediaFolder;
  nsCOMPtr<sbIWatchFolderService>          mWatchFolderService;
  
  // Keep a single instance of the file manager for this job
  nsCOMPtr<sbIMediaFileManager>            mMediaFileManager;
  
  // Timers for doing our work
  nsCOMPtr<nsITimer>                       mIntervalTimer;
  PRInt32                                  mIntervalTimerValue;

  // sbIJobProgress variables
  PRUint16                                 mStatus;
  nsTArray<nsString>                       mErrorMessages;
  nsCOMArray<sbIJobProgressListener>       mListeners;
  PRUint32                                 mCompletedItemCount;
  PRUint32                                 mTotalItemCount;
  nsString                                 mCurrentContentURL;

  // String bundle for status messages.
  nsCOMPtr<nsIStringBundle>                mStringBundle;
};

#endif // __SB_MEDIAMANAGEMENTJOB_H__