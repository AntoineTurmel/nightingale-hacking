/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * \file  DOMUtils.jsm
 * \brief Javascript source for the DOM utility services.
 */

//------------------------------------------------------------------------------
//
// DOM utility JSM configuration.
//
//------------------------------------------------------------------------------

EXPORTED_SYMBOLS = [ "DOMUtils", "DOMEventListenerSet" ];


//------------------------------------------------------------------------------
//
// DOM utility defs.
//
//------------------------------------------------------------------------------

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results
const Cu = Components.utils


//------------------------------------------------------------------------------
//
// DOM utility services.
//
//------------------------------------------------------------------------------

var DOMUtils = {
  //----------------------------------------------------------------------------
  //
  // DOM utility services.
  //
  //----------------------------------------------------------------------------

  /**
   * Load a document using the URI string specified by aDocumentURI and return
   * it as an nsIDOMDocument.
   *
   * \param aDocumentURI        Document URI string.
   *
   * \return                    An nsIDOMDocument document.
   */

  loadDocument: function DOMUtils_loadDocument(aDocumentURI) {
    // Open the document.
    var ioSvc = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
    var channel = ioSvc.newChannel(aDocumentURI, null, null);
    var inputStream = channel.open();

    // Load and return the document.
    var domParser = Cc["@mozilla.org/xmlextras/domparser;1"]
                      .createInstance(Ci.nsIDOMParser);
    return domParser.parseFromStream(inputStream,
                                     null,
                                     channel.contentLength,
                                     "text/xml");
  },


  /**
   *   Import the child elements of the parent element with the ID specified by
   * aSrcParentID within the document specified by aSrcDocument.  Import them
   * as child elements of the node specified by aDstNode.
   *   For each imported child, set attributes as specified by the
   * aChildAttrList object.  Use the name of each field in aChildAttrList as the
   * name of an attribute, and set the attribute value to the field value.
   *   Note that only child elements are imported.  Non-element child nodes such
   * as text nodes are are not imported.  The descendents of all imported child
   * elements are also imported, including non-element descendents.
   *
   * \param aDstNode            Destination node into which to import child
   *                            elements.
   * \param aSrcDocument        Document from which to import child elements.
   * \param aSrcParentID        ID of parent node from which to import child
   *                            elements.
   * \param aChildAttrList      List of child attributes to set.
   */

  importChildElements: function DOMUtils_importChildElements(aDstNode,
                                                             aSrcDocument,
                                                             aSrcParentID,
                                                             aChildAttrList) {
    // Get the destination document and the list of source children.
    var dstDoc = aDstNode.ownerDocument;
    var srcChildList = aSrcDocument.getElementById(aSrcParentID).childNodes;

    // Import the source elements.
    for (var i = 0; i < srcChildList.length; i++) {
      // Get the next source child.  Skip if not an element.
      var srcChild = srcChildList[i];
      if (srcChild.nodeType != Ci.nsIDOMNode.ELEMENT_NODE)
        continue;

      // Import the source child into the destination document.
      dstChild = dstDoc.importNode(srcChild, true);

      // Add the child to the destination node.
      aDstNode.appendChild(dstChild);

      // Add the child attributes.
      for (var attrName in aChildAttrList) {
        dstChild.setAttribute(attrName, aChildAttrList[attrName]);
      }
    }
  },


  /**
   * Copy the attributes specified by aAttributeList from the element specified
   * by aSrcElem to the element specified by aDstElem.  If an attribute is not
   * set in aSrcElem, do not change that attribute in aDstElem unless
   * aRemoveAttributes is true; if aRemoveAttributes is true, remove the
   * attribute from aDstElem.
   *
   * \param aSrcElem            Source element.
   * \param aDstElem            Destination element.
   * \param aAttributeList      Array of attribute names.
   * \param aRemoveAttributes   Remove attributes from aDstElem that aren't set
   *                            in aSrcElem.
   */

  copyAttributes: function DOMUtils_copyAttributes(aSrcElem,
                                                   aDstElem,
                                                   aAttributeList,
                                                   aRemoveAttributes) {
    // Copy the attributes.
    for (var i = 0; i < aAttributeList.length; i++) {
      // Get attribute name.
      var attribute = aAttributeList[i];

      // If source element does not have the attribute, do nothing or remove
      // the attribute as specified.
      if (!aSrcElem.hasAttribute(attribute)) {
        if (aRemoveAttributes)
          aDstElem.removeAttribute(attribute);
        continue;
      }

      // Copy the attribute from the source element to the destination if the
      // attribute values differ.
      var srcAttributeVal = aSrcElem.getAttribute(attribute);
      var dstAttributeVal = aDstElem.getAttribute(attribute);
      if (srcAttributeVal != dstAttributeVal)
        aDstElem.setAttribute(attribute, srcAttributeVal);
    }
  },


  /**
   * Search the children of the element specified by aRootElem for elements
   * with the attribute specified by aAttrName set to the value specified by
   * aAttrValue.  Return an array containing all matching elements.
   * If aAttrValue is not specified, return all elements with the specified
   * attribute, regardless of value.
   *
   * \param aRootElem           Root element from which to start searching.
   * \param aAttrName           Attribute name to search for.
   * \param aAttrValue          Attribute value to search for.
   *
   * \return                    Array containing found elements.
   */

  getElementsByAttribute: function DOMUtils_getElementsByAttribute(aRootElem,
                                                                   aAttrName,
                                                                   aAttrValue) {
    // Start searching for elements from the root.
    var matchList = [];
    this._getElementsByAttribute(aRootElem, aAttrName, aAttrValue, matchList);

    return matchList;
  },

  _getElementsByAttribute: function
                             DOMUtils__getElementsByAttribute(aRootElem,
                                                              aAttrName,
                                                              aAttrValue,
                                                              aMatchList) {
    // Search each of the children.
    var childList = aRootElem.childNodes;
    for (var i = 0; i < childList.length; i++) {
      // Check the child node for a match.
      var child = childList[i];
      if (child.hasAttributes() && child.hasAttribute(aAttrName)) {
        if (!aAttrValue || (child.getAttribute(aAttrName) == aAttrValue))
          aMatchList.push(child);
      }

      // Check each of the child's children.
      this._getElementsByAttribute(child, aAttrName, aAttrValue, aMatchList);
    }
  },


  //----------------------------------------------------------------------------
  //
  // DOM node destruction services.
  //
  //   When a node is removed from a DOM tree, any XBL bindings bound to that
  // node or its children are not detached.  Thus, the binding destructors are
  // not called.  The XBL bindings are not detached until the owner document is
  // destroyed.  This can lead to memory leaks if XBL bound nodes are
  // dynamically added and removed from a document.
  //   The DOM node destruction services provide support for releasing XBL
  // binding resources before the XBL binding is detached.  An XBL binding may
  // add a destroy function by calling addNodeDestroyFunc.  Multiple destroy
  // functions may be added for an XBL binding (e.g., for extended bindings).
  //   Before a node is removed from a document, destroyNode should be called.
  // This function recursively calls the destroy functions for all of the nodes
  // children, including the anonymous children.  Note that destroyNode must be
  // called before node removal since node removal removes XBL binding content.
  // If destroyNode is called after node removal, anonymous child nodes will not
  // be destroyed.
  //
  //----------------------------------------------------------------------------

  /**
   * Add the destroy function specified by aFunc to the list of destroy
   * functions for the node specified by aNode.  The destroy function is only
   * called once and is removed from the destroy function list when called.
   *
   * \param aNode               Node for which to add destroy function.
   * \param aFunc               Destroy function.
   */

  addNodeDestroyFunc: function DOMUtils_addNodeDestroyFunc(aNode,
                                                           aFunc) {
    // Ensure the destroy function list exists.
    if (!aNode.destroyFuncList)
      aNode.destroyFuncList = [];

    // Push the destroy function on the end of the list.
    aNode.destroyFuncList.push(aFunc);
  },


  /**
   * Recursively call the destroy functions for all child nodes, including
   * anonymous nodes, for the node specified by aNode.
   *
   * \param aNode               Node to destroy.
   */

  destroyNode: function DOMUtils_destroyNode(aNode) {
    // Destroy all of the node's children, including the anonymous children.
    var nodeDocument = aNode.ownerDocument;
    this.destroyNodeList(nodeDocument.getAnonymousNodes(aNode));
    this.destroyNodeList(aNode.childNodes);

    // Call the node destroy functions.
    while (aNode.destroyFuncList) {
      // Pop the next destroy function from the end of the list and call it.
      var destroyFunc = aNode.destroyFuncList.pop();
      destroyFunc();

      // If the destroy function list is empty, remove it.
      if (!aNode.destroyFuncList.length)
        aNode.destroyFuncList = null;
    }
  },


  /**
   * Destroy all nodes in the list specified by aNodeList.
   *
   * \param aNodeList           Array list of nodes to destroy.
   */

  destroyNodeList: function DOMUtils_destroyNodeList(aNodeList) {
    if (!aNodeList)
      return;

    for (var i = 0; i < aNodeList.length; i++) {
      this.destroyNode(aNodeList[i]);
    }
  }
};


