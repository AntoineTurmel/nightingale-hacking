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

// For Songbird properties.
Components.utils.import("resource://app/jsmodules/sbProperties.jsm");
Components.utils.import("resource://app/jsmodules/sbLibraryUtils.jsm");


// Open functions
//
// This file is not standalone

try
{
  function SBFileOpen( )
  {
    var PPS = Components.classes["@songbirdnest.com/Songbird/PlaylistPlayback;1"]
                           .getService(Components.interfaces.sbIPlaylistPlayback);
    // Make a filepicker thingie
    var fp = Components.classes["@mozilla.org/filepicker;1"]
            .createInstance(Components.interfaces.nsIFilePicker);

    // get some text for the filepicker window
    var sel = "Select";
    try
    {
      sel = theSongbirdStrings.getString("faceplate.select");
    } catch(e) {}

    // initialize the filepicker with our text, a parent and the mode
    fp.init(window, sel, Components.interfaces.nsIFilePicker.modeOpen);

    // Tell it what filters to be using
    var mediafiles = "Media Files";
    try
    {
      mediafiles = theSongbirdStrings.getString("open.mediafiles");
    } catch(e) {}

    // ask the playback core for supported extensions
    var files = "";
    var eExtensions = PPS.getSupportedFileExtensions();
    while (eExtensions.hasMore()) {
      files += ( "*." + eExtensions.getNext() + "; ");
    }

    // add a filter to show only supported media files
    fp.appendFilter(mediafiles, files);
    
    // add a filter to allow HTML files to be opened.
    fp.appendFilters(Components.interfaces.nsIFilePicker.filterHTML);

    // Show the filepicker
    var fp_status = fp.show();
    if ( fp_status == Components.interfaces.nsIFilePicker.returnOK )
    {
      // Use a nsIURI because it is safer and contains the scheme etc...
      var ios = Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
      var uri = ios.newFileURI(fp.file, null, null);
      
      // Linux specific hack to be able to read badly named files (bug 6227)
      // nsIIOService::newFileURI actually forces to be valid UTF8 - which isn't
      // correct if the file on disk manages to have an incorrect name
      // note that Mac OSX has a different persistentDescriptor
      if (fp.file instanceof Components.interfaces.nsILocalFile) {
        switch(getPlatformString()) {
          case "Linux":
            var spec = "file://" + escape(fp.file.persistentDescriptor);
            uri = ios.newURI(spec, null, null);
        }
      }

      // See if we're asking for an extension
      if ( isXPI( uri.spec ) )
      {
        installXPI( uri.spec );
      }
      else if ( PPS.isMediaURL(uri.spec) )
      {
        // And if we're good, play it.
        SBDataSetBoolValue("faceplate.seenplaying", false);
        SBDataSetStringValue("metadata.title", fp.file.leafName);
        SBDataSetStringValue("metadata.artist", "");
        SBDataSetStringValue("metadata.album", "");

        // Import the item.
        var item = SBImportURLIntoMainLibrary(uri);

        var libraryManager = Components.classes["@songbirdnest.com/Songbird/library/Manager;1"]
                                  .getService(Components.interfaces.sbILibraryManager);

        // show the view and play

        var view = LibraryUtils.createStandardMediaListView(libraryManager.mainLibrary);

        var index = view.getIndexForItem(item);
        
        // If we have a browser, try to show the view
        if (window.gBrowser) {
          gBrowser.showIndexInView(view, index);
        }
        
        // Play the item
        gPPS.playView(view, index);
      }
      else
      {
        // Unknown type, let the browser figure out what the best course
        // of action is.
        gBrowser.loadURI( uri.spec );
      }
    }
  }

  function SBUrlOpen( parentWindow )
  {
    // Make a magic data object to get passed to the dialog
    var url_open_data = new Object();
    url_open_data.URL = SBDataGetStringValue("faceplate.play.url");
    url_open_data.retval = "";
    // Open the modal dialog
    SBOpenModalDialog( "chrome://songbird/content/xul/openURL.xul", "open_url", "chrome,centerscreen", url_open_data, parentWindow );
    if ( url_open_data.retval == "ok" )
    {
      var library = LibraryUtils.webLibrary;

     // Use a nsIURI because it is safer and contains the scheme etc...
      var ios = Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
      try {
        alert(url_open_data.URL);
        var uri = ios.newURI(url_open_data.URL, null, null);
      }
      catch(e) {
        // Bad URL :(
        Components.utils.reportError(e);
        return;
      }

      // See if we're asking for an extension
      if ( isXPI( uri.spec ) )
      {
        installXPI( uri.spec );
      }
      else if ( gPPS.isMediaURL(uri.spec) )
      {
        var item = SBImportURLIntoWebLibrary(uri);

        // And if we're good, play it.
        SBDataSetBoolValue("faceplate.seenplaying", false);
        SBDataSetStringValue("metadata.title", uri.spec);
        SBDataSetStringValue("metadata.artist", "");
        SBDataSetStringValue("metadata.album", "");

        // Import the item.
        var item = SBImportURLIntoMainLibrary(uri);

        var libraryManager = Components.classes["@songbirdnest.com/Songbird/library/Manager;1"]
                                  .getService(Components.interfaces.sbILibraryManager);

        // show the view and play

        var view = LibraryUtils.createStandardMediaListView(libraryManager.mainLibrary);

        var index = view.getIndexForItem(item);
        
        // If we have a browser, try to show the view
        if (window.gBrowser) {
          gBrowser.showIndexInView(view, index);
        }
        
        // Play the item
        gPPS.playView(view, index);
      }
      else
      {
        // Unknown type, let the browser figure out what the best course
        // of action is.
        gBrowser.loadURI( uri.spec );
      }
    }
  }

  function SBPlaylistOpen()
  {
    try
    {
      var aPlaylistReaderManager =
        Components.classes["@songbirdnest.com/Songbird/PlaylistReaderManager;1"]
                  .getService(Components.interfaces.sbIPlaylistReaderManager);

      // Make a filepicker thingie
      var nsIFilePicker = Components.interfaces.nsIFilePicker;
      var fp = Components.classes["@mozilla.org/filepicker;1"]
              .createInstance(nsIFilePicker);
      var sel = "Open Playlist";
      try
      {
        sel = theSongbirdStrings.getString("faceplate.open.playlist");
      } catch(e) {}
      fp.init(window, sel, nsIFilePicker.modeOpen);

      // Tell it what filters to be using
      var filterlist = "";
      var extensionCount = new Object;
      var extensions = aPlaylistReaderManager.supportedFileExtensions(extensionCount);

      var first = true;
      for(var i = 0; i < extensions.length; i++)
      {
        var ext_list = extensions[i].split(",");
        for(var j = 0; j < ext_list.length; j++)
        {
          var ext = ext_list[j];
          if (ext.length > 0) {
            if (!first) // skip the first one
              filterlist += ";";
            first = false;
            filterlist += "*." + ext;
          }
        }
      }

      var playlistfiles = "Playlist Files";
      try
      {
        playlistfiles = theSongbirdStrings.getString("open.playlistfiles");
      } catch(e) {}
      fp.appendFilter(playlistfiles, filterlist);

      // Show it
      var fp_status = fp.show();
      if ( fp_status == nsIFilePicker.returnOK )
      {
        SBOpenPlaylistURI(fp.fileURL, fp.file.leafName);
      }
    }
    catch(err)
    {
      alert(err);
    }
  }
  
  function SBOpenPlaylistURI(aURI, aName) {
    var uri = aURI;
    if(!(aURI instanceof Components.interfaces.nsIURI)) {
      uri = newURI(aURI);
    }
    var name = aName;
    if (!aName) {
      name = uri.path;
      var p = name.lastIndexOf(".");
      if (p != -1) name = name.slice(0, p);
      p = name.lastIndexOf("/");
      if (p != -1) name = name.slice(p+1);
    }
    var aPlaylistReaderManager =
      Components.classes["@songbirdnest.com/Songbird/PlaylistReaderManager;1"]
                .getService(Components.interfaces.sbIPlaylistReaderManager);

    var library = Components.classes["@songbirdnest.com/Songbird/library/Manager;1"]
                            .getService(Components.interfaces.sbILibraryManager).mainLibrary;

    // Create the media list
    var mediaList = library.createMediaList("simple");
    mediaList.name = name;
    mediaList.setProperty("http://songbirdnest.com/data/1.0#originURL", uri.spec);

    aPlaylistReaderManager.originalURI = uri;
    var success = aPlaylistReaderManager.loadPlaylist(uri, mediaList, null, false, null);
    if (success == 1 &&
        mediaList.length) {
      var array = Components.classes["@songbirdnest.com/moz/xpcom/threadsafe-array;1"]
                            .createInstance(Components.interfaces.nsIMutableArray);
      for (var i = 0; i < mediaList.length; i++) {
        array.appendElement(mediaList.getItemByIndex(i), false);
      }

      // Send the items in the new media list to the metadata scanner
      var metadataJobManager =
        Components.classes["@songbirdnest.com/Songbird/MetadataJobManager;1"]
                  .getService(Components.interfaces.sbIMetadataJobManager);
      var metadataJob = metadataJobManager.newJob(array, 5);

      // Give the new media list focus
      if (typeof gBrowser != 'undefined') {
        gBrowser.loadMediaList(mediaList);
      }
    } else {
      library.remove(mediaList);
      return null;
    }
    return mediaList;
  }

  function log(str)
  {
    var consoleService = Components.classes['@mozilla.org/consoleservice;1']
                            .getService(Components.interfaces.nsIConsoleService);
    consoleService.logStringMessage( str );
  }

  function SBUrlExistsInDatabase( the_url )
  {
    var retval = false;
    try
    {
      aDBQuery = Components.classes["@songbirdnest.com/Songbird/DatabaseQuery;1"];
      if (aDBQuery)
      {
        aDBQuery = aDBQuery.createInstance();
        aDBQuery = aDBQuery.QueryInterface(Components.interfaces.sbIDatabaseQuery);

        if ( ! aDBQuery )
        {
          return false;
        }

        aDBQuery.setAsyncQuery(false);
        aDBQuery.setDatabaseGUID("testdb-0000");
        aDBQuery.addQuery('select * from test where url="' + the_url + '"' );
        var ret = aDBQuery.execute();

        resultset = aDBQuery.getResultObject();

        // we didn't find anything that matches our url
        if ( resultset.getRowCount() != 0 )
        {
          retval = true;
        }
      }
    }
    catch(err)
    {
      alert(err);
    }
    return retval;
  }

  // This function should be called when we need to open a URL but gBrowser is 
  // not available. Eventually this should be replaced by code that opens a new 
  // Songbird window, when we are able to do that, but for now, open in the 
  // default external browser.
  // If what you want to do is ALWAYS open in the default external browser,
  // use SBOpenURLInDefaultBrowser directly!
  function SBBrowserOpenURLInNewWindow( the_url ) {
    SBOpenURLInDefaultBrowser(the_url);
  }
  
  // This function opens a URL externally, in the default web browser for the system
  function SBOpenURLInDefaultBrowser( the_url ) {
    var externalLoader = (Components
              .classes["@mozilla.org/uriloader/external-protocol-service;1"]
            .getService(Components.interfaces.nsIExternalProtocolService));
    var nsURI = (Components
            .classes["@mozilla.org/network/io-service;1"]
            .getService(Components.interfaces.nsIIOService)
            .newURI(the_url, null, null));
    externalLoader.loadURI(nsURI, null);
  }

// Help
function onHelp()
{
  var helpitem = document.getElementById("menuitem_help_topics");
  onMenu(helpitem);
}



function SBOpenPreferences(paneID, parentWindow)
{
  if (!parentWindow) parentWindow = window;

  // On all systems except Windows pref changes should be instant.
  //
  // In mozilla this is the browser.prefereces.instantApply pref,
  // and is set at compile time.
  var instantApply = navigator.userAgent.indexOf("Windows") == -1;

  // BUG 5081 - You can't call restart in a modal window, so
  // we're making prefs non-modal on all platforms.
  // Original line:  var features = "chrome,titlebar,toolbar,centerscreen" + (instantApply ? ",dialog=no" : ",modal");
  var features = "chrome,titlebar,toolbar,centerscreen" + (instantApply ? ",dialog=no" : "");

  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Components.interfaces.nsIWindowMediator);
  var win = wm.getMostRecentWindow("Browser:Preferences");
  if (win) {
    win.focus();
    if (paneID) {
      var pane = win.document.getElementById(paneID);
      win.document.documentElement.showPane(pane);
    }
    return win;
  } else {
    return parentWindow.openDialog("chrome://browser/content/preferences/preferences.xul", "Preferences", features, paneID);
  }

  // to open connection settings only:
  // SBOpenModalDialog("chrome://browser/content/preferences/connection.xul", "chrome,centerscreen", null);
}

