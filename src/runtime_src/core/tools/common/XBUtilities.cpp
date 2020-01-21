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
#include <boost/tokenizer.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

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

std::string 
XBUtilities::create_usage_string(const std::string &_executableName,
                                 const std::string &_subCommand,
                                 const po::options_description &_od)
{
  const static int SHORT_OPTION_STRING_SIZE = 2;
    std::stringstream buffer;

    // Define the basic first
    buffer << _executableName << " " << _subCommand;

    const std::vector<boost::shared_ptr<po::option_description>> options = _od.options();

    // Gather up the short simple flags
    {
      bool firstShortFlagFound = false;
      for (auto & option : options) {
        // Get the option name
        std::string optionDisplayName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);

        // See if we have a long flag
        if (optionDisplayName.size() != SHORT_OPTION_STRING_SIZE)
          continue;

        // We are not interested in any arguments
        if (option->semantic()->max_tokens() > 0)
          continue;

        // This option shouldn't be required
        if (option->semantic()->is_required() == true) 
          continue;

        if (!firstShortFlagFound) {
          buffer << " [-";
          firstShortFlagFound = true;
        }

        buffer << optionDisplayName[1];
      }

      if (firstShortFlagFound == true) 
        buffer << "]";
    }

     
    // Gather up the long simple flags (flags with no short versions)
    {
      for (auto & option : options) {
        // Get the option name
        std::string optionDisplayName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);

        // See if we have a short flag
        if (optionDisplayName.size() == SHORT_OPTION_STRING_SIZE)
          continue;

        // We are not interested in any arguments
        if (option->semantic()->max_tokens() > 0)
          continue;

        // This option shouldn't be required
        if (option->semantic()->is_required() == true) 
          continue;

        std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_long);
        buffer << " [" << completeOptionName << "]";
      }
    }

    // Gather up the options with arguments
    for (auto & option : options) {
      // Skip if there are no arguments
      if (option->semantic()->max_tokens() == 0)
        continue;

      // This option shouldn't be required
      if (option->semantic()->is_required() == true) 
        continue;

      
      std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);
      buffer << " [" << completeOptionName << " arg]";
    }

    // Gather up the required options with arguments
    for (auto & option : options) {
      // Skip if there are no arguments
      if (option->semantic()->max_tokens() == 0)
        continue;

      // This option is required
      if (option->semantic()->is_required() == false) 
        continue;

      std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);
      buffer << " " << completeOptionName << " arg";
    }

  return buffer.str();
}

void 
XBUtilities::wrap_paragraph( const std::string & _unformattedString, 
                             unsigned int _indentWidth, 
                             unsigned int _columnWidth, 
                             bool _indentFirstLine,
                             std::string &_formattedString)
{
  // Set return variables to a now state
  _formattedString.clear();

  if (_indentWidth >= _columnWidth) {
    std::string errMsg = boost::str(boost::format("Internal Error: %s paragraph indent (%d) is greater than or equal to the column width (%d) ") % __FUNCTION__ % _indentWidth % _columnWidth);
    throw std::runtime_error(errMsg);
  }

  const unsigned int paragraphWidth = _columnWidth - _indentWidth;

  std::string::const_iterator lineBeginIter = _unformattedString.begin();
  const std::string::const_iterator paragraphEndIter = _unformattedString.end();

  unsigned int linesProcessed = 0;

  while (lineBeginIter < paragraphEndIter)  
  {
    // Remove leading spaces
    if ((linesProcessed > 0) && 
        (*lineBeginIter == ' ')) {
      lineBeginIter++;
      continue;
    }

    // Determine the end-of-the line to be examined
    std::string::const_iterator lineEndIter = lineBeginIter;
    auto remainingChars = std::distance(lineBeginIter, paragraphEndIter);
    if (remainingChars < paragraphWidth)
      lineEndIter += remainingChars;
    else
      lineEndIter += paragraphWidth;

    // Not last line
    if (lineEndIter != paragraphEndIter) {
      // Find a break between the words
      std::string::const_iterator lastSpaceIter = find(std::reverse_iterator<std::string::const_iterator>(lineEndIter),
                                                       std::reverse_iterator<std::string::const_iterator>(lineBeginIter), ' ').base();

      // See if we have gone to the beginning, if not then break the line
      if (lastSpaceIter != lineBeginIter) {
        lineEndIter = lastSpaceIter;
      }
    }
    
    // Add new line
    if (linesProcessed > 0)
      _formattedString += "\n";

    // Indent the line
    if ((linesProcessed > 0) || 
        (_indentFirstLine == true)) {
      for (size_t index = _indentWidth; index > 0; index--)
      _formattedString += " ";
    }

    // Write out the line
    _formattedString.append(lineBeginIter, lineEndIter);

    lineBeginIter = lineEndIter;              
    linesProcessed++;
  }
}   

void 
XBUtilities::wrap_paragraphs( const std::string & _unformattedString, 
                              unsigned int _indentWidth, 
                              unsigned int _columnWidth, 
                              bool _indentFirstLine,
                              std::string &_formattedString) 
{
  // Set return variables to a now state
  _formattedString.clear();

  if (_indentWidth >= _columnWidth) {
    std::string errMsg = boost::str(boost::format("Internal Error: %s paragraph indent (%d) is greater than or equal to the column width (%d) ") % __FUNCTION__ % _indentWidth % _columnWidth);
    throw std::runtime_error(errMsg);
  }

  typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
  boost::char_separator<char> sep{"\n", "", boost::keep_empty_tokens};
  tokenizer paragraphs{_unformattedString, sep};

  tokenizer::const_iterator iter = paragraphs.begin();
  while (iter != paragraphs.end()) {
    std::string formattedParagraph;
    wrap_paragraph(*iter, _indentWidth, _columnWidth, _indentFirstLine, formattedParagraph);
    _formattedString += formattedParagraph;
    _indentFirstLine = true; // We wish to indent all lines following the first

    ++iter;

    // Determine if a '\n' should be added
    if (iter != paragraphs.end()) 
      _formattedString += "\n";
  }
}

void 
XBUtilities::subcommand_help( const std::string &_executableName,
                              const std::string &_subCommand,
                              const std::string &_description, 
                              const po::options_description &_od, 
                              const std::string &_examples)
{
  std::cout << boost::format("SubCommand:  %s\n\n") % _subCommand;
 
  std::string formattedDescription;
  wrap_paragraphs(_description, 13, 80, false, formattedDescription);
  std::cout << boost::format("Description: %s\n\n") % formattedDescription;

  std::string usage = create_usage_string(_executableName, _subCommand, _od);
  std::cout << boost::format("Usage: %s \n\n") % usage;

  std::cout << _od << std::endl;
  if (!_examples.empty() ) {
    std::cout << "Example Syntax: " << _examples << std::endl;
  }
}

