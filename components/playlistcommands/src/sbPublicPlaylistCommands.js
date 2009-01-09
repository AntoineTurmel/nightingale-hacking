// JScript source code
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

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://app/jsmodules/ArrayConverter.jsm");
Cu.import("resource://app/jsmodules/sbProperties.jsm");
Cu.import("resource://app/jsmodules/kPlaylistCommands.jsm");
Cu.import("resource://app/jsmodules/sbAddToPlaylist.jsm");
Cu.import("resource://app/jsmodules/sbAddToDevice.jsm");
Cu.import("resource://app/jsmodules/sbLibraryUtils.jsm");
Cu.import("resource://app/jsmodules/DropHelper.jsm");
Cu.import("resource://app/jsmodules/SBJobUtils.jsm");

const WEB_PLAYLIST_CONTEXT      = "webplaylist";
const WEB_PLAYLIST_TABLE        = "webplaylist";
const WEB_PLAYLIST_TABLE_NAME   = "&device.webplaylist";
const WEB_PLAYLIST_LIBRARY_NAME = "&device.weblibrary";

function SelectionUnwrapper(selection) {
  this._selection = selection;
}

SelectionUnwrapper.prototype = {
  _selection: null,

  hasMoreElements : function() {
    return this._selection.hasMoreElements();
  },

  getNext : function() {
    var item = this._selection.getNext().mediaItem;
    item.setProperty(SBProperties.downloadStatusTarget,
                     item.library.guid + "," + item.guid);
    return item;
  },

  QueryInterface : function(iid) {
    if (iid.equals(Ci.nsISimpleEnumerator) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  }
}

function PublicPlaylistCommands() {
  this.m_mgr = Components.
    classes["@songbirdnest.com/Songbird/PlaylistCommandsManager;1"]
    .getService(Ci.sbIPlaylistCommandsManager);

  var obs = Cc["@mozilla.org/observer-service;1"]
              .getService(Ci.nsIObserverService);
  obs.addObserver(this, "final-ui-startup", false);
}

PublicPlaylistCommands.prototype.constructor = PublicPlaylistCommands;

PublicPlaylistCommands.prototype = {

  classDescription: "Songbird Public Playlist Commands",
  classID:          Components.ID("{1126ee77-2d85-4f79-a07a-b014da404e53}"),
  contractID:       "@songbirdnest.com/Songbird/PublicPlaylistCommands;1",

  m_defaultCommands               : null,
  m_webPlaylistCommands           : null,
  m_webMediaHistoryToolbarCommands: null,
  m_smartPlaylistsCommands        : null,
  m_downloadCommands              : null,
  m_downloadToolbarCommands       : null,
  m_downloadCommandsServicePane   : null,
  m_serviceTreeDefaultCommands    : null,
  m_mgr                           : null,

  // Define various playlist commands, they will be exposed to the playlist commands
  // manager so that they can later be retrieved and concatenated into bigger
  // playlist commands objects.

  // Commands that act on playlist items
  m_cmd_Play                      : null, // play the selected track
  m_cmd_Remove                    : null, // remove the selected track(s)
  m_cmd_Edit                      : null, // edit the selected track(s)
  m_cmd_Download                  : null, // download the selected track(s)
  m_cmd_Rescan                    : null, // rescan the selected track(s)
  m_cmd_Reveal                    : null, // reveal the selected track
  m_cmd_CopyTrackLocation         : null, // copy the select track(s) location(s)
  m_cmd_ShowDownloadPlaylist      : null, // switch the browser to show the download playlist
  m_cmd_PauseResumeDownload       : null, // auto-switching pause/resume track download
  m_cmd_CleanUpDownloads          : null, // clean up completed download items
  m_cmd_ClearHistory              : null, // clear the web media history
  m_cmd_UpdateSmartPlaylist       : null, // update the smart playlist
  m_cmd_EditSmartPlaylist         : null, // edit  the smart playlist properties
  m_cmd_SmartPlaylistSep          : null, // a separator (its own object for visible cb)
  
  // Commands that act on playlist themselves
  m_cmd_list_Remove               : null, // remove the selected playlist
  m_cmd_list_Rename               : null, // rename the selected playlist

  // ==========================================================================
  // INIT
  // ==========================================================================
  initCommands: function() {
    try {
      if (this.m_mgr.request(kPlaylistCommands.MEDIAITEM_DEFAULT)) {
        // already initialized ! bail out !
        return;
      }

      // Add an observer for the application shutdown event, so that we can
      // shutdown our commands

      var obs = Cc["@mozilla.org/observer-service;1"]
                  .getService(Ci.nsIObserverService);
      obs.removeObserver(this, "final-ui-startup");
      obs.addObserver(this, "quit-application", false);

      // --------------------------------------------------------------------------

      var prefs = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefBranch2);

      // --------------------------------------------------------------------------

      // Build playlist command actions

      // --------------------------------------------------------------------------
      // The PLAY button is made of two commands, one that is always there for
      // all instantiators, the other that is only created by the shortcuts
      // instantiator. This makes one toolbar button and menu item, and two
      // shortcut keys.
      // --------------------------------------------------------------------------

      const PlaylistCommandsBuilder = new Components.
        Constructor("@songbirdnest.com/Songbird/PlaylistCommandsBuilder;1",
                    "sbIPlaylistCommandsBuilder");

      this.m_cmd_Play = new PlaylistCommandsBuilder();

      // The first item, always created
      this.m_cmd_Play.appendAction(null,
                                   "library_cmd_play",
                                   "&command.play",
                                   "&command.tooltip.play",
                                   plCmd_Play_TriggerCallback);

      this.m_cmd_Play.setCommandShortcut(null,
                                         "library_cmd_play",
                                         "&command.shortcut.key.play",
                                         "&command.shortcut.keycode.play",
                                         "&command.shortcut.modifiers.play",
                                         true);

      this.m_cmd_Play.setCommandEnabledCallback(null,
                                                "library_cmd_play",
                                                plCmd_IsAnyTrackSelected);

      // The second item, only created by the keyboard shortcuts instantiator
      this.m_cmd_Play.appendAction(null,
                                   "library_cmd_play2",
                                   "&command.play",
                                   "&command.tooltip.play",
                                   plCmd_Play_TriggerCallback);

      this.m_cmd_Play.setCommandShortcut(null,
                                         "library_cmd_play2",
                                         "&command.shortcut.key.altplay",
                                         "&command.shortcut.keycode.altplay",
                                         "&command.shortcut.modifiers.altplay",
                                         true);

      this.m_cmd_Play.setCommandEnabledCallback(null,
                                                "library_cmd_play2",
                                                plCmd_IsAnyTrackSelected);

      this.m_cmd_Play.setCommandVisibleCallback(null,
                                                "library_cmd_play2",
                                                plCmd_IsShortcutsInstantiator);

      // --------------------------------------------------------------------------
      // The REMOVE button, like the play button, is made of two commands, one that
      // is always there for all instantiators, the other that is only created by
      // the shortcuts instantiator. This makes one toolbar button and menu item,
      // and two shortcut keys.
      // --------------------------------------------------------------------------

      this.m_cmd_Remove = new PlaylistCommandsBuilder();

      // The first item, always created
      this.m_cmd_Remove.appendAction(null,
                                     "library_cmd_remove",
                                     "&command.remove",
                                     "&command.tooltip.remove",
                                     plCmd_Remove_TriggerCallback);

      this.m_cmd_Remove.setCommandShortcut(null,
                                           "library_cmd_remove",
                                           "&command.shortcut.key.remove",
                                           "&command.shortcut.keycode.remove",
                                           "&command.shortcut.modifiers.remove",
                                           true);

      this.m_cmd_Remove.setCommandEnabledCallback(null,
                                                  "library_cmd_remove",
                                                  plCmd_AND(
                                                    plCmd_IsAnyTrackSelected,
                                                    plCmd_CanModifyPlaylistContent));

      // The second item, only created by the keyboard shortcuts instantiator,
      // and only for the mac.
      var sysInfo = Components.classes["@mozilla.org/system-info;1"]
                    .getService(Components.interfaces.nsIPropertyBag2);
      if ( sysInfo.getProperty("name") == "Darwin" ) {
        this.m_cmd_Remove.appendAction(null,
                                       "library_cmd_remove2",
                                       "&command.remove",
                                       "&command.tooltip.remove",
                                       plCmd_Remove_TriggerCallback);

        this.m_cmd_Remove.setCommandShortcut(null,
                                             "library_cmd_remove2",
                                             "&command.shortcut.key.altremove",
                                             "&command.shortcut.keycode.altremove",
                                             "&command.shortcut.modifiers.altremove",
                                             true);

        this.m_cmd_Remove.setCommandEnabledCallback(null,
                                                    "library_cmd_remove2",
                                                    plCmd_AND(
                                                      plCmd_IsAnyTrackSelected,
                                                      plCmd_CanModifyPlaylistContent));

        this.m_cmd_Remove.setCommandVisibleCallback(null,
                                                    "library_cmd_remove2",
                                                    plCmd_IsShortcutsInstantiator);
      }

      // --------------------------------------------------------------------------
      // The EDIT button
      // --------------------------------------------------------------------------

      this.m_cmd_Edit = new PlaylistCommandsBuilder();

      this.m_cmd_Edit.appendAction(null,
                                   "library_cmd_edit",
                                   "&command.edit",
                                   "&command.tooltip.edit",
                                   plCmd_Edit_TriggerCallback);

      this.m_cmd_Edit.setCommandShortcut(null,
                                         "library_cmd_edit",
                                         "&command.shortcut.key.edit",
                                         "&command.shortcut.keycode.edit",
                                         "&command.shortcut.modifiers.edit",
                                         true);

      this.m_cmd_Edit.setCommandEnabledCallback(null,
                                                "library_cmd_edit",
                                                plCmd_IsAnyTrackSelected);

      this.m_cmd_Edit.setCommandVisibleCallback(null,
                                                "library_cmd_edit",
                                                plCmd_CanModifyPlaylist);

      this.m_cmd_Edit.appendAction(null,
                                   "library_cmd_edit_readonly",
                                   "&command.edit.readonly",
                                   "&command.tooltip.edit.readonly",
                                   plCmd_Edit_TriggerCallback);

      this.m_cmd_Edit.setCommandShortcut(null,
                                         "library_cmd_edit_readonly",
                                         "&command.shortcut.key.edit",
                                         "&command.shortcut.keycode.edit",
                                         "&command.shortcut.modifiers.edit",
                                         true);

      this.m_cmd_Edit.setCommandEnabledCallback(null,
                                                "library_cmd_edit_readonly",
                                                plCmd_IsAnyTrackSelected);

      this.m_cmd_Edit.setCommandVisibleCallback(null,
                                                "library_cmd_edit_readonly",
                                                plCmd_NOT(
                                                  plCmd_CanModifyPlaylist));

      // --------------------------------------------------------------------------
      // The DOWNLOAD button
      // --------------------------------------------------------------------------

      this.m_cmd_Download = new PlaylistCommandsBuilder();

      this.m_cmd_Download.appendAction(null,
                                       "library_cmd_download",
                                       "&command.download",
                                       "&command.tooltip.download",
                                       plCmd_Download_TriggerCallback);

      this.m_cmd_Download.setCommandShortcut(null,
                                             "library_cmd_download",
                                             "&command.shortcut.key.download",
                                             "&command.shortcut.keycode.download",
                                             "&command.shortcut.modifiers.download",
                                             true);

      this.m_cmd_Download.setCommandEnabledCallback(null,
                                                    "library_cmd_download",
                                                    plCmd_Download_EnabledCallback);

      // --------------------------------------------------------------------------
      // The RESCAN button
      // --------------------------------------------------------------------------


      this.m_cmd_Rescan = new PlaylistCommandsBuilder();

      this.m_cmd_Rescan.appendAction(null,
                                     "library_cmd_rescan",
                                     "&command.rescan",
                                     "&command.tooltip.rescan",
                                     plCmd_Rescan_TriggerCallback);

      this.m_cmd_Rescan.setCommandShortcut(null,
                                           "library_cmd_rescan",
                                           "&command.shortcut.key.rescan",
                                           "&command.shortcut.keycode.rescan",
                                           "&command.shortcut.modifiers.rescan",
                                           true);

      this.m_cmd_Rescan.setCommandEnabledCallback(null,
                                                  "library_cmd_rescan",
                                                  plCmd_IsAnyTrackSelected);

      this.m_cmd_Rescan.setCommandVisibleCallback(null,
                                                  "library_cmd_rescan",
                                                  plCmd_IsRescanItemEnabled);

      // --------------------------------------------------------------------------
      // The REVEAL button
      // --------------------------------------------------------------------------


      this.m_cmd_Reveal = new PlaylistCommandsBuilder();

      this.m_cmd_Reveal.appendAction(null,
                                     "library_cmd_reveal",
                                     "&command.reveal",
                                     "&command.tooltip.reveal",
                                     plCmd_Reveal_TriggerCallback);

      this.m_cmd_Reveal.setCommandShortcut(null,
                                           "library_cmd_reveal",
                                           "&command.shortcut.key.reveal",
                                           "&command.shortcut.keycode.reveal",
                                           "&command.shortcut.modifiers.reveal",
                                           true);

      function plCmd_isSelectionRevealable(aContext, aSubMenuId, aCommandId, aHost) {
        var selection = unwrap(aContext.playlist).mediaListView.selection;
        if (selection.count != 1) { return false; }
        var items = selection.selectedIndexedMediaItems;
        var item = items.getNext().QueryInterface(Ci.sbIIndexedMediaItem).mediaItem;
        return (item.contentSrc.scheme == "file")
      }
      this.m_cmd_Reveal.setCommandEnabledCallback(null,
                                                  "library_cmd_reveal",
                                                  plCmd_isSelectionRevealable);

      // --------------------------------------------------------------------------
      // The COPY TRACK LOCATION button
      // --------------------------------------------------------------------------

      this.m_cmd_CopyTrackLocation = new PlaylistCommandsBuilder();

      this.m_cmd_CopyTrackLocation.appendAction(null,
                                                "library_cmd_copylocation",
                                                "&command.copylocation",
                                                "&command.tooltip.copylocation",
                                                plCmd_CopyTrackLocation_TriggerCallback);

      this.m_cmd_CopyTrackLocation.setCommandShortcut(null,
                                                      "library_cmd_copylocation",
                                                      "&command.shortcut.key.copylocation",
                                                      "&command.shortcut.keycode.copylocation",
                                                      "&command.shortcut.modifiers.copylocation",
                                                      true);

      this.m_cmd_CopyTrackLocation.setCommandEnabledCallback(null,
                                                             "library_cmd_copylocation",
                                                             plCmd_IsAnyTrackSelected);

      // --------------------------------------------------------------------------
      // The CLEAR HISTORY button
      // --------------------------------------------------------------------------

      this.m_cmd_ClearHistory = new PlaylistCommandsBuilder();

      this.m_cmd_ClearHistory.appendAction
                                        (null,
                                         "library_cmd_clearhistory",
                                         "&command.clearhistory",
                                         "&command.tooltip.clearhistory",
                                         plCmd_ClearHistory_TriggerCallback);

      this.m_cmd_ClearHistory.setCommandShortcut
                                  (null,
                                   "library_cmd_clearhistory",
                                   "&command.shortcut.key.clearhistory",
                                   "&command.shortcut.keycode.clearhistory",
                                   "&command.shortcut.modifiers.clearhistory",
                                   true);

      this.m_cmd_ClearHistory.setCommandEnabledCallback
                                                (null,
                                                 "library_cmd_clearhistory",
                                                 plCmd_WebMediaHistoryHasItems);

      // --------------------------------------------------------------------------
      // The SHOW DOWNLOAD PLAYLIST button
      // --------------------------------------------------------------------------

      this.m_cmd_ShowDownloadPlaylist = new PlaylistCommandsBuilder();

      this.m_cmd_ShowDownloadPlaylist.appendAction(null,
                                                  "library_cmd_showdlplaylist",
                                                  "&command.showdlplaylist",
                                                  "&command.tooltip.showdlplaylist",
                                                  plCmd_ShowDownloadPlaylist_TriggerCallback);

      this.m_cmd_ShowDownloadPlaylist.setCommandShortcut(null,
                                                        "library_cmd_showdlplaylist",
                                                        "&command.shortcut.key.showdlplaylist",
                                                        "&command.shortcut.keycode.showdlplaylist",
                                                        "&command.shortcut.modifiers.showdlplaylist",
                                                        true);

      this.m_cmd_ShowDownloadPlaylist.setCommandVisibleCallback(null,
                                                                "library_cmd_showdlplaylist",
                                                                plCmd_ContextHasBrowser);

      // --------------------------------------------------------------------------
      // The PAUSE/RESUME DOWNLOAD button
      // --------------------------------------------------------------------------

      this.m_cmd_PauseResumeDownload = new PlaylistCommandsBuilder();

      this.m_cmd_PauseResumeDownload.appendAction(null,
                                                  "library_cmd_pause",
                                                  "&command.pausedl",
                                                  "&command.tooltip.pause",
                                                  plCmd_PauseResumeDownload_TriggerCallback);

      this.m_cmd_PauseResumeDownload.setCommandShortcut(null,
                                                        "library_cmd_pause",
                                                        "&command.shortcut.key.pause",
                                                        "&command.shortcut.keycode.pause",
                                                        "&command.shortcut.modifiers.pause",
                                                        true);

      this.m_cmd_PauseResumeDownload.setCommandVisibleCallback(null,
                                                      "library_cmd_pause",
                                                      plCmd_IsDownloadingOrNotActive);

      this.m_cmd_PauseResumeDownload.setCommandEnabledCallback(null,
                                                      "library_cmd_pause",
                                                      plCmd_IsDownloadActive);

      this.m_cmd_PauseResumeDownload.appendAction(null,
                                                  "library_cmd_resume",
                                                  "&command.resumedl",
                                                  "&command.tooltip.resume",
                                                  plCmd_PauseResumeDownload_TriggerCallback);

      this.m_cmd_PauseResumeDownload.setCommandShortcut(null,
                                                        "library_cmd_resume",
                                                        "&command.shortcut.key.resume",
                                                        "&command.shortcut.keycode.resume",
                                                        "&command.shortcut.modifiers.resume",
                                                        true);

      this.m_cmd_PauseResumeDownload.setCommandVisibleCallback(null,
                                                      "library_cmd_resume",
                                                      plCmd_IsDownloadPaused);

      // --------------------------------------------------------------------------
      // The CLEAN UP DOWNLOADS button
      // --------------------------------------------------------------------------

      this.m_cmd_CleanUpDownloads = new PlaylistCommandsBuilder();

      this.m_cmd_CleanUpDownloads.appendAction
                                        (null,
                                         "library_cmd_cleanupdownloads",
                                         "&command.cleanupdownloads",
                                         "&command.tooltip.cleanupdownloads",
                                         plCmd_CleanUpDownloads_TriggerCallback);

      this.m_cmd_CleanUpDownloads.setCommandShortcut
                                  (null,
                                   "library_cmd_cleanupdownloads",
                                   "&command.shortcut.key.cleanupdownloads",
                                   "&command.shortcut.keycode.cleanupdownloads",
                                   "&command.shortcut.modifiers.cleanupdownloads",
                                   true);

      this.m_cmd_CleanUpDownloads.setCommandEnabledCallback
                                                (null,
                                                 "library_cmd_cleanupdownloads",
                                                 plCmd_DownloadHasCompletedItems);

      // --------------------------------------------------------------------------
      // The Remove Playlist action
      // --------------------------------------------------------------------------

      this.m_cmd_list_Remove = new PlaylistCommandsBuilder();

      this.m_cmd_list_Remove.appendAction(null,
                                         "playlist_cmd_remove",
                                         "&command.playlist.remove",
                                         "&command.tooltip.playlist.remove",
                                         plCmd_RemoveList_TriggerCallback);

      this.m_cmd_list_Remove.setCommandShortcut(null,
                                               "playlist_cmd_remove",
                                               "&command.playlist.shortcut.key.remove",
                                               "&command.playlist.shortcut.keycode.remove",
                                               "&command.playlist.shortcut.modifiers.remove",
                                               true);

      // disable the command for readonly playlists. 
      this.m_cmd_list_Remove.setCommandEnabledCallback
                                                (null,
                                                 "playlist_cmd_remove",
                                                 plCmd_CanModifyPlaylist);

      // --------------------------------------------------------------------------
      // The Rename Playlist action
      // --------------------------------------------------------------------------

      this.m_cmd_list_Rename = new PlaylistCommandsBuilder();

      this.m_cmd_list_Rename.appendAction(null,
                                         "playlist_cmd_rename",
                                         "&command.playlist.rename",
                                         "&command.tooltip.playlist.rename",
                                         plCmd_RenameList_TriggerCallback);

      this.m_cmd_list_Rename.setCommandShortcut(null,
                                               "playlist_cmd_rename",
                                               "&command.playlist.shortcut.key.rename",
                                               "&command.playlist.shortcut.keycode.rename",
                                               "&command.playlist.shortcut.modifiers.rename",
                                               true);

      // disable the command for readonly playlists.
      this.m_cmd_list_Rename.setCommandEnabledCallback(null,
                                                       "playlist_cmd_rename",
                                                       plCmd_CanModifyPlaylist);

      // --------------------------------------------------------------------------

      // Publish atomic commands

      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_PLAY, this.m_cmd_Play);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_REMOVE, this.m_cmd_Remove);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_EDIT, this.m_cmd_Edit);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_DOWNLOAD, this.m_cmd_Download);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_RESCAN, this.m_cmd_Rescan);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_REVEAL, this.m_cmd_Reveal);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_ADDTOPLAYLIST, SBPlaylistCommand_AddToPlaylist);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_ADDTODEVICE, SBPlaylistCommand_AddToDevice);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_COPYTRACKLOCATION, this.m_cmd_CopyTrackLocation);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_SHOWDOWNLOADPLAYLIST, this.m_cmd_ShowDownloadPlaylist);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_PAUSERESUMEDOWNLOAD, this.m_cmd_PauseResumeDownload);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_CLEANUPDOWNLOADS, this.m_cmd_CleanUpDownloads);
      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_CLEARHISTORY, this.m_cmd_ClearHistory);

      this.m_mgr.publish(kPlaylistCommands.MEDIALIST_REMOVE, this.m_cmd_list_Remove);
      this.m_mgr.publish(kPlaylistCommands.MEDIALIST_RENAME, this.m_cmd_list_Rename);

      // --------------------------------------------------------------------------
      // Construct and publish the main library commands
      // --------------------------------------------------------------------------

      this.m_defaultCommands = new PlaylistCommandsBuilder();

      this.m_defaultCommands.appendPlaylistCommands(null,
                                                    "library_cmdobj_edit",
                                                    this.m_cmd_Edit);
      this.m_defaultCommands.appendPlaylistCommands(null,
                                                    "library_cmdobj_reveal",
                                                    this.m_cmd_Reveal);
      
      this.m_defaultCommands.appendSeparator(null, "default_commands_separator_1");
      
      this.m_defaultCommands.appendPlaylistCommands(null,
                                                    "library_cmdobj_addtoplaylist",
                                                    SBPlaylistCommand_AddToPlaylist);
      this.m_defaultCommands.appendPlaylistCommands(null,
                                                    "library_cmdobj_addtodevice",
                                                    SBPlaylistCommand_AddToDevice);
      
      this.m_defaultCommands.appendSeparator(null, "default_commands_separator_2");

      this.m_defaultCommands.appendPlaylistCommands(null,
                                                    "library_cmdobj_rescan",
                                                    this.m_cmd_Rescan);

      this.m_defaultCommands.appendPlaylistCommands(null,
                                                    "library_cmdobj_remove",
                                                    this.m_cmd_Remove);
      
      this.m_defaultCommands.setVisibleCallback(plCmd_ShowDefaultInToolbarCheck);

      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_DEFAULT, this.m_defaultCommands);
      

      // --------------------------------------------------------------------------
      // Construct and publish the smart playlists commands
      // --------------------------------------------------------------------------

      this.m_cmd_UpdateSmartPlaylist = new PlaylistCommandsBuilder();

      this.m_cmd_UpdateSmartPlaylist.appendAction
                                        (null,
                                         "smartpl_cmd_update",
                                         "&command.smartpl.update",
                                         "&command.tooltip.smartpl.update",
                                         plCmd_UpdateSmartPlaylist_TriggerCallback);

      this.m_cmd_UpdateSmartPlaylist.setCommandShortcut
                                  (null,
                                   "smartpl_cmd_update",
                                   "&command.shortcut.key.smartpl.update",
                                   "&command.shortcut.keycode.smartpl.update",
                                   "&command.shortcut.modifiers.smartpl.update",
                                   true);

     this.m_cmd_UpdateSmartPlaylist.setCommandVisibleCallback(null, 
                                                              "smartpl_cmd_update",
                                                              plCmd_NOT(plCmd_isLiveUpdateSmartPlaylist));

      this.m_cmd_EditSmartPlaylist = new PlaylistCommandsBuilder();

      this.m_cmd_EditSmartPlaylist.appendAction
                                        (null,
                                         "smartpl_cmd_properties",
                                         "&command.smartpl.properties",
                                         "&command.tooltip.smartpl.properties",
                                         plCmd_EditSmartPlaylist_TriggerCallback);

      this.m_cmd_EditSmartPlaylist.setCommandShortcut
                                  (null,
                                   "smartpl_cmd_properties",
                                   "&command.shortcut.key.smartpl.properties",
                                   "&command.shortcut.keycode.smartpl.properties",
                                   "&command.shortcut.modifiers.smartpl.properties",
                                   true);

      this.m_cmd_EditSmartPlaylist.setCommandVisibleCallback(null, 
                                                             "smartpl_cmd_properties",
                                                             plCmd_CanModifyPlaylist);

      this.m_smartPlaylistsCommands = new PlaylistCommandsBuilder();
      
      this.m_smartPlaylistsCommands.appendPlaylistCommands(null,
                                                           "library_cmdobj_defaults",
                                                           this.m_defaultCommands);

      this.m_cmd_SmartPlaylistSep = new PlaylistCommandsBuilder();

      this.m_cmd_SmartPlaylistSep.appendSeparator(null, "smartpl_separator");
      
      this.m_cmd_SmartPlaylistSep.setVisibleCallback(plCmd_NOT(plCmd_ShowForToolbarCheck));

      this.m_smartPlaylistsCommands.appendPlaylistCommands(null,
                                                           "smartpl_cmdobj_sep",
                                                           this.m_cmd_SmartPlaylistSep);
                                                           
      this.m_smartPlaylistsCommands.appendPlaylistCommands(null,
                                                           "smartpl_cmdobj_update",
                                                           this.m_cmd_UpdateSmartPlaylist);

      this.m_smartPlaylistsCommands.appendPlaylistCommands(null,
                                                           "smartpl_cmdobj_properties",
                                                           this.m_cmd_EditSmartPlaylist);
      
      this.m_mgr.publish(kPlaylistCommands.MEDIALIST_UPDATESMARTMEDIALIST, this.m_cmd_UpdateSmartPlaylist);
      this.m_mgr.publish(kPlaylistCommands.MEDIALIST_EDITSMARTMEDIALIST, this.m_cmd_EditSmartPlaylist);

      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_SMARTPLAYLIST, this.m_smartPlaylistsCommands);
      this.m_mgr.registerPlaylistCommandsMediaItem("", "smart", this.m_smartPlaylistsCommands);

      // --------------------------------------------------------------------------
      // Construct and publish the web playlist commands
      // --------------------------------------------------------------------------

      this.m_webPlaylistCommands = new PlaylistCommandsBuilder();

      this.m_webPlaylistCommands.appendPlaylistCommands(null,
                                                        "library_cmdobj_play",
                                                        this.m_cmd_Play);
      this.m_webPlaylistCommands.appendPlaylistCommands(null,
                                                        "library_cmdobj_remove",
                                                        this.m_cmd_Remove);
      this.m_webPlaylistCommands.appendPlaylistCommands(null,
                                                        "library_cmdobj_download",
                                                        this.m_cmd_Download);
                                                        
                                                        
      this.m_webPlaylistCommands.appendPlaylistCommands(null,
                                                        "library_cmdobj_addtoplaylist",
                                                        SBPlaylistCommand_DownloadToPlaylist);

      this.m_webPlaylistCommands.appendPlaylistCommands(null,
                                                        "library_cmdobj_copylocation",
                                                        this.m_cmd_CopyTrackLocation);
      this.m_webPlaylistCommands.appendSeparator(null, "separator");
      this.m_webPlaylistCommands.appendPlaylistCommands(null,
                                                        "library_cmdobj_showdlplaylist",
                                                        this.m_cmd_ShowDownloadPlaylist);

      this.m_webPlaylistCommands.setVisibleCallback(plCmd_ShowDefaultInToolbarCheck);

      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_WEBPLAYLIST, this.m_webPlaylistCommands);

      // Register these commands to the web playlist

      var webListGUID =
        prefs.getComplexValue("songbird.library.web",
                              Ci.nsISupportsString);

      this.m_mgr.registerPlaylistCommandsMediaItem(webListGUID, "", this.m_webPlaylistCommands);

      // --------------------------------------------------------------------------
      // Construct and publish the web media history toolbar commands
      // --------------------------------------------------------------------------

      this.m_webMediaHistoryToolbarCommands = new PlaylistCommandsBuilder();

      this.m_webMediaHistoryToolbarCommands.appendPlaylistCommands
                                              (null,
                                               "library_cmdobj_clearhistory",
                                               this.m_cmd_ClearHistory);

      this.m_webMediaHistoryToolbarCommands.setVisibleCallback
                                                      (plCmd_ShowForToolbarCheck);

      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_WEBTOOLBAR,
                         this.m_webMediaHistoryToolbarCommands);

      // Register these commands to the web media history library

      this.m_mgr.registerPlaylistCommandsMediaItem
                                                (webListGUID,
                                                 "",
                                                 this.m_webMediaHistoryToolbarCommands);
      // --------------------------------------------------------------------------
      // Construct and publish the download playlist commands
      // --------------------------------------------------------------------------

      this.m_downloadCommands = new PlaylistCommandsBuilder();

      this.m_downloadCommands.appendPlaylistCommands(null,
                                                     "library_cmdobj_play",
                                                     this.m_cmd_Play);
      this.m_downloadCommands.appendPlaylistCommands(null,
                                                     "library_cmdobj_remove",
                                                     this.m_cmd_Remove);
      this.m_downloadCommands.appendPlaylistCommands(null,
                                                     "library_cmdobj_pauseresumedownload",
                                                     this.m_cmd_PauseResumeDownload);

      this.m_downloadCommands.setVisibleCallback(plCmd_HideForToolbarCheck);

      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_DOWNLOADPLAYLIST, this.m_downloadCommands);

      // Get the download device
      getDownloadDevice();

      // Register these commands to the download playlist

      var downloadListGUID =
        prefs.getComplexValue("songbird.library.download",
                              Ci.nsISupportsString);

      this.m_mgr.registerPlaylistCommandsMediaItem(downloadListGUID, "", this.m_downloadCommands);

      // --------------------------------------------------------------------------
      // Construct and publish the download toolbar commands
      // --------------------------------------------------------------------------

      this.m_downloadToolbarCommands = new PlaylistCommandsBuilder();

      this.m_downloadToolbarCommands.appendPlaylistCommands
                                              (null,
                                               "library_cmdobj_cleanupdownloads",
                                               this.m_cmd_CleanUpDownloads);
      this.m_downloadToolbarCommands.appendPlaylistCommands
                                            (null,
                                             "library_cmdobj_pauseresumedownload",
                                             this.m_cmd_PauseResumeDownload);

      this.m_downloadToolbarCommands.setVisibleCallback
                                                      (plCmd_ShowForToolbarCheck);
      this.m_downloadToolbarCommands.setInitCallback(plCmd_DownloadInit);
      this.m_downloadToolbarCommands.setShutdownCallback(plCmd_DownloadShutdown);

      this.m_mgr.publish(kPlaylistCommands.MEDIAITEM_DOWNLOADTOOLBAR,
                         this.m_downloadToolbarCommands);

      // Register these commands to the download playlist

      this.m_mgr.registerPlaylistCommandsMediaItem
                                                (downloadListGUID,
                                                 "",
                                                 this.m_downloadToolbarCommands);

      // --------------------------------------------------------------------------
      // Construct and publish the download service tree commands
      // --------------------------------------------------------------------------

      this.m_downloadCommandsServicePane = new PlaylistCommandsBuilder();

      this.m_downloadCommandsServicePane.
        appendPlaylistCommands(null,
                               "library_cmdobj_pauseresumedownload",
                               this.m_cmd_PauseResumeDownload);

      this.m_downloadCommandsServicePane.setInitCallback(plCmd_DownloadInit);
      this.m_downloadCommandsServicePane.setShutdownCallback(plCmd_DownloadShutdown);

      this.m_mgr.publish(kPlaylistCommands.MEDIALIST_DOWNLOADPLAYLIST, this.m_downloadCommandsServicePane);

      // Register these commands to the download playlist
      var downloadListGUID =
        prefs.getComplexValue("songbird.library.download",
                              Ci.nsISupportsString);

      this.m_mgr.registerPlaylistCommandsMediaList(downloadListGUID, "", this.m_downloadCommandsServicePane);

      // --------------------------------------------------------------------------
      // Construct and publish the service tree playlist commands
      // --------------------------------------------------------------------------

      this.m_serviceTreeDefaultCommands = new PlaylistCommandsBuilder();

      this.m_serviceTreeDefaultCommands.appendPlaylistCommands(null,
                                              "servicetree_cmdobj_remove",
                                              this.m_cmd_list_Remove);

      this.m_serviceTreeDefaultCommands.setCommandEnabledCallback(null,
                                              "servicetree_cmdobj_remove",
                                              plCmd_CanModifyPlaylist);
                                              
      this.m_serviceTreeDefaultCommands.appendPlaylistCommands(null,
                                              "servicetree_cmdobj_rename",
                                              this.m_cmd_list_Rename);

      this.m_serviceTreeDefaultCommands.setCommandEnabledCallback(null,
                                              "servicetree_cmdobj_rename",
                                              plCmd_CanModifyPlaylist);
      
      this.m_mgr.publish(kPlaylistCommands.MEDIALIST_DEFAULT, this.m_serviceTreeDefaultCommands);

      // Register these commands to simple and smart playlists

      this.m_mgr.registerPlaylistCommandsMediaList( "", "simple", this.m_serviceTreeDefaultCommands );
      this.m_mgr.registerPlaylistCommandsMediaList( "", "smart",  this.m_serviceTreeDefaultCommands );
    } catch (e) {
      Cu.reportError(e);
    }

    // notify observers that the default commands are now ready
    var observerService = Cc["@mozilla.org/observer-service;1"]
                            .getService(Ci.nsIObserverService);
    observerService.notifyObservers(null, "playlist-commands-ready", "default");
  },

  // ==========================================================================
  // SHUTDOWN
  // ==========================================================================
  shutdownCommands: function() {
    // notify observers that the default commands are going away
    var observerService = Cc["@mozilla.org/observer-service;1"]
                            .getService(Ci.nsIObserverService);
    observerService.notifyObservers(null, "playlist-commands-shutdown", "default");

    var prefs = Cc["@mozilla.org/preferences-service;1"]
                  .getService(Ci.nsIPrefBranch2);

    // Un-publish atomic commands

    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_PLAY, this.m_cmd_Play);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_REMOVE, this.m_cmd_Remove);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_EDIT, this.m_cmd_Edit);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_DOWNLOAD, this.m_cmd_Download);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_RESCAN, this.m_cmd_Rescan);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_REVEAL, this.m_cmd_Reveal);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_ADDTOPLAYLIST, SBPlaylistCommand_AddToPlaylist);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_ADDTODEVICE, SBPlaylistCommand_AddToDevice);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_COPYTRACKLOCATION, this.m_cmd_CopyTrackLocation);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_SHOWDOWNLOADPLAYLIST, this.m_cmd_ShowDownloadPlaylist);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_PAUSERESUMEDOWNLOAD, this.m_cmd_PauseResumeDownload);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_CLEANUPDOWNLOADS, this.m_cmd_CleanUpDownloads);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_CLEARHISTORY, this.m_cmd_ClearHistory);

    this.m_mgr.withdraw(kPlaylistCommands.MEDIALIST_REMOVE, this.m_cmd_list_Remove);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIALIST_RENAME, this.m_cmd_list_Rename);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIALIST_UPDATESMARTMEDIALIST, this.m_cmd_UpdateSmartPlaylist);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIALIST_EDITSMARTMEDIALIST, this.m_cmd_EditSmartPlaylist);

    // Un-publish bundled commands

    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_DEFAULT, this.m_defaultCommands);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_WEBPLAYLIST, this.m_webPlaylistCommands);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_WEBTOOLBAR, this.m_webMediaHistoryToolbarCommands);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_DOWNLOADPLAYLIST, this.m_downloadCommands);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_DOWNLOADTOOLBAR, this.m_downloadToolbarCommands);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIALIST_DEFAULT, this.m_serviceTreeDefaultCommands);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIALIST_DOWNLOADPLAYLIST, this.m_downloadCommandsServicePane);
    this.m_mgr.withdraw(kPlaylistCommands.MEDIAITEM_SMARTPLAYLIST, this.m_smartPlaylistsCommands);


    // Un-register download playlist commands

    var downloadListGUID =
      prefs.getComplexValue("songbird.library.download",
                            Ci.nsISupportsString);

    this.m_mgr.unregisterPlaylistCommandsMediaItem(downloadListGUID,
                                                   "",
                                                   this.m_downloadCommands);

    this.m_mgr.unregisterPlaylistCommandsMediaItem
                                              (downloadListGUID,
                                               "",
                                               this.m_downloadToolbarCommands);

    this.m_mgr.unregisterPlaylistCommandsMediaList
                                              (downloadListGUID,
                                               "",
                                               this.m_downloadCommandsServicePane);

    g_downloadDevice = null;

    g_webLibrary = null;

    // Un-register web playlist commands

    var webListGUID =
      prefs.getComplexValue("songbird.library.web",
                            Ci.nsISupportsString);

    this.m_mgr.unregisterPlaylistCommandsMediaItem(webListGUID,
                                                   "",
                                                   this.m_webPlaylistCommands);

    // Un-register web media history commands

    this.m_mgr.
      unregisterPlaylistCommandsMediaItem(webListGUID,
                                          "",
                                          this.m_webMediaHistoryToolbarCommands);

    // Un-register smart playlist commands

    this.m_mgr.
      unregisterPlaylistCommandsMediaItem("", 
                                          "smart", 
                                          this.m_smartPlaylistsCommands);
    

    // Un-register servicetree commands

    this.m_mgr.
      unregisterPlaylistCommandsMediaList("",
                                          "simple",
                                          this.m_serviceTreeDefaultCommands);
    this.m_mgr.
      unregisterPlaylistCommandsMediaList("",
                                          "smart",
                                          this.m_serviceTreeDefaultCommands);

    // Shutdown all command objects, this ensures that no external reference
    // remains in their internal arrays
    this.m_cmd_Play.shutdown();
    this.m_cmd_Remove.shutdown();
    this.m_cmd_Edit.shutdown();
    this.m_cmd_Download.shutdown();
    this.m_cmd_Rescan.shutdown();
    this.m_cmd_Reveal.shutdown();
    this.m_cmd_CopyTrackLocation.shutdown();
    this.m_cmd_ShowDownloadPlaylist.shutdown();
    this.m_cmd_PauseResumeDownload.shutdown();
    this.m_cmd_CleanUpDownloads.shutdown();
    this.m_cmd_ClearHistory.shutdown();
    this.m_cmd_list_Remove.shutdown();
    this.m_cmd_list_Rename.shutdown();
    this.m_cmd_UpdateSmartPlaylist.shutdown();
    this.m_cmd_EditSmartPlaylist.shutdown();
    this.m_cmd_SmartPlaylistSep.shutdown();
    this.m_defaultCommands.shutdown();
    this.m_webPlaylistCommands.shutdown();
    this.m_webMediaHistoryToolbarCommands.shutdown();
    this.m_smartPlaylistsCommands.shutdown();
    this.m_downloadCommands.shutdown();
    this.m_downloadToolbarCommands.shutdown();
    this.m_downloadCommandsServicePane.shutdown();
    this.m_serviceTreeDefaultCommands.shutdown();

    g_dataRemoteService = null;

    var obs = Cc["@mozilla.org/observer-service;1"]
                .getService(Ci.nsIObserverService);

    obs.removeObserver(this, "quit-application");
  },

  // nsIObserver
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "final-ui-startup":
        this.initCommands();
        break;
      case "quit-application":
        this.shutdownCommands();
        break;
    }
  },

  QueryInterface:
    XPCOMUtils.generateQI([Ci.nsIObserver])
};

