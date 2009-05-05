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

#include "sbiTunesImporter.h"

// C/C++ includes
#include <algorithm>

// Mozilla includes
#include <nsArrayUtils.h>
#include <nsCOMArray.h>
#include <nsComponentManagerUtils.h>
#include <nsIArray.h>
#include <nsIBufferedStreams.h>
#include <nsIFile.h>
#include <nsIFileURL.h>
#include <nsIInputStream.h>
#include <nsIIOService.h>
#include <nsILocalFile.h>
#include <nsIProperties.h>
#include <nsISimpleEnumerator.h>
#include <nsIURI.h>
#include <nsIXULRuntime.h>
#include <nsMemory.h>
#include <nsThreadUtils.h>

// NSPR includes
#include <prtime.h>

// Songbird includes
#include <sbIAlbumArtFetcherSet.h>
#include <sbIAlbumArtListener.h>
#include <sbFileUtils.h>
#include <sbILibrary.h>
#include <sbILocalDatabaseLibrary.h>
#include <sbIMediaList.h>
#include <sbIPropertyArray.h>
#include <sbLibraryUtils.h>
#include <sbIMediacoreTypeSniffer.h>
#include <sbPrefBranch.h>
#include <sbStringUtils.h>
#include <sbStandardProperties.h>
#ifdef SB_ENABLE_TEST_HARNESS
#include <sbITimingService.h>
#endif

#include "sbiTunesImporterAlbumArtListener.h"
#include "sbiTunesImporterBatchCreateListener.h"
#include "sbiTunesImporterJob.h"
#include "sbiTunesImporterStatus.h"

char const SB_ITUNES_LIBRARY_IMPORT_PREF_PREFIX[] = "library_import.itunes";
#define SB_ITUNES_GUID_PROPERTY "http://songbirdnest.com/data/1.0#iTunesGUID"

#ifdef PR_LOGGING
static PRLogModuleInfo* giTunesImporter = nsnull;
#define TRACE(args) \
  PR_BEGIN_MACRO \
  if (!giTunesImporter) \
  giTunesImporter = PR_NewLogModule("sbiTunesImporter"); \
  PR_LOG(giTunesImporter, PR_LOG_DEBUG, args); \
  PR_END_MACRO
#define LOG(args) \
  PR_BEGIN_MACRO \
  if (!giTunesImporter) \
  giTunesImporter = PR_NewLogModule("sbiTunesImporter"); \
  PR_LOG(giTunesImporter, PR_LOG_WARN, args); \
  PR_END_MACRO


#else
#define TRACE(args) /* nothing */
#define LOG(args)   /* nothing */
#endif /* PR_LOGGING */

/**
 * typedef for conversion functions used to convert values from iTunes to 
 * Songbird
 */
typedef nsString (*ValueConversion)(nsAString const & aValue);

/**
 * Convert the iTunes rating value specified by aITunesMetaValue to a Songbird
 * rating property value and return the result. 1 star is the same as 20 in 
 * iTunes
 *
 * \param aRating iTunes rating
 *
 * \return Songbird property value.
 */

nsString ConvertRating(nsAString const & aRating) {
  nsresult rv;
  if (aRating.IsEmpty()) {
    return nsString();
  }
  PRInt32 rating = aRating.ToInteger(&rv, 10);
  nsString result;
  if (NS_SUCCEEDED(rv)) {
    result.AppendInt((rating + 10) / 20, 10);
  }
  return result;
}

/**
 * Convert the iTunes duration value specified by aITunesMetaValue to a
 * Songbird duration property value and return the result.
 *
 * \param aDuration iTunes duration in seconds.
 *
 * \return Songbird property value.
 */

nsString ConvertDuration(nsAString const & aDuration) {
  nsresult rv;
  if (aDuration.IsEmpty()) {
    return nsString();
  }
  PRInt32 const duration = aDuration.ToInteger(&rv, 10);
  nsString result;
  if (NS_SUCCEEDED(rv)) {
    result.AppendInt(duration * 1000);
  }
  return result;
}

/**
 * Convert the iTunes date/time value specified by aITunesMetaValue to a
 * Songbird date/time property value and return the result.
 *
 * \param aDateTime iTunes date/time value.
 *
 * \return Songbird property value.
 */

nsString ConvertDateTime(nsAString const & aDateTime) {
  // If empty just return empty
  if (aDateTime.IsEmpty()) {
    return nsString();
  }
  // Convert "1970-01-01T00:00:00Z" to "1970/01/01 00:00:00 UTC".
  nsCString dateTime = ::NS_LossyConvertUTF16toASCII(aDateTime);
  nsCString::char_type *begin, *end;
  for (dateTime.BeginWriting(&begin, &end);begin != end; ++begin) {
    switch (*begin) {
      case '-': {
        *begin  = '/';
      }
      break;
      case 'T': {
        *begin = ' ';
      }
      break;
    }
  }
  dateTime.EndWriting();
  PRInt32 zIndex = dateTime.Find("Z");
  if (zIndex != -1) {
    dateTime.Replace(zIndex, 1, "GMT");
  }
  // Parse the date/time string into epoch time.
  PRTime prTime;
  PRStatus status = PR_ParseTimeString(dateTime.BeginReading(),
                                       PR_TRUE,
                                       &prTime);
  // On failure just return an emptry string
  if (status == PR_FAILURE) {
    NS_WARNING("Failed to parse time in sbiTunesImporter ConvertDateTime");
    return nsString();
  }
  prTime /= PR_USEC_PER_MSEC;
  
  return sbAutoString(static_cast<PRUint64>(prTime)); 
}

/**
 * Convert the iTunes 'kind' value specified by aITunesMetaValue to a
 * Songbird contentType property value and return the result.
 *
 * \param aKind iTunes media kind value.
 *
 * \return Songbird property value.
 */

nsString ConvertKind(nsAString const & aKind) {
  nsString result;
  
  if (aKind.Find("video") != -1) {
    result = NS_LITERAL_STRING("video");
  }
  else if (aKind.Find("audio") != -1) {
    result = NS_LITERAL_STRING("audio");
  }
  else if (aKind.EqualsLiteral("true")) {
    result = NS_LITERAL_STRING("podcast");
  }
 
  return result;
}

struct PropertyMap {
  char const * SBProperty;
  char const * ITProperty;
  ValueConversion mConversion;
};

