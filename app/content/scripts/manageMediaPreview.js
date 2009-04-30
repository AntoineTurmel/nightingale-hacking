/* vim: set sw=2 : */
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
 * Script for the media management preview dialog
 * see: chrome://songbird/content/xul/manageMediaPreview.xul
 * in source: app/content/xul/manageMediaPreview.xul
 */

//
// Defs.
//

if (typeof(Cc) == "undefined")
  var Cc = Components.classes;
if (typeof(Ci) == "undefined")
  var Ci = Components.interfaces;
if (typeof(Cr) == "undefined")
  var Cr = Components.results;
if (typeof(Cu) == "undefined")
  var Cu = Components.utils;

Cu.import("resource://app/jsmodules/sbLibraryUtils.jsm");
Cu.import("resource://app/jsmodules/StringUtils.jsm");

var mediaManagePreview = {
  job: null,

  onLoad: function mediaManagePreview_onLoad() {
    // Get the UI parameters.
    var dialogPB = window.arguments[0].QueryInterface(Ci.nsIDialogParamBlock);
    var mediaList = dialogPB.objects.queryElementAt(0, Ci.sbIMediaList);
    this.job = Cc["@songbirdnest.com/Songbird/media-manager/job;1"]
                 .createInstance(Ci.sbIMediaManagementJob);
    this.job.init(mediaList);
    // save the result, since touching anything at all can clobber it!
    var rv = Components.lastResult;
    if (rv == Cr.NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA || mediaList.length == 0) {
      // no items
      setTimeout(function mediaManagePreview_autoAccept() {
        document.documentElement.acceptDialog();
      }, 0);
    }
    this.job.QueryInterface(Ci.nsISimpleEnumerator);
    
    // don't delay showing the UI for this
    setTimeout(function mediaManagePreview_initialListing(self) {
      self.more();
    }, 0, this);
  },

  addItem: function mediaManagePreview_addItem() {
    if (!this.job || !this.job.hasMoreElements()) {
      return false;
    }
    var jobItem = this.job.getNext()
                      .QueryInterface(Ci.sbIMediaManagementJobItem);
    
    // make the new item by cloning the template
    var listItem = document.getElementById("preview_listitem_template")
                           .cloneNode(true);
    listItem.id = "preview_listitem_" + jobItem.item.guid;

    // insert the item into the document otherwise assignment doesn't work right
    document.getElementById("preview_listbox").appendChild(listItem);

    // figure out the action verbs
    var action = "unknown";
    if (jobItem.action & Ci.sbIMediaFileManager.MANAGE_DELETE) {
      action = "delete";
    } else if (jobItem.action & Ci.sbIMediaFileManager.MANAGE_COPY) {
      action = "copy";
    } else if (jobItem.action & Ci.sbIMediaFileManager.MANAGE_MOVE) {
      action = "move";
    } else if (jobItem.action & Ci.sbIMediaFileManager.MANAGE_RENAME) {
      action = "rename";
    }
    
    // fill in the files involved
    var srcURL = jobItem.item.contentSrc;
    var srcText = srcURL.spec;
    if (srcURL instanceof Ci.nsIFileURL) {
      listItem.file = srcURL.file;
      srcText = srcURL.file.path;
    } else {
      listItem.file= null;
    }
    
    var destText = "";
    if (action != "delete") {
      destText = jobItem.targetPath.path;
    }
    
    listItem.getElementsByAttribute("anonid", "preview_listitem_label")[0]
            .textContent = SBFormattedString("prefs.media_management.preview." + action,
                                             [srcText, destText]);

    // show the item
    listItem.collapsed = false;
    return true;
  },

  onPopupShowing: function mediaManagePreview_onPopupShowing(event) {
    var target = document.popupNode;
    if (!target) {
      event.preventDefault();
      return false;
    }
    loop: while(true) {
      switch(target.getAttribute("anonid")) {
        case "preview_listitem_description":
        case "preview_listitem_label":
          target = target.parentNode;
          break;
        default:
          break loop;
      }
    }
    var item = document.getElementById("perview_list_popupitem");
    item.disabled = (target.file == null);
    item.file = target.file;
    return true;
  },

  onRevealFile: function mediaManagePreview_onRevealFile(event) {
    var file = event.target.file;
    if (file && file instanceof Ci.nsILocalFile) {
      file.reveal();
    } else {
      Components.utils.reportError("Failed to reveal file!");
    }
  },

  more: function mediaManagePreview_more() {
    var richListBox = document.getElementById("preview_listbox");
    var length = richListBox.itemCount;
    var stringSuffix = "";
    for (var i = 0; i < 50; ++i) {
      if (!this.addItem()) {
        document.getElementById("preview_more_label").disabled = true;
        stringSuffix = ".complete";
        break;
      }
    }
    richListBox.ensureIndexIsVisible(length);
    document.getElementById("preview_count_label").value =
      SBFormattedString("prefs.media_management.preview.count" + stringSuffix,
                        [richListBox.itemCount]);
  }
};