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
* \file MetadataHandlerWMA.h
* \brief 
*/

#ifndef __METADATA_HANDLER_WMA_H__
#define __METADATA_HANDLER_WMA_H__

// INCLUDES ===================================================================
#include <nsCOMPtr.h>
#include <nsStringGlue.h>

#include "sbIMetadataHandler.h"
#include "sbIMetadataChannel.h"

// DEFINES ====================================================================
#define SONGBIRD_METADATAHANDLERWMA_CONTRACTID  "@songbirdnest.com/Songbird/MetadataHandler/WMA;1"
#define SONGBIRD_METADATAHANDLERWMA_CLASSNAME   "Songbird WMA Metadata Handler Interface"

// {2D42C52D-3B4E-4085-99F6-53DB14C5996C}
#define SONGBIRD_METADATAHANDLERWMA_CID { 0x2d42c52d, 0x3b4e, 0x4085, { 0x99, 0xf6, 0x53, 0xdb, 0x14, 0xc5, 0x99, 0x6c } }

// FUNCTIONS ==================================================================

// CLASSES ====================================================================

class nsIChannel;

class sbMetadataHandlerWMA : public sbIMetadataHandler
{
  NS_DECL_ISUPPORTS
  NS_DECL_SBIMETADATAHANDLER

  sbMetadataHandlerWMA();

protected:
  ~sbMetadataHandlerWMA();

  nsCOMPtr<sbIMutablePropertyArray>  m_PropertyArray;
  nsCOMPtr<sbIMetadataChannel> m_ChannelHandler;
  nsCOMPtr<nsIChannel>         m_Channel;
  PRBool                       m_Completed;
  PRBool                       m_COMInitialized;

  NS_METHOD ReadMetadataWMFSDK(const nsAString& aFilePath, PRInt32* _retval);
  NS_METHOD ReadMetadataWMP(const nsAString& aFilePath, PRInt32* _retval);
};

#endif // __METADATA_HANDLER_WMA_H__