/**
 * Mapping between Songbird properties and itTunes
 */
PropertyMap gPropertyMap[] = {
  { SB_ITUNES_GUID_PROPERTY,      "Persistent ID", 0 },
  { SB_PROPERTY_ALBUMARTISTNAME,  "Album Artist", 0 },
  { SB_PROPERTY_ALBUMNAME,        "Album", 0 },
  { SB_PROPERTY_ARTISTNAME,       "Artist", 0 },
  { SB_PROPERTY_BITRATE,          "Bit Rate", 0 },
  { SB_PROPERTY_BPM,              "BPM", 0 },
  { SB_PROPERTY_COMMENT,          "Comments", 0 },
  { SB_PROPERTY_COMPOSERNAME,     "Composer", 0 },
  { SB_PROPERTY_CONTENTTYPE,      "Kind", ConvertKind },
  { SB_PROPERTY_DISCNUMBER,       "Disc Number", 0 },
  { SB_PROPERTY_DURATION,         "Total Time", ConvertDuration },
  { SB_PROPERTY_GENRE,            "Genre", 0},
  { SB_PROPERTY_LASTPLAYTIME,     "Play Date UTC", ConvertDateTime },
  { SB_PROPERTY_LASTSKIPTIME,     "Skip Date", ConvertDateTime },
  { SB_PROPERTY_PLAYCOUNT,        "Play Count", 0 },
  { SB_PROPERTY_CONTENTTYPE,      "Podcast", ConvertKind },
  { SB_PROPERTY_RATING,           "Rating", ConvertRating },
  { SB_PROPERTY_SAMPLERATE,       "Sample Rate", 0 },
  { SB_PROPERTY_SKIPCOUNT,        "Skip Count", 0 },
  { SB_PROPERTY_TOTALDISCS,       "Disc Count", 0 },
  { SB_PROPERTY_TOTALTRACKS,      "Track Count", 0 },
  { SB_PROPERTY_TRACKNAME,        "Name", 0 },
  { SB_PROPERTY_TRACKNUMBER,      "Track Number", 0 },
  { SB_PROPERTY_YEAR,             "Year", 0 },
};

NS_IMPL_ISUPPORTS2(sbiTunesImporter, sbILibraryImporter,
                                     sbIiTunesXMLParserListener)

sbiTunesImporter::sbiTunesImporter() : mBatchEnded(PR_FALSE),
                                       mDataFormatVersion(DATA_FORMAT_VERSION),
                                       mFoundChanges(PR_FALSE),
                                       mImportPlaylists(PR_TRUE),
                                       mMissingMediaCount(0),
                                       mOSType(UNINITIALIZED),
                                       mTrackCount(0),
                                       mUnsupportedMediaCount(0)
  
{
  mTrackBatch.reserve(BATCH_SIZE);
}

sbiTunesImporter::~sbiTunesImporter()
{
  // Just make sure we were finalized
  Finalize();
}

nsresult
sbiTunesImporter::Cancel() {
  nsresult rv;
  nsString msg;
  rv = 
    SBGetLocalizedString(msg,
                         NS_LITERAL_STRING("import_library.job.status.cancelled"));
  if (NS_FAILED(rv)) { 
    // Show at least something
    msg = NS_LITERAL_STRING("Library import cancelled");
  }
  mStatus->SetStatusText(msg);
  mStatus->Done();
  mStatus->Update();
  return NS_OK;
}

/* readonly attribute AString libraryType; */
NS_IMETHODIMP 
sbiTunesImporter::GetLibraryType(nsAString & aLibraryType)
{
  aLibraryType = NS_LITERAL_STRING("iTunes");
  return NS_OK;
}

/* readonly attribute AString libraryReadableType; */
NS_IMETHODIMP
sbiTunesImporter::GetLibraryReadableType(nsAString & aLibraryReadableType)
{
  aLibraryReadableType = NS_LITERAL_STRING("iTunes");
  return NS_OK;
}

/* readonly attribute AString libraryDefaultFileName; */
NS_IMETHODIMP
sbiTunesImporter::GetLibraryDefaultFileName(nsAString & aLibraryDefaultFileName)
{
  aLibraryDefaultFileName = NS_LITERAL_STRING("iTunes Music Library.xml");
  return NS_OK;
}

