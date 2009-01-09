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
* \file  sbGStreamerMediacoreModule.cpp
* \brief Songbird GStreamer Mediacore Module Factory and Main Entry Point.
*/

#include <nsIGenericFactory.h>

#include "sbGStreamerMediacoreCID.h"
#include "sbGStreamerMediacoreFactory.h"
#include "sbGStreamerService.h"
#include "metadata/sbGStreamerMetadataHandler.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbGStreamerService, Init)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbGStreamerMediacoreFactory, Init);
SB_MEDIACORE_FACTORY_REGISTERSELF(sbGStreamerMediacoreFactory);

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbGStreamerMetadataHandler, Init)

static const nsModuleComponentInfo components[] =
{
  {
    SBGSTREAMERSERVICE_CLASSNAME,
    SBGSTREAMERSERVICE_CID,
    SBGSTREAMERSERVICE_CONTRACTID,
    sbGStreamerServiceConstructor
  },
  {
    SB_GSTREAMERMEDIACOREFACTORY_CLASSNAME, 
    SB_GSTREAMERMEDIACOREFACTORY_CID,
    SB_GSTREAMERMEDIACOREFACTORY_CONTRACTID,
    sbGStreamerMediacoreFactoryConstructor,
    sbGStreamerMediacoreFactoryRegisterSelf,
    sbGStreamerMediacoreFactoryUnregisterSelf
  },
  {
    SB_GSTREAMER_METADATA_HANDLER_CLASSNAME,
    SB_GSTREAMER_METADATA_HANDLER_CID,
    SB_GSTREAMER_METADATA_HANDLER_CONTRACTID,
    sbGStreamerMetadataHandlerConstructor
  },
};

NS_IMPL_NSGETMODULE(sbGstreamerMediacoreModule, components)