function unwrap(obj) {
  if (obj && obj.wrappedJSObject)
    obj = obj.wrappedJSObject;
  return obj;
}

// --------------------------------------------------------------------------
// Called when the play action is triggered
function plCmd_Play_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  // if something is selected, trigger the play event on the playlist
  if (plCmd_IsAnyTrackSelected(aContext, aSubMenuId, aCommandId, aHost)) {
    // Repurpose the command to act as a doubleclick
    unwrap(aContext.playlist).sendPlayEvent();
  }
}

// Called when the edit action is triggered
function plCmd_Edit_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  if (plCmd_IsAnyTrackSelected(aContext, aSubMenuId, aCommandId, aHost)) {
    var playlist = unwrap(aContext.playlist);
    playlist.onPlaylistEditor();
  }
}

// Called when the remove action is triggered
function plCmd_Remove_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  // remove the selected tracks, if any
  if (plCmd_IsAnyTrackSelected(aContext, aSubMenuId, aCommandId, aHost)) {
    unwrap(aContext.playlist).removeSelectedTracks();
  }
}

// Called when the download action is triggered
function plCmd_Download_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  try
  {
    var playlist = unwrap(aContext.playlist);
    var window = unwrap(aContext.window);
    if(playlist.mediaListView.selection.count) {
      onBrowserTransfer(new SelectionUnwrapper(
                       playlist.mediaListView.selection.selectedIndexedMediaItems));
    }
    else {
      var allItems = {
        items: [],
        onEnumerationBegin: function(aMediaList) {
        },
        onEnumeratedItem: function(aMediaList, aMediaItem) {
          this.items.push(aMediaItem);
        },
        onEnumerationEnd: function(aMediaList, aResultCode) {
        }
      };

      playlist.mediaList.enumerateAllItems(allItems);

      var itemEnum = ArrayConverter.enumerator(allItems.items);
      onBrowserTransfer(itemEnum);
    }
  }
  catch( err )
  {
    Cu.reportError(err);
  }
}

