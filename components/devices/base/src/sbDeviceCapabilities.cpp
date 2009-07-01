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

#include "sbDeviceCapabilities.h"

#include <nsIMutableArray.h>
#include <nsServiceManagerUtils.h>
#include <nsComponentManagerUtils.h>
#include <nsArrayUtils.h>

NS_IMPL_THREADSAFE_ISUPPORTS1(sbDeviceCapabilities, sbIDeviceCapabilities)

#include <prlog.h>
#include <prprf.h>
#include <prtime.h>

/**
 * To log this module, set the following environment variable:
 *   NSPR_LOG_MODULES=sbDeviceContent:5
 */
#ifdef PR_LOGGING
static PRLogModuleInfo* gDeviceCapabilitiesLog = nsnull;
#define TRACE(args) PR_LOG(gDeviceCapabilitiesLog, PR_LOG_DEBUG, args)
#define LOG(args)   PR_LOG(gDeviceCapabilitiesLog, PR_LOG_WARN, args)
#else
#define TRACE(args) /* nothing */
#define LOG(args)   /* nothing */
#endif /* PR_LOGGING */

sbDeviceCapabilities::sbDeviceCapabilities() :
isInitialized(false)
{
  nsresult rv = mContentTypes.Init();
  NS_ENSURE_SUCCESS(rv, /* void */);
  
  rv = mSupportedFormats.Init();
  NS_ENSURE_SUCCESS(rv, /* void */);
  
  rv = mFormatTypes.Init();
  NS_ENSURE_SUCCESS(rv, /* void */);
  
#ifdef PR_LOGGING
  if (!gDeviceCapabilitiesLog) {
    gDeviceCapabilitiesLog = PR_NewLogModule("sbDeviceCapabilities");
  }
#endif
  TRACE(("sbDeviceCapabilities[0x%.8x] - Constructed", this));
}

sbDeviceCapabilities::~sbDeviceCapabilities()
{
  TRACE(("sbDeviceCapabilities[0x%.8x] - Destructed", this));
}


