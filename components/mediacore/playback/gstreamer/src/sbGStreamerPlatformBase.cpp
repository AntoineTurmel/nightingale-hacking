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

#include "sbGStreamerPlatformBase.h"

#include "prlog.h"
#include "nsDebug.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gGStreamerLog;
#endif /* PR_LOGGING */

#define TRACE(args) PR_LOG(gGStreamerLog, PR_LOG_DEBUG, args)
#define LOG(args)   PR_LOG(gGStreamerLog, PR_LOG_WARN, args)

BasePlatformInterface::BasePlatformInterface () : 
    sbIGstPlatformInterface(),
    mDisplayWidth(0),
    mDisplayHeight(0),
    mDisplayX(0),
    mDisplayY(0),
    mDARNum(1),
    mDARDenom(1),
    mFullscreen(false),
    mVideoSink(NULL),
    mAudioSink(NULL)
{
}

BasePlatformInterface::~BasePlatformInterface ()
{
  if (mVideoSink)
    gst_object_unref (mVideoSink);
  if (mAudioSink)
    gst_object_unref (mAudioSink);
}

bool
BasePlatformInterface::GetFullscreen ()
{
  return mFullscreen;
}

void
BasePlatformInterface::SetFullscreen (bool fullscreen)
{
  if (fullscreen && !mFullscreen) {
    mFullscreen = true;
    FullScreen();
  }
  else if (!fullscreen && mFullscreen) {
    mFullscreen = false;
    UnFullScreen();
  }
  // Otherwise, we're already in the right mode, so don't do anything.
}

void
BasePlatformInterface::Resize (int x, int y, int width, int height)
{
  if (!mFullscreen) {
    // Only resize based on our XUL element if we're not in fullscreen mode.
    SetDisplayArea (x, y, width, height);
    ResizeVideo ();
  }
}

void
BasePlatformInterface::SetDisplayArea (int x, int y, int width, int height)
{
  mDisplayX = x;
  mDisplayY = y;
  mDisplayWidth = width;
  mDisplayHeight = height;
}

void
BasePlatformInterface::ResizeVideo ()
{
  LOG(("Resizing: %d, %d, %d, %d", mDisplayX, mDisplayY, 
              mDisplayWidth, mDisplayHeight));

  // We can draw anywhere in the passed area.
  // Now, calculate the area to use that will maintain the desired 
  // display-aspect-ratio.

  // First, we see if it'll fit if we use all available vertical space.
  // If it doesn't, we use all the available horizontal space.
  int height = mDisplayHeight;
  int width = mDisplayHeight * mDARNum / mDARDenom;
  int x, y;

  if (width <= mDisplayWidth) {
    // It fits; now offset appropriately.
    x = mDisplayX + (mDisplayWidth - width)/2;
    y = mDisplayY;
  }
  else {
    // it didn't fit this way, so use the other.
    width = mDisplayWidth;
    height = mDisplayWidth * mDARDenom / mDARNum;
    x = mDisplayX;
    y = mDisplayY + (mDisplayHeight - height)/2;
  }

  MoveVideoWindow (x, y, width, height);
}

void
BasePlatformInterface::SetDisplayAspectRatio (int numerator, int denominator)
{
  mDARNum = numerator;
  mDARDenom = denominator;

  ResizeVideo();
}

void
BasePlatformInterface::PrepareVideoWindow ()
{
  GstElement *element = NULL;
  GstXOverlay *xoverlay = NULL;

  if (GST_IS_BIN (mVideoSink)) {
    /* Get the actual implementing object from the bin */
    element = gst_bin_get_by_interface (GST_BIN (mVideoSink),
            GST_TYPE_X_OVERLAY);
  }
  else {
    element = mVideoSink;
  }

  if (GST_IS_X_OVERLAY (element)) {
    xoverlay = GST_X_OVERLAY (element);
  }
  else {
    LOG(("No xoverlay interface found, cannot set video window"));
    return;
  }

  SetXOverlayWindowID(xoverlay);
}