function plCmd_Rescan_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  try
  {
    var playlist = unwrap(aContext.playlist);
    var window = unwrap(aContext.window);

    if(playlist.mediaListView.selection.count) {
      var mediaItemArray = Cc["@songbirdnest.com/moz/xpcom/threadsafe-array;1"]
                             .createInstance(Ci.nsIMutableArray);
            
      var selection = playlist.mediaListView.selection.selectedIndexedMediaItems;
      while(selection.hasMoreElements()) {
        let item = selection.getNext()
                            .QueryInterface(Ci.sbIIndexedMediaItem).mediaItem;
        mediaItemArray.appendElement(item, false);
      }
      
      var metadataService = Cc["@songbirdnest.com/Songbird/FileMetadataService;1"]
                              .getService(Ci.sbIFileMetadataService);
      var job = metadataService.read(mediaItemArray);
      SBJobUtils.showProgressDialog(job, null);
    }
  }
  catch( err )
  {
    Cu.reportError(err);
  }
}

function plCmd_Reveal_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  try
  {
    var playlist = unwrap(aContext.playlist);
    var window = unwrap(aContext.window);

    if (playlist.mediaListView.selection.count != 1) { return; }
    
    var selection = playlist.mediaListView.selection.selectedIndexedMediaItems;
    var item = selection.getNext().QueryInterface(Ci.sbIIndexedMediaItem).mediaItem;
    if (!item) {
      Cu.reportError("No item selected in reveal playlist command.")
    }
    
    var uri = item.contentSrc;
    if (!uri || uri.scheme != "file") { return; }
    
    let f = uri.QueryInterface(Ci.nsIFileURL).file;
    try {
      // Show the directory containing the file and select the file
      f.QueryInterface(Ci.nsILocalFile);
      f.reveal();
    } catch (e) {
      // If reveal fails for some reason (e.g., it's not implemented on unix or
      // the file doesn't exist), try using the parent if we have it.
      let parent = f.parent.QueryInterface(Ci.nsILocalFile);
      if (!parent)
        return;
  
      try {
        // "Double click" the parent directory to show where the file should be
        parent.launch();
      } catch (e) {
        // If launch also fails (probably because it's not implemented), let the
        // OS handler try to open the parent
        var parentUri = Cc["@mozilla.org/network/io-service;1"]
                          .getService(Ci.nsIIOService).newFileURI(parent);

        var protocolSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"]
                            .getService(Ci.nsIExternalProtocolService);
        protocolSvc.loadURI(parentUri);
      }
    }
  }
  catch( err )
  {
    Cu.reportError(err);
  }
}


