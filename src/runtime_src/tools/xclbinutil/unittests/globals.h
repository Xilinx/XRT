
/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef __GLOBALS_h_
#define __GLOBALS_h_
#include <string>

namespace TestUtilities {

  void setResourceDir(const std::string &resourceDir);
  std::string getResourceDir();

  void setIsQuiet(bool isQuiet);
  bool isQuiet();
}

#endif