NS_IMETHODIMP
sbDeviceCapabilities::InitDone()
{
  NS_ENSURE_TRUE(!isInitialized, NS_ERROR_ALREADY_INITIALIZED);

  /* set this so we are not called again */
  isInitialized = true;
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceCapabilities::SetFunctionTypes(PRUint32 *aFunctionTypes,
                                       PRUint32 aFunctionTypesCount)
{
  NS_ENSURE_TRUE(!isInitialized, NS_ERROR_ALREADY_INITIALIZED);

  for (PRUint32 arrayCounter = 0; arrayCounter < aFunctionTypesCount; ++arrayCounter) {
    mFunctionTypes.AppendElement(aFunctionTypes[arrayCounter]);
  }

  return NS_OK;
}

NS_IMETHODIMP
sbDeviceCapabilities::SetEventTypes(PRUint32 *aEventTypes,
                                    PRUint32 aEventTypesCount)
{
  NS_ENSURE_TRUE(!isInitialized, NS_ERROR_ALREADY_INITIALIZED);

  for (PRUint32 arrayCounter = 0; arrayCounter < aEventTypesCount; ++arrayCounter) {
    mSupportedEvents.AppendElement(aEventTypes[arrayCounter]);
  }

  return NS_OK;
}


NS_IMETHODIMP
sbDeviceCapabilities::AddContentTypes(PRUint32 aFunctionType,
                                      PRUint32 *aContentTypes,
                                      PRUint32 aContentTypesCount)
{
  NS_ENSURE_ARG_POINTER(aContentTypes);
  NS_ENSURE_TRUE(!isInitialized, NS_ERROR_ALREADY_INITIALIZED);

  nsTArray<PRUint32> * nContentTypes = new nsTArray<PRUint32>(aContentTypesCount);
  
  for (PRUint32 arrayCounter = 0; arrayCounter < aContentTypesCount; ++arrayCounter) {
    nContentTypes->AppendElement(aContentTypes[arrayCounter]);
  }
  
  mContentTypes.Put(aFunctionType, nContentTypes);

  return NS_OK;
}

NS_IMETHODIMP
sbDeviceCapabilities::AddFormats(PRUint32 aContentType,
                                 const char * *aFormats,
                                 PRUint32 aFormatsCount)
{
  NS_ENSURE_ARG_POINTER(aFormats);
  NS_ENSURE_TRUE(!isInitialized, NS_ERROR_ALREADY_INITIALIZED);

  nsTArray<nsCString> * nFormats = new nsTArray<nsCString>(aFormatsCount);
  
  for (PRUint32 arrayCounter = 0; arrayCounter < aFormatsCount; ++arrayCounter) {
    nFormats->AppendElement(aFormats[arrayCounter]);
  }
  
  mSupportedFormats.Put(aContentType, nFormats);

  return NS_OK;
}

NS_IMETHODIMP
sbDeviceCapabilities::AddFormatType(nsAString const & aFormat, 
                                    nsISupports * aFormatType)
{
  NS_ENSURE_ARG_POINTER(aFormatType);
  
  PRBool const added = mFormatTypes.Put(aFormat, aFormatType);
  NS_ENSURE_TRUE(added, NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceCapabilities::GetSupportedFunctionTypes(PRUint32 *aArrayCount,
                                                PRUint32 **aFunctionTypes)
{
  NS_ENSURE_ARG_POINTER(aArrayCount);
  NS_ENSURE_ARG_POINTER(aFunctionTypes);
  NS_ENSURE_TRUE(isInitialized, NS_ERROR_NOT_INITIALIZED);

  PRUint32 arrayLen = mFunctionTypes.Length();
  PRUint32* outArray = (PRUint32*)nsMemory::Alloc(arrayLen * sizeof(PRUint32));
  NS_ENSURE_TRUE(outArray, NS_ERROR_OUT_OF_MEMORY);
  
  for (PRUint32 arrayCounter = 0; arrayCounter < arrayLen; arrayCounter++) {
    outArray[arrayCounter] = mFunctionTypes[arrayCounter];
  }

  *aArrayCount = arrayLen;
  *aFunctionTypes = outArray;
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceCapabilities::GetSupportedContentTypes(PRUint32 aFunctionType,
                                               PRUint32 *aArrayCount,
                                               PRUint32 **aContentTypes)
{
  NS_ENSURE_ARG_POINTER(aArrayCount);
  NS_ENSURE_ARG_POINTER(aContentTypes);
  NS_ENSURE_TRUE(isInitialized, NS_ERROR_NOT_INITIALIZED);
  
  nsTArray<PRUint32>* contentTypes;

  if (!mContentTypes.Get(aFunctionType, &contentTypes)) {
    NS_WARNING("There are no content types for the requested function type.");
    return NS_ERROR_NOT_AVAILABLE;
  }

  PRUint32 arrayLen = contentTypes->Length();
  PRUint32* outArray = (PRUint32*)nsMemory::Alloc(arrayLen * sizeof(PRUint32));
  if (!outArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  for (PRUint32 arrayCounter = 0; arrayCounter < arrayLen; arrayCounter++) {
    outArray[arrayCounter] = contentTypes->ElementAt(arrayCounter);
  }
  
  *aArrayCount = arrayLen;
  *aContentTypes = outArray;
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceCapabilities::GetSupportedFormats(PRUint32 aContentType,
                                          PRUint32 *aArrayCount,
                                          char ***aSupportedFormats)
{
  NS_ENSURE_ARG_POINTER(aArrayCount);
  NS_ENSURE_ARG_POINTER(aSupportedFormats);
  NS_ENSURE_TRUE(isInitialized, NS_ERROR_NOT_INITIALIZED);
  
  nsTArray<nsCString>* supportedFormats;

  if (!mSupportedFormats.Get(aContentType, &supportedFormats)) {
    NS_WARNING("Requseted content type is not available for this device.");
    return NS_ERROR_NOT_AVAILABLE;
  }

  PRUint32 arrayLen = supportedFormats->Length();
  char** outArray = reinterpret_cast<char**>(NS_Alloc(arrayLen * sizeof(char*)));
  if (!outArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  for (PRUint32 arrayCounter = 0; arrayCounter < arrayLen; arrayCounter++) {
    nsCString const & format = supportedFormats->ElementAt(arrayCounter);
    outArray[arrayCounter] = ToNewCString(format);
  }

  *aArrayCount = arrayLen;
  *aSupportedFormats = outArray;
  return NS_OK;
}

NS_IMETHODIMP
sbDeviceCapabilities::GetSupportedEvents(PRUint32 *aArrayCount,
                                         PRUint32 **aSupportedEvents)
{
  NS_ENSURE_ARG_POINTER(aArrayCount);
  NS_ENSURE_ARG_POINTER(aSupportedEvents);
  NS_ENSURE_TRUE(isInitialized, NS_ERROR_NOT_INITIALIZED);

  PRUint32 arrayLen = mSupportedEvents.Length();
  PRUint32* outArray = (PRUint32*)nsMemory::Alloc(arrayLen * sizeof(PRUint32));
  if (!outArray) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  for (PRUint32 arrayCounter = 0; arrayCounter < arrayLen; arrayCounter++) {
    outArray[arrayCounter] = mSupportedEvents[arrayCounter];
  }

  *aArrayCount = arrayLen;
  *aSupportedEvents = outArray;
  return NS_OK;
}

/**
 * Returns the list of constraints for the format
 */
NS_IMETHODIMP
sbDeviceCapabilities::GetFormatType(nsAString const & aFormat,
                                    nsISupports ** aFormatType) {
  NS_ENSURE_ARG_POINTER(aFormatType);
  
  return mFormatTypes.Get(aFormat, aFormatType) ? NS_OK : 
                                                  NS_ERROR_NOT_AVAILABLE;
}

/*******************************************************************************
 * sbImageSize
 */

NS_IMPL_ISUPPORTS1(sbImageSize, sbIImageSize)

sbImageSize::~sbImageSize()
{
  /* destructor code */
}

NS_IMETHODIMP  
sbImageSize::Initialize(PRInt32 aWidth, 
                        PRInt32 aHeight) {
  mWidth = aWidth;
  mHeight = aHeight;
  
  return NS_OK;
}

/* readonly attribute long width; */
NS_IMETHODIMP 
sbImageSize::GetWidth(PRInt32 *aWidth)
{
  NS_ENSURE_ARG_POINTER(aWidth);
  
  *aWidth = mWidth;
  return NS_OK;
}

/* readonly attribute long height; */
NS_IMETHODIMP 
sbImageSize::GetHeight(PRInt32 *aHeight)
{
  NS_ENSURE_ARG_POINTER(aHeight);
  
  *aHeight = mHeight;
  return NS_OK;
}

/*******************************************************************************
 * sbDevCapRange
 */

NS_IMPL_ISUPPORTS1(sbDevCapRange, sbIDevCapRange)

sbDevCapRange::~sbDevCapRange()
{
  /* destructor code */
}

NS_IMETHODIMP sbDevCapRange::Initialize(PRInt32 aMin, 
                                        PRInt32 aMax,
                                        PRInt32 aStep) {
  mMin = aMin;
  mMax = aMax;
  mStep = aStep;
  mValues.Clear();
  return NS_OK;
}

/* readonly attribute nsIArray values; */
NS_IMETHODIMP 
sbDevCapRange::GetValue(PRUint32 aIndex, PRInt32 * aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  
  *aValue = mValues[aIndex];
  return NS_OK;
}

NS_IMETHODIMP
sbDevCapRange::AddValue(PRInt32 aValue) 
{
  NS_ENSURE_TRUE(mValues.AppendElement(aValue), NS_ERROR_OUT_OF_MEMORY);
  
  return NS_OK;
}

NS_IMETHODIMP
sbDevCapRange::GetValueCount(PRUint32 * aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  
  *aCount = mValues.Length();
  return NS_OK;
}

/* readonly attribute long min; */
NS_IMETHODIMP 
sbDevCapRange::GetMin(PRInt32 *aMin)
{
  NS_ENSURE_ARG_POINTER(aMin);
  
  *aMin = mMin;
  return NS_OK;
}

/* readonly attribute long max; */
NS_IMETHODIMP 
sbDevCapRange::GetMax(PRInt32 *aMax)
{
  NS_ENSURE_ARG_POINTER(aMax);
  
  *aMax = mMax;
  return NS_OK;
}

/* readonly attribute long step; */
NS_IMETHODIMP 
sbDevCapRange::GetStep(PRInt32 *aStep)
{
  NS_ENSURE_ARG_POINTER(aStep);
  
  *aStep = mStep;
  return NS_OK;
}

NS_IMETHODIMP
sbDevCapRange::IsValueInRange(PRInt32 aValue, PRBool * aInRange) {
  NS_ENSURE_ARG_POINTER(aInRange);
  
  if (mValues.Length() > 0) {
    *aInRange = mValues.Contains(aValue);
  }
  else {
    *aInRange = aValue <= mMax && aValue >= mMin &&
               (mStep == 0 || ((aValue - mMin) % mStep == 0));
  }
  return NS_OK;
}

/*******************************************************************************
 * sbFormatTypeConstraint
 */

NS_IMPL_ISUPPORTS1(sbFormatTypeConstraint, sbIFormatTypeConstraint)


sbFormatTypeConstraint::~sbFormatTypeConstraint()
{
  /* destructor code */
}

NS_IMETHODIMP 
sbFormatTypeConstraint::Initialize(nsAString const & aConstraintName,
                                   nsIVariant * aMinValue,
                                   nsIVariant * aMaxValue) {
  NS_ENSURE_ARG_POINTER(aMinValue);
  NS_ENSURE_ARG_POINTER(aMaxValue);
  
  mConstraintName = aConstraintName;
  mMinValue = aMinValue;
  mMaxValue = aMaxValue;
  return NS_OK;
}

/* readonly attribute AString constraintName; */
NS_IMETHODIMP 
sbFormatTypeConstraint::GetConstraintName(nsAString & aConstraintName)
{
  aConstraintName = mConstraintName;
  return NS_OK;
}

/* readonly attribute nsIVariant constraintMinValue; */
NS_IMETHODIMP 
sbFormatTypeConstraint::GetConstraintMinValue(nsIVariant * *aConstraintMinValue)
{
  NS_ENSURE_ARG_POINTER(aConstraintMinValue);
  
  *aConstraintMinValue = mMinValue.get();
  NS_IF_ADDREF(*aConstraintMinValue);
  return NS_OK;
}

/* readonly attribute nsIVariant constraintMaxValue; */
NS_IMETHODIMP 
sbFormatTypeConstraint::GetConstraintMaxValue(nsIVariant * *aConstraintMaxValue)
{
  NS_ENSURE_ARG_POINTER(aConstraintMaxValue);
  
  *aConstraintMaxValue = mMaxValue.get();
  NS_IF_ADDREF(*aConstraintMaxValue);
  return NS_OK;
}

/*******************************************************************************
 * Image format type implementation
 */

/* Implementation file */
NS_IMPL_ISUPPORTS1(sbImageFormatType, sbIImageFormatType)

sbImageFormatType::~sbImageFormatType()
{
  /* destructor code */
}

NS_IMETHODIMP
sbImageFormatType::Initialize(nsACString const & aImageFormat,
                              nsIArray * aSupportedExplicitSizes,
                              sbIDevCapRange * aSupportedWidths,
                              sbIDevCapRange * aSupportedHeights) {
  NS_ENSURE_ARG_POINTER(aSupportedExplicitSizes);
  NS_ENSURE_ARG_POINTER(aSupportedWidths);
  NS_ENSURE_ARG_POINTER(aSupportedHeights);
  
  mImageFormat = aImageFormat;
  mSupportedExplicitSizes = aSupportedExplicitSizes;
  mSupportedWidths = aSupportedWidths;
  mSupportedHeights = aSupportedHeights;
  return NS_OK;
}

/* readonly attribute ACString imageFormat; */
NS_IMETHODIMP 
sbImageFormatType::GetImageFormat(nsACString & aImageFormat)
{
  aImageFormat = mImageFormat;
  return NS_OK;
}

/* readonly attribute nsIArray supportedExplicitSizes; */
NS_IMETHODIMP 
sbImageFormatType::GetSupportedExplicitSizes(nsIArray * *aSupportedExplicitSizes)
{
  NS_ENSURE_ARG_POINTER(aSupportedExplicitSizes);
  
  *aSupportedExplicitSizes = mSupportedExplicitSizes;
  NS_IF_ADDREF(*aSupportedExplicitSizes);
  return NS_OK;
}

/* readonly attribute sbIDevCapRange supportedWidths; */
NS_IMETHODIMP 
sbImageFormatType::GetSupportedWidths(sbIDevCapRange * *aSupportedWidths)
{
  NS_ENSURE_ARG_POINTER(aSupportedWidths);
  
  *aSupportedWidths = mSupportedWidths;
  NS_IF_ADDREF(*aSupportedWidths);
  return NS_OK;
}

/* readonly attribute sbIDevCapRange supportedHeights; */
NS_IMETHODIMP 
sbImageFormatType::GetSupportedHeights(sbIDevCapRange * *aSupportedHeights)
{
  NS_ENSURE_ARG_POINTER(aSupportedHeights);
  
  *aSupportedHeights = mSupportedHeights;
  NS_IF_ADDREF(*aSupportedHeights);
  return NS_OK;
}

/*******************************************************************************
 * Audio format type
 */

/* Implementation file */
NS_IMPL_ISUPPORTS1(sbAudioFormatType, sbIAudioFormatType)

sbAudioFormatType::~sbAudioFormatType()
{
  /* destructor code */
}

NS_IMETHODIMP 
sbAudioFormatType::Initialize(nsACString const & aContainerFormat,
                              nsACString const & aAudioCodec,
                              sbIDevCapRange * aSupportedBitrates,
                              sbIDevCapRange * aSupportedSampleRates,
                              sbIDevCapRange * aSupportedChannels,
                              nsIArray * aFormatSpecificConstraints) {
  mContainerFormat = aContainerFormat;
  mAudioCodec = aAudioCodec;
  mSupportedBitrates = aSupportedBitrates;
  mSupportedSampleRates = aSupportedSampleRates;
  mSupportedChannels = aSupportedChannels;
  mFormatSpecificConstraints = aFormatSpecificConstraints;
  
  return NS_OK;
}

/* readonly attribute ACString containerFormat; */
NS_IMETHODIMP 
sbAudioFormatType::GetContainerFormat(nsACString & aContainerFormat)
{
  aContainerFormat = mContainerFormat;
  return NS_OK;
}

/* readonly attribute ACString audioCodec; */
NS_IMETHODIMP 
sbAudioFormatType::GetAudioCodec(nsACString & aAudioCodec)
{
  aAudioCodec = mAudioCodec;
  return NS_OK;
}

/* readonly attribute sbIDevCapRange supportedBitrates; */
NS_IMETHODIMP 
sbAudioFormatType::GetSupportedBitrates(sbIDevCapRange * *aSupportedBitrates)
{
  NS_ENSURE_ARG_POINTER(aSupportedBitrates);
  *aSupportedBitrates = mSupportedBitrates;
  NS_IF_ADDREF(*aSupportedBitrates);
  return NS_OK;
}

/* readonly attribute sbIDevCapRange supportedSampleRates; */
NS_IMETHODIMP 
sbAudioFormatType::GetSupportedSampleRates(sbIDevCapRange * *aSupportedSampleRates)
{
  NS_ENSURE_ARG_POINTER(aSupportedSampleRates);
  *aSupportedSampleRates = mSupportedSampleRates;
  NS_IF_ADDREF(*aSupportedSampleRates);
  return NS_OK;
}

/* readonly attribute sbIDevCapRange supportedChannels; */
NS_IMETHODIMP 
sbAudioFormatType::GetSupportedChannels(sbIDevCapRange * *aSupportedChannels)
{
  NS_ENSURE_ARG_POINTER(aSupportedChannels);
  *aSupportedChannels = mSupportedChannels;
  NS_IF_ADDREF(*aSupportedChannels);
  return NS_OK;
}

/* readonly attribute nsIArray formatSpecificConstraints; */
NS_IMETHODIMP 
sbAudioFormatType::GetFormatSpecificConstraints(nsIArray * *aFormatSpecificConstraints)
{
  NS_ENSURE_ARG_POINTER(aFormatSpecificConstraints);
  *aFormatSpecificConstraints = mFormatSpecificConstraints;
  NS_IF_ADDREF(*aFormatSpecificConstraints);
  return NS_OK;
}