/*

// mig dice: MUY DEPRACATADO!!!

// Called when the "add to library" action is triggered
function plCmd_AddToLibrary_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  var libraryManager =
    Cc["@songbirdnest.com/Songbird/library/Manager;1"]
      .getService(Ci.sbILibraryManager);
  var mediaList = libraryManager.mainLibrary;

  var playlist = unwrap(aContext.playlist);
  var mediaListView = playlist.mediaListView;
  var selectionCount = mediaListView.selection.count;

  var unwrapper = new SelectionUnwrapper(mediaListView.selectedIndexedMediaItems);

  var oldLength = mediaList.length;
  mediaList.addSome(unwrapper);

  var itemsAdded = mediaList.length - oldLength;
  DNDUtils.reportAddedTracks(itemsAdded,
                             selectionCount - itemsAdded,
                             mediaList.name);
}
*/

// Called when the "copy track location" action is triggered
function plCmd_CopyTrackLocation_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  var clipboardtext = "";
  var urlCol = "url";
  var playlist = unwrap(aContext.playlist);

  var selectedEnum = playlist.mediaListView.selection.selectedIndexedMediaItems;
  while (selectedEnum.hasMoreElements()) {
    var curItem = selectedEnum.getNext();
    if (curItem) {
      var originURL = curItem.mediaItem.getProperty(SBProperties.contentURL);
      if (clipboardtext != "")
        clipboardtext += "\n";
      clipboardtext += originURL;
    }
  }

  var clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"]
                    .getService(Ci.nsIClipboardHelper);
  clipboard.copyString(clipboardtext);
}

