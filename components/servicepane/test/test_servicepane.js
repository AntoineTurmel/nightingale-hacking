/** vim: ts=2 sw=2 expandtab ai
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
 * \brief Basic service pane unit tests
 */

Components.utils.import("resource://app/jsmodules/XPCOMUtils.jsm");

function DBG(s) { log('DBG:test_servicepane: '+s); }

var testModule = {
  classId: Components.ID("{0da5eace-1dd2-11b2-a64a-eb82fc5ef0c3}"),
  classDescription: "Songbird Test Service Pane Module",
  contractId: "@songbirdnest.com/servicepane/test-module;1",

  _registered: false,
  QueryInterface: XPCOMUtils.generateQI([Ci.sbIServicePaneModule]),

  _processEvents: function() {
    let mainThread = Cc["@mozilla.org/thread-manager;1"]
                       .getService(Ci.nsIThreadManager)
                       .mainThread;
    while (mainThread.hasPendingEvents())
      mainThread.processNextEvent(true);
  },

  addToCategory: function() {
    if (!this._registered) {
      // Need to register component first
      let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      let factory = {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory]),
        createInstance: function(outer, iid) {
          return testModule.QueryInterface(iid);
        }
      };
      registrar.registerFactory(this.classId, this.classDescription,
                                this.contractId, factory);
      this._registered = true;
    }

    this._servicePaneInitParams = null;

    let catMgr = Cc["@mozilla.org/categorymanager;1"]
                   .getService(Ci.nsICategoryManager);
    catMgr.addCategoryEntry("service-pane", "service," + this.contractId,
                            this.contractId, false, true);

    // Notifications are sent asynchronously, wait for them to be processed
    this._processEvents();

    // servicePaneInit should have been called, return the arguments we received
    return this._servicePaneInitParams;
  },

  removeFromCategory: function() {
    let catMgr = Cc["@mozilla.org/categorymanager;1"]
                   .getService(Ci.nsICategoryManager);
    catMgr.deleteCategoryEntry("service-pane", "service," + this.contractId,
                               false);

    // Notifications are sent asynchronously, wait for them
    this._processEvents();
  },

  _servicePaneInitParams: null,
  servicePaneInit: function() {
    DBG("testModule.servicePaneInit() called");
    this._servicePaneInitParams = arguments;
  },

  _fillContextMenuParams: null,
  fillContextMenu: function() {
    DBG("testModule.fillContextMenu() called");
    this._fillContextMenuParams = arguments;
  },

  _fillNewItemMenuParams: null,
  fillNewItemMenu: function() {
    DBG("testModule.fillNewItemMenu() called");
    this._fillNewItemMenuParams = arguments;
  },

  _onSelectionChangedParams: null,
  onSelectionChanged: function() {
    DBG("testModule.onSelectionChanged() called");
    this._onSelectionChangedParams = arguments;
  },

  _canDropParams: null,
  _canDropResult: false,
  canDrop: function() {
    DBG("testModule.canDrop() called");
    this._canDropParams = arguments;
    return this._canDropResult;
  },

  _onDropParams: null,
  onDrop: function() {
    DBG("testModule.onDrop() called");
    this._onDropParams = arguments;
  },

  _onDragGestureParams: null,
  _onDragGestureResult: false,
  onDragGesture: function() {
    DBG("testModule.onDragGesture() called");
    this._onDragGestureParams = arguments;
    return this._onDragGestureResult;
  },

  _onBeforeRenameParams: null,
  onBeforeRename: function() {
    DBG("testModule.onBeforeRename() called");
    this._onBeforeRenameParams = arguments;
  },

  _onRenameParams: null,
  onRename: function() {
    DBG("testModule.onRename() called");
    this._onRenameParams = arguments;
  },

  stringbundle: null
};

/**
 * \brief Creates a string representation of the nodes tree. Example output:
 *        [node foo="bar" id="node"][node id="child"/][/node]
 *        Attributes are sorted by name.
 * \param aNode root node for serialization
 */