function SBOpenDownloadManager()
{
  var dlmgr = Components.classes['@mozilla.org/download-manager;1'].getService();
  dlmgr = dlmgr.QueryInterface(Components.interfaces.nsIDownloadManager);

  var windowMediator = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
  windowMediator = windowMediator.QueryInterface(Components.interfaces.nsIWindowMediator);

  var dlmgrWindow = windowMediator.getMostRecentWindow("Download:Manager");
  if (dlmgrWindow) {
    dlmgrWindow.focus();
  }
  else {
    window.open("chrome://mozapps/content/downloads/downloads.xul", "Download:Manager", "chrome,centerscreen,dialog=no,resizable", null);
  }
}

function SBWatchFolders( parentWindow )
{
  SBOpenModalDialog( "chrome://songbird/content/xul/watchFolders.xul", "", "chrome,centerscreen", null, parentWindow );
}

var theFileScanIsOpen = SB_NewDataRemote( "media_scan.open", null );
function SBScanMedia( parentWindow )
{
  theFileScanIsOpen.boolValue = true;
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  const CONTRACTID_FILE_PICKER = "@mozilla.org/filepicker;1";
  var fp = Components.classes[CONTRACTID_FILE_PICKER].createInstance(nsIFilePicker);
  var welcome = "Welcome";
  var scan = "Scan";
  try
  {
    welcome = theSongbirdStrings.getString("faceplate.welcome");
    scan = theSongbirdStrings.getString("faceplate.scan");
  } catch(e) {}
  if (getPlatformString() == "Darwin") {
    fp.init( window, scan, nsIFilePicker.modeGetFolder );
    var defaultDirectory =
    Components.classes["@mozilla.org/file/directory_service;1"]
              .getService(Components.interfaces.nsIProperties)
              .get("Home", Components.interfaces.nsIFile);
    defaultDirectory.append("Music");
    fp.displayDirectory = defaultDirectory;
  } else {
    fp.init( window, welcome + "\n\n" + scan, nsIFilePicker.modeGetFolder );
  }
  var res = fp.show();
  if ( res == nsIFilePicker.returnOK )
  {
    var media_scan_data = new Object();
    media_scan_data.URL = [fp.file.path];
    // Open the modal dialog
    SBOpenModalDialog( "chrome://songbird/content/xul/mediaScan.xul",
                       "media_scan",
                       "chrome,centerscreen",
                       media_scan_data );
  }
  theFileScanIsOpen.boolValue = false;
}