// Called whne the "show download playlist" action is triggered
function plCmd_ShowDownloadPlaylist_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  var window = unwrap(aContext.window);
  var browser = window.gBrowser;
  if (browser) {
    var device = getDownloadDevice();
    if (device) {
      browser.loadMediaList(device.downloadMediaList);
    }
  }
}

// Called when the pause/resume download action is triggered
function plCmd_PauseResumeDownload_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  var device = getDownloadDevice();
  if (!device) return;
  var deviceState = device.getDeviceState('');
  if ( deviceState == Ci.sbIDeviceBase.STATE_DOWNLOADING )
  {
    device.suspendTransfer('');
  }
  else if ( deviceState == Ci.sbIDeviceBase.STATE_DOWNLOAD_PAUSED )
  {
    device.resumeTransfer('');
  }
  // Command buttons will update when device sends notification
}

// Called when the clean up downloads action is triggered
function plCmd_CleanUpDownloads_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  var device = getDownloadDevice();
  if (!device) return;

  device.clearCompletedItems();

  // Command buttons will update when device sends notification
}

// Called when the clear history action is triggered
function plCmd_ClearHistory_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  var wl = getWebLibrary();
  if (wl) {
    wl.clear();
  }
}

// Called when the "burn to cd" action is triggered
function plCmd_BurnToCD_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  // if something is selected, trigger the burn event on the playlist
  if (plCmd_IsAnyTrackSelected(aContext, aSubMenuId, aCommandId, aHost)) {
    unwrap(aContext.playlist).sendBurnToCDEvent();
  }
}