function serializeTree(aNode) {
  let result = "[node";

  let attributes = aNode.attributes;
  let attrList = [];
  while (attributes.hasMore()) {
    let attr = attributes.getNext();
    attrList.push(" " + attr + '="' + aNode.getAttribute(attr) + '"');
  }
  attrList.sort();
  result += attrList.join("");

  let childNodes = aNode.childNodes;
  if (childNodes.hasMoreElements()) {
    // Node has children
    result += "]";
    while (childNodes.hasMoreElements()) {
      let child = childNodes.getNext().QueryInterface(Ci.sbIServicePaneNode);
      result += serializeTree(child);
    }
    result += "[/node]";
  }
  else {
    // No children, close "tag" immediately
    result += "/]";
  }

  return result;
}

function removeIfExists(SPS, ids) {
  for (var i=0; i<ids.length; i++) {
    var id = ids[i];
    DBG('removeIfExists id='+id+'\n');
    var node = SPS.getNode(id);
    if (node) {
      SPS.removeNode(node);
    }
  }
}

function testAttributes(aNode) {
  // Node shouldn't have any attributes yet
  assertEqual(serializeTree(aNode), '[node/]');
  assertEqual(aNode.getAttribute("foo"), null);
  assertEqual(aNode.getAttributeNS("ns", "foo"), null);
  assertFalse(aNode.hasAttribute("foo"));
  assertFalse(aNode.hasAttributeNS("ns", "foo"));

  // Try changing an attribute then
  aNode.setAttribute("foo", "bar");
  assertEqual(aNode.getAttribute("foo"), "bar");
  assertEqual(aNode.getAttributeNS("ns", "foo"), null);
  assertTrue(aNode.hasAttribute("foo"));
  assertFalse(aNode.hasAttributeNS("ns", "foo"));
  assertEqual(serializeTree(aNode), '[node foo="bar"/]');

  aNode.setAttribute("foo", "baz");
  assertEqual(serializeTree(aNode), '[node foo="baz"/]');

  aNode.removeAttribute("foo");
  assertEqual(serializeTree(aNode), '[node/]');

  assertEqual(aNode.getAttribute("foo"), null);
  assertFalse(aNode.hasAttribute("foo"));

  // Same thing with namespaces
  aNode.setAttributeNS("ns", "foo", "bar");
  assertEqual(aNode.getAttributeNS("ns", "foo"), "bar");
  assertEqual(aNode.getAttributeNS("wrongns", "foo"), null);
  assertEqual(aNode.getAttribute("foo"), null);
  assertTrue(aNode.hasAttributeNS("ns", "foo"));
  assertFalse(aNode.hasAttributeNS("wrongns", "foo"));
  assertFalse(aNode.hasAttribute("foo"));
  assertEqual(serializeTree(aNode), '[node ns:foo="bar"/]');

  aNode.setAttributeNS("ns", "foo", "baz");
  assertEqual(serializeTree(aNode), '[node ns:foo="baz"/]');

  aNode.removeAttribute("foo");
  aNode.removeAttributeNS("wrongns", "foo");
  assertEqual(serializeTree(aNode), '[node ns:foo="baz"/]');

  aNode.removeAttributeNS("ns", "foo");
  assertEqual(serializeTree(aNode), '[node/]');

  assertEqual(aNode.getAttributeNS("ns", "foo"), null);
  assertFalse(aNode.hasAttributeNS("ns", "foo"));

  // How about multiple attributes?
  aNode.setAttribute("foo", "bar");
  aNode.setAttribute("bar", "foo");
  aNode.setAttributeNS("ns", "foo", "baz");
  assertEqual(serializeTree(aNode), '[node bar="foo" foo="bar" ns:foo="baz"/]');
  assertTrue(aNode.hasAttribute("foo"));
  assertTrue(aNode.hasAttribute("bar"));
  assertTrue(aNode.hasAttributeNS("ns", "foo"));

  aNode.removeAttribute("foo");
  assertEqual(serializeTree(aNode), '[node bar="foo" ns:foo="baz"/]');
  
  aNode.removeAttribute("wrongns", "foo");
  assertEqual(serializeTree(aNode), '[node bar="foo" ns:foo="baz"/]');
  
  aNode.removeAttributeNS("ns", "foo");
  assertEqual(serializeTree(aNode), '[node bar="foo"/]');
  
  aNode.removeAttribute("bar");
  assertEqual(serializeTree(aNode), '[node/]');

  assertFalse(aNode.hasAttribute("foo"));
  assertFalse(aNode.hasAttribute("bar"));
  assertFalse(aNode.hasAttributeNS("ns", "foo"));

  // Now test the properties defined for some string attributes
  let stringProps = ["id", "className", "url", "image", "name", "tooltip",
                     "contractid", "stringbundle", "dndDragTypes",
                     "dndAcceptNear", "dndAcceptIn"];
  for each (let prop in stringProps) {
    DBG("Testing node property " + prop);

    // Special handling for "className" property - attribute name is "class"
    let attr = (prop == "className" ? "class" : prop);

    assertEqual(aNode.getAttribute(attr), null);
    assertEqual(aNode[prop], null);
    assertFalse(aNode.hasAttribute(attr));

    aNode[prop] = "foo"
    assertEqual(aNode.getAttribute(attr), "foo");
    assertEqual(aNode[prop], "foo");
    assertTrue(aNode.hasAttribute(attr));

    aNode[prop] = null;
    assertEqual(aNode.getAttribute(attr), null);
    assertEqual(aNode[prop], null);
    assertFalse(aNode.hasAttribute(attr));

    DBG("ok");
  }

  // Now test the boolean properties
  let booleanProps = {hidden: false, editable: false, isOpen: true};
  for (let prop in booleanProps) {
    DBG("Testing node property " + prop);

    let defaultVal = booleanProps[prop];

    assertEqual(aNode.getAttribute(prop), null);
    assertEqual(aNode[prop], defaultVal);
    assertFalse(aNode.hasAttribute(prop));
  
    aNode[prop] = !defaultVal;
    assertEqual(aNode.getAttribute(prop), defaultVal ? "false" : "true");
    assertEqual(aNode[prop], !defaultVal);
    assertTrue(aNode.hasAttribute(prop));
  
    aNode[prop] = defaultVal;
    assertEqual(aNode.getAttribute(prop), defaultVal ? "true" : "false");
    assertEqual(aNode[prop], defaultVal);
    assertTrue(aNode.hasAttribute(prop));
  
    aNode.removeAttribute(prop);

    DBG("ok");
  }

  // "properties" is just a different name for "className"
  aNode.className = "class1 class2";
  assertEqual(aNode.properties, "class1 class2");
  assertEqual(serializeTree(aNode), '[node class="class1 class2"/]');

  aNode.properties = null;
  assertEqual(aNode.className, null);

  // "displayName" should be a translated version of the "name" property
  aNode.name = "&test_dummy";
  assertEqual(aNode.displayName, "&test_dummy");

  aNode.stringbundle = "data:text/plain,test_dummy=foo";
  assertEqual(aNode.displayName, "foo");

  aNode.name = "test_dummy";
  assertEqual(aNode.displayName, "test_dummy");

  aNode.name = "&test_dummy";
  assertEqual(aNode.displayName, "foo");

  aNode.stringbundle = "data:text/plain,test_dummy=bar";
  assertEqual(aNode.displayName, "bar");

  aNode.stringbundle = null;
  assertEqual(aNode.displayName, "&test_dummy");

  // displayName should be able to use module's string bundle
  testModule.addToCategory();
  testModule.stringbundle = "data:text/plain,test_dummy=module-foo";
  aNode.contractid = testModule.contractId;
  assertEqual(aNode.displayName, "module-foo");

  aNode.contractid = null;
  assertEqual(aNode.displayName, "&test_dummy");

  // Once the module is removed it should no longer use it
  testModule.removeFromCategory();
  aNode.contractid = testModule.contractId;
  assertEqual(aNode.displayName, "&test_dummy");

  aNode.contractid = null;
  aNode.name = null;
  assertEqual(aNode.displayName, null);

  // Node still shouldn't have any attributes at the end of the test
  assertEqual(serializeTree(aNode), '[node/]');
}