/** Legacy function **/
function SBNewPlaylist()
{
  return makeNewPlaylist("simple");
}

function SBNewSmartPlaylist()
{
  return makeNewPlaylist("smart");
}

/**
 * Create a new playlist of the given type, using the service pane
 * to determine context and perform renaming
 *
 * Note: This function should move into the window controller somewhere
 *       once it exists.
 */
function makeNewPlaylist(mediaListType) {
  var servicePane = null;
  if (typeof gServicePane != 'undefined') servicePane = gServicePane;

  // Try to find the currently selected service pane node
  var selectedNode;
  if (servicePane) {
    selectedNode = servicePane.getSelectedNode();
  }
  
  // ensure the service pane is initialized (safe to do multiple times)
  var servicePaneService = Components.classes['@songbirdnest.com/servicepane/service;1']
                              .getService(Components.interfaces.sbIServicePaneService);
  servicePaneService.init();

  // Ask the library service pane provider to suggest where
  // a new playlist should be created
  var librarySPS = Components.classes['@songbirdnest.com/servicepane/library;1']
                             .getService(Components.interfaces.sbILibraryServicePaneService);
  var library = librarySPS.suggestLibraryForNewList(mediaListType, selectedNode);

  // Looks like no libraries support the given mediaListType
  if (!library) {
    throw("Could not find a library supporting lists of type " + mediaListType);
  }
  
  // Make sure the library is user editable, if it is not, use the main
  // library instead of the currently selected library.
  if (library.userEditable == false) {
    var libraryManager = 
      Components.classes["@songbirdnest.com/Songbird/library/Manager;1"]
                .getService(Components.interfaces.sbILibraryManager);
    
    library = libraryManager.mainLibrary;
  }

  // Create the playlist
  var mediaList = library.createMediaList(mediaListType);

  // Give the playlist a default name
  // TODO: Localization should be done internally
  mediaList.name = SBString("playlist", "Playlist");

  // If we have a servicetree, tell it to make the new playlist node editable
  if (servicePane) {
    // Find the servicepane node for our new medialist
    var node = librarySPS.getNodeForLibraryResource(mediaList);

    if (node) {
      // Ask the service pane to start editing our new node
      // so that the user can give it a name
      servicePane.startEditingNode(node);
    } else {
      throw("Error: Couldn't find a service pane node for the list we just created\n");
    }

  // Otherwise pop up a dialog and ask for playlist name
  } else {
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"  ]
                                  .getService(Components.interfaces.nsIPromptService);

    var input = {value: mediaList.name};
    var title = SBString("newPlaylist.title", "Create New Playlist");
    var prompt = SBString("newPlaylist.prompt", "Enter the name of the new playlist.");

    if (promptService.prompt(window, title, prompt, input, null, {})) {
      mediaList.name = input.value;
    }
  }
  return mediaList;
}