// Called when the "copy to device" action is triggered
function plCmd_CopyToDevice_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  // if something is selected, trigger the burn event on the playlist
  if (plCmd_IsAnyTrackSelected(aContext, aSubMenuId, aCommandId, aHost)) {
    //XXX not implemented
    //unwrap(aContext.playlist).sendCopyToDeviceEvent();
  }
}

// Called when the "remove playlist" action is triggered
function plCmd_RemoveList_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  unwrap(aContext.window).SBDeleteMediaList(aContext.medialist);
}

// Called when the "rename playlist" action is triggered
function plCmd_RenameList_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  var window = unwrap(aContext.window);
  var medialist = unwrap(aContext.medialist);
  var servicePane = window.gServicePane;
  // If we have a servicetree, tell it to make the new playlist node editable
  if (servicePane) {
    var librarySPS = Cc['@songbirdnest.com/servicepane/library;1']
                       .getService(Ci.sbILibraryServicePaneService);
    // Find the servicepane node for our new medialist
    var node = librarySPS.getNodeForLibraryResource(medialist);

    if (node) {
      // Ask the service pane to start editing our new node
      // so that the user can give it a name
      servicePane.startEditingNode(node);
    } else {
      throw("Error: Couldn't find a service pane node for the list we just created\n");
    }

  // Otherwise pop up a dialog and ask for playlist name
  } else {
    var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"  ]
                          .getService(Ci.nsIPromptService);

    var input = {value: medialist.name};
    var title = SBString("renamePlaylist.title", "Rename Playlist");
    var prompt = SBString("renamePlaylist.prompt", "Enter the new name of the playlist.");

    if (promptService.prompt(window, title, prompt, input, null, {})) {
      medialist.name = input.value;
    }
  }
}

