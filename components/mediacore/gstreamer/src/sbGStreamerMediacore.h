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

#ifndef __SB_GSTREAMERMEDIACORE_H__
#define __SB_GSTREAMERMEDIACORE_H__

#include <sbIGStreamerMediacore.h>

#include <nsIDOMEventListener.h>
#include <nsIDOMXULElement.h>
#include <nsIObserver.h>
#include <nsIPrefBranch2.h>

#include <nsAutoPtr.h>
#include <nsCOMPtr.h>

#include <sbIMediacore.h>
#include <sbIMediacorePlaybackControl.h>
#include <sbIMediacoreVideoWindow.h>
#include <sbIMediacoreVolumeControl.h>
#include <sbIMediacoreVotingParticipant.h>
#include <sbIMediacoreEventTarget.h>
#include <sbIMediacoreError.h>

#include <sbIPropertyArray.h>

#include <sbBaseMediacore.h>
#include <sbBaseMediacorePlaybackControl.h>
#include <sbBaseMediacoreVolumeControl.h>
#include <sbBaseMediacoreEventTarget.h>

#include <gst/gst.h>
#include "sbIGstPlatformInterface.h"
#include "sbIGstAudioFilter.h"

#include <vector>

class nsIURI;

class sbGStreamerMediacore : public sbBaseMediacore,
                             public sbBaseMediacorePlaybackControl,
                             public sbBaseMediacoreVolumeControl,
                             public sbIMediacoreVotingParticipant,
                             public sbIGStreamerMediacore,
                             public sbIMediacoreEventTarget,
                             public sbIMediacoreVideoWindow,
                             public nsIDOMEventListener,
                             public nsIObserver,
                             public sbIGstAudioFilter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLASSINFO
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_SBIMEDIACOREEVENTTARGET
  NS_DECL_SBIMEDIACOREVOTINGPARTICIPANT
  NS_DECL_SBIMEDIACOREVIDEOWINDOW
  NS_DECL_SBIGSTREAMERMEDIACORE

  sbGStreamerMediacore();

  nsresult Init();

  // sbBaseMediacore overrides
  virtual nsresult OnInitBaseMediacore();
  virtual nsresult OnGetCapabilities();
  virtual nsresult OnShutdown();

  // sbBaseMediacorePlaybackControl overrides
  virtual nsresult OnInitBaseMediacorePlaybackControl();
  virtual nsresult OnSetUri(nsIURI *aURI);
  virtual nsresult OnGetDuration(PRUint64 *aDuration);
  virtual nsresult OnGetPosition(PRUint64 *aPosition);
  virtual nsresult OnSetPosition(PRUint64 aPosition);
  virtual nsresult OnPlay();
  virtual nsresult OnPause();
  virtual nsresult OnStop();

  // sbBaseMediacoreVolumeControl overrides
  virtual nsresult OnInitBaseMediacoreVolumeControl();
  virtual nsresult OnSetMute(PRBool aMute);
  virtual nsresult OnSetVolume(PRFloat64 aVolume);

  // GStreamer message handling
  virtual void HandleMessage(GstMessage *message);
  virtual PRBool HandleSynchronousMessage(GstMessage *message);

  // sbIGstAudioFilter interface
  virtual nsresult AddAudioFilter(GstElement *aElement);
  virtual nsresult RemoveAudioFilter(GstElement *aElement);

