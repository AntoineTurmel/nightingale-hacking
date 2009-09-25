/*
 *=BEGIN SONGBIRD GPL
 *
 * This file is part of the Songbird web player.
 *
 * Copyright(c) 2005-2009 POTI, Inc.
 * http://www.songbirdnest.com
 *
 * This file may be licensed under the terms of of the
 * GNU General Public License Version 2 (the ``GPL'').
 *
 * Software distributed under the License is distributed
 * on an ``AS IS'' basis, WITHOUT WARRANTY OF ANY KIND, either
 * express or implied. See the GPL for the specific language
 * governing rights and limitations.
 *
 * You should have received a copy of the GPL along with this
 * program. If not, go to http://www.gnu.org/licenses/gpl.html
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *=END SONGBIRD GPL
 */

if (typeof(Cc) == "undefined")
  var Cc = Components.classes;
if (typeof(Ci) == "undefined")
  var Ci = Components.interfaces;
if (typeof(Cr) == "undefined")
  var Cr = Components.results;
if (typeof(Cu) == "undefined")
  var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://app/jsmodules/ArrayConverter.jsm");
Cu.import("resource://app/jsmodules/DOMUtils.jsm");
Cu.import("resource://app/jsmodules/PlatformUtils.jsm");

const CDRIPNS = 'http://songbirdnest.com/rdf/servicepane/cdrip#';
const SPNS = 'http://songbirdnest.com/rdf/servicepane#';

var sbCDRipServicePaneServiceConfig = {
  className:      "Songbird CD Rip Device Support Module",
  cid:            Components.ID("{9925b565-5c19-4feb-87a8-413d86570cd9}"),
  contractID:     "@songbirdnest.com/servicepane/cdDevice;1",
  
  ifList: [ Ci.sbIServicePaneModule,
            Ci.nsIObserver ],
            
  categoryList:
  [
    {
      category: "service-pane",
      entry: "cdrip-device"
    }
  ],

  devCatName:     "CD Rip Device",

  appQuitTopic:   "quit-application",

  devMgrURL:      "chrome://songbird/content/mediapages/cdripMediaView.xul"
};
if ("sbIWindowsAutoPlayActionHandler" in Ci) {
  sbCDRipServicePaneServiceConfig.ifList.push(Ci.sbIWindowsAutoPlayActionHandler);
}

function sbCDRipServicePaneService() {

}

sbCDRipServicePaneService.prototype.constructor = sbCDRipServicePaneService;