function plCmd_UpdateSmartPlaylist_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  var medialist = unwrap(aContext.medialist);
  if (medialist instanceof Ci.sbILocalDatabaseSmartMediaList)
    medialist.rebuild();
}

function plCmd_EditSmartPlaylist_TriggerCallback(aContext, aSubMenuId, aCommandId, aHost) {
  var medialist = unwrap(aContext.medialist);
  if (medialist instanceof Ci.sbILocalDatabaseSmartMediaList) {
    var watcher = Cc["@mozilla.org/embedcomp/window-watcher;1"]
                    .getService(Ci.nsIWindowWatcher);
    watcher.openWindow(aContext.window,
                      "chrome://songbird/content/xul/smartPlaylist.xul",
                      "_blank",
                      "chrome,resizable=yes,dialog=yes,centerscreen,modal,titlebar=no",
                      medialist);
    unwrap(aContext.playlist).refreshCommands();
  }
}

// Returns true when at least one track is selected in the playlist
function plCmd_IsAnyTrackSelected(aContext, aSubMenuId, aCommandId, aHost) {
  return ( unwrap(aContext.playlist).tree.currentIndex != -1 );
}

// Returns true when at least one track is selected in the playlist and none of the selected tracks have downloading forbidden
function plCmd_Download_EnabledCallback(aContext, aSubMenuId, aCommandId, aHost) {
  if (!plCmd_IsAnyTrackSelected(aContext, aSubMenuId, aCommandId, aHost)) {
    return false;
  }
  try {
    var playlist = unwrap(aContext.playlist);
    var window = unwrap(aContext.window);
    var enumerator = playlist.mediaListView.selection.selectedMediaItems;
    while (enumerator.hasMoreElements()) {
      var item = enumerator.getNext().QueryInterface(Ci.sbIMediaItem);
      if (!item) continue; // WTF?
      if (item.getProperty(SBProperties.disableDownload) == '1') {
        // one of the items has download disabled, we need to disable the command
        return false;
      }
    }
    // none of the items had download disabled, enable the command
    return true;
  } catch (e) {
    Cu.reportError(err);
    // something bad happened - I say no.
    return false;
  }
}

// Returns true if the 'rescan' item command is enabled
function plCmd_IsRescanItemEnabled(aContext, aSubMenuId, aCommandId, aHost) {
  var prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Ci.nsIPrefBranch2);
  var enabled = false;
  try {
   enabled = prefs.getBoolPref("songbird.commands.enableRescanItem");
  } catch (e) {
    // nothing to do
  }
  return enabled;
}

// Returns true if the host is the shortcuts instantiator
function plCmd_IsShortcutsInstantiator(aContext, aSubMenuId, aCommandId, aHost) {
  return (aHost == "shortcuts");
}

// Returns true if the host is the toolbar instantiator
function plCmd_IsToolbarInstantiator(aContext, aSubMenuId, aCommandId, aHost) {
  return (aHost == "toolbar");
}

// Returns true if the current playlist isn't the library
function plCmd_IsNotLibraryContext(aContext, aSubMenuId, aCommandId, aHost) {
  var medialist = unwrap(aContext.medialist);
  return (medialist.library != medialist);
}

// Returns true if the playlist can be modified (is not read-only)
function plCmd_CanModifyPlaylist(aContext, aSubMenuId, aCommandId, aHost) {
  return !(plCmd_isReadOnlyLibrary(aContext, aSubMenuId, aCommandId, aHost) ||
           plCmd_isReadOnlyPlaylist(aContext, aSubMenuId, aCommandId, aHost));
}

// Returns true if the playlist content can be modified (is not read-only)
function plCmd_CanModifyPlaylistContent(aContext, aSubMenuId, aCommandId, aHost) {
  return !(plCmd_isReadOnlyLibraryContent(aContext, aSubMenuId, aCommandId, aHost) ||
           plCmd_isReadOnlyPlaylistContent(aContext, aSubMenuId, aCommandId, aHost));
}

// Returns true if the library the playlist is in is read-only
function plCmd_isReadOnlyLibrary(aContext, aSubMenuId, aCommandId, aHost) {
  var medialist = unwrap(aContext.medialist);
  return !medialist.library.userEditable;
}

// Returns true if the playlist is read-only
function plCmd_isReadOnlyPlaylist(aContext, aSubMenuId, aCommandId, aHost) {
  var medialist = unwrap(aContext.medialist);
  return !medialist.userEditable;
}

// Returns true if the library the playlist is in is read-only
function plCmd_isReadOnlyLibraryContent(aContext, aSubMenuId, aCommandId, aHost) {
  if (plCmd_isReadOnlyLibrary(aContext, aSubMenuId, aCommandId, aHost))
    return true;
  var medialist = unwrap(aContext.medialist);
  return !medialist.library.userEditableContent;
}

// Returns true if the playlist is read-only
function plCmd_isReadOnlyPlaylistContent(aContext, aSubMenuId, aCommandId, aHost) {
  if (plCmd_isReadOnlyPlaylist(aContext, aSubMenuId, aCommandId, aHost))
    return true;
  var medialist = unwrap(aContext.medialist);
  return !medialist.userEditableContent;
}

// Returns true if the conditions are ok for adding tracks to the library
function plCmd_CanAddToLibrary(aContext, aSubMenuId, aCommandId, aHost) {
  return plCmd_IsAnyTrackSelected(aContext, aSubMenuId, aCommandId, aHost) &&
         plCmd_CanModifyPlaylistContent(aContext, aSubMenuId, aCommandId, aHost) &&
         plCmd_IsNotLibraryContext(aContext, aSubMenuId, aCommandId, aHost);
}

// Returns true if a download is currently in progress (not paused)
function plCmd_IsDownloading(aContext, aSubMenuId, aCommandId, aHost) {
  var device = getDownloadDevice();
  if (!device) return false;
  var deviceState = device.getDeviceState('');
  return (deviceState == Ci.sbIDeviceBase.STATE_DOWNLOADING);
}

// Returns true if a download is currently paused
function plCmd_IsDownloadPaused(aContext, aSubMenuId, aCommandId, aHost) {
  var device = getDownloadDevice();
  if (!device) return false;
  var deviceState = device.getDeviceState('');
  return (deviceState == Ci.sbIDeviceBase.STATE_DOWNLOAD_PAUSED);
}

// Returns true if a download has been started, regardless of wether it has
// been paused or not
function plCmd_IsDownloadActive(aContext, aSubMenuId, aCommandId, aHost) {
  return plCmd_IsDownloading(aContext, aSubMenuId, aCommandId, aHost) ||
        plCmd_IsDownloadPaused(aContext, aSubMenuId, aCommandId, aHost);
}

// Returns true if a download is in progress or if there is not download that
// has been started at all
function plCmd_IsDownloadingOrNotActive(aContext, aSubMenuId, aCommandId, aHost) {
  return plCmd_IsDownloading(aContext, aSubMenuId, aCommandId, aHost) ||
        !plCmd_IsDownloadActive(aContext, aSubMenuId, aCommandId, aHost);
}

