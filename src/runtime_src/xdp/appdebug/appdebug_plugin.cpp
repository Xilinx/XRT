/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "appdebug_plugin.h"
#include "appdebugmanager.h"

namespace appdebug {

  // When the library gets loaded, this singleton will be instantiated
  static AppDebugManager adm ;

  bool active()
  {
    return AppDebugManager::isActive() ;
  }

  xocl::platform* getcl_platform_id()
  {
    return adm.getcl_platform_id() ;
  }

} // end namespace appdebug

extern "C"
void initAppDebug()
{
  // Just called to make sure the library is loaded and the static object
  //  is created.
}
