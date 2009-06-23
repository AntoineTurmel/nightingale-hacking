/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/**
* \file  deviceProgress.js
* \brief Javascript source for the device progress widget.
*/

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
// Device progress widget.
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
// Device progress defs.
//
//------------------------------------------------------------------------------

// Component manager defs.
if (typeof(Cc) == "undefined")
  var Cc = Components.classes;
if (typeof(Ci) == "undefined")
  var Ci = Components.interfaces;
if (typeof(Cr) == "undefined")
  var Cr = Components.results;
if (typeof(Cu) == "undefined")
  var Cu = Components.utils;

Cu.import("resource://app/jsmodules/sbProperties.jsm");
Cu.import("resource://app/jsmodules/WindowUtils.jsm");


//------------------------------------------------------------------------------
//
// Device progress configuration.
//
//------------------------------------------------------------------------------

/*
 * operationInfoList            List of device operation information records.
 *   state                      Operation state value.
 *   localeSuffix               Locale string key suffix for operation.
 *   progressMeterUndetermined  If true, display undetermined progress meter for
 *                              operation.
 *   progressMeterDetermined    If true, display determined progress meter for
 *                              operation.
 *   canBeCompleted             If true, operation can be completed and should
 *                              be set for last completed operation.
 *   showIdleMessage            If true, and this was the last completed
 *                              operation, then reflect that in the idle msg.
 *   showProgress               If true, progress should be displayed for
 *                              operation.
 *   updateIdle                 If true, update UI for an idle operation.
 *   updateBusy                 If true, update UI for a busy operation.
 */

var DPWCfg = {
  operationInfoList: [
    /* Idle. */
    {
      state: Ci.sbIDevice.STATE_IDLE,
      localeSuffix: "idle",
      updateIdle: true
    },

    /* Sync. */
    {
      state: Ci.sbIDevice.STATE_SYNCING,
      localeSuffix: "syncing",
      progressMeterUndetermined: true,
      canBeCompleted: true,
      showIdleMessage: true,
      showProgress: true,
      updateBusy: true
    },

    /* Copy. */
    {
      state: Ci.sbIDevice.STATE_COPYING,
      localeSuffix: "copying",
      progressMeterDetermined: true,
      canBeCompleted: true,
      showIdleMessage: true,
      needsMediaItem: true,
      showProgress: true,
      updateBusy: true
    },

    /* Delete. */
    {
      state: Ci.sbIDevice.STATE_DELETING,
      localeSuffix: "deleting",
      progressMeterDetermined: true,
      canBeCompleted: true,
      showIdleMessage: true,
      needsMediaItem: true,
      showProgress: true,
      updateBusy: true
    },

    /* Update. */
    {
      state: Ci.sbIDevice.STATE_UPDATING,
      localeSuffix: "updating",
      progressMeterDetermined: true,
      canBeCompleted: true,
      needsMediaItem: true,
      showProgress: true,
      updateBusy: true
    },

    /* Mount. */
    {
      state: Ci.sbIDevice.STATE_MOUNTING,
      localeSuffix: "mounting",
      progressMeterUndetermined: true,
      canBeCompleted: true,
      showProgress: true,
      updateBusy: true
    },

    /* Cancel. */
    {
      state: Ci.sbIDevice.STATE_CANCEL,
      localeSuffix: "cancelling",
      progressMeterUndetermined: true,
      canBeCompleted: true,
      showProgress: true,
      updateBusy: true
    },
    
    /* Transcode */
    {
      state: Ci.sbIDevice.STATE_TRANSCODE,
      localeSuffix: "transcoding",
      progressMeterDetermined: true,
      canBeCompleted: true,
      showProgress: true,
      updateBusy: true
    }
  ]
};


//------------------------------------------------------------------------------
//
// Device progress services.
//
//------------------------------------------------------------------------------