function testTreeManipulation(SPS, aRoot) {
  let hadException;

  // Root shouldn't have any children yet
  assertEqual(serializeTree(aRoot), '[node/]');

  let node1 = SPS.createNode();
  assertTrue(node1 instanceof Ci.sbIServicePaneNode);
  node1.id = "node1";

  let node2 = SPS.createNode();
  assertTrue(node2 instanceof Ci.sbIServicePaneNode);
  node2.id = "node2";

  let node3 = SPS.createNode();
  assertTrue(node3 instanceof Ci.sbIServicePaneNode);
  node3.id = "node3";

  // Initially all relational properties should be null
  assertEqual(aRoot.firstChild, null);
  assertEqual(aRoot.lastChild, null);
  assertEqual(node1.parentNode, null);
  assertEqual(node1.nextSibling, null);
  assertEqual(node1.previousSibling, null);

  // Appending node as first child
  aRoot.appendChild(node1);
  node1.appendChild(node2);
  assertEqual(serializeTree(aRoot), '[node][node id="node1"][node id="node2"/][/node][/node]');

  // Appending node as second child
  aRoot.appendChild(node3);
  assertEqual(serializeTree(aRoot), '[node][node id="node1"][node id="node2"/][/node][node id="node3"/][/node]');

  // Relational properties should be a lot more interesting now
  assertEqual(aRoot.firstChild, node1);
  assertEqual(aRoot.lastChild, node3);
  assertEqual(node1.firstChild, node2);
  assertEqual(node1.lastChild, node2);
  assertEqual(node3.firstChild, null);
  assertEqual(node3.lastChild, null);
  assertEqual(node1.parentNode, aRoot);
  assertEqual(node2.parentNode, node1);
  assertEqual(node3.parentNode, aRoot);
  assertEqual(node1.nextSibling, node3);
  assertEqual(node1.previousSibling, null);
  assertEqual(node2.nextSibling, null);
  assertEqual(node2.previousSibling, null);
  assertEqual(node3.nextSibling, null);
  assertEqual(node3.previousSibling, node1);

  // Appending a node that is already in a different part of the tree
  aRoot.appendChild(node2);
  assertEqual(serializeTree(aRoot), '[node][node id="node1"/][node id="node3"/][node id="node2"/][/node]');

  aRoot.appendChild(node1);
  assertEqual(serializeTree(aRoot), '[node][node id="node3"/][node id="node2"/][node id="node1"/][/node]');

  // Removing a node
  aRoot.removeChild(node3);
  assertEqual(serializeTree(aRoot), '[node][node id="node2"/][node id="node1"/][/node]');

  // Removing a node that isn't a child should throw
  hadException = false;
  try {
    node1.removeChild(node2);
  }
  catch (e) {
    hadException = true;
  }
  assertTrue(hadException);

  hadException = false;
  try {
    node1.removeChild(node3);
  }
  catch (e) {
    hadException = true;
  }
  assertTrue(hadException);

  // Inserting a node before another
  aRoot.insertBefore(node3, node1);
  assertEqual(serializeTree(aRoot), '[node][node id="node2"/][node id="node3"/][node id="node1"/][/node]');

  // Inserting a node that is already in a different part of the tree
  aRoot.insertBefore(node3, node2);
  assertEqual(serializeTree(aRoot), '[node][node id="node3"/][node id="node2"/][node id="node1"/][/node]');

  // Test relational properties again
  assertEqual(aRoot.firstChild, node3);
  assertEqual(aRoot.lastChild, node1);
  assertEqual(node1.firstChild, null);
  assertEqual(node1.lastChild, null);
  assertEqual(node2.firstChild, null);
  assertEqual(node2.lastChild, null);
  assertEqual(node3.firstChild, null);
  assertEqual(node3.lastChild, null);
  assertEqual(node1.parentNode, aRoot);
  assertEqual(node2.parentNode, aRoot);
  assertEqual(node3.parentNode, aRoot);
  assertEqual(node1.nextSibling, null);
  assertEqual(node1.previousSibling, node2);
  assertEqual(node2.nextSibling, node1);
  assertEqual(node2.previousSibling, node3);
  assertEqual(node3.nextSibling, node2);
  assertEqual(node3.previousSibling, null);

  // Inserting before a node that isn't a child should throw
  hadException = false;
  try {
    node1.insertBefore(node3, node2);
  }
  catch (e) {
    hadException = true;
  }
  assertTrue(hadException);
  
  aRoot.removeChild(node2);
  hadException = false;
  try {
    node1.insertBefore(node3, node2);
  }
  catch (e) {
    hadException = true;
  }
  assertTrue(hadException);

  // Replacing a node
  aRoot.replaceChild(node2, node1);
  assertEqual(serializeTree(aRoot), '[node][node id="node3"/][node id="node2"/][/node]');

  // Replacing with a node that is already part of the tree
  aRoot.replaceChild(node3, node2);
  assertEqual(serializeTree(aRoot), '[node][node id="node3"/][/node]');

  // Replacing a node that isn't a child should throw
  hadException = false;
  try {
    aRoot.replaceChild(node1, node2);
  }
  catch (e) {
    hadException = true;
  }
  assertTrue(hadException);
  
  node3.appendChild(node2);
  hadException = false;
  try {
    aRoot.replaceChild(node1, node2);
  }
  catch (e) {
    hadException = true;
  }
  assertTrue(hadException);

  // Inserting a parent node into the child should throw
  let rootParent = aRoot.parentNode;
  hadException = false;
  try {
    aRoot.appendChild(aRoot)
  }
  catch (e) {
    hadException = true;
  }
  assertTrue(hadException);

  hadException = false;
  try {
    node3.appendChild(aRoot)
  }
  catch (e) {
    hadException = true;
  }
  assertTrue(hadException);

  hadException = false;
  try {
    node2.appendChild(aRoot)
  }
  catch (e) {
    hadException = true;
  }
  assertTrue(hadException);

  assertEqual(rootParent, aRoot.parentNode);

  // Clean up
  aRoot.removeChild(node3);
  assertEqual(serializeTree(aRoot), '[node/]');
}


