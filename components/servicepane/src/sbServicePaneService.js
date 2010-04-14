/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 :miv */
/*
 *=BEGIN SONGBIRD GPL
 *
 * This file is part of the Songbird web player.
 *
 * Copyright(c) 2005-2010 POTI, Inc.
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

/**
 * \file ServicePaneService.js
 * \brief the service pane service manages the tree behind the service pane
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Ce = Components.Exception;
const Cu = Components.utils;

Components.utils.import("resource://app/jsmodules/XPCOMUtils.jsm");
Components.utils.import("resource://app/jsmodules/ArrayConverter.jsm");
Components.utils.import("resource://app/jsmodules/DebugUtils.jsm");
Components.utils.import("resource://app/jsmodules/StringUtils.jsm");

const SP="http://songbirdnest.com/rdf/servicepane#";

const LOG = DebugUtils.generateLogFunction("sbServicePaneService");

function deprecationWarning(msg) {
  // The code that called a deprecated function is two stack frames above us
  let caller = Components.stack;
  if (caller)
    caller = caller.caller;
  if (caller)
    caller = caller.caller;

  // Skip empty frames
  while (caller && !caller.filename)
    caller = caller.caller;

  let consoleService = Cc["@mozilla.org/consoleservice;1"]
                         .getService(Ci.nsIConsoleService);
  let scriptError = Cc["@mozilla.org/scripterror;1"]
                      .createInstance(Ci.nsIScriptError);
  scriptError.init(msg,
                   caller ? caller.filename : null,
                   caller ? caller.sourceLine : null,
                   caller ? caller.lineNumber : null,
                   0,
                   Ci.nsIScriptError.warningFlag,
                   "sbServicePaneService");
  consoleService.logMessage(scriptError);
}

function ServicePaneNode(servicePane, comparisonFunction) {
  this._servicePane = servicePane;
  this._comparisonFunction = comparisonFunction;
  this._attributes = {__proto__: null};
  this._childNodes = [];

  // Allow accessing internal properties when we get a wrapped node passed
  this.wrappedJSObject = this;
}

ServicePaneNode.prototype = {
  _attributes: null,
  _childNodes: null,
  _parentNode: null,
  _nodeIndex: null,
  _servicePane: null,
  _comparisonFunction: null,
  _stringBundle: null,
  _stringBundleURI: null,
  _isInTree: false,

  QueryInterface: XPCOMUtils.generateQI([Ci.sbIServicePaneNode]),

  // Generic properties

  get isContainer() {
    deprecationWarning("sbIServicePaneNode.isContainer is deprecated and may " +
                       "be removed in future. All nodes are containers now.");
    return this.getAttribute("isContainer") != "false";
  },

  get properties() {
    deprecationWarning("sbIServicePaneNode.properties is deprecated and " +
                       "may be removed in future. Consider using " +
                       "sbIServicePaneNode.className instead.");
    return this.className;
    
  },
  set properties(aValue) {
    deprecationWarning("sbIServicePaneNode.properties is deprecated and " +
                       "may be removed in future. Consider using " +
                       "sbIServicePaneNode.className instead.");
    return (this.className = aValue);
  },

  get displayName() {
    // Only names beginning with & should be translated
    let name = this.name;
    if (!name || name[0] != "&")
      return name;

    // Cache the string bundle so that we don't retrieve it more than once
    if (!this._stringBundle) {
      let stringbundleURI = this.stringbundle;
  
      if (!stringbundleURI)  {
        // Try module's stringbundle
        let contractid = this.contractid;
        if (contractid && contractid in this._servicePane._modulesByContractId) {
          try {
            let module = this._servicePane._modulesByContractId[contractid];
            stringbundleURI = module.stringbundle;
          }
          catch (e) {
            Components.utils.reportError(e);
          }
        }
      }
  
      try {
        this._stringBundle = new SBStringBundle(stringbundleURI);
        this._stringBundleURI = stringbundleURI;
      }
      catch (e) {
        LOG("sbServicePaneNode.displayName: failed retrieving string bundle " + stringbundleURI);
        Components.utils.reportError(e);
      }
    }

    // Try to get displayed name from string bundle - fall back to key name
    if (this._stringBundle)
      return this._stringBundle.get(name.substr(1), name);

    return name;
  },

  get isInTree() this._isInTree,
  set isInTree(aValue) {
    // Convert the parameter to a real boolean value, for sake of comparisons
    aValue = !!aValue;

    if (aValue == this._isInTree)
      return aValue;

    // Set the value and propagate the change to child nodes
    this._isInTree = aValue;
    for each (let child in this._childNodes)
      child.isInTree = aValue;

    // Update service pane data
    let notificationMethod = (aValue ? "_registerNode" : "_unregisterNode");
    for each (let attr in ["id", "url"])
      if (attr in this._attributes)
        this._servicePane[notificationMethod](attr, this._attributes[attr], this);

    return this._isInTree;
  },

  // Attribute functions without namespace

  hasAttribute: function(aName) aName in this._attributes,

  getAttribute: function(aName) {
    if (aName in this._attributes)
      return this._attributes[aName];
    else
      return null;
  },

  setAttribute: function(aName, aValue) {
    if (this.isInTree && (aName == "id" || aName == "url")) {
      if (aName in this._attributes)
        this._servicePane._unregisterNode(aName, this._attributes[aName], this);
      if (aValue !== null)
        this._servicePane._registerNode(aName, aValue, this);
    }
    else if (aName == "stringbundle" || aName == "contractid")
      delete this._stringBundle;

    if (aValue === null)
      delete this._attributes[aName];
    else
      this._attributes[aName] = aValue;

    return aValue;
  },

  removeAttribute: function(aName) {
    if (this.isInTree && (aName == "id" || aName == "url") &&
                          aName in this._attributes) {
      this._servicePane._unregisterNode(aName, this._attributes[aName], this);
    }
    else if (aName == "stringbundle" || aName == "contractid")
      delete this._stringBundle;

    delete this._attributes[aName];
  },

  // Attribute functions with namespace - these simply combine namespace and
  // attribute name and fall back to the regular attribute functions

  hasAttributeNS: function(aNamespace, aName)
    this.hasAttribute(aNamespace + ":" + aName),

  getAttributeNS: function(aNamespace, aName)
    this.getAttribute(aNamespace + ":" + aName),

  setAttributeNS: function(aNamespace, aName, aValue)
    this.setAttribute(aNamespace + ":" + aName, aValue),

  removeAttributeNS: function(aNamespace, aName)
    this.removeAttribute(aNamespace + ":" + aName),

  // DOM Properties

  get attributes() {
    let attrNames = [];
    for (let name in this._attributes)
      attrNames.push(name);
    return ArrayConverter.stringEnumerator(attrNames);
  },

  get childNodes() ArrayConverter.enumerator(this._childNodes),

  get firstChild() {
    if (this._childNodes.length)
      return this._childNodes[0];
    else
      return null;
  },

  get lastChild() {
    if (this._childNodes.length)
      return this._childNodes[this._childNodes.length - 1];
    else
      return null;
  },

  get parentNode() this._parentNode,

  get previousSibling() {
    let parent = this._parentNode;
    if (!parent)
      return null;

    let index = this._nodeIndex - 1;
    if (index >= 0)
      return parent._childNodes[index];
    else
      return null;
  },

  get nextSibling() {
    let parent = this._parentNode;
    if (!parent)
      return null;

    let index = this._nodeIndex + 1;
    if (index < parent._childNodes.length)
      return parent._childNodes[index];
    else
      return null;
  },

  // DOM methods

  appendChild: function(aChild) {
    return this.insertBefore(aChild, null);
  },

  insertBefore: function(aChild, aBefore) {
    // Unwrap parameters so that we can access internal properties
    if (aChild)
      aChild = aChild.wrappedJSObject;
    if (aBefore)
      aBefore = aBefore.wrappedJSObject;

    if (!aChild)
      throw Ce("Node to be inserted/appended is a required parameter");

    for (let parent = this; parent; parent = parent._parentNode)
      if (parent == aChild)
        throw Ce("Cannot insert/append a node to its child");

    if (this._comparisonFunction) {
      // Use comparison function to determine node position
      let index = 0;
      for (; index < this._childNodes.length; index++)
        if (this._comparisonFunction(aChild, this._childNodes[index]) >= 0)
          break;
      if (index < this._childNodes.length)
        aBefore = this._childNodes[index];
      else
        aBefore = null;
    }

    if (aBefore && aBefore._parentNode != this)
      throw Ce("Cannot insert before a node that isn't a child");

    if (aBefore == aChild)
      return aChild;    // nothing to do

    if (aChild._parentNode)
      aChild._parentNode.removeChild(aChild);

    let index = aBefore ? aBefore._nodeIndex : this._childNodes.length;
    for (let i = index; i < this._childNodes.length; i++)
      this._childNodes[i]._nodeIndex++;

    this._childNodes.splice(index, 0, aChild);
    aChild._nodeIndex = index;
    aChild._parentNode = this;
    aChild.isInTree = this.isInTree;
    return aChild;
  },

  removeChild: function(aChild) {
    // Unwrap parameters so that we can access internal properties
    if (aChild)
      aChild = aChild.wrappedJSObject;

    if (!aChild || aChild._parentNode != this)
      throw Ce("Cannot remove a node that isn't a child");

    let index = aChild._nodeIndex;
    for (let i = index + 1; i < this._childNodes.length; i++)
      this._childNodes[i]._nodeIndex--;

    this._childNodes.splice(index, 1);
    delete aChild._nodeIndex;
    delete aChild._parentNode;
    aChild.isInTree = null;
    return aChild;
  },

  replaceChild: function(aChild, aOldChild) {
    // Unwrap parameters so that we can access internal properties
    if (aChild)
      aChild = aChild.wrappedJSObject;
    if (aOldChild)
      aOldChild = aOldChild.wrappedJSObject;

    if (!aOldChild || aOldChild._parentNode != this)
      throw Ce("Cannot replace a node that isn't a child");

    let insertBefore = aOldChild.nextSibling;
    this.removeChild(aOldChild);
    this.insertBefore(aChild, insertBefore);
  },

  // Attribute shortcuts

  get id() this.getAttribute("id"),
  set id(aValue) this.setAttribute("id", aValue),
  get className() this.getAttribute("class"),
  set className(aValue) this.setAttribute("class", aValue),
  get url() this.getAttribute("url"),
  set url(aValue) this.setAttribute("url", aValue),
  get image() this.getAttribute("image"),
  set image(aValue) this.setAttribute("image", aValue),
  get name() this.getAttribute("name"),
  set name(aValue) this.setAttribute("name", aValue),
  get tooltip() this.getAttribute("tooltip"),
  set tooltip(aValue) this.setAttribute("tooltip", aValue),
  get hidden() this.getAttribute("hidden") == "true",
  set hidden(aValue) {
    if (!aValue)
      this.removeAttributeNS(SP, "HideList");
    return this.setAttribute("hidden", aValue ? "true" : "false");
  },
  get editable() this.getAttribute("editable") == "true",
  set editable(aValue) this.setAttribute("editable", aValue ? "true" : "false"),
  get isOpen() this.getAttribute("isOpen") != "false",
  set isOpen(aValue) this.setAttribute("isOpen", aValue ? "true" : "false"),
  get contractid() this.getAttribute("contractid"),
  set contractid(aValue) this.setAttribute("contractid", aValue),
  get stringbundle() this.getAttribute("stringbundle"),
  set stringbundle(aValue) this.setAttribute("stringbundle", aValue),
  get dndDragTypes() this.getAttribute("dndDragTypes"),
  set dndDragTypes(aValue) this.setAttribute("dndDragTypes", aValue),
  get dndAcceptNear() this.getAttribute("dndAcceptNear"),
  set dndAcceptNear(aValue) this.setAttribute("dndAcceptNear", aValue),
  get dndAcceptIn() this.getAttribute("dndAcceptIn"),
  set dndAcceptIn(aValue) this.setAttribute("dndAcceptIn", aValue),
};


function ServicePaneService () {
  LOG("Service pane initialization started");

  // Create root node - children are sorted automatically

  this._nodesById = {__proto__: null};
  this._nodesByUrl = {__proto__: null};
  this._root = new ServicePaneNode(this, function(aNode1, aNode2) {
    // Nodes with lower weight go first
    let weight1 = parseInt(aNode1.getAttributeNS(SP, 'Weight')) || 0;
    let weight2 = parseInt(aNode2.getAttributeNS(SP, 'Weight')) || 0;
    if (weight1 != weight2)
      return weight1 - weight2;

    // For equal weight, sort alphabetically
    let name1 = aNode1.displayName;
    let name2 = aNode2.displayName;
    if (name1 < name2)
      return -1;
    else if (name1 > name2)
      return 1;

    return 0;
  });
  this._root.isInTree = true;

  // Initialize modules

  let catMgr = Cc["@mozilla.org/categorymanager;1"]
                 .getService(Ci.nsICategoryManager);
  let enumerator = catMgr.enumerateCategory("service-pane");
  let moduleEntries = ArrayConverter.JSArray(enumerator).map(
    function(entry) entry.QueryInterface(Ci.nsISupportsCString).data
  );
  moduleEntries.sort();

  this._modules = [];
  this._modulesByContractId = {__proto__: null};
  this._categoryEntriesCache = {__proto__: null};
  for each (let entry in moduleEntries)
    this._loadModule(catMgr, entry);
  LOG("Service pane initialization completed successfully");

  // Listen for modules being added or removed
  let observerService = Cc["@mozilla.org/observer-service;1"]
                          .getService(Ci.nsIObserverService);
  observerService.addObserver(this, "xpcom-category-entry-added", true);
  observerService.addObserver(this, "xpcom-category-entry-removed", true);
}

ServicePaneService.prototype = {
  // XPCOM component info
  classID: Components.ID("{eb5c665a-bfe2-49f1-a747-cd3554e55606}"),
  classDescription: "Songbird Service Pane Service",
  contractID: "@songbirdnest.com/servicepane/service;1",

  _modules: null,
  _modulesByContractId: null,
  _categoryEntriesCache: null,
  _root: null,
  _nodesById: null,
  _nodesByUrl: null,
  _nodeIndex: 0,

  get root() this._root,

  QueryInterface: XPCOMUtils.generateQI([Ci.sbIServicePaneService,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function(subject, topic, data) {
    switch (topic) {
      case "xpcom-category-entry-added":
        if (data == "service-pane" && subject instanceof Ci.nsISupportsCString) {
          let catMgr = Cc["@mozilla.org/categorymanager;1"]
                         .getService(Ci.nsICategoryManager);
          this._loadModule(catMgr, subject.data);
        }
        break;
      case "xpcom-category-entry-removed":
        if (data == "service-pane" && subject instanceof Ci.nsISupportsCString) {
          this._removeModule(subject.data);
        }
        break;
    }
  },

  _loadModule: function ServicePaneService__loadModule(catMgr, entry) {
    let contractId = catMgr.getCategoryEntry("service-pane", entry);
    this._categoryEntriesCache[entry] = contractId;

    // Don't load if already loaded
    if (contractId in this._modulesByContractId)
      return;

    LOG("Trying to load service pane module: " + contractId);
    try {
      let module = Cc[contractId].getService(Ci.sbIServicePaneModule);
      module.servicePaneInit(this);
      this._modules.push(module);
      this._modulesByContractId[contractId] = module;
      LOG("Service pane module successfully initialized");
    } catch (e) {
      LOG("Error instantiating service pane module: " + e);
    }
  },

  _removeModule: function ServicePaneService__removeModule(entry) {
    // Don't bother if we don't know this module
    if (!(entry in this._categoryEntriesCache))
      return;
    let contractId = this._categoryEntriesCache[entry];
    if (!(contractId in this._modulesByContractId))
      return;

    let module = this._modulesByContractId[contractId];
    for (let i = 0; i < this._modules.length; i++)
      if (this._modules[i] == module)
        this._modules.splice(i--, 1);
    delete this._modulesByContractId[contractId];
    LOG("Service pane module " + contractId + " removed");
  },

  init: function ServicePaneService_init() {
    deprecationWarning("sbIServicePaneService.init() is deprecated, you no " +
                       "longer need to call it.");
  },

  createNode: function ServicePaneService_createNode() {
    return new ServicePaneNode(this, null);
  },

  _registerNode: function ServicePaneService__registerNode(
                                                      aAttr, aValue, aNode) {
    let table;
    if (aAttr == "id")
      table = this._nodesById;
    else if (aAttr == "url")
      table = this._nodesByUrl;
    else
      return;

    if (!(aValue in table))
      table[aValue] = [];

    table[aValue].push(aNode);
  },

  _unregisterNode: function ServicePaneService__unregisterNode(
                                                      aAttr, aValue, aNode) {
    let table;
    if (aAttr == "id")
      table = this._nodesById;
    else if (aAttr == "url")
      table = this._nodesByUrl;
    else
      return;

    if (aValue in table)
    {
      let list = table[aValue];
      for (let i = 0; i < list.length; i++)
        if (list[i] == aNode)
          list.splice(i--, 1);
    }
  },

  getNode: function ServicePaneService_getNode(aId) {
    if (aId in this._nodesById && this._nodesById[aId].length) {
      if (this._nodesById[aId].length > 1) {
        // Warn if multiple nodes with same ID exist
        let caller = Components.stack.caller;

        // Skip empty frames
        while (caller && !caller.filename)
          caller = caller.caller;

        let consoleService = Cc["@mozilla.org/consoleservice;1"]
                               .getService(Ci.nsIConsoleService);
        let scriptError = Cc["@mozilla.org/scripterror;1"]
                            .createInstance(Ci.nsIScriptError);
        scriptError.init("Multiple service pane nodes with ID '" + aId + "' exist, only returning one node.",
                         caller ? caller.filename : null,
                         caller ? caller.sourceLine : null,
                         caller ? caller.lineNumber : null,
                         0,
                         Ci.nsIScriptError.warningFlag,
                         "sbServicePaneService");
        consoleService.logMessage(scriptError);
      }
      return this._nodesById[aId][0];
    }
    else
      return null;
  },

  getNodeForURL: function ServicePaneService_getNodeForURL(aUrl) {
    if (aUrl in this._nodesByUrl && this._nodesByUrl[aUrl].length)
      return this._nodesByUrl[aUrl][0];
    else
      return null;
  },

  getNodesByAttributeNS: function ServicePaneService_getNodesByAttributeNS(
                                                  aNamespace, aName, aValue) {
    function findNodeRecursive(aNode, aAttrName, aValue, aResult) {
      for each (let child in aNode._childNodes) {
        if (child.getAttribute(aAttrName) == aValue)
          aResult.push(child);

        findNodeRecursive(child, aAttrName, aValue, aResult);
      }
    }

    let result = [];
    let attrName = (aNamespace == null ? aName : aNamespace + ":" + aName);
    findNodeRecursive(this._root, attrName, aValue, result);
    return ArrayConverter.nsIArray(result);
  },

  addListener: function ServicePaneService_addListener(aListener) {
    // TODO
  },

  removeListener: function ServicePaneService_removeListener(aListener) {
    // TODO
  },

  fillContextMenu: function ServicePaneService_fillContextMenu(
                                          aId, aContextMenu, aParentWindow) {
    let node = aId ? this.getNode(aId) : null;
    for each (let module in this._modules) {
      try {
        module.fillContextMenu(node, aContextMenu, aParentWindow);
      } catch (ex) {
        Components.utils.reportError(ex);
      }
    }
  },

  fillNewItemMenu: function ServicePaneService_fillNewItemMenu(
                                          aId, aContextMenu, aParentWindow) {
    let node = aId ? this.getNode(aId) : null;
    for each (let module in this._modules) {
      try {
        module.fillNewItemMenu(node, aContextMenu, aParentWindow);
      } catch (ex) {
        Components.utils.reportError(ex);
      }
    }
  },

  onSelectionChanged: function ServicePaneService_onSelectionChanged(
                                          aId, aContainer, aParentWindow) {
    let node = aId ? this.getNode(aId) : null;
    for each (let module in this._modules) {
      try {
        module.onSelectionChanged(node, aContainer, aParentWindow);
      } catch (ex) {
        Components.utils.reportError(ex);
      }
    }
  },

  /**
   * Called before a node is renamed by the user.
   * Delegates to the module that owns the given node.
   */
  onBeforeRename: function ServicePaneService_onBeforeRename(aID) {
    let node = this.getNode(aID);
    if (!node || !node.editable)
      return;
  
    // Pass the message on to the node owner
    if (node.contractid && node.contractid in this._modulesByContractId) {
      let module = this._modulesByContractId[node.contractid];
      module.onBeforeRename(node);
    }
  },

  /**
   * Called when a node is renamed by the user.
   * Delegates to the module that owns the given node.
   */
  onRename: function ServicePaneService_onRename(aID, aNewName) {
    let node = this.getNode(aID);
    if (!node || !node.editable)
      return;
  
    // Pass the message on to the node owner
    if (node.contractid && node.contractid in this._modulesByContractId) {
      let module = this._modulesByContractId[node.contractid];
      module.onRename(node, aNewName);
    }
  },

  addNode: function ServicePaneService_addNode(aId, aParent, aContainer) {
    deprecationWarning("sbIServicePaneService.addNode() is deprecated and " +
                       "may be removed in future. Consider using " +
                       "sbIServicePaneService.createNode() instead and " +
                       "adding it to the parent yourself.");

    if (!aParent)
      throw Ce("You need to supply a parent for addNode().");

    if (aId && this.getNode(aId)) {
      // Original implementation returned null for attempts to duplicate a node
      return null;
    }

    let node = this.createNode();
    node.id = (aId !== null ? aId : "_generatedNodeId" + ++this._nodeIndex);
    if (!aContainer)
      node.setAttribute("isContainer", "false");
    aParent.appendChild(node);

    return node;
  },

  removeNode: function ServicePaneService_removeNode(aNode) {
    deprecationWarning("sbIServicePaneService.removeNode() is deprecated and " +
                       "may be removed in future. Consider using " +
                       "sbIServicePaneNode.removeChild() instead.");

    if (!aNode)
      throw Ce("You need to supply a node for removeNode().");
    if (!aNode.parentNode)
      throw Ce("Cannot remove a node that doesn't have a parent.");

    aNode.parentNode.removeChild(aNode);
  },

  setNodeHidden: function ServicePaneService_setNodeHidden(
                                                  aNode, aContractID, aHide) {
    // Get the list of components that have hidden the node.
    let hideList = aNode.getAttributeNS(SP, "HideList");
    if (hideList) {
      hideList = hideList.split(" ");
    }
    else {
      hideList = [];
    }
  
    // If hiding, add the component to the list of components hiding the node.
    // Otherwise, remove the component from the list.
    let hideIndex = hideList.indexOf(aContractID);
    if (aHide) {
      if (hideIndex < 0)
        hideList.push(aContractID);
    }
    else {
      while (hideIndex >= 0) {
        hideList.splice(hideIndex, 1);
        hideIndex = hideList.indexOf(aContractID);
      }
    }
  
    // Update the list of components hiding the node.  Set the node as hidden if
    // any component is hiding it.
    aNode.setAttributeNS(SP, "HideList", hideList.join(" "));
    aNode.hidden = (hideList.length > 0);
  },

  sortNode: function ServicePaneService_sortNode() {
    deprecationWarning("sbIServicePaneService.sortNode() is deprecated, you no " +
                       "longer need to call it.");
  },

  save: function ServicePaneService_save() {
    deprecationWarning("sbIServicePaneService.save() is deprecated, you no " +
                       "longer need to call it.");
  },

  _canDropReorder: function ServicePaneService__canDropReorder(
                                          aNode, aDragSession, aOrientation) {
    // see if we can handle the drag and drop based on node properties
    let types = [];
    if (aOrientation == 0) {
      // drop in
      if (aNode.dndAcceptIn) {
        types = aNode.dndAcceptIn.split(',');
      }
    } else {
      // drop near
      if (aNode.dndAcceptNear) {
        types = aNode.dndAcceptNear.split(',');
      }
    }
    for each (let type in types) {
      if (aDragSession.isDataFlavorSupported(type)) {
        return type;
      }
    }
    return null;
  },

  canDrop: function ServicePaneService_canDrop(
                                  aId, aDragSession, aOrientation, aWindow) {
    LOG("canDrop(" + aId + ")");

    let node = this.getNode(aId);
    if (!node) {
      return false;
    }
  
    // see if we can handle the drag and drop based on node properties
    if (this._canDropReorder(node, aDragSession, aOrientation)) {
      return true;
    }
  
    // let the module that owns this node handle this
    if (node.contractid && node.contractid in this._modulesByContractId) {
      let module = this._modulesByContractId[node.contractid];
      return module.canDrop(node, aDragSession, aOrientation, aWindow);
    }
    return false;
  },

  onDrop: function ServicePaneService_onDrop(
                                    aId, aDragSession, aOrientation, aWindow) {
    LOG("onDrop(" + aId + ")");

    let node = this.getNode(aId);
    if (!node) {
      return;
    }
    // see if this is a reorder we can handle based on node properties
    let type = this._canDropReorder(node, aDragSession, aOrientation);
    if (type) {
      // we're in business
  
      // do the dance to get our data out of the dnd system
      // create an nsITransferable
      let transferable = Cc["@mozilla.org/widget/transferable;1"]
                           .createInstance(Ci.nsITransferable);
      // specify what kind of data we want it to contain
      transferable.addDataFlavor(type);
      // ask the drag session to fill the transferable with that data
      aDragSession.getData(transferable, 0);
      // get the data from the transferable
      let data = {};
      transferable.getTransferData(type, data, {});
      // it's always a string. always.
      data = data.value.QueryInterface(Ci.nsISupportsString).data;
  
      // for drag and drop reordering the data is just the servicepane node id
      let droppedNode = this.getNode(data);
  
      // fail if we can't get the node or it is the node we are over
      if (!droppedNode || node == droppedNode) {
        return;
      }
  
      if (aOrientation == 0) {
        // drop into
        node.appendChild(droppedNode);
      } else if (aOrientation > 0) {
        // drop after
        node.parentNode.insertBefore(droppedNode, node.nextSibling);
      } else {
        // drop before
        node.parentNode.insertBefore(droppedNode, node);
      }
      return;
    }
  
    // or let the module that owns this node handle it
    if (node.contractid && node.contractid in this._modulesByContractId) {
      let module = this._modulesByContractId[node.contractid];
      module.onDrop(node, aDragSession, aOrientation, aWindow);
    }
  },

  onDragGesture: function ServicePaneService_onDragGesture(aId, aTransferable) {
    LOG("onDragGesture(" + aId + ")");
  
    let node = this.getNode(aId);
    if (!node) {
      return false;
    }
  
    let success = false;
  
    // create a transferable
    let transferable = Cc["@mozilla.org/widget/transferable;1"]
                         .createInstance(Ci.nsITransferable);
  
    // get drag types from the node data
    if (node.dndDragTypes) {
      let types = node.dndDragTypes.split(',');
      for each (let type in types) {
        transferable.addDataFlavor(type);
        let text = Components.classes["@mozilla.org/supports-string;1"].
           createInstance(Components.interfaces.nsISupportsString);
        text.data = node.id;
        // double the length - it's unicode - this is stupid
        transferable.setTransferData(type, text, text.data.length * 2);
        success = true;
      }
    }
  
    if (node.contractid && node.contractid in this._modulesByContractId) {
      let module = this._modulesByContractId[node.contractid];
      if (module.onDragGesture(node, transferable)) {
        success = true;
      }
    }
  
    if (success) {
      aTransferable.value = transferable;
    }
  
    LOG(" success=" + success);
  
    return success;
  }
};

var NSGetModule = XPCOMUtils.generateNSGetModule([ServicePaneService]);
