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
* \file  deviceFirmwareWizard.js
* \brief Javascript source for the device firmware wizard dialog.
*/

Components.utils.import("resource://app/jsmodules/DOMUtils.jsm");
Components.utils.import("resource://app/jsmodules/SBUtils.jsm");

if (typeof(Cc) == "undefined")
  var Cc = Components.classes;
if (typeof(Ci) == "undefined")
  var Ci = Components.interfaces;
if (typeof(Cr) == "undefined")
  var Cr = Components.results;
if (typeof(Cu) == "undefined")
  var Cu = Components.utils;

var deviceFirmwareWizard = {
  _device: null,
  _deviceFirmwareUpdater: null,
  _wizardElem: null,
  
  _currentOperation: null,

  get wizardElem() {
    if (!this._wizardElem)
      this._wizardElem = document.getElementById("device_firmware_wizard");
    return this._wizardElem;
  },

  doLoad: function deviceFirmwareWizard_doLoad() {
    this._initialize();
  },


  doUnload: function deviceFirmwareWizard_doUnload() {
    this._finalize();
  },

  doFinish: function deviceFirmwareWizard_doFinish() {
  },

  doCancel: function deviceFirmwareWizard_doCancel() {
    this._deviceFirmwareUpdater.cancel(this._device);
  },

  doPageShow: function deviceFirmwareWizard_doPageShow() {
    this._initialize();
    this.update();

    var currentPage = this.wizardElem.currentPage;
    
    switch(currentPage.id) {
      case "device_firmware_check":
        this._currentOperation = "cfu";
        this._deviceFirmwareUpdater.autoUpdate(this._device, this);
      break;
      
      default:
        throw new Error("not reached");
    }
  },
  
  doBack: function deviceFirmwareWizard_onBack(aEvent) {
  },
  
  doNext: function deviceFirmwareWizard_onNext(aEvent) {
  },
  
  doExtra1: function deviceFirmwareWizard_onExtra1(aEvent) {
  },
  
  doExtra2: function deviceFirmwareWizard_onExtra2(aEvent) {
  },
  
  onDeviceEvent: function deviceFirmwareWizard_onDeviceEvent(aEvent) {
    this._handleDeviceEvent(aEvent);
  },

  update: function deviceFirmwareWizard_update() {
  },


  _initialize: function deviceFirmwareWizard__initialize() {
    if (this._initialized)
      return;

    var dialogPB = 
      window.arguments[0].QueryInterface(Ci.nsIDialogParamBlock);

    this._device = dialogPB.objects.queryElementAt(0, Ci.sbIDevice);

    this._deviceFirmwareUpdater = 
      Cc["@songbirdnest.com/Songbird/Device/Firmware/Updater;1"]
        .getService(Ci.sbIDeviceFirmwareUpdater);

    this._wizardElem = document.getElementById("device_firmware_wizard");
    this._domEventListenerSet = new DOMEventListenerSet();

    var browserBox = document.getElementById("device_firmware_wizard_release_notes_box");
    this._domEventListenerSet.add(browserBox, "collapse", this._handleBrowserCollapse, true, false);

    this._wizardElem.canAdvance = true;
    this._wizardElem.canRewind = true;

    this._initialized = true;
  },


  _finalize: function deviceFirmwareWizard__finalize() {
    this._device = null;
    this._deviceFirmwareUpdater = null;
    
    if (this._domEventListenerSet)
      this._domEventListenerSet.removeAll();
    this._domEventListenerSet = null;
  },
  
  _handleBrowserCollapse: function deviceFirmwareWizard__handleBrowserCollapse(aEvent) {
    // XXXAus: Errr, that isn't the right way to do it, but it works for now.
    //         Ideally there should be no hardcoded width and height values in
    //         here. And no hardcoded multipliers either!
    if(aEvent.detail == false) {
      var height = window.innerHeight;
      height *= 2.6;
      window.resizeTo(window.innerWidth, height);
    }
  },

  _handleDeviceEvent: function deviceFirmwareWizard__handleDeviceEvent(aEvent) {
    switch(this._currentOperation) {
      case "cfu":
        this._handleCheckForUpdate(aEvent);
      break;
      
      default:
        throw new Error("not reached");
    }
  },
  
  _handleCheckForUpdate: function deviceFirmwareWizard__handleCheckForUpdate(aEvent) {
    if(aEvent.type == Ci.sbIDeviceEvent.EVENT_FIRMWARE_CFU_END) {
      var progressDeck = document.getElementById("device_firmware_wizard_check_deck");
      if(aEvent.data == true) {
        var newVerDesc = 
          document.getElementById("device_firmware_wizard_check_newver_description");
        
        var handler = this._deviceFirmwareUpdater.getActiveHandler(this._device);
                
        var text = SBFormattedString("device.firmware.wizard.check.newver.description", 
                                     [handler.latestFirmwareReadableVersion]);
                                     
        newVerDesc.appendChild(document.createTextNode(text));
        
        var remindMeLaterButton = this.wizardElem.getButton("back");
        var installNewFirmwareButton = this.wizardElem.getButton("next");
        
        remindMeLaterButton.label = 
          SBString("device.firmware.wizard.check.remind_me_later.label");
        remindMeLaterButton.accessKey = null;
        
        installNewFirmwareButton.label = 
          SBString("device.firmware.wizard.check.install.label");
        installNewFirmwareButton.accessKey = null;
        
        this.wizardElem.currentPage.setAttribute("showback", "true");
        this.wizardElem.currentPage.setAttribute("shownext", "true");
                  
        var browser = document.getElementById("device_firmware_wizard_release_notes_browser");
        browser.setAttribute("src", handler.releaseNotesLocation.spec);
        
        progressDeck.selectedPanel = 
          document.getElementById("device_firmware_wizard_check_new_box");
      }
      else {
        progressDeck.selectedPanel = 
          document.getElementById("device_firmware_wizard_check_already_box");
      }      
    }
  },
  
  _handleDownloadFirmware: function deviceFirmwareWizard__handleDownloadFirmware(aEvent) {
  }
};