sbCDRipServicePaneService.prototype = {
  classDescription: sbCDRipServicePaneServiceConfig.className,
  classID: sbCDRipServicePaneServiceConfig.cid,
  contractID: sbCDRipServicePaneServiceConfig.contractID,

  _cfg: sbCDRipServicePaneServiceConfig,
  _xpcom_categories: sbCDRipServicePaneServiceConfig.categoryList,

  // Services to use.
  _deviceManagerSvc:      null,
  _deviceServicePaneSvc:  null,
  _observerSvc:           null,
  _servicePaneSvc:        null,

  _deviceInfoList:        [],
  
  // ************************************
  // sbIServicePaneService implementation
  // ************************************
  servicePaneInit: function sbCDRipServicePaneService_servicePaneInit(aServicePaneService) {
    this._servicePaneSvc = aServicePaneService;
    this._initialize();
  },

  fillContextMenu: function sbCDRipServicePaneService_fillContextMenu(aNode,
                                                                  aContextMenu,
                                                                  aParentWindow) {
    // Get the node device ID.  Do nothing if not a device node.
    var deviceID = aNode.getAttributeNS(CDRIPNS, "DeviceId");
    if (!deviceID)
      return;
  
    // Get the device node type.
    var deviceNodeType = aNode.getAttributeNS(CDRIPNS, "deviceNodeType");

    // Import device context menu items into the context menu.
    if (deviceNodeType == "cd-device") {
      DOMUtils.importChildElements(aContextMenu,
                                   this._deviceContextMenuDoc,
                                   "cddevice_context_menu_items",
                                   { "device-id": deviceID,
                                     "service_pane_node_id": aNode.id });
    }
  },

  fillNewItemMenu: function sbCDRipServicePaneService_fillNewItemMenu(aNode,
                                                                  aContextMenu,
                                                                  aParentWindow) {
  },

  onSelectionChanged: function sbCDRipServicePaneService_onSelectionChanged(aNode,
                                                          aContainer,
                                                          aParentWindow) {
  },

  canDrop: function sbCDRipServicePaneService_canDrop(aNode, 
                                                  aDragSession, 
                                                  aOrientation, 
                                                  aWindow) {
    // Currently no drag and drop allowed
    return false;
  },

  onDrop: function sbCDRipServicePaneService_onDrop(aNode, 
                                                aDragSession, 
                                                aOrientation, 
                                                aWindow) {
    // Currently no drag and drop allowed
  },
  
  onDragGesture: function sbCDRipServicePaneService_onDragGesture(aNode, 
                                                              aTransferable) {
    // Currently no drag and drop allowed
  },

  onRename: function sbCDRipServicePaneService_onRename(aNode, 
                                                    aNewName) {
    // Rename is not allowed for CD Devices
  },

  shutdown: function sbCDRipServicePaneService_shutdown() {
    // Do nothing, since we shut down on quit-application
  },

  // ************************************
  // nsIObserver implementation
  // ************************************
  observe: function sbCDRipServicePaneService_observe(aSubject, 
                                                  aTopic, 
                                                  aData) {
    switch (aTopic) {
      case this._cfg.appQuitTopic :
        this._shutdown();
      break;
    }

  },
  
  // ************************************
  // nsISupports implementation
  // ************************************
  QueryInterface: XPCOMUtils.generateQI(sbCDRipServicePaneServiceConfig.ifList),

  // ************************************
  // Internal methods
  // ************************************
  
  /**
   * \brief Initialize the CD Device nodes.
   */
  _initialize: function sbCDRipServicePaneService_initialize() {
    this._observerSvc = Cc["@mozilla.org/observer-service;1"]
                          .getService(Ci.nsIObserverService);
    
    this._observerSvc.addObserver(this, this._cfg.appQuitTopic, false);

    this._deviceServicePaneSvc = Cc["@songbirdnest.com/servicepane/device;1"]
                                   .getService(Ci.sbIDeviceServicePaneService);

    this._deviceManagerSvc = Cc["@songbirdnest.com/Songbird/DeviceManager;2"]
                               .getService(Ci.sbIDeviceManager2);
 
    // Remove all stale nodes.
    this._removeDeviceNodes(this._servicePaneSvc.root);
 
    // Add a listener for CDDevice Events
    var deviceEventListener = {
      cdDeviceServicePaneSvc: this,
      
      onDeviceEvent: function deviceEventListener_onDeviceEvent(aDeviceEvent) {
        this.cdDeviceServicePaneSvc._processDeviceManagerEvent(aDeviceEvent);
      }
    };
    
    this._deviceEventListener = deviceEventListener;
    this._deviceManagerSvc.addEventListener(deviceEventListener);
    
    this._createConnectedDevices();

    // Initialize the device info list.
    var deviceEnum = this._deviceManagerSvc.devices.enumerate();
    while (deviceEnum.hasMoreElements()) {
      this._addDevice(deviceEnum.getNext().QueryInterface(Ci.sbIDevice));
    }

    // load the cd-device context menu document
    this._deviceContextMenuDoc =
          DOMUtils.loadDocument
            ("chrome://songbird/content/xul/device/deviceContextMenu.xul");

    if (PlatformUtils.platformString == "Windows_NT") {
      // Register autoplay handler
      var autoPlayActionHandler = {
        cdDeviceServicePaneSvc: this,

        handleAction: function autoPlayActionHandler_handleAction(aAction, aActionArg) {
          return this.cdDeviceServicePaneSvc._processAutoPlayAction(aAction);
        },

        QueryInterface: XPCOMUtils.generateQI([Ci.sbIWindowsAutoPlayActionHandler])
      }

      this._autoPlayActionHandler = autoPlayActionHandler;
      var windowsAutoPlayService =
            Cc["@songbirdnest.com/Songbird/WindowsAutoPlayService;1"]
              .getService(Ci.sbIWindowsAutoPlayService);
      windowsAutoPlayService.addActionHandler
        (autoPlayActionHandler,
         Ci.sbIWindowsAutoPlayService.ACTION_CD_RIP);
    }
  },
  
  /**
   * \brief Shutdown and remove the CD Device nodes.
   */
  _shutdown: function sbCDRipServicePaneService_shutdown() {
    this._observerSvc.removeObserver(this, this._cfg.appQuitTopic);

    this._deviceManagerSvc.removeEventListener(this._deviceEventListener);
    this._deviceEventListener = null;
    
    if (PlatformUtils.platformString == "Windows_NT") {
      // Unregister autoplay handler
      var windowsAutoPlayService =
            Cc["@songbirdnest.com/Songbird/WindowsAutoPlayService;1"]
              .getService(Ci.sbIWindowsAutoPlayService);
      windowsAutoPlayService.removeActionHandler
        (this._autoPlayActionHandler,
         Ci.sbIWindowsAutoPlayService.ACTION_CD_RIP);
      this._autoPlayActionHandler = null;
    }

    // Purge all device nodes before shutdown.
    this._removeDeviceNodes(this._servicePaneSvc.root);
    
    // Remove all references to nodes
    this._removeAllDevices();
    this._deviceInfoList = [];

    this._deviceManagerSvc = null;
    this._deviceServicePaneSvc = null;
    this._servicePaneSvc = null;
    this._observerSvc = null;
  },

  /**
   * \brief Process events from the Device Manager.
   * \param aDeviceEvent - Event that occured for a device.
   */
  _processDeviceManagerEvent:
    function sbCDRipServicePaneService_processDeviceManagerEvent(aDeviceEvent) {

    switch(aDeviceEvent.type) {
      case Ci.sbIDeviceEvent.EVENT_DEVICE_ADDED:
        var result = this._addDeviceFromEvent(aDeviceEvent);

        // if we successfully added the device, switch the media tab
        // to the CD rip view
        if (result)
          this._loadCDViewFromEvent(aDeviceEvent);
        break;

      case Ci.sbIDeviceEvent.EVENT_DEVICE_REMOVED:
        this._removeDeviceFromEvent(aDeviceEvent);
        break;

      case Ci.sbICDDeviceEvent.EVENT_CDLOOKUP_INITIATED:
        this._updateState(aDeviceEvent, true);
        break;
      
      case Ci.sbICDDeviceEvent.EVENT_CDLOOKUP_COMPLETED:
        this._updateState(aDeviceEvent, false);
        break;

      case Ci.sbIDeviceEvent.EVENT_DEVICE_STATE_CHANGED:
        this._updateState(aDeviceEvent, false);
        break;
    
      default:
        break;
    }
  },

  /**
   * \brief Handles autoplay actions initiated by the user.
   * \param aAction - Action to be handled.
   */
  _processAutoPlayAction:
      function sbCDRipServicePaneService_processAutoPlayAction(aAction) {

    switch (aAction) {
      case Ci.sbIWindowsAutoPlayService.ACTION_CD_RIP:
        // No way to tell which CD the user meant to rip, let's take one randomly
        // (hopefully the last one enumerated is also the last one added)
        var deviceNode = null;
        for each (let deviceInfo in this._deviceInfoList) {
          if (!deviceInfo.svcPaneNode.hidden) {
            deviceNode = deviceInfo.svcPaneNode;
          }
        }

        if (deviceNode) {
          Cc['@mozilla.org/appshell/window-mediator;1']
            .getService(Ci.nsIWindowMediator)
            .getMostRecentWindow('Songbird:Main').gBrowser
            .loadURI(deviceNode.url, null, null, null, "_media");
        }
        else {
          // CD is probably not recognized yet, don't do anything - the view
          // will switch to it anyway once it is.
        }

        return true;

      default:
        return false;
    }
  },

  /**
   * \brief Load the CD Rip media view as response to a device event
   * \param aDeviceEvent - Device event being acted upon.
   */
  _loadCDViewFromEvent:
      function sbCDRipServicePaneService_loadCDViewFromEvent(aDeviceEvent) {

    var device = aDeviceEvent.data.QueryInterface(Ci.sbIDevice);
    var url = this._cfg.devMgrURL + "?device-id=" + device.id;
    Cc['@mozilla.org/appshell/window-mediator;1']
      .getService(Ci.nsIWindowMediator)
      .getMostRecentWindow('Songbird:Main').gBrowser
      .loadURI(url, null, null, null, "_media");
  },

  /**
   * \brief Updates the state of the service pane node.
   * \param aDeviceEvent - Device event of device.
   */
  _updateState: function sbCDRipServicePaneService__updateState(aDeviceEvent,
                                                                aForceBusy) {
    // Get the device and its node.
    var device = aDeviceEvent.origin.QueryInterface(Ci.sbIDevice);
    var deviceId = device.id;
    var deviceType = device.parameters.getProperty("DeviceType");

    // We only care about CD devices
    if (deviceType != "CD")
      return;

    if (typeof(this._deviceInfoList[deviceId]) != 'undefined') {
      var devNode = this._deviceInfoList[deviceId].svcPaneNode;

      // Get the device properties and clear the busy property.
      devProperties = devNode.properties.split(" ");
      devProperties = devProperties.filter(function(aProperty) {
                                             return aProperty != "busy";
                                           });

      // Set the busy property if the device is busy.
      if (aForceBusy || device.state != Ci.sbIDevice.STATE_IDLE) {
        // Clear success state from previous rip
        devProperties = devProperties.filter(function(aProperty) {
                                               return aProperty != "successful" &&
                                                      aProperty != "unsuccessful";
                                             });
        devProperties.push("busy");
      } else {
        if (devNode.hasAttributeNS(CDRIPNS, "LastState")) {
          var lastState = devNode.getAttributeNS(CDRIPNS, "LastState");
          if (lastState == Ci.sbIDevice.STATE_TRANSCODE) {
            if (this._checkErrors(device)) {
              devProperties.push("unsuccessful");
            } else {
              devProperties.push("successful");
            }
          }
        }
      }
      devNode.setAttributeNS(CDRIPNS, "LastState", device.state);

      // Write back the device node properties.
      devNode.properties = devProperties.join(" ");
    }
  },
 
  /**
   * Get the devices library (defaults to first library)
   * \param aDevice - Device to get library from
   */
  _getDeviceLibrary: function sbCDRipServicePaneService__getDeviceLibrary(aDevice) {
    // Get the libraries for device
    var libraries = aDevice.content.libraries;
    if (libraries.length < 1) {
      // Oh no, we have no libraries
      Cu.reportError("Device " + aDevice.id + " has no libraries!");
      return null;
    }

    // Get the requested library
    var deviceLibrary = libraries.queryElementAt(0, Ci.sbIMediaList);
    if (!deviceLibrary) {
      Cu.reportError("Unable to get library for device: " + aDevice.id);
      return null;
    }
    
    return deviceLibrary;
  },
  
  /**
   * Check for any ripped tracks that failed
   * \param aDevice - Device to check
   * \return True if errors occured, false otherwise
   */
  _checkErrors: function sbCDRipServicePaneService__checkErrors(aDevice) {
    // Check for any tracks that have a failed status
    var deviceLibrary = this._getDeviceLibrary(aDevice);
    var errorCount = 0;
    try {
      // Get all the did not successfully ripped tracks
      var rippedItems = deviceLibrary.getItemsByProperty(SBProperties.cdRipStatus,
                                                         "4|100");
      errorCount = rippedItems.length;  
    } catch (err) {}
    
    return (errorCount > 0);
  },
  
  /**
   * \brief Add a device that has media from event information.
   * \param aDeviceEvent - Device event of added device.
   */
  _addDeviceFromEvent:
    function sbCDRipServicePaneService_addDeviceFromEvent(aDeviceEvent) {

    var device = aDeviceEvent.data.QueryInterface(Ci.sbIDevice);
    var deviceType = device.parameters.getProperty("DeviceType");

    // We only care about CD devices
    if (deviceType != "CD")
      return false;

    try {
      this._addDevice(device);
    }
    catch(e) {
      Cu.reportError(e);
      return false;
    }
    return true;
  },
  
  /**
   * \brief Remove a device that no longer has media from event information.
   * \param aDeviceEvent - Device event of removed device.
   */
  _removeDeviceFromEvent:
    function sbCDRipServicePaneService_removeDeviceFromEvent(aDeviceEvent) {

    var device = aDeviceEvent.data.QueryInterface(Ci.sbIDevice);
    var deviceType = device.parameters.getProperty("DeviceType");

    // We only care about CD devices
    if (deviceType != "CD")
      return;

    try {
      this._removeDevice(device);
    }
    catch(e) {
      Cu.reportError(e);
    }
  },
  
  /**
   * \brief Add a device that has media.
   * \param aDevice - Device to add, this will only be added if it is of type
   *        "CD".
   */
  _addDevice: function sbCDRipServicePaneService_addDevice(aDevice) {
    var device = aDevice.QueryInterface(Ci.sbIDevice);
    var devId = device.id;
    
    // Do nothing if device is not an CD device.
    var deviceType = device.parameters.getProperty("DeviceType");
    if (deviceType != "CD") {
      return;
    }
    
    // TODO:
    // We need to do a check to ensure that the media inserted is readable and
    // an audio disc
    // if (!device.readable ||
    //     device.getDiscType() != Ci.sbICDDevice.AUDIO_DISC_TYPE) {
    //   return;
    // }

    // Add a cd rip node in the service pane.
    var devNode = this._deviceServicePaneSvc.createNodeForDevice2(device);
    devNode.setAttributeNS(CDRIPNS, "DeviceId", devId);
    devNode.setAttributeNS(CDRIPNS, "deviceNodeType", "cd-device");
    devNode.properties = "cd-device";
    devNode.contractid = this._cfg.contractID;
    devNode.url = this._cfg.devMgrURL + "?device-id=" + devId;
    devNode.editable = false;
    devNode.name = "CD RIP";//device.properties.friendlyName;
    devNode.hidden = false;
 
    this._deviceInfoList[devId] = {};
    this._deviceInfoList[devId].svcPaneNode = devNode;
  },
  
  /**
   * \brief Remove a CD Device from the service pane, and all nodes under it.
   * \param aNode - Service pane node of CD Device to remove.
   */
  _removeDeviceNodes: function sbCDRipServicePaneService_removeCDDeviceNodes(
                                                                        aNode) {
    // Remove child device nodes.
    if (aNode.isContainer) {
      var childEnum = aNode.childNodes;
      while (childEnum.hasMoreElements()) {
        var child = childEnum.getNext().QueryInterface(Ci.sbIServicePaneNode);
        this._removeDeviceNodes(child);
      }
    }

    // Remove cd device node.
    if (aNode.contractid == this._cfg.contractID)
      aNode.hidden = true;
  },
  
  /**
   * \brief Remove a known CD Device from the service pane.
   * \param aDevice - Device to remove.
   */
  _removeDevice: function sbCDRipServicePaneService_removeDevice(aDevice) {
    var device = aDevice.QueryInterface(Ci.sbIDevice);
    var devId = device.id;
    
    var devInfo = this._deviceInfoList[devId];
    if (!devInfo) {
      return;
    }

    // Remove the device node.
    devInfo.svcPaneNode.hidden = true;

    // Remove device info list entry.
    delete this._deviceInfoList[devId];
  },
  
  /**
   * \brief Remove all known CD Devices from the service pane.
   */
  _removeAllDevices: function sbCDRipServicePaneService_removeAllDevices() {
    for (var devId in this._deviceInfoList) {
      // Remove the device node.
      this._deviceInfoList[devId].svcPaneNode.hidden = true;

      // Remove device info list entry.
      delete this._deviceInfoList[devId];
    }
  },

  /**
   * \brief Create all CD Devices that already exist.
   */
  _createConnectedDevices: function sbCDRipServicePaneService_createConnectedDevices() {
    var devices = ArrayConverter.JSArray(this._deviceManagerSvc.devices);
    for each (device in devices) {
      try {
        this._addDevice(device);
      }
      catch(e) {
        Components.utils.reportError(e);
      }
    }
  }
};

// Instantiate an XPCOM module.
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([sbCDRipServicePaneService]);
}