function SBExtensionsManagerOpen( parentWindow )
{
  if (!parentWindow) parentWindow = window;
  const EM_TYPE = "Extension:Manager";

  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var theEMWindow = wm.getMostRecentWindow(EM_TYPE);
  if (theEMWindow) {
    theEMWindow.focus();
    return;
  }

  const EM_URL = "chrome://mozapps/content/extensions/extensions.xul?type=extensions";
  const EM_FEATURES = "chrome,menubar,extra-chrome,toolbar,dialog=no,resizable";
  parentWindow.openDialog(EM_URL, "", EM_FEATURES);
}

function SBTrackEditorOpen( initialTab, parentWindow ) {
  if (!parentWindow) parentWindow = window;
  var browser;
  if (typeof SBGetBrowser == 'function') 
    browser = SBGetBrowser();
  if (browser) {
    if (browser.currentMediaPage) {
      var view = browser.currentMediaPage.mediaListView;
      if (view) {
        var numSelected = view.selection.count;
        if (numSelected > 1) {
          const BYPASSKEY = "trackeditor.multiplewarning.bypass";
          const STRINGROOT = "trackeditor.multiplewarning.";
          if (!SBDataGetBoolValue(BYPASSKEY)) {
            var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                          .getService(Components.interfaces.nsIPromptService);
            check = { value: false };
            
            var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                .getService(Components.interfaces.nsIStringBundleService);
            var songbirdStrings = sbs.createBundle("chrome://songbird/locale/songbird.properties");
            var strTitle = songbirdStrings.GetStringFromName(STRINGROOT + "title");
            var strMsg = songbirdStrings.formatStringFromName(STRINGROOT + "message", [numSelected], 1);
            var strCheck = songbirdStrings.GetStringFromName(STRINGROOT + "check");
            
            var r = promptService.confirmEx(window, 
                                    strTitle, 
                                    strMsg, 
                                    Ci.nsIPromptService.STD_YES_NO_BUTTONS, 
                                    null, 
                                    null, 
                                    null, 
                                    strCheck, 
                                    check);
            if (check.value == true) {
              SBDataSetBoolValue(BYPASSKEY, true);
            }
            if (r == 1) { // 0 = yes, 1 = no
              return;
            }
          }
        } else if (numSelected < 1) {
          // no track is selected, can't invoke the track editor on nothing !
          return;
        }
         
        // xxxlone> note that the track editor is modal, so the window will 
        // never exist. the code is left here in case we ever change back to
        // a modeless track editor.
        var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                           .getService(Components.interfaces.nsIWindowMediator);
        var theTE = wm.getMostRecentWindow("Songbird:TrackEditor");
        if (theTE) {
          theTE.focus();
        } else {
          SBOpenModalDialog("chrome://songbird/content/xul/trackEditor.xul", 
                            "Songbird:TrackEditor", "chrome,centerscreen", 
                            initialTab, parentWindow);
        }
      }
    }
  }
}

