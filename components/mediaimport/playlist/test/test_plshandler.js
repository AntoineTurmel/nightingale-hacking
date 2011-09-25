/*
//
// BEGIN NIGHTINGALE GPL
//
// This file is part of the Nightingale web player.
//
// Copyright(c) 2005-2008 POTI, Inc.
// http://getnightingale.com
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
// END NIGHTINGALE GPL
//
*/

/**
 * \brief Test file
 */

function runTest () {

  var library = createLibrary("test_playlistreader");

  var tests;
  var platform = getPlatform();
  if (platform == "Windows_NT") {
    tests = [
      {
        originalURI: null,
        file: "win_parse.pls",
        result: "win_parsepls_result.xml"
      },
      {
        originalURI: "http://www.foo.com/",
        file: "relative_remote.pls",
        result: "relative_remote_result.xml"
      },
      {
        originalURI: "http://www.foo.com/mp3",
        file: "absolute_remote.pls",
        result: "absolute_remote_result.xml"
      },
      {
        originalURI: "file:///c:/Documents%20and%20Settings/steve/Desktop/blah.mp3",
        file: "win_relative_local.pls",
        result: "win_relative_local_result.xml"
      },
      {
        originalURI: null,
        file: "win_utf8.pls",
        result: "win_utf8_result.xml"
      }
    ];
  } else {
    tests = [
      {
        originalURI: null,
        file: "maclin_parse.pls",
        result: "maclin_parsepls_result.xml"
      },
      {
        originalURI: "http://www.foo.com/",
        file: "relative_remote.pls",
        result: "relative_remote_result.xml"
      },
      {
        originalURI: "http://www.foo.com/mp3",
        file: "absolute_remote.pls",
        result: "absolute_remote_result.xml"
      },
      {
        originalURI: "file:///home/steve/blah.pls",
        file: "maclin_relative_local.pls",
        result: "maclin_relative_local_result.xml"
      },
      {
        originalURI: null,
        file: "maclin_utf8.pls",
        result: "maclin_utf8_result.xml"
      }
    ];
  }

  for (var i = 0; i < tests.length; i++) {
    var t = tests[i];
    log("testing file " + t.file);
    library.clear();
    var handler = Cc["@getnightingale.com/Nightingale/Playlist/Reader/PLS;1"]
                    .createInstance(Ci.sbIPlaylistReader);

    var file = getFile(t.file);
    if (t.originalURI) {
      handler.originalURI = newURI(t.originalURI);
    }
    handler.read(file, library, false);
    assertMediaList(library, getFile(t.result));
  }

  // Test duplicates
  library.clear();
  var mediaList = library.createMediaList("simple");
  var handler = Cc["@getnightingale.com/Nightingale/Playlist/Reader/PLS;1"]
                  .createInstance(Ci.sbIPlaylistReader);

  var file = getFile("relative_remote.pls");
  handler.originalURI = newURI("http://www.foo.com");

  handler.read(file, mediaList, false);
  assertEqual(mediaList.length, 3);
  assertEqual(mediaList.library.length, 4);
  handler.read(file, mediaList, false);
  assertEqual(mediaList.length, 6);
  assertEqual(mediaList.library.length, 4);

  // Should not change the list
  handler.read(file, mediaList, true);
  assertEqual(mediaList.library.length, 4);
  assertEqual(mediaList.length, 6);
}