protected:
  virtual ~sbGStreamerMediacore();

  nsresult DestroyPipeline();
  nsresult CreatePlaybackPipeline();

  void DispatchMediacoreEvent (unsigned long type, 
          nsIVariant *aData = NULL, sbIMediacoreError *aError = NULL);

  void HandleAboutToFinishSignal ();
  void HandleTagMessage (GstMessage *message);
  void HandleStateChangedMessage (GstMessage *message);
  void HandleEOSMessage (GstMessage *message);
  void HandleErrorMessage (GstMessage *message);
  void HandleWarningMessage (GstMessage *message);
  void HandleBufferingMessage (GstMessage *message);
  void HandleRedirectMessage (GstMessage *message);
  void OnVideoCapsSet(GstCaps *caps);

  nsresult LogMessageToErrorConsole(nsString message, PRUint32 flags);

  GstElement *CreateSinkFromPrefs(const char *aSinkDescription);
  GstElement *CreateVideoSink();
  GstElement *CreateAudioSink();

  nsresult InitPreferences();
  nsresult ReadPreferences();
  nsresult SetBufferingProperties(GstElement *aPipeline);
  nsresult SendInitialBufferingEvent();

  nsresult GetFileSize(nsIURI *aURI, PRInt64 *aFileSize);

  void AbortAndRestartPlayback();

private:
  // Static helper for C callback
  static GstBusSyncReply syncHandler(GstBus *bus, GstMessage *message, 
          gpointer data);
  static void aboutToFinishHandler(GstElement *playbin, gpointer data);
  static void videoCapsSetHelper(GObject *obj, GParamSpec *pspec, 
          sbGStreamerMediacore *core);
  static void currentVideoSetHelper(GObject *obj, GParamSpec *pspec, 
          sbGStreamerMediacore *core);

protected:
  // Protects all access to mPipeline
  PRMonitor*  mMonitor;

  PRBool mHaveVideoWindow; // true if we have a video window and a 
                           // platform-integration backend that can use it 
                           // (preconditions for video to be enabled)

  GstElement *mPipeline;
  nsAutoPtr<sbIGstPlatformInterface> mPlatformInterface;

  nsAutoPtr<sbBaseMediacoreEventTarget> mBaseEventTarget;

  nsCOMPtr<nsIPrefBranch2> mPrefs;

  std::vector<GstElement*> mAudioFilters;

  GstElement *mReplaygainElement;

  // Metadata, both in original GstTagList form, and transformed into an
  // sbIPropertyArray. Both may be NULL.
  GstTagList *mTags;
  nsCOMPtr<sbIPropertyArray> mProperties;

  // Distinguish between being stopped, and stopping due to reaching the end.
  PRBool mStopped;
  // Track whether we're currently buffering
  PRBool mBuffering;
  // Track if we are using a live pipeline
  PRBool mIsLive;

  // If we've seen an error for the current mediacore, suppress further
  // error messages.
  PRBool mHasSeenError;

  // The GstState we want to reach (not necessarily the current pipeline
  // pending state)
  GstState mTargetState;

  // the video box
  nsCOMPtr<nsIDOMXULElement> mVideoWindow;
  nsCOMPtr<nsIDOMWindow> mDOMWindow;

  // Various things read from preferences
  PRBool mVideoDisabled; // Whether video is disabled via prefs

  nsCString mVideoSinkDescription;
  nsCString mAudioSinkDescription;

  PRInt64 mAudioSinkBufferTime; // Audio sink buffer time in usecs
  PRInt32 mStreamingBufferSize; // Streaming buffer max size in bytes

  PRBool mResourceIsLocal;   // True if the current resource is a local file.
  PRInt64 mResourceSize;     // Size of current playing file, or -1 if unknown.

  PRBool mGaplessDisabled;   // If true, gapless playback is disabled for the
                             // currently-in-use pipeline. Recreating the 
                             // pipeline will re-enable gapless.
  PRBool mPlayingGaplessly;  // Gapless playback is currently happening - we're
                             // on a second or subsequent file in a gapless
                             // sequence.
 
  PRBool mAbortingPlayback;  // Playback is being aborted (not normally 
                             // stopped), and bus messages should not be 
                             // processed.

  nsCString mCurrentUri;     // UTF-8 String form (as used by GStreamer) of 
                             // the currently-playing URI.
};

#endif /* __SB_GSTREAMERMEDIACORE_H__ */