function testNodeByAttribute(SPS, aRoot)
{
  let methods = {id: "getNode", url: "getNodeForURL"};
  let attributes = ["id", "url"];

  let node1 = SPS.createNode();
  assertTrue(node1 instanceof Ci.sbIServicePaneNode);
  node1.id = "id_node1";
  node1.url = "url_node1";

  let node2 = SPS.createNode();
  assertTrue(node2 instanceof Ci.sbIServicePaneNode);
  node2.id = "id_node2";
  node2.url = "url_node2";

  // We didn't add the nodes yet, they shouldn't be returned
  for each (let attr in attributes) {
    let method = methods[attr];
    assertEqual(SPS[method](attr + "_node1"), null);
    assertEqual(SPS[method](attr + "_node2"), null);
  }

  // Add the nodes - they should be returned now
  aRoot.appendChild(node2);
  aRoot.insertBefore(node1, node2);
  for each (let attr in attributes) {
    let method = methods[attr];
    assertEqual(SPS[method](attr + "_node1"), node1);
    assertEqual(SPS[method](attr + "_node2"), node2);
  }

  // Moving nodes in the tree shouldn't change anything
  node1.appendChild(node2);
  for each (let attr in attributes) {
    let method = methods[attr];
    assertEqual(SPS[method](attr + "_node1"), node1);
    assertEqual(SPS[method](attr + "_node2"), node2);
  }

  // Removing the root node should remove its children as well
  aRoot.removeChild(node1);
  for each (let attr in attributes) {
    let method = methods[attr];
    assertEqual(SPS[method](attr + "_node1"), null);
    assertEqual(SPS[method](attr + "_node2"), null);
  }

  // Re-adding the root node should re-add its children as well
  aRoot.appendChild(node1);
  for each (let attr in attributes) {
    let method = methods[attr];
    assertEqual(SPS[method](attr + "_node1"), node1);
    assertEqual(SPS[method](attr + "_node2"), node2);
  }

  // Changing the attribute should make sure the node is returned by its new attribute
  node1.id = "id_node3";
  node1.url = "url_node3";
  for each (let attr in attributes) {
    let method = methods[attr];
    assertEqual(SPS[method](attr + "_node1"), null);
    assertEqual(SPS[method](attr + "_node2"), node2);
    assertEqual(SPS[method](attr + "_node3"), node1);
  }

  // Changing the attribute back should change everything back
  node1.id = "id_node1";
  node1.url = "url_node1";
  for each (let attr in attributes) {
    let method = methods[attr];
    assertEqual(SPS[method](attr + "_node1"), node1);
    assertEqual(SPS[method](attr + "_node2"), node2);
    assertEqual(SPS[method](attr + "_node3"), null);
  }

  // For duplicate IDs/URLs any one of the nodes should be returned
  node2.id = "id_node1";
  node2.url = "url_node1";
  for each (let attr in attributes) {
    let method = methods[attr];
    let node = SPS[method](attr + "_node1");
    assertTrue(node == node1 || node == node2);
  }

  // Changing the attribute back should restore consistency
  node2.id = "id_node2";
  node2.url = "url_node2";
  for each (let attr in attributes) {
    let method = methods[attr];
    assertEqual(SPS[method](attr + "_node1"), node1);
    assertEqual(SPS[method](attr + "_node2"), node2);
  }

  // Removing the attribute should make sure the node isn't returned
  node1.id = null;
  node1.url = null;
  for each (let attr in attributes) {
    let method = methods[attr];
    assertEqual(SPS[method](attr + "_node1"), null);
    assertEqual(SPS[method](attr + "_node2"), node2);
  }

  // Try getting nodes by generic attributes without namespaces
  let nodes;
  node1.setAttribute("foo", "bar");
  nodes = SPS.getNodesByAttributeNS(null, "foo", "bar");
  assertEqual(nodes.length, 1);
  assertEqual(nodes.queryElementAt(0, Ci.sbIServicePaneNode), node1);

  nodes = SPS.getNodesByAttributeNS(null, "foo", "not bar");
  assertEqual(nodes.length, 0);

  nodes = SPS.getNodesByAttributeNS("wrongns", "foo", "bar");
  assertEqual(nodes.length, 0);

  node2.setAttribute("foo", "not bar")
  nodes = SPS.getNodesByAttributeNS(null, "foo", "bar");
  assertEqual(nodes.length, 1);
  assertEqual(nodes.queryElementAt(0, Ci.sbIServicePaneNode), node1);

  node2.setAttribute("foo", "bar")
  nodes = SPS.getNodesByAttributeNS(null, "foo", "bar");
  assertEqual(nodes.length, 2);

  node1.removeAttribute("foo");
  node2.removeAttribute("foo");
  nodes = SPS.getNodesByAttributeNS(null, "foo", "bar");
  assertEqual(nodes.length, 0);

  // Now the same thing with namespaces
  node1.setAttributeNS("ns", "foo", "bar");
  nodes = SPS.getNodesByAttributeNS("ns", "foo", "bar");
  assertEqual(nodes.length, 1);
  assertEqual(nodes.queryElementAt(0, Ci.sbIServicePaneNode), node1);

  nodes = SPS.getNodesByAttributeNS("ns", "foo", "not bar");
  assertEqual(nodes.length, 0);

  nodes = SPS.getNodesByAttributeNS(null, "foo", "bar");
  assertEqual(nodes.length, 0);

  node2.setAttributeNS("ns", "foo", "not bar")
  nodes = SPS.getNodesByAttributeNS("ns", "foo", "bar");
  assertEqual(nodes.length, 1);
  assertEqual(nodes.queryElementAt(0, Ci.sbIServicePaneNode), node1);

  node2.setAttributeNS("ns", "foo", "bar")
  nodes = SPS.getNodesByAttributeNS("ns", "foo", "bar");
  assertEqual(nodes.length, 2);

  node1.removeAttributeNS("ns", "foo");
  node2.removeAttributeNS("ns", "foo");
  nodes = SPS.getNodesByAttributeNS("ns", "foo", "bar");
  assertEqual(nodes.length, 0);

  // Make sure that nodes aren't found by attribute outside document
  node1.setAttribute("foo", "bar");
  aRoot.removeChild(node1);
  nodes = SPS.getNodesByAttributeNS(null, "foo", "bar");
  assertEqual(nodes.length, 0);
  
  // Root node should still have no children after the test
  assertEqual(serializeTree(aRoot), '[node/]');
}


