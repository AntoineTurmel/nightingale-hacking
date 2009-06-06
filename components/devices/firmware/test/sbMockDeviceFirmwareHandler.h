/* vim: set sw=2 :miv */
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

#include "sbBaseDeviceFirmwareHandler.h"

class sbMockDeviceFirmwareHandler : public sbBaseDeviceFirmwareHandler
{
public:
  NS_DECL_ISUPPORTS
    
  sbMockDeviceFirmwareHandler();

  virtual nsresult OnInit();
  virtual nsresult OnCanUpdate(sbIDevice *aDevice, 
                               PRBool *_retval);
  virtual nsresult OnRefreshInfo(sbIDevice *aDevice, 
                                 sbIDeviceEventListener *aListener);
  virtual nsresult OnUpdate(sbIDevice *aDevice, 
                            sbIDeviceFirmwareUpdate *aFirmwareUpdate, 
                            sbIDeviceEventListener *aListener);
  virtual nsresult OnVerifyDevice(sbIDevice *aDevice, 
                                  sbIDeviceEventListener *aListener);
  virtual nsresult OnVerifyUpdate(sbIDevice *aDevice, 
                                  sbIDeviceFirmwareUpdate *aFirmwareUpdate, 
                                  sbIDeviceEventListener *aListener);
  virtual nsresult OnHttpRequestCompleted();

private:
  virtual ~sbMockDeviceFirmwareHandler();

protected:
  nsresult HandleRefreshInfoRequest();
  
};