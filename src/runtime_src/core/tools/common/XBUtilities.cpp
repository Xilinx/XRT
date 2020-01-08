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
#include "common/core_system.h"
#include <boost/property_tree/json_parser.hpp>

// System - Include Files
#include <iostream>
#include <map>

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBUtilities;

// ------ S T A T I C   V A R I A B L E S -------------------------------------
static bool m_bVerbose = false;
static bool m_bTrace = false;


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
XBUtilities::setTrace(bool _bTrace)
{
  if (_bTrace) {
    trace("Enabling Tracing");
  } else {
    trace("Disabling Tracing");
  }

  m_bTrace = _bTrace;
}


void 
XBUtilities::message_(MessageType _eMT, const std::string& _msg, bool _endl)
{
  static std::map<MessageType, std::string> msgPrefix = {
    { MT_MESSAGE, "" },
    { MT_INFO, "Info: " },
    { MT_WARNING, "Warning: " },
    { MT_ERROR, "Error: " },
    { MT_VERBOSE, "Verbose: " },
    { MT_FATAL, "Fatal: " },
    { MT_TRACE, "Trace: " },
    { MT_UNKNOWN, "<type unknown>: " },
  };

  // A simple DRC check
  if (_eMT > MT_UNKNOWN) {
    _eMT = MT_UNKNOWN;
  }

  // Verbosity is not enabled
  if ((m_bVerbose == false) && (_eMT == MT_VERBOSE)) {
      return;
  }

  // Tracing is not enabled
  if ((m_bTrace == false) && (_eMT == MT_TRACE)) {
      return;
  }

  std::cout << msgPrefix[_eMT] << _msg;

  if (_endl == true) {
    std::cout << std::endl;
  }
}

void 
XBUtilities::message(const std::string& _msg, bool _endl) 
{ 
  message_(MT_MESSAGE, _msg, _endl); 
}

void 
XBUtilities::info(const std::string& _msg, bool _endl)    
{ 
  message_(MT_INFO, _msg, _endl); 
}

void 
XBUtilities::warning(const std::string& _msg, bool _endl) 
{ 
  message_(MT_WARNING, _msg, _endl); 
}

void 
XBUtilities::error(const std::string& _msg, bool _endl)
{ 
  message_(MT_ERROR, _msg, _endl); 
}

void 
XBUtilities::verbose(const std::string& _msg, bool _endl) 
{ 
  message_(MT_VERBOSE, _msg, _endl); 
}

void 
XBUtilities::fatal(const std::string& _msg, bool _endl)   
{ 
  message_(MT_FATAL, _msg, _endl); 
}

void 
XBUtilities::trace(const std::string& _msg, bool _endl)   
{ 
  message_(MT_TRACE, _msg, _endl); 
}



void 
XBUtilities::trace_print_tree(const std::string & _name, 
                              const boost::property_tree::ptree & _pt)
{
  if (m_bTrace == false) {
    return;
  }

  XBUtilities::trace(_name + " (JSON Tree)");

  std::ostringstream buf;
  boost::property_tree::write_json(buf, _pt, true /*Pretty print*/);
  XBUtilities::message(buf.str());
}