function testAddRemoveNode(SPS, aRoot) {
  // This should add a node to the root
  let node1 = SPS.addNode("node1", aRoot, true);
  assertTrue(node1 instanceof Ci.sbIServicePaneNode);
  assertEqual(node1.id, "node1");
  assertTrue(node1.isContainer);
  assertEqual(node1.parentNode, aRoot);
  assertEqual(aRoot.lastChild, node1);


  // Without an ID parameter a random ID should be generated
  let node2 = SPS.addNode(null, aRoot, true);
  assertTrue(node2 instanceof Ci.sbIServicePaneNode);
  assertNotEqual(node2.id, null);
  assertTrue(node2.isContainer);
  assertEqual(node2.parentNode, aRoot);
  assertEqual(node2.previousSibling, node1);
  assertEqual(node2.nextSibling, null);

  let node3 = SPS.addNode(null, aRoot, false);
  assertTrue(node3 instanceof Ci.sbIServicePaneNode);
  assertNotEqual(node3.id, null);
  assertNotEqual(node3.id, node2.id);
  assertFalse(node3.isContainer);

  // This should remove a node
  SPS.removeNode(node2);
  assertEqual(node2.parentNode, null);
  assertEqual(node1.nextSibling, node3);
  assertEqual(node3.previousSibling, node1);

  // Clean up
  aRoot.removeChild(node1);
  aRoot.removeChild(node3);
}


