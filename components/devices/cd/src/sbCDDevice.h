/* vim: set sw=2 :miv */
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

#ifndef SBCDDEVICE_H_
#define SBCDDEVICE_H_

#include <sbAutoRWLock.h>
#include <sbBaseDevice.h>
#include <sbDeviceCapabilities.h>
#include <sbDeviceStatusHelper.h>
#include <sbIDeviceProperties.h>
#include <sbITranscodeManager.h>
#include <sbMemoryUtils.h>

#include <nsID.h>
#include <nsIRunnable.h>
#include <nsIThread.h>

class sbIDeviceContent;
class nsIPropertyBag;

//------------------------------------------------------------------------------
//
// Thread support strategy:
//
//   The CD device base services provide support for multi-threaded access
// using a combination of locks and request queues.  Certain CD device object
// fields may only be accessed from the request thread.
//   The CD device base object fields are divided into different categories
// depending upon their access policy.
//
//   Fields within the "not locked" category are created during initialization
// and may be accessed at any time without acquiring a lock.
//
//   Fields within the "connect lock" category must only be accessed within the
// connect lock, mConnectLock.  All of these fields except mConnected are only
// usable when the device is connected.  These fields are initialized during
// device connection and finalized when the device is disconnected.
//   The connect lock is a read/write lock.  Only the Connect and Disconnect
// methods acquire a write lock on the connect lock.  All other methods acquire
// read locks.
//   In order to access the "connect lock" fields, a method must first acquire a
// read lock on the connect lock and check the mConnected field to see if the
// device is connected.  If the device is not connected, the method must release
// the connect lock and not access any other "connect lock" field.  If the
// device is connected, the method may access any "connect lock" field as long
// as it hold the connect lock.
//   Since the connect lock is a read/write lock, acquiring a read lock does not
// lock out other threads from acquiring a read lock.  This allows the request
// thread to maintain a long lived read lock on the connect lock without locking
// out other threads; it only locks out device disconnect.
//   The "connect lock" only ensures that the "connect lock" fields won't be
// finalized from a disconnect.  Some "connect lock" fields may need to be
// protected with another lock to ensure thread safety.
//
//   Fields within the "Request Thread" category may only be accessed from the
// request thread.
//
//   Fields within the "properties lock" category must only be accessed within
// the properties lock.  While the properties object itself may be thread-safe,
// code that reads and conditionally writes properties requires locks (e.g.,
// property udpates).
//
// Connect lock
//
//   mConnected
//   mMountPath
//   mMountURI
//   mReqAddedEvent
//   mReqThread
//   mReqWaitMonitor
//   mDeviceLibrary
//   mCapabilities
//
// Request Thread
//
//   mStatus
//   mTranscodeManager
//
// Properties lock
//   mProperties
//   mPrevFriendlyNameInitialized;
//   mPrevFriendlyName;
//   mPrevFriendlyNameExists;
//
// Not locked
//
//   mConnectLock
//   mDeviceID
//   mControllerID
//   mCreationProperties
//   mAbortRequests
//   mIsHandlingRequests
//   mDeviceContent
//   mDeviceSettingsLock
//   mPropertiesLock
//
//------------------------------------------------------------------------------

/**
 * This class provides the core CD device implementation
 * Each instance represents a physical device.
 */
class sbCDDevice : public sbBaseDevice,
                   public nsIClassInfo
{
public:
  //
  // Implemented interfaces.
  //

  NS_DECL_ISUPPORTS
  NS_DECL_SBIDEVICE
  NS_DECL_NSICLASSINFO

  /**
   * Initializes the default values for the device object
   */
  sbCDDevice();

  /**
   * Performs any final cleanup.
   */
  virtual ~sbCDDevice();

  /**
   * Initializes the device object for a specific device
   */
  nsresult InitDevice();

private:
  /**
   * Protects the mProperites member and updating it's contents
   */
  PRMonitor* mPropertiesLock;

  /**
   * Provides a reader/writer lock around the connection
   * During connection/disconnection a write lock is held. General access is
   * to protected properties is done via a read lock.
   */
  PRRWLock* mConnectLock;

  /**
   * Abort request indicator
   */
  PRInt32 mAbortRequests;

  /**
   * Transcode manager used to rip the tracks
   */
  nsCOMPtr<sbITranscodeManager> mTranscodeManager;


  /**
   * The Device ID
   */
  nsID mDeviceID;

  /**
   * ID of the controller that created us
   */
  nsID mControllerID;

  /**
   * Indicates whether we're connected or not
   */
  PRBool mConnected;

  /**
   * The path to the CD drive
   */
  nsString mMountPath;


  /**
   * Our device library
   */
  nsCOMPtr<sbIDeviceLibrary> mDeviceLibrary;

  /**
   * Holds the device capabilities
   */
  nsCOMPtr<sbIDeviceCapabilities> mCapabilities;

  /**
   * Creation properties for the device
   */
  nsCOMPtr<nsIPropertyBag> mCreationProperties;

  /**
   * The device properties
   */
  nsCOMPtr<sbIDeviceProperties> mProperties;

  /**
   * The device content object
   */
  nsCOMPtr<sbIDeviceContent> mDeviceContent;

  /**
   * The device status helper
   */
  sbDeviceStatusHelper mStatus;


  /**
   * Used to trigger request processing
   */
  nsCOMPtr<nsIRunnable> mReqAddedEvent;

  /**
   * The request handler thread
   */
  nsCOMPtr<nsIThread> mReqThread;

  /**
   * Used to signal an abort during transcoding
   */
  PRMonitor* mReqWaitMonitor;


  /**
   * Rebuilds the capabilities object mCapabilities
   */
  nsresult CapabilitiesReset();

  /**
   * Creates the device ID for the device
   */
  nsresult CreateDeviceID(nsID & aDeviceID);

  /**
   * Initialize the request queue connection
   */
  nsresult ReqConnect();

  /**
   * Stops processing of requests
   */
  nsresult ReqProcessingStop();

  /**
   * Tears down the request queue
   */
  nsresult ReqDisconnect();

  /**
   * Processes a request. This is called when there are requests
   * pending.
   */
  nsresult ProcessRequest();
};

#define SB_CD_DEVICE_AUTO_INVOKE(aName, aMethod)                              \
  SB_AUTO_NULL_CLASS(aName, sbCDDevice*, mValue->aMethod)

#endif /* SBCDDEVICE_H_ */