/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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
#include "XBUtilitiesCore.h"

#include "core/common/error.h"

// 3rd Party Library - Include Files
#include <boost/algorithm/string/split.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/tokenizer.hpp>

// System - Include Files
#include <regex>


// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBUtilities;

// ------ S T A T I C   V A R I A B L E S -------------------------------------
static bool m_bVerbose = false;
static bool m_bTrace = false;
static bool m_disableEscapeCodes = false;
static bool m_bShowHidden = false;
static bool m_bForce = false;


// ------ F U N C T I O N S ---------------------------------------------------
void
XBUtilities::setVerbose(bool _bVerbose)
{
  bool prevVerbose = m_bVerbose;

  if ((prevVerbose == true) && (_bVerbose == false))
    verbose("Disabling Verbosity");

  m_bVerbose = _bVerbose;

  if ((prevVerbose == false) && (_bVerbose == true))
    verbose("Enabling Verbosity");
}

bool
XBUtilities::getVerbose()
{
  return m_bVerbose;
}

void
XBUtilities::setTrace(bool _bTrace)
{
  if (_bTrace)
    trace("Enabling Tracing");
  else
    trace("Disabling Tracing");

  m_bTrace = _bTrace;
}


void
XBUtilities::setShowHidden(bool _bShowHidden)
{
  if (_bShowHidden)
    trace("Hidden commands and options will be shown.");
  else
    trace("Hidden commands and options will be hidden");

  m_bShowHidden = _bShowHidden;
}

bool
XBUtilities::getShowHidden()
{
  return m_bShowHidden;
}

void
XBUtilities::setForce(bool _bForce)
{
  m_bForce = _bForce;

  if (m_bForce)
    trace("Enabling force option");
  else
    trace("Disabling force option");
}

bool
XBUtilities::getForce()
{
  return m_bForce;
}

void
XBUtilities::disable_escape_codes(bool _disable)
{
  m_disableEscapeCodes = _disable;
}

bool
XBUtilities::is_escape_codes_disabled() {
  return m_disableEscapeCodes;
}


void
XBUtilities::message_(MessageType _eMT, const std::string& _msg, bool _endl, std::ostream & _ostream)
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

  _ostream << msgPrefix[_eMT] << _msg;

  if (_endl == true) {
    _ostream << std::endl;
  }
}

void
XBUtilities::message(const std::string& _msg, bool _endl, std::ostream & _ostream)
{
  message_(MT_MESSAGE, _msg, _endl, _ostream);
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
XBUtilities::verbose(const boost::format& _msg, bool _endl)
{
  verbose(_msg.str(), _endl);
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


std::string
XBUtilities::wrap_paragraphs( const std::string & unformattedString,
                              unsigned int indentWidth,
                              unsigned int columnWidth,
                              bool indentFirstLine) {
  std::vector<std::string> lines;

  // Process the string
  std::string workingString;

  for (const auto &entry : unformattedString) {
    // Do we have a new line added by the user
    if (entry == '\n') {
      lines.push_back(workingString);
      workingString.clear();
      continue;
    }

    workingString += entry;

    // Check to see if this string is too long
    if (workingString.size() >= columnWidth) {
      // Find the beginning of the previous 'word'
      auto index = workingString.find_last_of(" ");

      // None found, keep on adding characters till we find a space
      if (index == std::string::npos)
        continue;

      // Add the line and populate the next line
      lines.push_back(workingString.substr(0, index));
      workingString = workingString.substr(index + 1);
    }
  }

  if (!workingString.empty())
    lines.push_back(workingString);

  // Early exit, nothing here
  if (lines.size() == 0)
    return std::string();

  // -- Build the formatted string
  std::string formattedString;

  // Iterate over the lines building the formatted string
  const std::string indention(indentWidth, ' ');
  auto iter = lines.begin();
  while (iter != lines.end()) {
    // Add an indention
    if (iter != lines.begin() || indentFirstLine)
      formattedString += indention;

    // Add formatted line
    formattedString += *iter;

    // Don't add a '\n' on the last line
    if (++iter != lines.end())
      formattedString += "\n";
  }

  return formattedString;
}

bool
XBUtilities::can_proceed(bool force)
{
    bool proceed = false;
    std::string input;

    std::cout << "Are you sure you wish to proceed? [Y/n]: ";

    if (force)
        std::cout << "Y (Force override)" << std::endl;
    else
        std::getline(std::cin, input);

    // Ugh, the std::transform() produces windows compiler warnings due to
    // conversions from 'int' to 'char' in the algorithm header file
    boost::algorithm::to_lower(input);
    //std::transform( input.begin(), input.end(), input.begin(), [](unsigned char c){ return std::tolower(c); });
    //std::transform( input.begin(), input.end(), input.begin(), ::tolower);

    // proceeds for "y", "Y" and no input
    proceed = ((input.compare("y") == 0) || input.empty());
    if (!proceed)
        std::cout << "Action canceled." << std::endl;
    return proceed;
}

void
XBUtilities::sudo_or_throw_err()
{
#ifndef _WIN32
    if ((getuid() == 0) || (geteuid() == 0))
        return;
    std::cout << "ERROR: root privileges required." << std::endl;
    throw std::errc::operation_canceled;
#endif
}

void
XBUtilities::throw_cancel(const std::string& msg)
{
  throw_cancel(boost::format("%s") % msg);
}

void
XBUtilities::throw_cancel(const boost::format& format)
{
  throw xrt_core::error(std::errc::operation_canceled, boost::str(format));
}