//------------------------------------------------------------------------------
//
// DOM event listener set services.
//
//   These services may be used to maintain a set of DOM event listeners and to
// facilitate the removal of DOM event listeners.
//
//------------------------------------------------------------------------------

/**
 * Construct a DOM event listener set object.
 */

function DOMEventListenerSet()
{
  // Initialize some fields.
  this._eventListenerList = {};
}

// Set constructor.
DOMEventListenerSet.prototype.constructor = DOMEventListenerSet;

// Define the class.
DOMEventListenerSet.prototype = {
  //
  // DOM event listener set fields.
  //
  //   _eventListenerList       List of event listeners.
  //   _nextEventListenerID     Next event listener ID.
  //

  _eventListenerList: null,
  _nextEventListenerID: 0,


  /**
   * Add an event listener for the element specified by aElement with the
   * parameters specified by aType, aListener, and aUseCapture.  Return an ID
   * that may be used to reference the added listener.
   *
   * \param aElement            Element for which to add an event listener.
   * \param aType               Type of event for which to listen.
   * \param aListener           Listener function.
   * \param aUseCapture         True if event capture should be used.
   *
   * \return                    Event listener ID.
   */

  add: function DOMEventListenerSet_add(aElement,
                                        aType,
                                        aListener,
                                        aUseCapture) {
    // Create the event listener object.
    var eventListener = {};
    eventListener.id = this._nextEventListenerID++;
    eventListener.element = aElement;
    eventListener.type = aType;
    eventListener.listener = aListener;
    eventListener.useCapture = aUseCapture;

    // Add the event listener.
    eventListener.element.addEventListener(eventListener.type,
                                           eventListener.listener,
                                           eventListener.useCapture);
    this._eventListenerList[eventListener.id] = eventListener;

    return (eventListener.id);
  },


  /**
   * Remove the event listener specified by aEventListenerID.
   *
   * \param aEventListenerID    ID of event listener to remove.
   */

  remove: function DOMEventListenerSet_remove(aEventListenerID) {
    // Get the event listener.  Do nothing if not found.
    var eventListener = this._eventListenerList[aEventListenerID];
    if (!eventListener)
      return;

    // Remove the event listener.
    eventListener.element.removeEventListener(eventListener.type,
                                              eventListener.listener,
                                              eventListener.useCapture);
    delete this._eventListenerList[aEventListenerID];
  },


  /**
   * Remove all event listeners.
   */

  removeAll: function DOMEventListenerSet_removeAll() {
    // Remove all event listeners.
    for (var id in this._eventListenerList) {
      this.remove(id);
    }
    this._eventListenerList = {};
  }
};