/* readonly attribute AString libraryDefaultFilePath; */
NS_IMETHODIMP
sbiTunesImporter::GetLibraryDefaultFilePath(nsAString & aLibraryDefaultFilePath)
{
  nsresult rv;
  nsCOMPtr<nsIProperties> directoryService = 
    do_CreateInstance("@mozilla.org/file/directory_service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIFile> libraryFile;

  nsString defaultLibraryName;
  rv = GetLibraryDefaultFileName(defaultLibraryName);
  NS_ENSURE_SUCCESS(rv, rv);
  
  /* Search for an iTunes library database file. */
  /*XXXErikS Should localize directory names. */
  switch (GetOSType())
  {
    case MAC_OS : {
      rv = directoryService->Get("Music",
                                 NS_GET_IID(nsIFile),
                                 getter_AddRefs(libraryFile));
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = libraryFile->Append(NS_LITERAL_STRING("iTunes"));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    break;
    case WINDOWS_OS : {
      rv = directoryService->Get("Pers",
                                 NS_GET_IID(nsIFile),
                                 getter_AddRefs(libraryFile));
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = libraryFile->Append(NS_LITERAL_STRING("My Music"));
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = libraryFile->Append(NS_LITERAL_STRING("iTunes"));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    break;
    default : {
      rv = directoryService->Get("Home",
                                 NS_GET_IID(nsIFile),
                                 getter_AddRefs(libraryFile));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    break;
  }

  rv = libraryFile->Append(defaultLibraryName);
  NS_ENSURE_SUCCESS(rv, rv);
  
  /* If the library file exists, get its path. */
  PRBool exists = PR_FALSE;
  rv = libraryFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (exists) {
    nsString path;
    rv = libraryFile->GetPath(path);
    NS_ENSURE_SUCCESS(rv, rv);
    
    aLibraryDefaultFilePath = path;
    return NS_OK;
  }
  
  return NS_OK;
}

/* readonly attribute AString libraryFileExtensionList; */
NS_IMETHODIMP 
sbiTunesImporter::GetLibraryFileExtensionList(
    nsAString & aLibraryFileExtensionList)
{
  aLibraryFileExtensionList = NS_LITERAL_STRING("xml");
  return NS_OK;
}

/* readonly attribute boolean libraryPreviouslyImported; */
NS_IMETHODIMP 
sbiTunesImporter::GetLibraryPreviouslyImported(
    PRBool *aLibraryPreviouslyImported)
{
  nsresult rv;
  sbPrefBranch prefs(SB_ITUNES_LIBRARY_IMPORT_PREF_PREFIX , &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCString const & temp = prefs.GetCharPref("lib_prev_mod_time", nsCString());
  *aLibraryPreviouslyImported = temp.IsEmpty() ? PR_FALSE : PR_TRUE;
  return NS_OK;
}

/* readonly attribute AString libraryPreviousImportPath; */
NS_IMETHODIMP
sbiTunesImporter::GetLibraryPreviousImportPath(
    nsAString & aLibraryPreviousImportPath)
{
  nsresult rv;
  sbPrefBranch prefs(SB_ITUNES_LIBRARY_IMPORT_PREF_PREFIX , &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  aLibraryPreviousImportPath = 
    ::NS_ConvertUTF8toUTF16(prefs.GetCharPref("lib_prev_path", 
                                              nsCString()));
  return NS_OK;
}

/* void initialize (); */
NS_IMETHODIMP
sbiTunesImporter::Initialize()
{
  nsresult rv = miTunesLibSig.Initialize();
  NS_ENSURE_SUCCESS(rv, rv);
  
  mIOService = 
    do_CreateInstance("@mozilla.org/network/io-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mAlbumArtFetcher = 
    do_CreateInstance("@songbirdnest.com/Songbird/album-art-fetcher-set;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  
  rv = GetMainLibrary(getter_AddRefs(mLibrary));
  NS_ENSURE_SUCCESS(rv, rv);

  mLDBLibrary = do_QueryInterface(mLibrary, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
#ifdef SB_ENABLE_TEST_HARNESS
  // Ignore errors, we'll just not do timing
  mTimingService = do_GetService("@songbirdnest.com/Songbird/TimingService;1", &rv);
#endif
  
  rv = miTunesDBServices.Initialize();
  NS_ENSURE_SUCCESS(rv, rv);
  
  mPlaylistBlacklist = 
    SBLocalizedString(NS_LITERAL_STRING("import_library.itunes.excluded_playlists"), 
                      nsString());
  return NS_OK;
}

/* void finalize (); */
NS_IMETHODIMP 
sbiTunesImporter::Finalize()
{
  if (!mBatchEnded) {
    mBatchEnded = PR_TRUE;
    mLDBLibrary->ForceEndUpdateBatch();
  }
  mListener = nsnull;
  mLibrary = nsnull;
  mLDBLibrary = nsnull;
  if (mStatus.get()) {
    mStatus->Finalize();
  }
  return NS_OK;
}

nsresult sbiTunesImporter::DBModified(sbPrefBranch & aPrefs,
                                       nsAString const & aLibPath,
                                       PRBool * aModified) {
  *aModified = PR_TRUE;
  nsresult rv;
  
  // Check that the path is the same
  nsString prevPath;
  rv = GetLibraryPreviousImportPath(prevPath);
  if (NS_FAILED(rv) || !aLibPath.Equals(prevPath)) {
    return NS_OK;
  }
  
  // Check the last modified times
  nsCOMPtr<nsILocalFile> file = do_CreateInstance(NS_LOCALFILE_CONTRACTID);
  rv = file->InitWithPath(aLibPath);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }
  
  PRInt64 lastModified;
  rv = file->GetLastModifiedTime(&lastModified);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }
  nsCString const & temp = aPrefs.GetCharPref("lib_prev_mod_time", nsCString());
  if (temp.IsEmpty()) {
    return NS_OK;
  }
  PRInt64 prevLastModified = 
    nsString_ToInt64(NS_ConvertASCIItoUTF16(temp), &rv);
  if (NS_SUCCEEDED(rv)) {
    *aModified = lastModified != prevLastModified;
  }
  return NS_OK;
}
/* sbIJobProgress import (in AString aLibFilePath, 
 *                        in AString aGUID,
 *                        in boolean aCheckForChanges); */
NS_IMETHODIMP 
sbiTunesImporter::Import(const nsAString & aLibFilePath, 
                          const nsAString & aGUID, 
                          PRBool aCheckForChanges, 
                          sbIJobProgress ** aJobProgress)
{
  TRACE(("sbiTunesImporter::Import(%s, %s, %s)",
         NS_LossyConvertUTF16toASCII(aLibFilePath).get(),
         NS_LossyConvertUTF16toASCII(aGUID).get(),
         (aCheckForChanges ? "true" : "false")));
  // Must be started on the main thread
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_FAILURE);
  
  nsresult rv;
  
  mLibraryPath = aLibFilePath;
  mImport = aCheckForChanges ? PR_FALSE : PR_TRUE;
  sbPrefBranch prefs(SB_ITUNES_LIBRARY_IMPORT_PREF_PREFIX, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsRefPtr<sbiTunesImporterJob> jobProgress = sbiTunesImporterJob::New();
  mStatus = 
    std::auto_ptr<sbiTunesImporterStatus>(sbiTunesImporterStatus::New(jobProgress));
  NS_ENSURE_TRUE(mStatus.get(), NS_ERROR_FAILURE);

  mStatus->Initialize();

  mDataFormatVersion = prefs.GetIntPref("version", DATA_FORMAT_VERSION);
  // If we're checking for changes and the db wasn't modified, just exit
  PRBool modified;
  if (!mImport && NS_SUCCEEDED(DBModified(prefs, 
                                         mLibraryPath, 
                                         &modified)) && !modified) {
    /* Update status. */
    rv = mStatus->Reset();
    NS_ENSURE_SUCCESS(rv, rv);
    
    mStatus->SetStatusText(NS_LITERAL_STRING("No library changes found"));
    mStatus->Done();
    mStatus->Update();
    return NS_OK;
  }
  mImportPlaylists = PR_FALSE;
  
  if (mImport) {
    PRBool const dontImportPlaylists = prefs.GetBoolPref("dont_import_playlists", PR_FALSE);
    mImportPlaylists = !dontImportPlaylists;
  }
  mTypeSniffer = do_CreateInstance("@songbirdnest.com/Songbird/Mediacore/TypeSniffer;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
#ifdef SB_ENABLE_TEST_HARNESS 
  if (mTimingService) {
      mTimingIdentifier = NS_LITERAL_STRING("ITunesImport-");
      mTimingIdentifer.AppendInt(time(0));
      mTimingService->StartPerfTimer(mTimingIdentifier);
  }
#endif
  
  rv = sbOpenInputStream(mLibraryPath, getter_AddRefs(mStream));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mStream->Available(&mStreamSize);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mStatus->SetProgressMax(mStreamSize);
  
  nsAString const & msg = 
    mImport ? NS_LITERAL_STRING("Importing library") :
              NS_LITERAL_STRING("Checking for changes in library");
                       ;
  mStatus->SetStatusText(msg);

  // Scope batching
  {
    // Put the library in batch mode
    mLDBLibrary->ForceBeginUpdateBatch();
    
    mParser = sbiTunesXMLParser::New();


    rv = mParser->Parse(mStream, this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aJobProgress = jobProgress;
  NS_IF_ADDREF(*aJobProgress);
  return NS_OK;
}

/* void setListener (in sbILibraryImporterListener aListener); */
NS_IMETHODIMP 
sbiTunesImporter::SetListener(sbILibraryImporterListener *aListener)
{
  NS_WARN_IF_FALSE(mListener == nsnull, "Listener was previously set");
  
  mListener = aListener;
  
  return NS_OK;
}

sbiTunesImporter::OSType sbiTunesImporter::GetOSType()
{
  if (mOSType == UNINITIALIZED) {
    nsresult rv;
    nsCOMPtr<nsIXULRuntime> appInfo = 
      do_CreateInstance("@mozilla.org/xre/app-info;1", &rv);
    NS_ENSURE_SUCCESS(rv, UNKNOWN_OS);
    
    nsCString osName;
    rv = appInfo->GetOS(osName);
    NS_ENSURE_SUCCESS(rv, UNKNOWN_OS);
    
    ToLowerCase(osName);
    
    /* Determine the OS type. */
    if (osName.Find("darwin") != -1) {
      mOSType = MAC_OS;
    }
    else if (osName.Find("linux") != -1) {
      mOSType = LINUX_OS;
    }
    else if (osName.Find("win") != -1) {
      mOSType = WINDOWS_OS;
    }
    else { 
      mOSType = UNKNOWN_OS;
    }
  }
  return mOSType;
}

// sbIiTunesXMLParserListener implementation

/* void onTopLevelProperties (in sbIStringMap aProperties); */
NS_IMETHODIMP sbiTunesImporter::OnTopLevelProperties(sbIStringMap *aProperties)
{
  nsresult rv = aProperties->Get(NS_LITERAL_STRING("Library Persistent ID"), 
                                 miTunesLibID);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString id(NS_LITERAL_STRING("Library Persistent ID"));
  id.Append(miTunesLibID);
  rv = miTunesLibSig.Update(id);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

NS_IMETHODIMP 
sbiTunesImporter::OnTrack(sbIStringMap *aProperties)
{
  if (mStatus->CancelRequested()) {
    Cancel();
    return NS_ERROR_ABORT;
  }
  nsresult rv = UpdateProgress();
  iTunesTrack * const track = new iTunesTrack;
  NS_ENSURE_TRUE(track, NS_ERROR_OUT_OF_MEMORY);

  rv  = track->Initialize(aProperties);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mTrackBatch.push_back(track);
  if (mTrackBatch.size() == BATCH_SIZE) {
    ProcessTrackBatch();
  }
  return NS_OK;
}

NS_IMETHODIMP 
sbiTunesImporter::OnTracksComplete() {
  if (mTrackBatch.size() > 0) {
    ProcessTrackBatch();
  }
  return NS_OK;
}

PRBool 
sbiTunesImporter::ShouldImportPlaylist(sbIStringMap * aProperties) {
  nsString playlistName;
  nsresult rv = aProperties->Get(NS_LITERAL_STRING("Name"), playlistName);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  
  nsString master;
  // Don't care if this errors
  aProperties->Get(NS_LITERAL_STRING("Master"), master);
  
  nsString smartInfo;
  aProperties->Get(NS_LITERAL_STRING("Smart Info"), smartInfo);
  
  nsString delimitedName;
  delimitedName.AppendLiteral(":");
  delimitedName.Append(playlistName);
  delimitedName.AppendLiteral(":");
  // If it has tracks, is not the master playlist, is not a smart playlist, and
  // is not on the black list it should be imported
  return !master.EqualsLiteral("true") &&
         smartInfo.IsEmpty() &&
         mPlaylistBlacklist.Find(delimitedName) == -1;
}

static nsresult
FindPlaylistByName(sbILibrary * aLibrary,
                   nsAString const & aPlaylistName,
                   sbIMediaList ** aMediaList) {
  NS_ENSURE_ARG_POINTER(aMediaList);
  // Clear out so if we fail to find we return an empty string  
  nsCOMArray<sbIMediaItem> mediaItems;
  nsresult rv = 
    sbLibraryUtils::GetItemsByProperty(aLibrary, 
                                       NS_LITERAL_STRING(SB_PROPERTY_MEDIALISTNAME),
                                       aPlaylistName,
                                       mediaItems);
  if (NS_SUCCEEDED(rv) && mediaItems.Count() > 0) {
    NS_WARN_IF_FALSE(mediaItems.Count() > 1, "We got multiple playlists for "
                                             "the same name in iTunesImport");
    // nsCOMArray doesn't addref on access so no need for an nsCOMPtr here
    sbIMediaItem * mediaItem = mediaItems.ObjectAt(0);
    if (mediaItem) {
      rv = mediaItem->QueryInterface(NS_GET_IID(sbIMediaList),
                                     reinterpret_cast<void**>(aMediaList));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  // Not found so return null
  *aMediaList = nsnull;
  return NS_OK;
}

nsresult
sbiTunesImporter::UpdateProgress() {
  PRUint32 bytesAvailable;
  nsresult rv = mStream->Available(&bytesAvailable);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mStatus->SetProgress(mStreamSize - bytesAvailable);
  return NS_OK;
}

NS_IMETHODIMP
sbiTunesImporter::OnPlaylist(sbIStringMap *aProperties, 
                             PRInt32 *aTrackIds, 
                             PRUint32 aTrackIdsCount)
{
  if (mStatus->CancelRequested()) {
    Cancel();
    return NS_ERROR_ABORT;
  }
  nsresult rv = UpdateProgress();
  if (ShouldImportPlaylist(aProperties)) {
    nsString playlistID;
    rv = aProperties->Get(NS_LITERAL_STRING("Playlist Persistent ID"), 
                          playlistID);
    NS_ENSURE_SUCCESS(rv, rv);
          
    NS_NAMED_LITERAL_STRING(name, "Name");
    nsString playlistName;
    rv = aProperties->Get(name, playlistName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsString text(name);
    text.Append(playlistName);
    
    rv = miTunesLibSig.Update(text);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (mImportPlaylists) {
      nsString playlistSBGUID;
      rv = miTunesDBServices.GetSBIDFromITID(miTunesLibID, playlistID, 
                                             playlistSBGUID);
      nsCOMPtr<sbIMediaList> mediaList; 
      if ((NS_FAILED(rv) || playlistSBGUID.IsEmpty()) 
          && mDataFormatVersion < 2) {
        rv = FindPlaylistByName(mLibrary,
                                playlistName, 
                                getter_AddRefs(mediaList));
        NS_ENSURE_SUCCESS(rv, rv);
        
        rv = miTunesDBServices.WaitForCompletion(
                 sbiTunesDatabaseServices::INFINITE_WAIT);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      ImportPlaylist(aProperties,
                     aTrackIds,
                     aTrackIdsCount,
                     mediaList);
    }
  }
  return NS_OK;
}

static nsresult
ComputePlaylistSignature(sbiTunesSignature & aSignature, 
                         sbIMediaList * aMediaList) {
  PRUint32 length;
  nsresult rv = aMediaList->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsString guid;
  nsCOMPtr<sbIMediaItem> mediaItem;
  for (PRUint32 index = 0; index < length; ++index) {
    rv = aMediaList->GetItemByIndex(index, getter_AddRefs(mediaItem));
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = mediaItem->GetGuid(guid);
    NS_ENSURE_SUCCESS(rv, rv);
    
    aSignature.Update(guid);
  }
  return NS_OK;
}

static nsresult
IsPlaylistDirty(sbIMediaList * aMediaList, 
                PRBool & aIsDirty) {
  sbiTunesSignature signature;
  nsresult rv = ComputePlaylistSignature(signature, aMediaList);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsString computedSignature;
  rv = signature.GetSignature(computedSignature);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsString playlistGuid;
  rv = aMediaList->GetGuid(playlistGuid);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsString storedSignature;
  rv = signature.RetrieveSignature(playlistGuid, storedSignature);
  NS_ENSURE_SUCCESS(rv, rv);
  
  aIsDirty = computedSignature.Equals(storedSignature);
  
  return NS_OK;
}

nsresult
sbiTunesImporter::GetDirtyPlaylistAction(nsAString const & aPlaylistName,
                                         nsAString & aAction) {
  aAction = NS_LITERAL_STRING("replace");
  // If the user hasn't given an action for all yet, ask them again
  if (mPlaylistAction.IsEmpty()) { 
  
    PRBool applyAll;
    nsresult rv = mListener->OnDirtyPlaylist(aPlaylistName,
                                              &applyAll, 
                                              aAction);
    NS_ENSURE_SUCCESS(rv, rv);
    if (applyAll) {
      mPlaylistAction = aAction;
    }
  }
  else { // The user gave us an answer for the rest, so use that
    aAction = mPlaylistAction;
  }
  return NS_OK;
}

static nsresult
AddItemsToPlaylist(sbIMediaList * aMediaList,
                   nsIMutableArray * aItems) {
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = aItems->Enumerate(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = aMediaList->AddSome(enumerator);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

nsresult
sbiTunesImporter::ProcessPlaylistItems(sbIMediaList * aMediaList,
                                       PRInt32 * aTrackIds,
                                       PRUint32 aTrackIdsCount) {
  nsresult rv;
  nsCOMPtr<nsIMutableArray> tracks = 
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIMediaItem> mediaItem;
  for (PRUint32 index = 0; index < aTrackIdsCount; ++index) {
    // If we're at a batch point
    if (((index + 1) % BATCH_SIZE) == 0) {
      rv = AddItemsToPlaylist(aMediaList, tracks);
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = tracks->Clear();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    nsString trackID;
    trackID.AppendInt(aTrackIds[index], 10);
    
    // Add the iTuens track persistent ID to the iTunes library signature
    nsString text;
    text.AppendLiteral("Persistent ID");
    text.Append(miTunesLibID);
    text.Append(trackID);
    rv = miTunesLibSig.Update(text);
    NS_ENSURE_SUCCESS(rv, rv);
    
    TrackIDMap::const_iterator iter = mTrackIDMap.find(trackID);
    if (iter != mTrackIDMap.end()) {
      rv = mLibrary->GetItemByGuid(iter->second, getter_AddRefs(mediaItem));
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = tracks->AppendElement(mediaItem, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  rv = AddItemsToPlaylist(aMediaList, tracks);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

static nsresult
StorePlaylistSignature(sbIMediaList * aMediaList) {
  sbiTunesSignature signature;
  nsresult rv = signature.Initialize();
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = ComputePlaylistSignature(signature, aMediaList);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsString theSignature;
  rv = signature.GetSignature(theSignature);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsString guid;
  rv = aMediaList->GetGuid(guid);
  NS_ENSURE_SUCCESS(rv, rv);
    
  rv = signature.StoreSignature(guid, theSignature);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

nsresult
sbiTunesImporter::ImportPlaylist(sbIStringMap *aProperties, 
                                 PRInt32 *aTrackIds, 
                                 PRUint32 aTrackIdsCount,
                                 sbIMediaList * aMediaList) {
  nsresult rv;
  
  nsCOMPtr<sbIMediaList> mediaList(aMediaList);
  PRBool isDirty = PR_TRUE;
  if (mediaList) {
    rv = IsPlaylistDirty(mediaList, isDirty);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  nsString playlistiTunesID;
  rv = aProperties->Get(NS_LITERAL_STRING("Playlist Persistent ID"),
                        playlistiTunesID);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsString playlistName;
  rv = aProperties->Get(NS_LITERAL_STRING("Name"), playlistName);
  NS_ENSURE_SUCCESS(rv, rv);
    
  nsCString action("replace");
  if (!mImportPlaylists) {
    action = "keep";
  }
  else if (mediaList && isDirty) {   
    nsString userAction;
    rv = GetDirtyPlaylistAction(playlistName, userAction);
    NS_ENSURE_SUCCESS(rv, rv);
    action = NS_LossyConvertUTF16toASCII(userAction);
  }
  if (action.Equals("replace")) {
    mFoundChanges = PR_TRUE;
    if (mediaList) {
      nsString guid;
      rv = mediaList->GetGuid(guid);
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = mLibrary->Remove(mediaList);
      NS_ENSURE_SUCCESS(rv, rv);
      
      // Playlist is dead so no more references
      mediaList = nsnull;
      
      // Remove the old entry
      rv = miTunesDBServices.RemoveSBIDEntry(guid);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (aTrackIdsCount > 0) {
      rv = mLibrary->CreateMediaList(NS_LITERAL_STRING("simple"), 
                                     nsnull, 
                                     getter_AddRefs(mediaList));
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = mediaList->SetName(playlistName);
      NS_ENSURE_SUCCESS(rv, rv);
      
      nsString guid;
      rv = mediaList->GetGuid(guid);
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = miTunesDBServices.MapID(miTunesLibID, playlistiTunesID, guid);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = miTunesDBServices.WaitForCompletion(
               sbiTunesDatabaseServices::INFINITE_WAIT);
      NS_ENSURE_SUCCESS(rv, rv);
    
      rv = ProcessPlaylistItems(mediaList,
                                aTrackIds,
                                aTrackIdsCount);
      NS_ENSURE_SUCCESS(rv, rv);
    
      StorePlaylistSignature(mediaList);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
sbiTunesImporter::OnPlaylistsComplete() {
  /* Update status. */
  mStatus->Reset();
  char const * completionMsg;
  if (mImport) {
    completionMsg = "Library import complete";
  }
  else {
    if (mFoundChanges) {
      completionMsg = "Found library changes";
    }
    else {
      completionMsg = "No library changes found";
    }
  }
  
  if (!mBatchEnded) {
    mLDBLibrary->ForceEndUpdateBatch();
    mBatchEnded = PR_TRUE;
  }
  
  mStatus->SetStatusText(NS_ConvertASCIItoUTF16(completionMsg));
  
  mStatus->Done();
  mStatus->Update();

#ifdef SB_ENABLE_TEST_HARNESS
  if (mTimingService) {
    mTimingService->StopPerfTimer(mTimingIdentifier);
  }
#endif
  
  /* If checking for changes and changes were */
  /* found, send a library changed event.     */
  if (!mImport && mFoundChanges) {
      mListener->OnLibraryChanged(mLibraryPath, miTunesLibID);
  }
  /* If non-existent media is encountered, send an event. */
  if (mImport) {
      nsresult rv;
      sbPrefBranch prefs(SB_ITUNES_LIBRARY_IMPORT_PREF_PREFIX, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      
      // Update the previous path preference
      prefs.SetCharPref("lib_prev_path", NS_ConvertUTF16toUTF8(mLibraryPath));
      
      // Update the last modified time
      nsCOMPtr<nsILocalFile> file = 
        do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = file->InitWithPath(mLibraryPath);
      NS_ENSURE_SUCCESS(rv, rv);
      
      PRInt64 lastModified;
      file->GetLastModifiedTime(&lastModified);
      sbAutoString lastModifiedStr(static_cast<PRUint64>(lastModified));
      prefs.SetCharPref("lib_prev_mod_time",
                        NS_ConvertUTF16toUTF8(lastModifiedStr));
    if (mMissingMediaCount > 0)  {
      mListener->OnNonExistentMedia(mMissingMediaCount,
                                    mTrackCount);
    }

    /* If unsupported media is encountered, send an event. */
    if (mUnsupportedMediaCount > 0) {
      mListener->OnUnsupportedMedia();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
sbiTunesImporter::OnError(const nsAString & aErrorMessage, PRBool *_retval)
{
  LOG(("XML Parsing error: %s\n", 
      ::NS_LossyConvertUTF16toASCII(aErrorMessage).get()));
  mListener->OnImportError();
  return NS_OK;
}

struct sbiTunesImporterEnumeratePropertiesData
{
  sbiTunesImporterEnumeratePropertiesData(sbIPropertyArray * aProperties, 
                                          nsresult * rv) :
    mProperties(aProperties),
    mNeedsUpdating(PR_FALSE) {
    mChangedProperties = do_CreateInstance("@songbirdnest.com/Songbird/Properties/MutablePropertyArray;1", rv);
  }
  nsCOMPtr<sbIPropertyArray> mProperties;
  nsCOMPtr<sbIMutablePropertyArray> mChangedProperties;
  PRBool mNeedsUpdating;
};

static PLDHashOperator
EnumReadFunc(nsAString const & aKey,
             nsString aValue,
             void* aUserArg) {
  sbiTunesImporterEnumeratePropertiesData * data = 
    reinterpret_cast<sbiTunesImporterEnumeratePropertiesData *>(aUserArg);
  nsString currentValue;
  data->mProperties->GetPropertyValue(aKey, currentValue);
  if (!aValue.Equals(currentValue)) {
    data->mChangedProperties->AppendProperty(aKey, aValue);
  }
  return PL_DHASH_NEXT;
}

nsresult sbiTunesImporter::ProcessUpdates() {
  nsresult rv;
  TrackBatch::iterator const end = mTrackBatch.end();
  for (TrackBatch::iterator iter = mTrackBatch.begin();
       iter != end;
       ++iter) {
    iTunesTrack * const track = *iter;
    nsString guid;
    rv = miTunesDBServices.GetSBIDFromITID(miTunesLibID, track->mTrackID, guid);
    if (NS_SUCCEEDED(rv) && !guid.IsEmpty()) {
      nsString name;
      track->mProperties.Get(NS_LITERAL_STRING(SB_PROPERTY_TRACKNAME), &name);
      
      mTrackIDMap.insert(TrackIDMap::value_type(track->mTrackID, guid));
      track->mSBGuid = guid;
      nsCOMPtr<sbIMediaItem> mediaItem;
      rv = mLibrary->GetMediaItem(guid, getter_AddRefs(mediaItem));
      if (NS_SUCCEEDED(rv)) {
        mFoundChanges = PR_TRUE;
        *iter = nsnull;
      
        nsCOMPtr<sbIPropertyArray> properties;
        rv = mediaItem->GetProperties(nsnull, getter_AddRefs(properties));
        if (NS_SUCCEEDED(rv)) {
          sbiTunesImporterEnumeratePropertiesData data(properties, &rv);
          NS_ENSURE_SUCCESS(rv, rv);
          
          track->mProperties.EnumerateRead(EnumReadFunc, &data);
          if (data.mNeedsUpdating) {
            rv = mediaItem->SetProperties(data.mChangedProperties);
            NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), 
                             "Failed to set a property on iTunes import");
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult 
sbiTunesImporter::ProcessNewItems(
  TracksByID & aTrackMap,
  nsIArray ** aNewItems) {
  
  nsresult rv;
  
  nsCOMPtr<nsIMutableArray> uriArray = 
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMutableArray> propertyArrays = 
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIURI> uri;

  TrackBatch::iterator const begin = mTrackBatch.begin();
  TrackBatch::iterator const end = mTrackBatch.end();
  for (TrackBatch::iterator iter = begin; iter != end; ++iter) {
    // Skip if null (processed above)
    if (*iter) {
      nsString name;
      (*iter)->mProperties.Get(NS_LITERAL_STRING(SB_PROPERTY_TRACKNAME), 
                               &name);

      nsString persistentID;
      PRBool ok = 
        (*iter)->mProperties.Get(NS_LITERAL_STRING(SB_ITUNES_GUID_PROPERTY), 
                                 &persistentID);
      NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
      
      aTrackMap.insert(TracksByID::value_type(persistentID, iter - begin));

      nsCOMPtr<nsIFile> file;
      // Get the location and create a URI object and add it to the array
      rv = (*iter)->GetTrackURI(GetOSType(), 
                                mIOService,
                                miTunesLibSig, 
                                getter_AddRefs(uri));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIFileURL> trackFile = do_QueryInterface(uri, &rv);
        PRBool trackExists = PR_FALSE;
        if (NS_SUCCEEDED(rv)) {
          rv = trackFile->GetFile(getter_AddRefs(file));
          if (NS_SUCCEEDED(rv)) {
            file->Exists(&trackExists);
          }
          else {
            nsCString spec;
            uri->GetSpec(spec);
            LOG(("processTrack: File protocol error %d\n", rv));
            LOG(("%s\n", spec.BeginReading()));
          }
          if (!trackExists) {
            ++mMissingMediaCount;
          }
        }
        // Check if the track media is supported and add result to the iTunes
        // library signature.  This ensures the signature changes if support 
        // for the track media is added (e.g., by installing an extension).
        PRBool supported = PR_FALSE;
        // Ignore errors, default to not supported
        mTypeSniffer->IsValidMediaURL(uri, &supported);
        
        if (!supported) {
          ++mUnsupportedMediaCount;
        }
        nsString sig(NS_LITERAL_STRING("supported"));
        if (supported) {
          sig.AppendLiteral("true");
        }
        else {
          sig.AppendLiteral("false");
        }
        rv = miTunesLibSig.Update(sig);
        if (supported) {
          mFoundChanges = PR_TRUE;
          if (file) {
            // Add the track content length property.
            PRInt64 fileSize = 0;
            file->GetFileSize(&fileSize);
            (*iter)->mProperties.Put(NS_LITERAL_STRING(SB_PROPERTY_CONTENTLENGTH),
                                     sbAutoString(static_cast<PRUint64>(fileSize)));  
            NS_ENSURE_SUCCESS(rv, rv);
          }
          ++mTrackCount;
          rv = uriArray->AppendElement(uri, PR_FALSE);
          NS_ENSURE_SUCCESS(rv, rv);
  
          // Add the track's property array to the array
          nsCOMPtr<sbIPropertyArray> propertyArray;
          rv = (*iter)->GetPropertyArray(getter_AddRefs(propertyArray));
          NS_ENSURE_SUCCESS(rv, rv);
          
          rv = propertyArrays->AppendElement(propertyArray, PR_FALSE);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
    }
  }
  PRUint32 length;
  rv = propertyArrays->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsRefPtr<sbiTunesImporterBatchCreateListener> batchCreateListener = 
    sbiTunesImporterBatchCreateListener::New();
  if (length > 0) {
    rv = mLibrary->BatchCreateMediaItemsAsync(batchCreateListener,
                                              uriArray, 
                                              propertyArrays, 
                                              PR_FALSE);
    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));
    NS_ENSURE_SUCCESS(rv, rv);
    while (!batchCreateListener->Completed()) {
      ::NS_ProcessPendingEvents(mainThread, PR_INTERVAL_NO_TIMEOUT);
    }
    rv = batchCreateListener->GetNewMediaItems(aNewItems);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    *aNewItems = nsnull;
  }
  return NS_OK;
}

nsresult 
sbiTunesImporter::ProcessCreatedItems(
  nsIArray * aCreatedItems,
  TracksByID const & aTrackMap) {
  PRUint32 length;
  nsresult rv = aCreatedItems->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  
  TracksByID::const_iterator trackMapEnd = aTrackMap.end();
  nsCOMPtr<sbIMediaItem> mediaItem;
  nsCOMPtr<sbIAlbumArtListener> albumArtListener = 
    sbiTunesImporterAlbumArtListener::New();
  for (PRUint32 index = 0; index < length; ++index) {
    
    mediaItem = do_QueryElementAt(aCreatedItems, 
                                  index, 
                                  &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsString guid;
    rv = mediaItem->GetGuid(guid);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsString iTunesGUID;
    rv = mediaItem->GetProperty(NS_LITERAL_STRING(SB_ITUNES_GUID_PROPERTY),
                                iTunesGUID);
    TracksByID::const_iterator iter = aTrackMap.find(iTunesGUID);
    if (iter != trackMapEnd) {
      mTrackBatch[iter->second]->mSBGuid = guid;
      mTrackIDMap.insert(TrackIDMap::value_type(
                           mTrackBatch[iter->second]->mTrackID,
                           guid));
    }
    mAlbumArtFetcher->SetFetcherType(sbIAlbumArtFetcherSet::TYPE_LOCAL);
    mAlbumArtFetcher->FetchAlbumArtForTrack(mediaItem,
                                            albumArtListener);
  }
  // batchCreateMediaItemsAsync won't return media items for duplicate tracks.
  // In order to get the corresponding media item for each duplicate track,
  // createMediaItem must be called.  Thus, for each track that does not have
  // a corresponding media item, call createMediaItem.
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<sbIPropertyArray> propertyArray;
  TrackBatch::iterator const end = mTrackBatch.end();
  for (TrackBatch::iterator iter = mTrackBatch.begin();
       iter != end;
       ++iter) {
    iTunesTrack * const track = *iter;
    if (track->mSBGuid.IsEmpty()) {
      rv = track->GetTrackURI(GetOSType(), 
                              mIOService,
                              miTunesLibSig, 
                              getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = track->GetPropertyArray(getter_AddRefs(propertyArray));
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = mLibrary->CreateMediaItem(uri, 
                                     propertyArray, 
                                     PR_FALSE, 
                                     getter_AddRefs(mediaItem));
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = mediaItem->GetGuid(track->mSBGuid);
      NS_ENSURE_SUCCESS(rv, rv);      
      mTrackIDMap.insert(TrackIDMap::value_type(track->mTrackID, 
                                                track->mSBGuid));
    }
    if (!track->mSBGuid.IsEmpty()) {
      rv = miTunesDBServices.MapID(miTunesLibID,
                                   track->mTrackID,
                                   track->mSBGuid);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

inline
void sbiTunesImporter::DestructiTunesTrack(iTunesTrack * aTrack) {
  delete aTrack;
}

nsresult sbiTunesImporter::ProcessTrackBatch() {
  nsresult rv;

  rv = ProcessUpdates();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIArray> newItems;
  TracksByID trackMap;
  rv = ProcessNewItems(trackMap,
                       getter_AddRefs(newItems));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = ProcessCreatedItems(newItems, 
                           trackMap);
  NS_ENSURE_SUCCESS(rv, rv);
  
  std::for_each(mTrackBatch.begin(), mTrackBatch.end(), DestructiTunesTrack);
  mTrackBatch.clear();
  
  return NS_OK;
}

nsresult sbiTunesImporter::iTunesTrack::Initialize(sbIStringMap * aProperties) {
  nsresult rv = aProperties->Get(NS_LITERAL_STRING("Track ID"), mTrackID);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool ok = mProperties.Init(32);
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
    
  NS_NAMED_LITERAL_STRING(location, "Location");
  nsString URI;
  
  rv = aProperties->Get(location, URI);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = mProperties.Put(location, URI);
  NS_ENSURE_SUCCESS(rv, rv);
  
  for (int index = 0; index < NS_ARRAY_LENGTH(gPropertyMap); ++index) {
    PropertyMap const & propertyMapEntry = gPropertyMap[index];
    nsString value;
    rv = aProperties->Get(NS_ConvertASCIItoUTF16(propertyMapEntry.ITProperty),
                          value);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "sbIStringMap::Get failed");
    if (!value.IsVoid()) {
      if (propertyMapEntry.mConversion) {
        value = propertyMapEntry.mConversion(value);
      }
      mProperties.Put(NS_ConvertASCIItoUTF16(propertyMapEntry.SBProperty),
                      value);
    }
  }
  return NS_OK;
}

static PLDHashOperator
ConvertToPropertyArray(nsAString const & aKey,
                       nsString aValue,
                       void * aUserArg) {
  sbIMutablePropertyArray * array = 
    reinterpret_cast<sbIMutablePropertyArray*>(aUserArg);
  nsresult rv = array->AppendProperty(aKey, aValue);
  NS_ENSURE_SUCCESS(rv, ::PL_DHASH_STOP);
  return PL_DHASH_NEXT;
}

nsresult 
sbiTunesImporter::iTunesTrack::GetPropertyArray(
    sbIPropertyArray ** aPropertyArray) {
  
  nsresult rv;
  nsCOMPtr<sbIMutablePropertyArray> array = 
    do_CreateInstance("@songbirdnest.com/Songbird/Properties/MutablePropertyArray;1",
                      &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mProperties.EnumerateRead(ConvertToPropertyArray, array.get());
  nsCOMPtr<sbIPropertyArray> propertyArray = do_QueryInterface(array);
  propertyArray.forget(aPropertyArray);
  return NS_OK;
}

/** nsIRunnable implementation **/
nsresult 
sbiTunesImporter::iTunesTrack::GetTrackURI(
  sbiTunesImporter::OSType aOSType,
  nsIIOService * aIOService,
  sbiTunesSignature & aSignature,
  nsIURI ** aTrackURI) {
  NS_ENSURE_ARG_POINTER(aTrackURI);

  if (mURI) {
    *aTrackURI = mURI.get();
    NS_ADDREF(*aTrackURI);
    return NS_OK;
  }
  nsString uri16;
  if (!mProperties.Get(NS_LITERAL_STRING("Location"), &uri16)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  
  // Convert to UTF8
  nsCString uri = NS_ConvertUTF16toUTF8(uri16);
  // Need this because Find's case insentive compare version can't be reached
  nsCString uriLowerCase(uri);
  ToLowerCase(uriLowerCase);
  
  nsCString adjustedURI;
  
  if (uri[uri.Length() - 1] == '/') {
    uri.Cut(uri.Length() -1, 1);
  }
  
  if (uriLowerCase.Find("file://localhost//") == 0) {
    adjustedURI.AssignLiteral("file://///");
    uri.Cut(0, 18);
  }
  else if (uriLowerCase.Find("file://localhost/") == 0) {
    adjustedURI.AssignLiteral("file:///");
    uri.Cut(0,17);
  }
  else {
    char const c = uri[0];
    if (uri.Length() > 3 && 
        (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') &&
        uri[1] == ':' &&
        uri[2] == '/') {
      adjustedURI = "file:///";
      uri.Cut(0, 3);
    }
    else {
      adjustedURI = "file:////";
    }
  }

  adjustedURI.Append(uri);

  // Convert to lower case on Windows since it's case-insensitive.
  if (aOSType == WINDOWS_OS) {
    ToLowerCase(adjustedURI);
  }

  nsString locationSig;
  locationSig.AssignLiteral("Location");
  locationSig.AppendLiteral(adjustedURI.BeginReading());
  // Add file location to iTunes library signature.
  nsresult rv = aSignature.Update(locationSig);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Get the track URI.
  rv = aIOService->NewURI(adjustedURI, nsnull, nsnull, aTrackURI);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}