// Returns true if any completed download items exist
function plCmd_DownloadHasCompletedItems(aContext, aSubMenuId, aCommandId, aHost) {
  var device = getDownloadDevice();
  if (!device) return false;
  return (device.completedItemCount > 0);
}

// Returns true if there are items in the web media history
function plCmd_WebMediaHistoryHasItems(aContext, aSubMenuId, aCommandId, aHost) {
  var wl = getWebLibrary();
  return wl && !wl.isEmpty;
}

// Returns true if the supplied context contains a gBrowser object
function plCmd_ContextHasBrowser(aContext, aSubMenuId, aCommandId, aHost) {
  var window = unwrap(aContext.window);
  return (window.gBrowser);
}

// Returns true if the playlist is a smart playlist
function plCmd_isSmartPlaylist(aContext, aSubMenuId, aCommandId, aHost) {
  var medialist = unwrap(aContext.medialist);
  return (medialist.type == "smart");
}

function plCmd_isLiveUpdateSmartPlaylist(aContext, aSubMenuId, aCommandId, aHost) {
  var medialist = unwrap(aContext.medialist);
  if (medialist instanceof Ci.sbILocalDatabaseSmartMediaList)
    return medialist.autoUpdate;
  return false;
} 
 
// Returns a function that will return the conjunction of the result of the inputs
function plCmd_AND( /* comma separated list (not array) of functions */ ) {
  var methods = Array.prototype.concat.apply([], arguments);
  return function _plCmd_Conjunction(aContext, aSubMenuId, aCommandId, aHost) {
    for each (var f in methods) {
      if (!f(aContext, aSubMenuId, aCommandId, aHost))
        return false;
    }
    return true;
  }
}

// Returns a function that will return the disjunction of the result of the inputs
function plCmd_OR( /* comma separated list (not array) of functions */ ) {
  var methods = Array.prototype.concat.apply([], arguments);
  return function _plCmd_Disjunction(aContext, aSubMenuId, aCommandId, aHost) {
    for each (var f in methods) {
      if (f(aContext, aSubMenuId, aCommandId, aHost))
        return true;
    }
    return false;
  }
}

// Returns a function that will return the negation of the result of the input
function plCmd_NOT( aFunction ) {
  return function _plCmd_Negation(aContext, aSubMenuId, aCommandId, aHost) {
    return !aFunction(aContext, aSubMenuId, aCommandId, aHost);
  }
}

// Always return false
function plCmd_False(aContext, aSubMenuId, aCommandId, aHost) {
  return false
}

function onBrowserTransfer(mediaItems, parentWindow)
{
  var ddh =
    Cc["@songbirdnest.com/Songbird/DownloadDeviceHelper;1"]
      .getService(Ci.sbIDownloadDeviceHelper);
  ddh.downloadSome(mediaItems);
}

var g_downloadDevice = null;
function getDownloadDevice() {
  try
  {
    if (!g_downloadDevice) {
      var deviceManager = Cc["@songbirdnest.com/Songbird/DeviceManager;1"]
                            .getService(Ci.sbIDeviceManager);

      if (deviceManager)
      {
        var downloadCategory = 'Songbird Download Device';
        if (deviceManager.hasDeviceForCategory(downloadCategory))
        {
          g_downloadDevice = deviceManager.getDeviceByCategory(downloadCategory);
          g_downloadDevice = g_downloadDevice.QueryInterface
                                      (Ci.sbIDownloadDevice);
        }
      }
    }
    return g_downloadDevice;
  } catch(e) {
    Cu.reportError(e);
  }
  return null;
}

var g_webLibrary = null
function getWebLibrary() {
  try {
    if (g_webLibrary == null) {
      var prefs = Cc["@mozilla.org/preferences-service;1"]
        .getService(Ci.nsIPrefBranch2);
      var webListGUID = prefs.getComplexValue("songbird.library.web",
          Ci.nsISupportsString);
      var libraryManager =
        Cc["@songbirdnest.com/Songbird/library/Manager;1"]
          .getService(Ci.sbILibraryManager);
      g_webLibrary = libraryManager.getLibrary(webListGUID);
    }
  } catch(e) {
    Cu.reportError(e);
  }

  return g_webLibrary;
}

function plCmd_ShowDefaultInToolbarCheck(aContext, aHost) {
  if (aHost == "toolbar") {
    if (dataRemote("commands.showdefaultintoolbar", null).boolValue) return true;
    return false;
  }
  return true;
}

function plCmd_HideForToolbarCheck(aContext, aHost) {
  return (aHost != "toolbar");
}

function plCmd_ShowForToolbarCheck(aContext, aHost) {
  return (aHost == "toolbar");
}

function plCmd_DownloadInit(aContext, aHost) {
  var implementorContext = {
    context: aContext,
    batchHelper: new LibraryUtils.BatchHelper(),
    needRefresh: false,

    // sbIDeviceBaseCallback
    onTransferComplete: function(aMediaItem, aStatus) {
      this.refreshCommands();
    },

    onStateChanged: function(aDeviceIdentifier, aState) {
      this.refreshCommands();
    },

    onDeviceConnect: function(aDeviceIdentifier) {},
    onDeviceDisconnect: function(aDeviceIdentifier) {},
    onTransferStart: function(aMediaItem) {},


    // sbIMediaListListener
    onAfterItemRemoved: function(aMediaList, aMediaItem) {
      return this.onMediaListChanged();
    },

    onListCleared: function(aMediaList) {
      return this.onMediaListChanged();
    },

    onBatchBegin: function(aMediaList) {
      this.batchHelper.begin();
    },

    onBatchEnd: function(aMediaList) {
      this.batchHelper.end();
      if (!this.batchHelper.isActive() && this.needRefresh) {
        this.refreshCommands();
        this.needRefresh = false;
      }
    },

    onItemAdded: function(aMediaList, aMediaItem, aIndex) { return true; },
    onBeforeItemRemoved: function(aMediaList, aMediaItem, aIndex) {
      return true;
    },
    onItemUpdated: function(aMediaList, aMediaItem, aProperties) {
      return true;
    },
    onItemMoved: function(aMediaList, aFromIndex, aToIndex) { return true; },


    onMediaListChanged: function() {
      if (this.batchHelper.isActive()) {
        this.needRefresh = true;
        return true;
      }
      this.refreshCommands();
      return false;
    },

    refreshCommands: function() {
      var playlist = unwrap(this.context.playlist);
      if (playlist)
        playlist.refreshCommands();
    }
  };

  var device = getDownloadDevice();
  if (!device) return false;
  device.addCallback(implementorContext);
  device.downloadMediaList.addListener
                              (implementorContext,
                               false,
                               Ci.sbIMediaList.LISTENER_FLAGS_AFTERITEMREMOVED |
                               Ci.sbIMediaList.LISTENER_FLAGS_LISTCLEARED |
                               Ci.sbIMediaList.LISTENER_FLAGS_BATCHBEGIN |
                               Ci.sbIMediaList.LISTENER_FLAGS_BATCHEND);
  aContext.implementorContext = implementorContext;
}

function plCmd_DownloadShutdown(aContext, aHost) {
  var device = getDownloadDevice();
  if (!device) return false;
  if (aContext.implementorContext) {
    device.removeCallback(aContext.implementorContext);
    device.downloadMediaList.removeListener(aContext.implementorContext);
  }
  aContext.implementorContext = null;
}

var g_dataRemoteService = null;
function dataRemote(aKey, aRoot) {
  if (!g_dataRemoteService) {
    g_dataRemoteService = new Components.
      Constructor( "@songbirdnest.com/Songbird/DataRemote;1",
                  Ci.sbIDataRemote,
                  "init");
  }
  return g_dataRemoteService(aKey, aRoot);
}

function LOG(str) {
  var consoleService = Cc['@mozilla.org/consoleservice;1']
                         .getService(Ci.nsIConsoleService);
  consoleService.logStringMessage(str);
  dump(str+"\n");
};

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([PublicPlaylistCommands],
  function(aCompMgr, aFileSpec, aLocation) {
    XPCOMUtils.categoryManager.addCategoryEntry(
      "app-startup",
      PublicPlaylistCommands.prototype.classDescription,
      "service," + PublicPlaylistCommands.prototype.contractID,
      true,
      true);
  });
}
