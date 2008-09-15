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

#ifndef _SB_GSTREAMER_PLATFORM_INTERFACE_H_
#define _SB_GSTREAMER_PLATFORM_INTERFACE_H_

#include <gst/gst.h>

/* Internal interface to platform-specific aspects.
 * TODO: Currently this only handles the video windows, we'll probably need
 * to add more bits later.
 */
class sbIGstPlatformInterface
{
public:
  // Resize the available video window to the current size/location of the
  // video box object.
  // The actual video will often be smaller, in order to maintain the display
  // aspect ratio.
  virtual void ResizeToWindow () = 0;

  // Get the current fullscreen status (true for fullscreen, false for windowed)
  virtual bool GetFullscreen () = 0;

  // Set to fullscreen mode (if fullscreen is true) or windowed mode (if it is
  // false). If already in this mode, nothing will change.
  virtual void SetFullscreen (bool fullscreen)  = 0;

  // Set the desired Display Aspect Ratio (DAR). This interface is provided
  // to allow overriding the aspect ratio (e.g. to display 4:3 content 
  // fullscreen on a 16:9 monitor with no black bars).
  virtual void SetDisplayAspectRatio (int numerator, int denominator) = 0;

  // Create a GStreamer video sink element appropriate for this platform.
  virtual GstElement * CreateVideoSink () = 0;

  // Create a GStreamer audio sink element appropriate for this platform.
  virtual GstElement * CreateAudioSink () = 0;

  // Called when a video window is required by the gstreamer element, so
  // that any necessary setup can be done.
  virtual void PrepareVideoWindow() = 0;
};

#endif // _SB_GSTREAMER_PLATFORM_INTERFACE_H_
