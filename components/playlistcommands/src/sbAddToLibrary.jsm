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

Components.utils.import("resource://app/jsmodules/sbProperties.jsm");
Components.utils.import("resource://app/jsmodules/DropHelper.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

const ADDTOLIBRARY_MENU_TYPE      = "menuitem";
const ADDTOLIBRARY_MENU_ID        = "library_cmd_addtolibrary";
const ADDTOLIBRARY_MENU_NAME      = "&command.addtolibrary";
const ADDTOLIBRARY_MENU_TOOLTIP   = "&command.tooltip.addtolibrary";
const ADDTOLIBRARY_MENU_KEY       = "&command.shortcut.key.addtolibrary";
const ADDTOLIBRARY_MENU_KEYCODE   = "&command.shortcut.keycode.addtolibrary";
const ADDTOLIBRARY_MENU_MODIFIERS = "&command.shortcut.modifiers.addtolibrary";


const ADDTOLIBRARY_COMMAND_ID = "library_cmd_addtolibrary";

EXPORTED_SYMBOLS = [ "addToLibraryHelper",
                     "SBPlaylistCommand_AddToLibrary" ];

// ----------------------------------------------------------------------------
// The "Add to device" dynamic command object
// ----------------------------------------------------------------------------
var SBPlaylistCommand_AddToLibrary =
{
  context: {
    playlist: null,
    window: null
  },

  addToLibrary: null,

  root_commands :
  {
    types: [ADDTOLIBRARY_MENU_TYPE],
    ids: [ADDTOLIBRARY_MENU_ID],
    names: [ADDTOLIBRARY_MENU_NAME],
    tooltips: [ADDTOLIBRARY_MENU_TOOLTIP],
    keys: [ADDTOLIBRARY_MENU_KEY],
    keycodes: [ADDTOLIBRARY_MENU_KEYCODE],
    enableds: [true],
    modifiers: [ADDTOLIBRARY_MENU_MODIFIERS],
    playlistCommands: [null]
  },

  _getMenu: function(aSubMenu)
  {
    return this.root_commands;
  },

  getVisible: function( aHost )
  {
    return true;
  },

  getNumCommands: function( aSubMenu, aHost )
  {
    return this._getMenu(aSubMenu).ids.length;
  },

  getCommandId: function( aSubMenu, aIndex, aHost )
  {
    return this._getMenu(aSubMenu).ids[ aIndex ] || "";
  },

  getCommandType: function( aSubMenu, aIndex, aHost )
  {
    return this._getMenu(aSubMenu).types[ aIndex ] || "";
  },

  getCommandText: function( aSubMenu, aIndex, aHost )
  {
    return this._getMenu(aSubMenu).names[ aIndex ] || "";
  },

  getCommandFlex: function( aSubMenu, aIndex, aHost )
  {
    return ( this._getMenu(aSubMenu).types[ aIndex ] == "separator" ) ? 1 : 0;
  },

  getCommandToolTipText: function( aSubMenu, aIndex, aHost )
  {
    return this._getMenu(aSubMenu).tooltips[ aIndex ] || "";
  },

  getCommandValue: function( aSubMenu, aIndex, aHost )
  {
  },

  instantiateCustomCommand: function( aDocument, aId, aHost )
  {
    return null;
  },

  refreshCustomCommand: function( aElement, aId, aHost )
  {
  },

  getCommandVisible: function( aSubMenu, aIndex, aHost )
  {
    return true;
  },

  getCommandFlag: function( aSubmenu, aIndex, aHost )
  {
    return false;
  },

  getCommandChoiceItem: function( aChoiceMenu, aHost )
  {
    return "";
  },

  getCommandEnabled: function( aSubMenu, aIndex, aHost )
  {
    if (this.context.playlist.tree.currentIndex == -1) return false;
    return this._getMenu(aSubMenu).enableds[ aIndex ];
  },

  getCommandShortcutModifiers: function ( aSubMenu, aIndex, aHost )
  {
    return this._getMenu(aSubMenu).modifiers[ aIndex ] || "";
  },

  getCommandShortcutKey: function ( aSubMenu, aIndex, aHost )
  {
    return this._getMenu(aSubMenu).keys[ aIndex ] || "";
  },

  getCommandShortcutKeycode: function ( aSubMenu, aIndex, aHost )
  {
    return this._getMenu(aSubMenu).keycodes[ aIndex ] || "";
  },

  getCommandShortcutLocal: function ( aSubMenu, aIndex, aHost )
  {
    return true;
  },

  getCommandSubObject: function ( aSubMenu, aIndex, aHost )
  {
    return this._getMenu(aSubMenu).playlistCommands[ aIndex ] || null;
  },

  onCommand: function( aSubMenu, aIndex, aHost, id, value )
  {
    this.addToLibrary.handleCommand(id, this.context);
  },

  // The object registered with the sbIPlaylistCommandsManager interface acts
  // as a template for instances bound to specific playlist elements

  dupObject: function (obj) {
    var r = {};
    for ( var i in obj )
    {
      r[ i ] = obj[ i ];
    }
    return r;
  },

  duplicate: function()
  {
    var obj = this.dupObject(this);
    obj.context = this.dupObject(this.context);
    return obj;
  },

  initCommands: function(aHost) {
    this.addToLibrary = new addToLibraryHelper();
  },

  shutdownCommands: function() {
    this.context = null;
  },

  setContext: function( context )
  {
    var playlist = context.playlist;
    var window = context.window;

    // Ah.  Sometimes, things are being secure.

    if ( playlist && playlist.wrappedJSObject )
      playlist = playlist.wrappedJSObject;

    if ( window && window.wrappedJSObject )
      window = window.wrappedJSObject;

    this.context.playlist = playlist;
    this.context.window = window;
  },

  QueryInterface : function(aIID)
  {
    if (!aIID.equals(Components.interfaces.sbIPlaylistCommands) &&
        !aIID.equals(Components.interfaces.nsISupportsWeakReference) &&
        !aIID.equals(Components.interfaces.nsISupports))
    {
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
  }
}; // SBPlaylistCommand_Addtolibrary declaration

function addToLibraryHelper() {}
addToLibraryHelper.prototype = {
  deviceManager: Cc["@songbirdnest.com/Songbird/DeviceManager;2"]
      .getService(Ci.sbIDeviceManager2),
  libraryManager: Cc["@songbirdnest.com/Songbird/library/Manager;1"]
      .getService(Ci.sbILibraryManager),

  // handle click on a device command item
  handleCommand: function addtolibraryHelper_handleCommand(id, context) {
    try {
      if ( id == ADDTOLIBRARY_COMMAND_ID) {
        var library = this.libraryManager.mainLibrary;
        if (library) {
          var oldLength = library.length;
          var selection = context.playlist.mediaListView
                                 .selection.selectedMediaItems;
          library.addSome(selection);
        }
        var added = library.length - oldLength;
        DNDUtils.reportAddedTracks(added, 0, library.name);
        return true;
      }
    } catch (e) {
      Components.utils.reportError(e);
    }
    return false;
  }
};