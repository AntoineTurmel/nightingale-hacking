/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 :miv */
/*
 *=BEGIN SONGBIRD GPL
 *
 * This file is part of the Songbird web player.
 *
 * Copyright(c) 2005-2009 POTI, Inc.
 * http://www.songbirdnest.com
 *
 * This file may be licensed under the terms of of the
 * GNU General Public License Version 2 (the ``GPL'').
 *
 * Software distributed under the License is distributed
 * on an ``AS IS'' basis, WITHOUT WARRANTY OF ANY KIND, either
 * express or implied. See the GPL for the specific language
 * governing rights and limitations.
 *
 * You should have received a copy of the GPL along with this
 * program. If not, go to http://www.gnu.org/licenses/gpl.html
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *=END SONGBIRD GPL
 */

#include "sbDefaultBaseDeviceCapabilitiesRegistrar.h"

// Mozilla includes
#include <nsArrayUtils.h>
#include <nsISupportsPrimitives.h>
#include <nsIVariant.h>
#include <nsIWritablePropertyBag2.h>
#include <nsMemory.h>
#include <prlog.h>

// Songbird includes
#include <sbIDevice.h>
#include <sbIDeviceCapabilities.h>
#include <sbIDeviceEvent.h>
#include <sbIDeviceEventTarget.h>
#include <sbIDeviceManager.h>
#include <sbIMediaItem.h>
#include <sbITranscodeManager.h>
#include <sbProxiedComponentManager.h>
#include <sbStandardProperties.h>
#include <sbStringUtils.h>
#include <sbVariantUtils.h>

/**
 * To log this module, set the following environment variable:
 *   NSPR_LOG_MODULES=sbDefaultBaseDeviceCapabilitiesRegistrar:5
 */
#ifdef PR_LOGGING
static PRLogModuleInfo* gDefaultBaseDeviceCapabilitiesRegistrarLog = nsnull;
#define TRACE(args) PR_LOG(gDefaultBaseDeviceCapabilitiesRegistrarLog , PR_LOG_DEBUG, args)
#define LOG(args)   PR_LOG(gDefaultBaseDeviceCapabilitiesRegistrarLog , PR_LOG_WARN, args)
#ifdef __GNUC__
#define __FUNCTION__ __PRETTY_FUNCTION__
#endif /* __GNUC__ */
#else
#define TRACE(args) /* nothing */
#define LOG(args)   /* nothing */
#endif

PRInt32 const K = 1000;

sbDefaultBaseDeviceCapabilitiesRegistrar::
  sbDefaultBaseDeviceCapabilitiesRegistrar()
{
  #ifdef PR_LOGGING
    if (!gDefaultBaseDeviceCapabilitiesRegistrarLog)
      gDefaultBaseDeviceCapabilitiesRegistrarLog =
        PR_NewLogModule("sbDefaultBaseDeviceCapabilitiesRegistrar");
  #endif
}

sbDefaultBaseDeviceCapabilitiesRegistrar::
  ~sbDefaultBaseDeviceCapabilitiesRegistrar()
{
}