function SBSubscribe(mediaList, defaultUrl, parentWindow)
{
  // Make sure the argument is a dynamic media list
  if (mediaList) {
    if (!(mediaList instanceof Components.interfaces.sbIMediaList))
      throw Components.results.NS_ERROR_INVALID_ARG;

    var isSubscription =
      mediaList.getProperty("http://songbirdnest.com/data/1.0#isSubscription");
    if (isSubscription != "1")
      throw Components.results.NS_ERROR_INVALID_ARG;
  }

  if (defaultUrl && !(defaultUrl instanceof Components.interfaces.nsIURI))
    throw Components.results.NS_ERROR_INVALID_ARG;

  var params = Components.classes["@songbirdnest.com/moz/xpcom/threadsafe-array;1"]
                         .createInstance(Components.interfaces.nsIMutableArray);
  params.appendElement(mediaList, false);
  params.appendElement(defaultUrl, false);

  // Open the window
  SBOpenModalDialog("chrome://songbird/content/xul/subscribe.xul",
                    "",
                    "chrome,centerscreen",
                    params,
                    parentWindow);
}

// TODO: This function should be renamed.  See openAboutDialog in browserUtilities.js
function About( parentWindow )
{
  // Make a magic data object to get passed to the dialog
  var about_data = new Object();
  about_data.retval = "";
  // Open the modal dialog
  SBOpenModalDialog( "chrome://songbird/content/xul/about.xul", "about", "chrome,centerscreen", about_data, parentWindow );
  if ( about_data.retval == "ok" )
  {
  }
}