function testModuleInteraction(SPS, aRoot) {
  aRoot.id = "test-root";
  aRoot.editable = "true";

  let fakeElement = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMXULElement]),
    get wrappedJSObject() this
  };
  let fakeWindow = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMWindow]),
    get wrappedJSObject() this
  };

  // Adding module to category should call servicePaneInit with SPS as parameter
  let initArgs = testModule.addToCategory();
  assertEqual(initArgs.length, 1);
  assertEqual(initArgs[0], SPS);

  // fillContextMenu should get node by ID and pass other parameters to modules
  // unchanged
  testModule._fillContextMenuParams = null;
  SPS.fillContextMenu("test-root", fakeElement, fakeWindow);
  assertNotEqual(testModule._fillContextMenuParams, null);
  assertEqual(testModule._fillContextMenuParams.length, 3);
  assertEqual(testModule._fillContextMenuParams[0], aRoot);
  assertEqual(testModule._fillContextMenuParams[1].wrappedJSObject, fakeElement);
  assertEqual(testModule._fillContextMenuParams[2].wrappedJSObject, fakeWindow);

  // fillNewItemMenu should get node by ID and pass other parameters to modules
  // unchanged
  testModule._fillNewItemMenuParams = null;
  SPS.fillNewItemMenu("test-root", fakeElement, fakeWindow);
  assertNotEqual(testModule._fillNewItemMenuParams, null);
  assertEqual(testModule._fillNewItemMenuParams.length, 3);
  assertEqual(testModule._fillNewItemMenuParams[0], aRoot);
  assertEqual(testModule._fillNewItemMenuParams[1].wrappedJSObject, fakeElement);
  assertEqual(testModule._fillNewItemMenuParams[2].wrappedJSObject, fakeWindow);

  // onSelectionChanged should get node by ID and pass other parameters to modules
  // unchanged
  testModule._onSelectionChangedParams = null;
  SPS.onSelectionChanged("test-root", fakeElement, fakeWindow);
  assertNotEqual(testModule._onSelectionChangedParams, null);
  assertEqual(testModule._onSelectionChangedParams.length, 3);
  assertEqual(testModule._onSelectionChangedParams[0], aRoot);
  assertEqual(testModule._onSelectionChangedParams[1].wrappedJSObject, fakeElement);
  assertEqual(testModule._onSelectionChangedParams[2].wrappedJSObject, fakeWindow);

  // onBeforeRename should trigger node's owner
  aRoot.contractid = testModule.contractId;
  testModule._onBeforeRenameParams = null;
  SPS.onBeforeRename("test-root");
  assertNotEqual(testModule._onBeforeRenameParams, null);
  assertEqual(testModule._onBeforeRenameParams.length, 1);
  assertEqual(testModule._onBeforeRenameParams[0], aRoot);
  aRoot.contractid = null;

  // onRename should trigger node's owner
  aRoot.contractid = testModule.contractId;
  testModule._onRenameParams = null;
  SPS.onRename("test-root", "Dummy title");
  assertNotEqual(testModule._onRenameParams, null);
  assertEqual(testModule._onRenameParams.length, 2);
  assertEqual(testModule._onRenameParams[0], aRoot);
  assertEqual(testModule._onRenameParams[1], "Dummy title");
  aRoot.contractid = null;

  // After module's removal it should no longer be triggered
  testModule.removeFromCategory();
  
  testModule._fillContextMenuParams = null;
  SPS.fillContextMenu("test-root", fakeElement, fakeWindow);
  assertEqual(testModule._fillContextMenuParams, null);
  
  testModule._fillNewItemMenuParams = null;
  SPS.fillNewItemMenu("test-root", fakeElement, fakeWindow);
  assertEqual(testModule._fillNewItemMenuParams, null);

  testModule._onSelectionChangedParams = null;
  SPS.onSelectionChanged("test-root", fakeElement, fakeWindow);
  assertEqual(testModule._onSelectionChangedParams, null);

  aRoot.contractid = testModule.contractId;
  testModule._onRenameParams = null;
  SPS.onRename("test-root", "Dummy title");
  assertEqual(testModule._onRenameParams, null);
  aRoot.contractid = null;

  // Clean up
  aRoot.id = null;
  aRoot.removeAttribute("editable");
}