/* readonly attribute PRUint32 type; */
NS_IMETHODIMP
sbDefaultBaseDeviceCapabilitiesRegistrar::GetType(PRUint32 *aType)
{
  TRACE(("%s: default", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aType);

  *aType = sbIDeviceCapabilitiesRegistrar::DEFAULT;
  return NS_OK;
}

NS_IMETHODIMP
sbDefaultBaseDeviceCapabilitiesRegistrar::
  AddCapabilities(sbIDevice *aDevice,
                  sbIDeviceCapabilities *aCapabilities) {
  TRACE(("%s", __FUNCTION__));
  NS_ENSURE_ARG_POINTER(aDevice);
  NS_ENSURE_ARG_POINTER(aCapabilities);

  nsresult rv;

  // Look for capabilities settings in the device preferences
  nsCOMPtr<nsIVariant> capabilitiesVariant;
  rv = aDevice->GetPreference(NS_LITERAL_STRING("capabilities"),
                              getter_AddRefs(capabilitiesVariant));
  if (NS_SUCCEEDED(rv)) {
    PRUint16 dataType;
    rv = capabilitiesVariant->GetDataType(&dataType);
    NS_ENSURE_SUCCESS(rv, rv);

    if ((dataType == nsIDataType::VTYPE_INTERFACE) ||
        (dataType == nsIDataType::VTYPE_INTERFACE_IS)) {
      nsCOMPtr<nsISupports>           capabilitiesISupports;
      nsCOMPtr<sbIDeviceCapabilities> capabilities;
      rv = capabilitiesVariant->GetAsISupports
                                  (getter_AddRefs(capabilitiesISupports));
      NS_ENSURE_SUCCESS(rv, rv);
      capabilities = do_QueryInterface(capabilitiesISupports, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aCapabilities->AddCapabilities(capabilities);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }
  }

  // Nothing in the capabilities preferences, so use defaults
  static PRUint32 functionTypes[] = {
    sbIDeviceCapabilities::FUNCTION_AUDIO_PLAYBACK,
    sbIDeviceCapabilities::FUNCTION_IMAGE_DISPLAY
  };

  rv = aCapabilities->SetFunctionTypes(functionTypes,
                                       NS_ARRAY_LENGTH(functionTypes));
  NS_ENSURE_SUCCESS(rv, rv);

  static PRUint32 audioContentTypes[] = {
    sbIDeviceCapabilities::CONTENT_AUDIO
  };

  rv = aCapabilities->AddContentTypes(sbIDeviceCapabilities::FUNCTION_AUDIO_PLAYBACK,
                                      audioContentTypes,
                                      NS_ARRAY_LENGTH(audioContentTypes));
  NS_ENSURE_SUCCESS(rv, rv);

  static PRUint32 imageContentTypes[] = {
    sbIDeviceCapabilities::CONTENT_IMAGE,
  };

  rv = aCapabilities->AddContentTypes(sbIDeviceCapabilities::FUNCTION_IMAGE_DISPLAY,
                                      imageContentTypes,
                                      NS_ARRAY_LENGTH(imageContentTypes));
  NS_ENSURE_SUCCESS(rv, rv);

  static char const * audioFormats[] = {
    "audio/mpeg",
  };

  rv = aCapabilities->AddFormats(sbIDeviceCapabilities::CONTENT_AUDIO,
                                 audioFormats,
                                 NS_ARRAY_LENGTH(audioFormats));
  NS_ENSURE_SUCCESS(rv, rv);

  static char const * imageFormats[] = {
    "image/jpeg"
  };
  rv = aCapabilities->AddFormats(sbIDeviceCapabilities::CONTENT_IMAGE,
                                 imageFormats,
                                 NS_ARRAY_LENGTH(imageFormats));
  NS_ENSURE_SUCCESS(rv, rv);

  /**
   * Default MP3 bit rates
   */
  static PRInt32 DefaultMinMP3BitRate = 8 * K;
  static PRInt32 DefaultMaxMP3BitRate = 320 * K;

  struct sbSampleRates
  {
    PRUint32 const Rate;
    char const * const TextValue;
  };
  /**
   * Default MP3 sample rates
   */
  static PRUint32 DefaultMP3SampleRates[] =
  {
    8000,
    11025,
    12000,
    16000,
    22050,
    24000,
    32000,
    44100,
    48000
  };
  // Build the MP3 bit rate range
  nsCOMPtr<sbIDevCapRange> bitRateRange =
    do_CreateInstance(SB_IDEVCAPRANGE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bitRateRange->Initialize(DefaultMinMP3BitRate, DefaultMaxMP3BitRate, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  // Build the MP3 sample rate range
  nsCOMPtr<sbIDevCapRange> sampleRateRange =
    do_CreateInstance(SB_IDEVCAPRANGE_CONTRACTID, &rv);
  for (PRUint32 index = 0;
       index < NS_ARRAY_LENGTH(DefaultMP3SampleRates);
       ++index) {
    rv = sampleRateRange->AddValue(DefaultMP3SampleRates[index]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Build the MP3 Channel range
  nsCOMPtr<sbIDevCapRange> channelRange =
    do_CreateInstance(SB_IDEVCAPRANGE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channelRange->Initialize(1, 2, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIAudioFormatType> audioFormatType =
    do_CreateInstance(SB_IAUDIOFORMATTYPE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = audioFormatType->Initialize(NS_LITERAL_CSTRING("id3"),  // container type
                                   NS_LITERAL_CSTRING("mp3"),  // codec
                                   bitRateRange,
                                   sampleRateRange,
                                   channelRange,
                                   nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aCapabilities->AddFormatType(NS_LITERAL_STRING("audio/mpeg"),
                                    audioFormatType);
  NS_ENSURE_SUCCESS(rv, rv);

  /* We also specify a wide-ranging JPEG support type. This is used for album
     art. Devices that don't do album art at all aren't hurt by this, and every
     device that DOES do album art is capable of handling JPEG. */
  nsCOMPtr<sbIImageFormatType> imageFormatType =
    do_CreateInstance(SB_IIMAGEFORMATTYPE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Assume by default that anything from 16x16 to 2048x2048 is supported,
  // and have a default size of 200x200 
  // These are pretty arbitrary choices!

  /* Minimum and maximum width/height we permit for album art images */
  const PRInt32 defaultMinImageDimension = 16;
  const PRInt32 defaultMaxImageDimension = 2048;

  /* Default width/height for images if we have to transcode */
  const PRInt32 defaultImageWidth = 200;
  const PRInt32 defaultImageHeight = 200;

  nsCOMPtr<sbIDevCapRange> imageRange =
    do_CreateInstance(SB_IDEVCAPRANGE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = imageRange->Initialize(defaultMinImageDimension,
                              defaultMaxImageDimension,
                              1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMutableArray> imageSizes =
    do_CreateInstance("@songbirdnest.com/moz/xpcom/threadsafe-array;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIImageSize> defaultImageSize =
    do_CreateInstance(SB_IMAGESIZE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = defaultImageSize->Initialize(defaultImageWidth, defaultImageHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = imageSizes->AppendElement(defaultImageSize, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = imageFormatType->Initialize(NS_LITERAL_CSTRING("image/jpeg"),
                                   imageSizes,
                                   imageRange,
                                   imageRange);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aCapabilities->AddFormatType(NS_LITERAL_STRING("image/jpeg"),
                                    imageFormatType);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
sbDefaultBaseDeviceCapabilitiesRegistrar::InterestedInDevice(sbIDevice *aDevice,
                                                             PRBool *retval)
{
  NS_ENSURE_ARG_POINTER(retval);
  *retval = PR_FALSE;
  return NS_OK;
}