function SBNewFolder() {
  var servicePane = gServicePane;

  // Try to find the currently selected service pane node
  var selectedNode;
  if (servicePane) {
    selectedNode = servicePane.getSelectedNode();
  }

  // The bookmarks service knows how to make folders...
  var bookmarks = Components.classes['@songbirdnest.com/servicepane/bookmarks;1']
      .getService(Components.interfaces.sbIBookmarks);

  // ask the bookmarks service to make a new folder
  var folder = bookmarks.addFolder(SBString('bookmarks.newfolder.defaultname',
                                            'New Folder'));

  // start editing the new folder
  if (gServicePane) {
    // we can find the pane so we can edit it inline
    gServicePane.startEditingNode(folder);
  } else {
    // or not - let's pop a dialog
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"  ]
                                  .getService(Components.interfaces.nsIPromptService);

    var input = {value: folder.name};
    var title = SBString("bookmarks.newfolder.title", "Create New Playlist");
    var prompt = SBString("bookmarks.newfolder.prompt", "Enter the name of the new playlist.");

    if (promptService.prompt(window, title, prompt, input, null, {})) {
      folder.name = input.value;
    }
  }
  return folder;
}


/**
 * Opens the update manager and checks for updates to the application.
 */
function checkForUpdates()
{
  var um =
      Components.classes["@mozilla.org/updates/update-manager;1"].
      getService(Components.interfaces.nsIUpdateManager);
  var prompter =
      Components.classes["@mozilla.org/updates/update-prompt;1"].
      createInstance(Components.interfaces.nsIUpdatePrompt);

  // If there's an update ready to be applied, show the "Update Downloaded"
  // UI instead and let the user know they have to restart the browser for
  // the changes to be applied.
  if (um.activeUpdate && um.activeUpdate.state == "pending")
    prompter.showUpdateDownloaded(um.activeUpdate);
  else
    prompter.checkForUpdates();
}

function buildHelpMenu()
{
  var updates =
      Components.classes["@mozilla.org/updates/update-service;1"].
      getService(Components.interfaces.nsIApplicationUpdateService);
  var um =
      Components.classes["@mozilla.org/updates/update-manager;1"].
      getService(Components.interfaces.nsIUpdateManager);

  // Disable the UI if the update enabled pref has been locked by the
  // administrator or if we cannot update for some other reason
  var checkForUpdates = document.getElementById("updateCmd");
  var canUpdate = updates.canUpdate;
  checkForUpdates.setAttribute("disabled", !canUpdate);
  if (!canUpdate)
    return;

  var strings = document.getElementById("songbird_strings");
  var activeUpdate = um.activeUpdate;

  // If there's an active update, substitute its name into the label
  // we show for this item, otherwise display a generic label.
  function getStringWithUpdateName(key) {
    if (activeUpdate && activeUpdate.name)
      return strings.getFormattedString(key, [activeUpdate.name]);
    return strings.getString(key + "Fallback");
  }

  // By default, show "Check for Updates..."
  var key = "default";
  if (activeUpdate) {
    switch (activeUpdate.state) {
    case "downloading":
      // If we're downloading an update at present, show the text:
      // "Downloading Firefox x.x..." otherwise we're paused, and show
      // "Resume Downloading Firefox x.x..."
      key = updates.isDownloading ? "downloading" : "resume";
      break;
    case "pending":
      // If we're waiting for the user to restart, show: "Apply Downloaded
      // Updates Now..."
      key = "pending";
      break;
    }
  }
  checkForUpdates.label = getStringWithUpdateName("updateCmd_" + key);
}

