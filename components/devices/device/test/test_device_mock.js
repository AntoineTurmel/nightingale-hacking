/* vim: set sw=2 :miv */
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

/**
 * \brief Device tests - Mock device
 */

function runTest () {
  var device = Components.classes["@songbirdnest.com/Songbird/Device/DeviceTester/MockDevice;1"]
                         .createInstance(Components.interfaces.sbIDevice);
  assertEqual(device.name, "Bob's Mock Device");
  
  assertEqual("" + device.id, "" + device.id, "device ID not equal");
  assertEqual("" + device.controllerId, "" + device.controllerId, "controller ID not equal");
  
  assertFalse(device.connected);
  
  device.connect();
  assertTrue(device.connected);
  try {
    device.connect();
    fail("Re-connected device");
  } catch(e){
    /* expected to throw */
  }
  assertTrue(device.connected);

  device.disconnect();
  assertFalse(device.connected);
  try {
    device.disconnect();
    fail("Re-disconnected device");
  } catch(e) {
    /* expected to throw */
  }
  assertFalse(device.connected);
  
  assertFalse(device.threaded);
  
  test_prefs(device);
  
  /* TODO: device.capabilities */
  
  /* TODO: device.content */
  
  /* TODO: device.parameters */
  
  test_event(device);
  
  test_request(device);
  
  test_library(device);
 
}

function test_prefs(device) {
  assertTrue(typeof(device.getPreference("hello")) == "undefined");
  
  device.setPreference("world", 3);
  assertEqual(device.getPreference("world"), 3);
  assertEqual(typeof(device.getPreference("world")), "number");
  device.setPreference("world", "goat");
  assertEqual(device.getPreference("world"), "goat");
  assertEqual(typeof(device.getPreference("world")), "string");
  device.setPreference("world", true);
  assertEqual(device.getPreference("world"), true);
  assertEqual(typeof(device.getPreference("world")), "boolean");
  
  with (Components.interfaces.sbIDevice) {
    device.setPreference("state", 0);
    assertEqual(device.state, STATE_IDLE);
    device.setPreference("state", 1);
    assertEqual(device.state, STATE_SYNCING);
    device.setPreference("state", 2);
    assertEqual(device.state, STATE_COPYING);
    device.setPreference("state", 3);
    assertEqual(device.state, STATE_DELETING);
    device.setPreference("state", 4);
    assertEqual(device.state, STATE_UPDATING);
    device.setPreference("state", 5);
    assertEqual(device.state, STATE_MOUNTING);
    device.setPreference("state", 6);
    assertEqual(device.state, STATE_DOWNLOADING);
    device.setPreference("state", 7);
    assertEqual(device.state, STATE_UPLOADING);
    device.setPreference("state", 8);
    assertEqual(device.state, STATE_DOWNLOAD_PAUSED);
    device.setPreference("state", 9);
    assertEqual(device.state, STATE_UPLOAD_PAUSED);
    device.setPreference("state", 10);
    assertEqual(device.state, STATE_DISCONNECTED);
  }
}

function test_event(device) {
  /* test as a event target */
  // I didn't bother with CI on the mock device
  device.QueryInterface(Components.interfaces.sbIDeviceEventTarget);
  var wasFired = false;
  var handler = function handler() { wasFired = true; }
  device.addEventListener(handler);
  var event = Components.classes["@songbirdnest.com/Songbird/DeviceManager;2"]
                        .getService(Components.interfaces.sbIDeviceManager2)
                        .createEvent(0);
  device.dispatchEvent(event);
  assertTrue(wasFired, "event handler not called");
  
  device.removeEventListener(handler);
}

function test_request(device) {
  /* test as sbIMockDevice (request push/pop) */
  device.QueryInterface(Ci.sbIMockDevice);
  
  // simple index-only
  device.submitRequest(device.REQUEST_UPDATE,
                       createPropertyBag({index: 10}));
  checkPropertyBag(device.popRequest(), {index: 10});
  
  // priority
  const MAXINT = (-1) >>> 1; // max signed int (to test that we're not allocating)
  device.submitRequest(device.REQUEST_UPDATE,
                       createPropertyBag({index: MAXINT, priority: MAXINT}));
  device.submitRequest(device.REQUEST_UPDATE,
                       createPropertyBag({index: 99, priority: 99}));
  device.submitRequest(device.REQUEST_UPDATE,
                       createPropertyBag({index: 100}));
  checkPropertyBag(device.popRequest(), {index: 99, priority: 99});
  checkPropertyBag(device.popRequest(), {index: 100}); /* default */
  checkPropertyBag(device.popRequest(), {index: MAXINT, priority: MAXINT});
  
  // peek
  device.submitRequest(device.REQUEST_UPDATE,
                       createPropertyBag({index: 42}));
  checkPropertyBag(device.peekRequest(), {index: 42});
  checkPropertyBag(device.popRequest(), {index: 42});
  
  // test the properties
  var item = { QueryInterface:function(){return this} };
  item.wrappedJSObject = item;
  var list = { QueryInterface:function(){return this} };
  list.wrappedJSObject = list;
  var data = { /* nothing needed */ };
  data.wrappedJSObject = data;
  var params = { item: item,
                 list: list,
                 data: data,
                 index: 999,
                 otherIndex: 1024,
                 priority: 37};
  device.submitRequest(0x01dbeef, createPropertyBag(params));
  var request = device.popRequest();
  checkPropertyBag(request, params);
  log("item transfer ID: " + request.getProperty("itemTransferID"));
  assertTrue(request.getProperty("itemTransferID") > 3,
             "Obviously bad item transfer ID");
  
  request = null; /* unleak */
}

function test_library(device) {
  if (!device.connected)
    device.connect();
  assertEqual(device,
              device.content
                    .libraries
                    .queryElementAt(0, Ci.sbIDeviceLibrary)
                    .device);
  // stop a circular reference
  if (device.connected)
    device.disconnect();
}

function createPropertyBag(aParams) {
  var bag = Cc["@mozilla.org/hash-property-bag;1"]
              .createInstance(Ci.nsIWritablePropertyBag);
  for (var name in aParams) {
    bag.setProperty(name, aParams[name]);
  }
  return bag;
}

function checkPropertyBag(aBag, aParams) {
  for (var name in  aParams) {
    try {
      var val = aBag.getProperty(name);
    } catch (e) {
      log('Failed to get property "' + name + '"');
      throw(e);
    }
    assertTrue(val, 'Cannot find property "' + name + '"');
    if (typeof(aParams[name]) == "object" && "wrappedJSObject" in aParams[name])
      val = val.wrappedJSObject;
    assertEqual(aParams[name],
                val,
                'property "' + name + '" not equal');
    log('"' + name + '" is ' + val);
  }
}
