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

#include "sbiTunesAgentMacProcessor.h"

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>
#import <CoreFoundation/CoreFoundation.h>
#include <sys/param.h>

#define STRINGIT2(arg) #arg
#define STRINGIT(arg) STRINGIT2(arg)

// Agent constants, these should be unified soon.
#define AGENT_EXPORT_FILENAME    "songbird_export.task"
#define AGENT_ERROR_FILENAME     "itunesexporterrors.txt"
#define AGENT_LOG_FILENAME       "itunesexport.log"
#define AGENT_SHUTDOWN_FILENAME  "songbird_export.shutdown"

#define AGENT_ITUNES_SLEEP_INTERVAL 5000 


//------------------------------------------------------------------------------
// Misc utility methods

static NSString *gSongbirdProfilePath = nil;

static NSString * 
GetSongbirdProfilePath()
{
  if (!gSongbirdProfilePath) {
    NSMutableString *profilePath = [[NSMutableString alloc] init];
    
    FSRef appSupportFolderRef;
    OSErr err = ::FSFindFolder(kUserDomain,
                               kApplicationSupportFolderType,
                               kCreateFolder,
                               &appSupportFolderRef);
    if (err != noErr) {
      return profilePath;
    }

    // Convert to a CFURL to get the file path
    CFURLRef folderUrlRef;
    folderUrlRef = CFURLCreateFromFSRef(kCFAllocatorDefault, 
                                        &appSupportFolderRef);
    if (!folderUrlRef) {
      return profilePath;
    }

    [profilePath appendFormat:@"%@/%s/", 
      [(NSURL *)folderUrlRef path],
      STRINGIT(SB_APPNAME) STRINGIT(SB_PROFILE_VERSION)];
    
    CFRelease(folderUrlRef);
    gSongbirdProfilePath = [[NSString alloc] initWithString:profilePath];
  }

  return gSongbirdProfilePath;
}

//------------------------------------------------------------------------------

sbiTunesAgentProcessor* sbCreatesbiTunesAgentProcessor()
{
  return new sbiTunesAgentMacProcessor();
}

//------------------------------------------------------------------------------

sbiTunesAgentMacProcessor::sbiTunesAgentMacProcessor()
{
}

sbiTunesAgentMacProcessor::~sbiTunesAgentMacProcessor()
{
}

std::string
sbiTunesAgentMacProcessor::GetNextTaskfilePath()
{
  std::string taskFilePath;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  BOOL found = NO;
  NSString *curFilename = nil;
  NSString *exportFilename = 
    [NSString stringWithUTF8String:AGENT_EXPORT_FILENAME];
  NSString *songbirdProfilePath = GetSongbirdProfilePath();

  NSDirectoryEnumerator *dirEnum =
    [[NSFileManager defaultManager] enumeratorAtPath:GetSongbirdProfilePath()];
  
  while ((curFilename = [dirEnum nextObject])) {
    if ([curFilename rangeOfString:exportFilename].location != NSNotFound) {
      found = YES;
      break;
    }
  }

  if (found) {
    taskFilePath = [songbirdProfilePath UTF8String];
    taskFilePath.append([curFilename UTF8String]);
  }

  [pool release];
  return taskFilePath;
}

bool
sbiTunesAgentMacProcessor::GetIsItunesRunning()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  
  bool isRunning = false;
  NSDictionary *curAppDict = nil;
  NSEnumerator *openAppsEnum = 
    [[[NSWorkspace sharedWorkspace] launchedApplications] objectEnumerator];
  while ((curAppDict = [openAppsEnum nextObject])) {
    NSString *curAppPath = 
      [curAppDict objectForKey:@"NSApplicationBundleIdentifier"];

    if ([curAppPath isEqualToString:@"com.apple.iTunes"]) {
      isRunning = true;
      break;
    }
  }

  [pool release];
  return isRunning;
}

//------------------------------------------------------------------------------
// sbiTunesAgentProcessor

bool
sbiTunesAgentMacProcessor::TaskFileExists()
{
  std::string nextTaskFilepath = GetNextTaskfilePath();
  return !nextTaskFilepath.empty();
}

void
sbiTunesAgentMacProcessor::RemoveTaskFile()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  // Remove the current agent task file.
  NSString *currentTaskPath = 
    [NSString stringWithUTF8String:mCurrentTaskFile.c_str()];

  [[NSFileManager defaultManager] removeFileAtPath:currentTaskPath
                                             handler:nil];
  [pool release];
}

sbError
sbiTunesAgentMacProcessor::WaitForiTunes()
{
  while (!GetIsItunesRunning()) {
    // This method needs to block for now.
    Sleep(AGENT_ITUNES_SLEEP_INTERVAL);
  }
  
  return sbNoError;
}

bool
sbiTunesAgentMacProcessor::ErrorHandler(sbError const & aError)
{
  // todo write me!
  return false;
}

sbError
sbiTunesAgentMacProcessor::RegisterForStartOnLogin()
{
  // todo write me!
  // bug 16115
  return sbNoError;  
}

sbError
sbiTunesAgentMacProcessor::UnregisterForStartOnLogin()
{
  // todo write me!
  // bug 16115
  return sbNoError;
}

sbError
sbiTunesAgentMacProcessor::AddTracks(std::string const & aSource,
                                     Tracks const & aPaths)
{
  // todo write me!
  // bug 16117
  return sbNoError;
}

sbError
sbiTunesAgentMacProcessor::RemovePlaylist(std::string const & aPlaylistName)
{
  // todo write me!
  // bug 16121
  return sbNoError;
}

sbError
sbiTunesAgentMacProcessor::CreatePlaylist(std::string const & aPlaylistName)
{
  // todo write me!
  // bug 16119
  return sbNoError;
}

bool
sbiTunesAgentMacProcessor::OpenTaskFile(std::ifstream & aStream)
{
  std::string taskFilePath(GetNextTaskfilePath());
  if (taskFilePath.empty()) {
    return false;
  }

  mCurrentTaskFile = taskFilePath;
  aStream.open(mCurrentTaskFile.c_str());
  return true;
}

void
sbiTunesAgentMacProcessor::Log(std::string const & aMsg)
{
  if (mLogState != DEACTIVATED) {
    if (mLogState != OPENED) {
      std::string logPath([GetSongbirdProfilePath UTF8String]);
      logPath += AGENT_LOG_FILENAME;
      mLogStream.open(logPath.c_str());
      // If we can't open then don't bother trying again
      mLogState = mLogStream ? OPENED : DEACTIVATED;
    }
    mLogStream << aMsg << std::endl;
  }
}

bool
sbiTunesAgentMacProcessor::Shutdown()
{
  // No cleanup needed just yet.
  return true;
}

void
sbiTunesAgentMacProcessor::Sleep(unsigned long aMilliseconds)
{
  // usleep() takes microseconds (x1000)
  usleep(aMilliseconds * 1000);
}

void
sbiTunesAgentMacProcessor::ShutdownDone()
{
  // No cleanup needed just yet.
}
