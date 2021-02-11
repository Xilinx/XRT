
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

#include "globals.h"
#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;


static std::string m_resourceDir;

void 
TestUtilities::setResourceDir(const std::string &resourceDir)
{
  m_resourceDir = resourceDir;
}

std::string 
TestUtilities::getResourceDir()
{
  return m_resourceDir;
}

void 
TestUtilities::setIsQuiet(bool isQuiet)
{
  XUtil::setQuiet(isQuiet);
}

bool 
TestUtilities::isQuiet()
{
  return XUtil::isQuiet();
}