var DPW = {
  //
  // Device progress object fields.
  //
  //   _cfg                     Configuration.
  //   _widget                  Device info widget.
  //   _deviceID                Device ID.
  //   _curItemIndex            Current index in the batch of items to process
  //   _totalItems              Total items in the batch
  //   _operationInfoTable      Table mapping operation state values to
  //                            operation information records.
  //   _lastEventOperation      Operation of last state changed event.
  //   _lastCompletedEventOperation
  //                            Last completed operation of last state changed
  //                            event.
  //   _showProgress            If true, progress status should be displayed.
  //
  //   _progressInfoBox         Device progress info.
  //   _syncButton              Sync button element.
  //   _idleBox                 Idle message & spacer.
  //   _cancelButton            Cancel button element.
  //   _finishButton            Finish button element.
  //   _progressMeter           Progress meter element.
  //   _progressTextLabel       Progress text label element.
  //   _idleLabel               Idle text label element.
  //   _idleBox                 Idle box holding idle text.
  //

  _cfg: DPWCfg,
  _widget: null,
  _deviceID: null,
  _curItemIndex: null,
  _totalItems: null,
  _operationInfoTable: null,
  _lastEventOperation: Ci.sbIDevice.STATE_IDLE,
  _lastCompletedEventOperation: Ci.sbIDevice.STATE_IDLE,
  _showProgress: false,

  _progressInfoBox: null,
  _syncButton: null,
  _cancelButton: null,
  _finishButton: null,
  _progressMeter: null,
  _progressTextLabel: null,
  _idleLabel: null,
  _idleBox: null,


  /**
   * \brief Initialize the device progress services for the device progress
   *        widget specified by aWidget.
   *
   * \param aWidget             Device progress widget.
   */

  initialize: function DPW__initialize(aWidget) {
    // Get the device widget.
    this._widget = aWidget;

    // Initialize the operation info table.
    this._operationInfoTable = {};
    for (var i = 0; i < this._cfg.operationInfoList.length; i++) {
      var operationInfo = this._cfg.operationInfoList[i];
      this._operationInfoTable[operationInfo.state] = operationInfo;
    }

    // Get some widget elements.
    this._progressInfoBox = this._getElement("progress_information_box");
    this._syncButton   = this._getElement("sync_operation_button");
    this._cancelButton = this._getElement("cancel_operation_button");
    this._finishButton = this._getElement("finish_progress_button");
    this._progressMeter = this._getElement("progress_meter");
    this._progressTextLabel = this._getElement("progress_text_label");
    this._idleBox = this._getElement("idle_box");
    this._idleLabel = this._getElement("idle_label");

    // Initialize object fields.
    this._deviceID = this._widget.deviceID;
    this._device = this._widget.device;

    // Initialize the device services.
    this._deviceInitialize();

    // Get the device operation total items and current item index data remotes.
    var createDataRemote = new Components.Constructor(
                                    "@songbirdnest.com/Songbird/DataRemote;1",
                                    Ci.sbIDataRemote,
                                    "init");
    this._totalItems = createDataRemote(
                    this._deviceID + ".status.totalcount", null);
    this._curItemIndex = createDataRemote(
                    this._deviceID + ".status.workcount", null);

    // Simulate a device state changed event to initialize the operation state
    // and update the UI.
    this._handleStateChanged({ data: this._device.state });
  },


  /**
   * \brief Finalize the device progress services.
   */

  finalize: function DPW_finalize() {
    // Finalize the device services.
    this._deviceFinalize();

    // Clear object fields.
    this._widget = null;
    this._deviceID = null;
    this._operationInfoTable = null;
    this._progressInfoBox = null;
    this._idleBox = null;
    this._syncButton   = null;
    this._cancelButton = null;
    this._finishButton = null;
    this._progressMeter = null;
    this._progressTextLabel = null;
    this._idleLabel = null;
  },


  //----------------------------------------------------------------------------
  //
  // Device progress UI update services.
  //
  //----------------------------------------------------------------------------

  /**
   * \brief Update the device progress UI according to the current device state.
   */

  _update: function DPW__update() {
    // Get the current device state.
    var deviceState = this._device.state;

    // Dispatch progress update processing based upon the current device state.
    var deviceStateOperationInfo = this._getOperationInfo(deviceState);
    if (deviceStateOperationInfo.updateBusy)
      this._updateProgressBusy();
    else if (deviceStateOperationInfo.updateIdle)
      this._updateProgressIdle();

    // Show cancel button while busy and finish button while idle.
    var cancelButtonHidden;
    var finishButtonHidden;
    if (deviceState == Ci.sbIDevice.STATE_IDLE) {
      cancelButtonHidden = true;
      finishButtonHidden = false;
    } else {
      cancelButtonHidden = false;
      finishButtonHidden = true;
    }

    // Hide or show progress.
    var hideProgress = !this._showProgress;
    this._progressInfoBox.hidden = hideProgress;
    this._idleBox.hidden = !hideProgress;
    if (hideProgress) {
      cancelButtonHidden = true;
      finishButtonHidden = true;
    }

    // Only show sync button if not showing progress.
    this._syncButton.hidden = this._showProgress;

    // Set cancel and hide button hidden property.
    this._cancelButton.hidden = cancelButtonHidden;
    this._finishButton.hidden = finishButtonHidden;
  },


  /**
   * \brief Update the progress UI for the idle state.
   */

  _updateProgressIdle: function DPW__updateProgressIdle() {
    var oInfo = this._getOperationInfo(this._lastCompletedEventOperation);
    if (oInfo.showIdleMessage) {
      var key = "device.status.progress_complete_" + oInfo.localeSuffix;
      var deviceErrorMonitor =
          Cc["@songbirdnest.com/device/error-monitor-service;1"]
            .getService(Ci.sbIDeviceErrorMonitor);
      if (deviceErrorMonitor.deviceHasErrors(this._device))
          key += "_errors";
      this._idleLabel.value = SBFormattedString(key,
                                                [ this._totalItems.intValue ],
                                                "");
    } else {
        this._idleLabel.value = SBString("device.status.progress_complete", "");
    }

    // Clear the list of errors
    this._clearDeviceErrors();

    // Set to no longer show progress.
    this._showProgress = false;
  },


  /**
   * \brief Update the progress UI for the busy state.
   */

  _updateProgressBusy: function DPW__updateProgressBusy() {
    var localeKey;

    // Get the device status.
    var deviceStatus = this._device.currentStatus;
    var operation = deviceStatus.currentState;
    var subOperation = deviceStatus.currentSubState;
    var curItemIndex = this._curItemIndex.intValue;
    var totalItems = this._totalItems.intValue;

    // Get the operation info.
    var operationInfo = this._getOperationInfo(operation);
    var operationLocaleSuffix = operationInfo.localeSuffix;
    var operationNeedsMediaItem = operationInfo.needsMediaItem;

    // Get the sub-operation info.  If not syncing, use the main operation as
    // the sub-operation.
    if (operation != Ci.sbIDevice.STATE_SYNCING)
      subOperation = operation;
    var subOperationInfo = this._getOperationInfo(subOperation);
    var subOperationLocaleSuffix = subOperationInfo.localeSuffix;

    // If syncing, prepend the sync operation locale suffix to the sub-operation
    // locale suffix.
    if (operation == Ci.sbIDevice.STATE_SYNCING) {
      subOperationLocaleSuffix =
        operationInfo.localeSuffix + "_" + subOperationInfo.localeSuffix;
    }

    // Get the operation media item info.
    var mediaItem = deviceStatus.mediaItem;
    var mediaItemML = null;
    var itemName;
    var itemArtist;
    var itemAlbum;
    if (mediaItem) {
      try { mediaItemML = mediaItem.QueryInterface(Ci.sbIMediaList); }
      catch (ex) {}
    }
    if (mediaItemML) {
      itemName = mediaItemML.name;
    } else if (mediaItem) {
      itemName = mediaItem.getProperty(SBProperties.trackName);
      itemArtist = mediaItem.getProperty(SBProperties.artistName);
      itemAlbum = mediaItem.getProperty(SBProperties.albumName);
    }
    itemName = itemName ? itemName : "";
    itemArtist = itemArtist ? itemArtist : "";
    itemAlbum = itemAlbum ? itemAlbum : "";

    // Update the operation progress meter.
    if (operationInfo.progressMeterDetermined) {
      // Compute the progress from 0-100.
      var progress = 100;
      if ((totalItems > 0) &&
          (curItemIndex >= 0) &&
          (curItemIndex < totalItems)) {
        progress = Math.round((curItemIndex / totalItems) * 100);
      }

      // Set the progress meter to determined.
      this._progressMeter.setAttribute("mode", "determined");
      this._progressMeter.value = progress;
    } else if (operationInfo.progressMeterUndetermined) {
      this._progressMeter.setAttribute("mode", "undetermined");
      this._progressMeter.value = 0;
    } else {
      this._progressMeter.setAttribute("mode", "determined");
      this._progressMeter.value = 0;
    }

    // Update the operation progress text.
    localeKey = "device.status.progress_header_" + operationLocaleSuffix;
    this._progressTextLabel.value =
           SBFormattedString(localeKey, [ curItemIndex, totalItems ], "");

/*
    // Update the sub-operation progress text.
    if (mediaItem || !operationNeedsMediaItem) {
      localeKey = "device.status.progress_footer_" + subOperationLocaleSuffix;
      var params = [];
      params.push(itemName);
      params.push(itemArtist);
      params.push(itemAlbum);
      
      // Add some special info for transcoding...
      if (operation == Ci.sbIDevice.STATE_TRANSCODE) {
        if (deviceStatus.remainingTime == -1) {
          params.push("Unknown");
        } else {
          params.push(deviceStatus.remainingTime);
        }
      }
      this._progressText2Label.value = SBFormattedString(localeKey, params, "");
    } else {
      this._progressText2Label.value = "";
    }
*/
  },


  //----------------------------------------------------------------------------
  //
  // Device progress event handler services.
  //
  //----------------------------------------------------------------------------

  /**
   * \brief Handle the event specified by aEvent for elements with defined
   *        actions.
   *
   * \param aEvent              Event to handle.
   */

  onAction: function DPW_onAction(aEvent) {
    // Dispatch processing of action.
    switch (aEvent.target.getAttribute("action")) {
      case "finish" :
        this._finish();
      break;

      case "show_errors":
        this._displayErrors();
      break;

      default :
        break;
    }
  },


  /**
   * \brief Handle the device state changed event specified by aEvent.
   *
   * \param aEvent              Device state changed event.
   *
   *   This function tracks the sequence of device states in order to properly
   * update the UI.  Since the device state changed events are queued, the
   * current device state may not be the same as the state received in a state
   * changed event.  Without tracking all of the device state changed events,
   * some device states and operations may be missed, resulting in incorrect UI
   * updates.
   */

  _handleStateChanged: function DPW__handleStateChanged(aEvent) {
    // Update the last event operation.
    var prevLastEventOperation = this._lastEventOperation;
    this._lastEventOperation = aEvent.data;

    // Get the last and previous last event operation info.
    var lastEventOperationInfo = this._getOperationInfo
                                        (this._lastEventOperation);
    var prevLastEventOperationInfo = this._getOperationInfo
                                            (prevLastEventOperation);

    // Set to show progress if busy.
    if (lastEventOperationInfo.showProgress)
      this._showProgress = true;

    // Update the last completed event operation.
    if (prevLastEventOperationInfo.canBeCompleted)
      this._lastCompletedEventOperation = prevLastEventOperation;

    // Update the widget.
    this._update();
  },


  //----------------------------------------------------------------------------
  //
  // Device progress XUL services.
  //
  //----------------------------------------------------------------------------

  /**
   * \brief Return the XUL element with the ID specified by aElementID.  Use the
   *        element "sbid" attribute as the ID.
   *
   * \param aElementID          ID of element to get.
   *
   * \return Element.
   */

  _getElement: function DIW__getElement(aElementID) {
    return document.getAnonymousElementByAttribute(this._widget,
                                                   "sbid",
                                                   aElementID);
  },


  //----------------------------------------------------------------------------
  //
  // Internal device info services.
  //
  //----------------------------------------------------------------------------

  /**
   * \brief Cleans up the progress widget on the summary page to an IDLE state.
   */

  _finish: function DPW__finish() {
    // Clear errors.
    this._progressTextLabel.removeAttribute("error");
    try {
      var deviceErrorMonitor =
            Cc["@songbirdnest.com/device/error-monitor-service;1"]
              .getService(Ci.sbIDeviceErrorMonitor);
      deviceErrorMonitor.clearErrorsForDevice(this._device);
      this._checkForErrors(this._device);
    } catch (err) {
      Cu.reportError(err);
    }

    // Set to no longer show progress.
    this._showProgress = false;

    // Update the UI.
    this._update();
  },


  /**
   * \brief Get and return the operation information record for the device
   *        operation specified by aOperation.
   *
   * \param aOperation          Device operation.
   *
   * \return                    Device operation information record.
   */

  _getOperationInfo: function DPW__getOperationInfo(aOperation) {
    var operationInfo = this._operationInfoTable[aOperation];
    if (!operationInfo)
      operationInfo = {};
    return operationInfo;
  },


  /**
   * \brief Display errors that occured during device operations.
   */

  _displayErrors: function DPW__displayErrors() {
    try {
      var deviceErrorMonitor =
            Cc["@songbirdnest.com/device/error-monitor-service;1"]
              .getService(Ci.sbIDeviceErrorMonitor);
      if (deviceErrorMonitor.deviceHasErrors(this._device)) {
        var errorItems = deviceErrorMonitor.getErrorsForDevice(this._device);
        WindowUtils.openModalDialog
          (window,
           "chrome://songbird/content/xul/device/deviceErrorDialog.xul",
           "device_error_dialog",
           "chrome,centerscreen",
           [ "", this._device, errorItems ],
           null);

        // Clear the errors now that the user has seen them
        deviceErrorMonitor.clearErrorsForDevice(this._device);
        this._checkForErrors(this._device);
      }
    } catch (err) {
      Cu.reportError(err);
    }
  },


  //----------------------------------------------------------------------------
  //
  // Device progress device services.
  //
  //   These services provide an interface to the device object.
  //   These services register for any pertinent device notifications and call
  // _update to update the UI accordingly.  In particular, these services
  // register to receive notifications when the device state changes.
  //
  //----------------------------------------------------------------------------

  //
  // Device info services fields.
  //
  //   _device                  sbIDevice object.
  //

  _device: null,


  /**
   * \brief Initialize the device services.
   */

  _deviceInitialize: function DPW__deviceInitialize() {
    // Add a listener for status operations
    if (this._device) {
      var deviceEventTarget = this._device;
      deviceEventTarget.QueryInterface(Ci.sbIDeviceEventTarget);
      deviceEventTarget.addEventListener(this);
    }
  },

  /**
   * \brief Finalize the device services.
   */

  _deviceFinalize: function DPW__deviceFinalize() {
    // Clear object fields.
    if (this._device) {
      var deviceEventTarget = this._device;
      deviceEventTarget.QueryInterface(Ci.sbIDeviceEventTarget);
      deviceEventTarget.removeEventListener(this);
    }
    this._device = null;
  },

  /**
   * \brief Gets a pref value (string) or returns a default if not found.
   *
   * \param aPrefID id of preference to get.
   * \param aDefaultValue default value to retun in case of error.
   *
   * \return string value of preference or aDefaultValue if error occurs.
   */

  _getPrefValue : function DPW__getPrefValue(aPrefID, aDefaultValue) {
    var prefService = Cc['@mozilla.org/preferences-service;1']
                        .getService(Ci.nsIPrefBranch);
    try {
      return prefService.getCharPref(aPrefID);
    } catch (err) {
      return aDefaultValue;
    }
  },

  _checkForErrors: function DPW__checkForErrors() {
    this._progressTextLabel.removeAttribute("error");
    try {
      var deviceErrorMonitor = Cc["@songbirdnest.com/device/error-monitor-service;1"]
                                 .getService(Ci.sbIDeviceErrorMonitor);
      var hasErrors = deviceErrorMonitor.deviceHasErrors(this._device);
      this._progressTextLabel.setAttribute("error", hasErrors);
      if (hasErrors) {
        this._progressInfoBox.hidden = false;
        this._idleBox.hidden = true;
      }
    } catch (err) {
      Cu.reportError(err);
    }
  },

  _clearDeviceErrors: function DPW__clearDeviceErrors() {
    try {
      var deviceErrorMonitor = Cc["@songbirdnest.com/device/error-monitor-service;1"]
                                 .getService(Ci.sbIDeviceErrorMonitor);
      deviceErrorMonitor.clearErrorsForDevice(this._device);
    } catch (err) {
      Cu.reportError(err);
    }
  },

  /**
   * \brief Listener for device events.
   *
   * \sa sbIDeviceEvent.idl
   */

   onDeviceEvent : function DPW_onDeviceEvent(aEvent) {
    switch (aEvent.type) {
      case Ci.sbIDeviceEvent.EVENT_DEVICE_STATE_CHANGED:
        this._handleStateChanged(aEvent);
        break;

      case Ci.sbIDeviceEvent.EVENT_DEVICE_TRANSFER_START:
      case Ci.sbIDeviceEvent.EVENT_DEVICE_TRANSFER_PROGRESS:
      case Ci.sbIDeviceEvent.EVENT_DEVICE_TRANSFER_END:
      case Ci.sbIDeviceEvent.EVENT_DEVICE_MOUNTING_START:
      case Ci.sbIDeviceEvent.EVENT_DEVICE_MOUNTING_PROGRESS:
      case Ci.sbIDeviceEvent.EVENT_DEVICE_MOUNTING_END:
      case Ci.sbIDeviceEvent.EVENT_DEVICE_PREFS_CHANGED:
      case Ci.sbIDeviceEvent.EVENT_DEVICE_TRANSCODE_START:
      case Ci.sbIDeviceEvent.EVENT_DEVICE_TRANSCODE_PROGRESS:
      case Ci.sbIDeviceEvent.EVENT_DEVICE_TRANSCODE_END:
        this._update();
      break;
    }
  },


  /**
   * \brief Return the device status data remote prefix.
   *
   * \return Device status data remote prefix.
   */

  _deviceGetStatusDRPrefix: function DPW__deviceGetStatusDRPrefix() {
    return "device." + this._deviceID + ".status";
  },


  /**
   * \brief Return true if device is idle.
   *
   * \return True if device is idle.
   */

  _deviceIsIdle: function DPW__deviceIsIdle() {
    return this._isIdle;
  },

  /**
   * \brief Return true if device is syncing
   */
  _deviceIsSyncing: function DPW__deviceIsSyncing() {
    return this._isSyncing;
  }
};


