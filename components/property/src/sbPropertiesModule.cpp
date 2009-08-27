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

#include "nsIGenericFactory.h"

#include "sbBooleanPropertyInfo.h"
#include "sbDatetimePropertyInfo.h"
#include "sbDurationPropertyInfo.h"
#include "sbDownloadButtonPropertyBuilder.h"
#include "sbImagePropertyBuilder.h"
#include "sbNumberPropertyInfo.h"
#include "sbPropertyArray.h"
#include "sbPropertyFactory.h"
#include "sbPropertyManager.h"
#include "sbOriginPageImagePropertyBuilder.h"
#include "sbRatingPropertyBuilder.h"
#include "sbSimpleButtonPropertyBuilder.h"
#include "sbStatusPropertyBuilder.h"
#include "sbTextPropertyInfo.h"
#include "sbURIPropertyInfo.h"

#include "sbPropertiesCID.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbPropertyArray, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(sbPropertyFactory);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbPropertyManager, Init);

NS_GENERIC_FACTORY_CONSTRUCTOR(sbPropertyOperator);
NS_GENERIC_FACTORY_CONSTRUCTOR(sbDatetimePropertyInfo);
NS_GENERIC_FACTORY_CONSTRUCTOR(sbDurationPropertyInfo);
NS_GENERIC_FACTORY_CONSTRUCTOR(sbNumberPropertyInfo);
NS_GENERIC_FACTORY_CONSTRUCTOR(sbTextPropertyInfo);
NS_GENERIC_FACTORY_CONSTRUCTOR(sbURIPropertyInfo);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbBooleanPropertyInfo, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbDownloadButtonPropertyBuilder, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbStatusPropertyBuilder, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbSimpleButtonPropertyBuilder, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbImagePropertyBuilder, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbRatingPropertyBuilder, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(sbOriginPageImagePropertyBuilder, Init);

static const nsModuleComponentInfo components[] =
{
	{
    SB_MUTABLEPROPERTYARRAY_DESCRIPTION,
    SB_MUTABLEPROPERTYARRAY_CID,
    SB_MUTABLEPROPERTYARRAY_CONTRACTID,
    sbPropertyArrayConstructor
	},
	{
    SB_PROPERTYFACTORY_DESCRIPTION,
    SB_PROPERTYFACTORY_CID,
    SB_PROPERTYFACTORY_CONTRACTID,
    sbPropertyFactoryConstructor
	},
  {
    SB_PROPERTYMANAGER_DESCRIPTION,
    SB_PROPERTYMANAGER_CID,
    SB_PROPERTYMANAGER_CONTRACTID,
    sbPropertyManagerConstructor
  },
  {
    SB_PROPERTYOPERATOR_DESCRIPTION,
    SB_PROPERTYOPERATOR_CID,
    SB_PROPERTYOPERATOR_CONTRACTID,
    sbPropertyOperatorConstructor
  },
  {
    SB_DATETIMEPROPERTYINFO_DESCRIPTION,
    SB_DATETIMEPROPERTYINFO_CID,
    SB_DATETIMEPROPERTYINFO_CONTRACTID,
    sbDatetimePropertyInfoConstructor
  },
  {
    SB_DURATIONPROPERTYINFO_DESCRIPTION,
    SB_DURATIONPROPERTYINFO_CID,
    SB_DURATIONPROPERTYINFO_CONTRACTID,
    sbDurationPropertyInfoConstructor
  },
  {
    SB_NUMBERPROPERTYINFO_DESCRIPTION,
    SB_NUMBERPROPERTYINFO_CID,
    SB_NUMBERPROPERTYINFO_CONTRACTID,
    sbNumberPropertyInfoConstructor
  },
  {
    SB_TEXTPROPERTYINFO_DESCRIPTION,
    SB_TEXTPROPERTYINFO_CID,
    SB_TEXTPROPERTYINFO_CONTRACTID,
    sbTextPropertyInfoConstructor
  },
  {
    SB_URIPROPERTYINFO_DESCRIPTION,
    SB_URIPROPERTYINFO_CID,
    SB_URIPROPERTYINFO_CONTRACTID,
    sbURIPropertyInfoConstructor
  },
  {
    SB_BOOLEANPROPERTYINFO_DESCRIPTION,
    SB_BOOLEANPROPERTYINFO_CID,
    SB_BOOLEANPROPERTYINFO_CONTRACTID,
    sbBooleanPropertyInfoConstructor
  },
  {
    SB_DOWNLOADBUTTONPROPERTYBUILDER_DESCRIPTION,
    SB_DOWNLOADBUTTONPROPERTYBUILDER_CID,
    SB_DOWNLOADBUTTONPROPERTYBUILDER_CONTRACTID,
    sbDownloadButtonPropertyBuilderConstructor
  },
  {
    SB_STATUSPROPERTYBUILDER_DESCRIPTION,
    SB_STATUSPROPERTYBUILDER_CID,
    SB_STATUSPROPERTYBUILDER_CONTRACTID,
    sbStatusPropertyBuilderConstructor
  },
  {
    SB_SIMPLEBUTTONPROPERTYBUILDER_DESCRIPTION,
    SB_SIMPLEBUTTONPROPERTYBUILDER_CID,
    SB_SIMPLEBUTTONPROPERTYBUILDER_CONTRACTID,
    sbSimpleButtonPropertyBuilderConstructor
  },
  {
    SB_IMAGEPROPERTYBUILDER_DESCRIPTION,
    SB_IMAGEPROPERTYBUILDER_CID,
    SB_IMAGEPROPERTYBUILDER_CONTRACTID,
    sbImagePropertyBuilderConstructor
  },
  {
    SB_RATINGPROPERTYBUILDER_DESCRIPTION,
    SB_RATINGPROPERTYBUILDER_CID,
    SB_RATINGPROPERTYBUILDER_CONTRACTID,
    sbRatingPropertyBuilderConstructor
  },
  {
    SB_ORIGINPAGEIMAGEPROPERTYBUILDER_DESCRIPTION,
    SB_ORIGINPAGEIMAGEPROPERTYBUILDER_CID,
    SB_ORIGINPAGEIMAGEPROPERTYBUILDER_CONTRACTID,
    sbOriginPageImagePropertyBuilderConstructor
  },
};

NS_IMPL_NSGETMODULE(SongbirdPropertiesModule, components)
