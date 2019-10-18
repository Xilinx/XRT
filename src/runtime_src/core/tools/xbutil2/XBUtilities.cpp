/**
 * Copyright (C) 2019 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "XBUtilities.h"

// 3rd Party Library - Include Files

// System - Include Files
#include <iostream>

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBUtilities;

// ------ S T A T I C   V A R I A B L E S -------------------------------------
static bool m_bVerbose = false;


// ------ F U N C T I O N S ---------------------------------------------------
void 
XBUtilities::setVerbose(bool _bVerbose)
{
  bool prevVerbose = m_bVerbose;

  if ((prevVerbose == true) && (_bVerbose == false)) {
    verbose("Disabling Verbosity");
  }

  m_bVerbose = _bVerbose;

  if ((prevVerbose == false) && (_bVerbose == true)) {
    verbose("Enabling Verbosity");
  }
}

void 
XBUtilities::message(const std::string& _msg, bool _endl)
{
  std::cout << _msg;
  if (_endl == true) {
    std::cout << std::endl;
  }
}

void 
XBUtilities::error(const std::string& _msg, bool _endl)
{
  std::cerr << "Error: " << _msg;
  if (_endl == true) {
    std::cout << std::endl;
  }
}
void 
XBUtilities::warning(const std::string& _msg, bool _endl)
{
  std::cout << "Warning: " << _msg;
  if (_endl == true) {
    std::cout << std::endl;
  }
}



void 
XBUtilities::verbose(const std::string& _msg, bool _endl)
{
  // Make sure we have something to report
  if (m_bVerbose == false) {
    return;
  }

  std::cout << "Verbose: " << _msg;
  if (_endl == true) {
    std::cout << std::endl;
  }
}


