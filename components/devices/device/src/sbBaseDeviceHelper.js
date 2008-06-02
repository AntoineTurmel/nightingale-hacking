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

/**
 * This is a helper class for devices
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://app/jsmodules/ArrayConverter.jsm");
Components.utils.import("resource://app/jsmodules/StringUtils.jsm");
Components.utils.import("resource://app/jsmodules/sbStorageFormatter.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

function BaseDeviceHelper() {}

BaseDeviceHelper.prototype = {

/*
  boolean hasSpaceForWrite(in unsigned long long aSpaceNeeded,
                           in sbIDeviceLibrary aLibrary,
                           [optional] in sbIDevice aDevice,
                           [optional] out unsigned long long aSpaceRemaining);
 */
  hasSpaceForWrite: function BaseDeviceHelper_hasSpaceForWrite(
    aSpaceNeeded, aLibrary, aDevice, aSpaceRemaining)
  {
    var device = aDevice;
    if (!device) {
      // no device supplied, need to find it
      const deviceMgr = Cc["@songbirdnest.com/Songbird/DeviceManager;2"]
                          .getService(Ci.sbIDeviceRegistrar);
      var foundLib = false;
      for (device in ArrayConverter.JSArray(deviceMgr.devices)) {
        for (var library in ArrayConverter.JSArray(device.content.libraries)) {
          if (library.equals(aLibrary)) {
            foundLib = true;
            break;
          }
        }
        if (foundLib)
          break;
      }
      if (!foundLib) {
        throw Components.Exception("Failed to find device for library");
      }
    }

    // figure out the space remaining
    var spaceRemaining =
      device.properties
            .properties
            .getPropertyAsInt64("http://songbirdnest.com/device/1.0#freeSpace");
    if (aSpaceRemaining) {
      aSpaceRemaining.value = spaceRemaining;
    }
    
    // if we have enough space, no need to look at anything else
    if (aSpaceNeeded < spaceRemaining)
      return true;
    
    // need to ask the user
    const bundleSvc = Cc["@mozilla.org/intl/stringbundle;1"]
                        .getService(Ci.nsIStringBundleService);
    const branding = bundleSvc.createBundle("chrome://branding/locale/brand.properties");
    const bundle = bundleSvc.createBundle("chrome://songbird/locale/songbird.properties");
    
    function L10N(aKey, aDefault, aBundle) {
      var stringBundle = aBundle || bundle;
      var retval = aDefault || aKey;
      try {
        retval = stringBundle.GetStringFromName(aKey);
      } catch (e){
        Components.utils.reportError(e);
      }
      return retval;
    }
    
    var isManualMgmt = (aLibrary.mgmtType == Ci.sbIDeviceLibrary.MGMT_TYPE_MANUAL);
    var messageKeyPrefix = "device.error.not_enough_freespace.prompt." +
                           (isManualMgmt ? "manual" : "sync");
    
    var messageParams = [
      device.name,
      StorageFormatter.format(aSpaceNeeded),
      StorageFormatter.format(spaceRemaining),
      L10N("brandShortName", "Songbird", branding)
    ];
    var message = bundle.formatStringFromName(messageKeyPrefix + ".message",
                                              messageParams,
                                              messageParams.length);
    
    const prompter = Cc["@songbirdnest.com/Songbird/Prompter;1"]
                       .getService(Ci.sbIPrompter);
    var neverPromptAgain = { value: false };
    var abortRequest = prompter.confirmEx(null, /* parent */
                                          L10N(messageKeyPrefix + ".title"),
                                          message,
                                          Ci.nsIPromptService.STD_YES_NO_BUTTONS,
                                          null, null, null, /* button text */
                                          null, /* checkbox message TODO */
                                          neverPromptAgain);
    return (!abortRequest);
  },

/*
  boolean queryUserSpaceExceeded(in nsIDOMWindow       aParent,
                                 in sbIDevice          aDevice,
                                 in boolean            aSyncDeviceOperation,
                                 in unsigned long long aSpaceNeeded,
                                 in unsigned long long aSpaceAvailable);
 */
  queryUserSpaceExceeded: function BaseDeviceHelper_queryUserSpaceExceeded
                                     (aParent,
                                      aDevice,
                                      aSyncDeviceOperation,
                                      aSpaceNeeded,
                                      aSpaceAvailable,
                                      aAbort)
  {
    // get the branding string bundle
    var bundleSvc = Cc["@mozilla.org/intl/stringbundle;1"]
                      .getService(Ci.nsIStringBundleService);
    var branding = bundleSvc.createBundle
                               ("chrome://branding/locale/brand.properties");

    var messageKeyPrefix = "device.error.not_enough_freespace.prompt." +
                           (aSyncDeviceOperation ? "sync" : "manual");

    var message = SBFormattedString
                    (messageKeyPrefix + ".message",
                     [ aDevice.name,
                       StorageFormatter.format(aSpaceNeeded),
                       StorageFormatter.format(aSpaceAvailable),
                       SBString("brandShortName", "Songbird", branding) ]);

    var prompter = Cc["@songbirdnest.com/Songbird/Prompter;1"]
                     .getService(Ci.sbIPrompter);
    var neverPromptAgain = { value: false };
    var abortRequest = prompter.confirmEx
                                  (aParent,
                                   SBString(messageKeyPrefix + ".title"),
                                   message,
                                   Ci.nsIPromptService.STD_YES_NO_BUTTONS,
                                   null,
                                   null,
                                   null,
                                   null,
                                   neverPromptAgain);

    return !abortRequest;
  },

  contractID: "@songbirdnest.com/Songbird/Device/Base/Helper;1",
  classDescription: "Helper component for device implementations",
  classID: Components.ID("{ebe6e08a-0604-44fd-a3d7-2be556b96b24}"),
  QueryInterface: XPCOMUtils.generateQI([
    Components.interfaces.sbIDeviceHelper
    ])
};

NSGetModule = XPCOMUtils.generateNSGetModule([BaseDeviceHelper], null, null);