function javascriptConsole() {
  window.open("chrome://global/content/console.xul", "global:console", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar,titlebar");
}

// Match filenames ending with .xpi or .jar
function isXPI(filename) {
  return /\.(xpi|jar)$/i.test(filename);
}

// Prompt the user to install the given XPI.
function installXPI(localFilename)
{
  var inst = { xpi: localFilename };
  InstallTrigger.install( inst );  // "InstallTrigger" is a Moz Global.  Don't grep for it.
  // http://developer.mozilla.org/en/docs/XPInstall_API_Reference:InstallTrigger_Object
}

/**
 * \brief Import a URL into the main library.
 * \param url URL of item to import, also accepts nsIURI's.
 * \return The media item that was created.
 * \retval null Error during creation of item.
 */
function SBImportURLIntoMainLibrary(url) {
  var libraryManager = Components.classes["@songbirdnest.com/Songbird/library/Manager;1"]
                                  .getService(Components.interfaces.sbILibraryManager);

  var library = libraryManager.mainLibrary;

  if (url instanceof Components.interfaces.nsIURI) url = url.spec;
  if (getPlatformString() == "Windows_NT") url = url.toLowerCase();


  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
    .getService(Components.interfaces.nsIIOService);

  var uri = null;
  try {
    if( typeof(url.spec) == "undefined" ) {
      uri = ioService.newURI(url, null, null);
    }
    else {
      uri = url;
    }
  }
  catch (e) {
    log(e);
    uri = null;
  }

  if(!uri) {
    return null;
  }

  // skip import of the item if it already exists
  var mediaItem = getFirstItemByProperty(library, "http://songbirdnest.com/data/1.0#contentURL", url);
  if (mediaItem)
    return mediaItem;

  try {
    mediaItem = library.createMediaItem(uri);
  }
  catch(e) {
    log(e);
    mediaItem = null;
  }

  if(!mediaItem) {
    return null;
  }

  var metadataJobMgr = Components.classes["@songbirdnest.com/Songbird/MetadataJobManager;1"]
    .getService(Components.interfaces.sbIMetadataJobManager);

  var items = Components.classes["@songbirdnest.com/moz/xpcom/threadsafe-array;1"]
    .createInstance(Components.interfaces.nsIMutableArray);

  items.appendElement(mediaItem, false);
  metadataJobMgr.newJob(items, 5);

  return mediaItem;
}

function SBImportURLIntoWebLibrary(url) {
  var library = LibraryUtils.webLibrary;
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
    .getService(Components.interfaces.nsIIOService);

  var uri = null;
  try {
    if( typeof(url.spec) == "undefined" ) {
      uri = ioService.newURI(url, null, null);
    }
    else {
      uri = url;
    }
  }
  catch (e) {
    log(e);
    uri = null;
  }

  if(!uri) {
    return null;
  }

  var mediaItem = null;
  try {
    mediaItem = library.createMediaItem(uri);
  }
  catch(e) {
    log(e);
    mediaItem = null;
  }

  if(!mediaItem) {
    return null;
  }

  var metadataJobMgr = Components.classes["@songbirdnest.com/Songbird/MetadataJobManager;1"]
    .getService(Components.interfaces.sbIMetadataJobManager);

  var items = Components.classes["@songbirdnest.com/moz/xpcom/threadsafe-array;1"]
    .createInstance(Components.interfaces.nsIMutableArray);

  items.appendElement(mediaItem, false);
  metadataJobMgr.newJob(items, 5);

  return mediaItem;
}


function getFirstItemByProperty(aMediaList, aProperty, aValue) {

  var listener = {
    item: null,
    onEnumerationBegin: function() {
    },
    onEnumeratedItem: function(list, item) {
      this.item = item;
      return Components.interfaces.sbIMediaListEnumerationListener.CANCEL;
    },
    onEnumerationEnd: function() {
    }
  };

  aMediaList.enumerateItemsByProperty(aProperty,
                                      aValue,
                                      listener );

  return listener.item;
}


}
catch (e)
{
  alert(e);
}