function testListeners (SPS) {
  var test_node_id = 'SB:ListenerTest';
  // if our test node already exists, let's throw it away
  removeIfExists(SPS, [test_node_id]);


  // define a listener
  var listener = { };
  // what calls has the listener recieved
  listener.calls = [];
  // the listener callback which collects the calls is receives
  listener.nodePropertyChanged = 
    function nodePropertyChanged(aNodeId, aPropertyName) {
      log('nodePropertyChanged('+aNodeId+', '+aPropertyName+')');
      this.calls.push([aNodeId, aPropertyName]);
    }


  // add our listener
  SPS.addListener(listener);
  // it should have no calls
  assertEqual(listener.calls.length, 0);

  // create a new node
  var test_node = SPS.addNode(test_node_id, SPS.root, false);
  // it should have six calls:
  //  http://songbirdnest.com/rdf/servicepane#Hidden)
  //  http://www.w3.org/1999/02/22-rdf-syntax-ns#nextVal)
  //  http://www.w3.org/1999/02/22-rdf-syntax-ns#nextVal)
  //  http://www.w3.org/1999/02/22-rdf-syntax-ns#_24)
  //  http://songbirdnest.com/rdf/servicepane#Editable)
  //  http://songbirdnest.com/rdf/servicepane#Weight)
  assertEqual(listener.calls.length, 6);

  // reset the listener call list
  listener.calls = []

  // set a property
  test_node.setAttributeNS('http://example.com/ns/','property', 'value');
  // it should have one call
  assertEqual(listener.calls.length, 1);
  // it should be on our node
  assertEqual(listener.calls[0][0], test_node_id);
  // it should be our test property
  assertEqual(listener.calls[0][1], 'http://example.com/ns/property');

  // reset the listener call list
  listener.calls = [];

  // remove the listener
  SPS.removeListener(listener);
  // it should have no calls
  assertEqual(listener.calls.length, 0);

  // manipulate our node
  test_node.setAttributeNS('http://example.com/ns/','otherproperty', 'value');
  // the listener should have no calls
  assertEqual(listener.calls.length, 0);

  // remove our test node
  SPS.root.removeChild(test_node);
}


