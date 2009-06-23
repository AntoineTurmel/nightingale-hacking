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

#ifndef sbMediaListEnumArrayHelper_h_
#define sbMediaListEnumArrayHelper_h_

#include <nsCOMPtr.h>
#include <nsIMutableArray.h>
#include <sbIMediaListListener.h>


//------------------------------------------------------------------------------
//
// Helper class for fetching an array of media items during an enumeration.
//
//------------------------------------------------------------------------------

class sbMediaListEnumArrayHelper : public sbIMediaListEnumerationListener
{
public:
  sbMediaListEnumArrayHelper();
  virtual ~sbMediaListEnumArrayHelper();

  NS_DECL_ISUPPORTS
  NS_DECL_SBIMEDIALISTENUMERATIONLISTENER

  //
  // \brief Resets and clears the media items array. Be sure to call this
  //        method before using this class as a enum listener.
  //
  nsresult New();

  NS_IMETHOD GetMediaItemsArray(nsIArray **aOutArray);

protected:
  nsCOMPtr<nsIMutableArray> mItemsArray;
};

#endif  // sbMediaListEnumArrayHelper_h_

