/*
 *=BEGIN SONGBIRD GPL
 *
 * This file is part of the Songbird web player.
 *
 * Copyright(c) 2005-2009 POTI, Inc.
 * http://www.songbirdnest.com
 *
 * This file may be licensed under the terms of of the
 * GNU General Public License Version 2 (the "GPL").
 *
 * Software distributed under the License is distributed
 * on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either
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

function newTOC(aFirstTrack, aLastTrack, aLead)
{
  var toc = Cc["@songbirdnest.com/Songbird/MockCDTOC;1"]
             .createInstance(Ci.sbIMockCDTOC);
  toc.initialize(aFirstTrack, aLastTrack, aLead);
 
  return toc;
}

function sbMakeMidnightRock()
{
  var toc = newTOC(1, 15, 285675);

  // tracks:
  toc.addTocEntry(32, 309, 0);
  toc.addTocEntry(23260, 231, 0);
  toc.addTocEntry(40612, 242, 0);
  toc.addTocEntry(58770, 191, 0);
  toc.addTocEntry(73145, 310, 0);
  toc.addTocEntry(96415, 290, 0);
  toc.addTocEntry(118232, 301, 0);
  toc.addTocEntry(140867, 259, 0);
  toc.addTocEntry(160322, 316, 0);
  toc.addTocEntry(184085, 222, 0);
  toc.addTocEntry(200777, 236, 0);
  toc.addTocEntry(218535, 185, 0);
  toc.addTocEntry(232437, 211, 0);
  toc.addTocEntry(248320, 184, 0);
  toc.addTocEntry(262145, 313, 0);
  
  return toc;
}

//------------------------------------------------------------------------------
//
// Mock CD TOC "Burnshit Here" by Brian Glaze
//
//------------------------------------------------------------------------------

function sbMakeBabyOneMoreTime()
{
  var toc = newTOC(1, 12, 260335);

  toc.addTocEntry(0, 211, 1);
  toc.addTocEntry(15847, 200, 2);
  toc.addTocEntry(30859, 246, 3);
  toc.addTocEntry(49320, 202, 4);
  toc.addTocEntry(64479, 245, 5);
  toc.addTocEntry(82865, 312, 6);
  toc.addTocEntry(106307, 234, 7);
  toc.addTocEntry(123929, 243, 8);
  toc.addTocEntry(142217, 216, 9);
  toc.addTocEntry(158447, 223, 10);
  toc.addTocEntry(175179, 223, 11);
  toc.addTocEntry(203309, 760, 12);

  return toc;
}

//------------------------------------------------------------------------------
//
// Mock CD TOC "All That You Can't Leave Behind" by U2
//
//------------------------------------------------------------------------------

function sbMakeAllThatYouCantLeaveBehind()
{
  var toc = newTOC(1, 11, 225562);

  toc.addTocEntry(150, 248, 1);
  toc.addTocEntry(18843, 272, 2);
  toc.addTocEntry(39601, 227, 3);
  toc.addTocEntry(56966, 296, 4);
  toc.addTocEntry(79487, 267, 5);
  toc.addTocEntry(99796, 219, 6);
  toc.addTocEntry(116534, 226, 7);
  toc.addTocEntry(133832, 288, 8);
  toc.addTocEntry(155768, 258, 9);
  toc.addTocEntry(175400, 330, 10);
  toc.addTocEntry(200468, 331, 11);

  return toc;
}

//------------------------------------------------------------------------------
//
// Mock CD TOC "Incredibad" by The Lonely Island
//
//------------------------------------------------------------------------------

function sbMakeIncredibad()
{
  var toc = newTOC(1, 19, 190565);

  toc.addTocEntry(150, 76, 1);
  toc.addTocEntry(5896, 155, 2);
  toc.addTocEntry(17528, 151, 3);
  toc.addTocEntry(28879, 156, 4);
  toc.addTocEntry(40599, 126, 5);
  toc.addTocEntry(50106, 139, 6);
  toc.addTocEntry(60584, 64, 7);
  toc.addTocEntry(65394, 193, 8);
  toc.addTocEntry(79870, 34, 9);
  toc.addTocEntry(82446, 106, 10);
  toc.addTocEntry(90457, 123, 11);
  toc.addTocEntry(99748, 193, 12);
  toc.addTocEntry(114258, 126, 13);
  toc.addTocEntry(123750, 161, 14);
  toc.addTocEntry(135829, 65, 15);
  toc.addTocEntry(140754, 167, 16);
  toc.addTocEntry(153283, 175, 17);
  toc.addTocEntry(166425, 149, 18);
  toc.addTocEntry(177440, 179, 19);

  return toc;
}

//------------------------------------------------------------------------------
//
// Mock CD service implementation.
//
//------------------------------------------------------------------------------

function sbMockCDService()
{
  this._mDevices = [];
  this._mListeners = [];

  // Push in two mock cd drive devices.
  var device = Cc["@songbirdnest.com/Songbird/MockCDDevice;1"]
                   .createInstance(Ci.sbIMockCDDevice);
  device.initialize("Songbird MockCD Device 8000",
                    true,
                    false,
                    false,
                    Ci.sbIDeviceController.AUDIO_DISC_TYPE,
                    false,
                    this);
  this._mDevices.push(device.QueryInterface(Ci.sbICDDevice));

  device = Cc["@songbirdnest.com/Songbird/MockCDDevice;1"]
                   .createInstance(Ci.sbIMockCDDevice);
  device.initialize("Songbird MockCD Device 7000",
                    true,
                    false,
                    false,
                    Ci.sbIDeviceController.AUDIO_DISC_TYPE,
                    false,
                    this);
  this._mDevices.push(device.QueryInterface(Ci.sbICDDevice));
}

sbMockCDService.prototype =
{
  _mDevices:    null,
  _mListeners:  null,

  //
  // sbICDDeviceService:
  //
  get nbDevices()
  {
    return this._mDevices.length;
  },

  getDevice: function sbMockCDDevice_getDevice(aDeviceIndex)
  {
    return this._mDevices[aDeviceIndex];
  },

  getCDDevices: function sbMockCDService_getCDDevics()
  {
    return ArrayConverter.nsIArray(this._mDevices).enumerate();
  },

  registerListener: function sbMockCDDevice_registerListener(aListener)
  {
    aListener.QueryInterface(Ci.sbICDDeviceListener);
    this._mListeners.push(aListener);
  },

  removeListener: function sbMockCDDevice_removeListener(aListener)
  {
    var index = this._mListeners.indexOf(aListener);
    if (index > -1) {
      this._mListeners.splice(index, 1);
    }
  },

  //
  // sbIMockCDDeviceController
  //
  insertMedia: function sbMockCDService_insertMedia(aCDDevice, aMediaDisc)
  {
    var curCDDevice = this._findDevice(aCDDevice);
    if (!curCDDevice || curCDDevice.isDiskInserted) {
      return;
    }

    // Determine which TOC to insert into the device.
    var mockMediaTOC;
    switch (aMediaDisc) {
      case Ci.sbIMockCDDeviceController.MOCK_MEDIA_DISC_MIDNIGHT_ROCK:
        mockMediaTOC = sbMakeMidnightRock();
        break;
      case Ci.sbIMockCDDeviceController.MOCK_MEDIA_DISC_BABY_ONE_MORE_TIME:
        mockMediaTOC = sbMakeBabyOneMoreTime();
        break;
      case Ci.sbIMockCDDeviceController.MOCK_MEDIA_DISC_U2:
        mockMediaTOC = sbMakeAllThatYouCantLeaveBehind();
        break;
      case Ci.sbIMockCDDeviceController.MOCK_MEDIA_DISC_INCREDIBAD:
        mockMediaTOC = sbMakeIncredibad();
        break;
    }

    curCDDevice.discTOC = mockMediaTOC;
    this._onMediaInserted(curCDDevice);
  },

  ejectMedia: function sbMockCDService_ejectMedia(aCDDevice)
  {
    var curCDDevice = this._findDevice(aCDDevice);

    if (!curCDDevice || !curCDDevice.isDiscInserted) {
      return;
    }

    curCDDevice.eject();
    this._onMediaEjected(curCDDevice);
  },

  notifyEject: function sbMockCDService_notifyEject(aCDDevice)
  {
    var curCDDevice = this._findDevice(aCDDevice);
    if (!curCDDevice) {
      return;
    }

    this._onMediaEjected(curCDDevice);
  },

  _findDevice: function sbMockCDService_findDevice(aCDDevice)
  {
    // Return the local copy of our Mock CD Device that we have stashed in
    // |_mDevices|.
    var foundDevice = null;
    for (var i = 0; i < this._mDevices.length && foundDevice == null; i++) {
      if (this._mDevices[i].name == aCDDevice.name) {
        foundDevice = this._mDevices[i];
      }
    }

    return foundDevice;
  },

  _onMediaEjected: function sbMockCDService_onMediaEjected(aDevice)
  {
    this._mListeners.forEach(
      function(aListener) {
        try {
          aListener.onMediaEjected(aDevice);
        }
        catch (e) {
          Cu.reportError(e);
        }
      });
  },

  _onMediaInserted: function sbMockCDService_onMediaInserted(aDevice)
  {
    this._mListeners.forEach(
      function(aListener) {
        try {
          aListener.onMediaInserted(aDevice);
        }
        catch (e) {
          Cu.reportError(e);
        }
      });
  },

  classDescription: "Songbird Mock CD Device Service",
  classID: Components.ID("{C701A410-7F71-41E9-A07A-C765A4F04D41}"),
  contractID: "@songbirdnest.com/device/cd/mock-cddevice-service;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.sbICDDeviceService,
                                         Ci.sbIMockCDDeviceController])
};

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([sbMockCDService]);
}