function runTest (SPS) {
  let SPS = Cc["@songbirdnest.com/servicepane/service;1"].getService(Ci.sbIServicePaneService);
  
  // okay, did we get it?
  assertNotEqual(SPS, null);

  // Do we have a valid root?
  assertTrue(SPS.root instanceof Ci.sbIServicePaneNode);

  // Can we create new nodes?
  let testRoot = SPS.createNode();
  assertTrue(testRoot instanceof Ci.sbIServicePaneNode);

  // Test attribute functions while not in tree
  assertEqual(testRoot.parentNode, null);
  testAttributes(testRoot);

  // Now test them while in tree
  SPS.root.appendChild(testRoot);
  assertEqual(testRoot.parentNode, SPS.root);
  testAttributes(testRoot);

  // Test manipulation of a subtree that isn't part of the tree
  SPS.root.removeChild(testRoot);
  assertEqual(testRoot.parentNode, null);
  testTreeManipulation(SPS, testRoot);

  // Now test it while the subtree is attached to the tree
  SPS.root.appendChild(testRoot);
  assertEqual(testRoot.parentNode, SPS.root);
  testTreeManipulation(SPS, testRoot);

  // Test deprecated SPS.addNode/removeNode methods
  testAddRemoveNode(SPS, testRoot);

  // Test whether querying SPS for nodes works correctly
  testNodeByAttribute(SPS, testRoot);

  // Test functions that are forwarded to modules
  testModuleInteraction(SPS, testRoot);

  // test the listeners
  // TODO
  // testListeners(SPS);

  // Clean up
  SPS.root.removeChild(testRoot);
}
