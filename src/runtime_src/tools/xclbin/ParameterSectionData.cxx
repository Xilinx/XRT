/**
 * Copyright (C) 2018 Xilinx, Inc
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
#include "ParameterSectionData.h"

#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;

ParameterSectionData::ParameterSectionData(const std::string &_formattedString)
  : m_formatType(Section::FT_UNKNOWN)
  , m_formatTypeStr("")
  , m_file("")
  , m_section("")
  , m_eKind(BITSTREAM)
{
  transformFormattedString(_formattedString);
}

ParameterSectionData::~ParameterSectionData()
{
  // Empty
}


void 
ParameterSectionData::transformFormattedString(const std::string _formattedString)
// String being parsed:  <section>:<formatType>:<filename>
// Example String:       BUILD_METADATA:JSON:MY_FILE.JSON
{
  const std::string& delimiters = ":";      // Our delimiter

  // Working variables
  std::string::size_type pos = 0;
  std::string::size_type lastPos = 0;
  std::vector<std::string> tokens;

  // Parse the string until the entire string has been parsed or 3 tokens have been found
  while((lastPos < _formattedString.length() + 1) && 
        (tokens.size() < 3))
  {
    pos = _formattedString.find_first_of(delimiters, lastPos);

    if ( (pos == std::string::npos) ||
         (tokens.size() == 2) ){
       pos = _formattedString.length();
    }

    std::string token = _formattedString.substr(lastPos, pos-lastPos);
    tokens.push_back(token);
    lastPos = pos + 1;
  }

  if (tokens.size() != 3) {
    std::string errMsg = XUtil::format("Error: Expected format <section>:<format>:<file> when using adding a section.  Received: %s.", _formattedString.c_str());
    throw std::runtime_error(errMsg);
  }

  // -- Format Type --
  m_formatType = Section::getFormatType(tokens[1]);
  m_formatTypeStr = tokens[1];

  // -- Section --
  if ( !tokens[0].empty() ) {
    enum axlf_section_kind eKind;
    if (Section::translateSectionKindStrToKind(tokens[0], eKind) == false) {
      std::string errMsg = XUtil::format("Error: Section '%s' isn't a valid section name.", tokens[0].c_str());
      throw std::runtime_error(errMsg);
    }
  }

  if ( tokens[0].empty() && (m_formatType != Section::FT_JSON)) {
    std::string errMsg = "Error: Empty sections names are only permitted with JSON format files.";
    throw std::runtime_error(errMsg);
  }
  m_section = tokens[0];

  // -- File --
  m_file = tokens[2];
}

const std::string &
ParameterSectionData::getFile()
{
  return m_file;
}

enum Section::FormatType 
ParameterSectionData::getFormatType()
{
  return m_formatType;
}

const std::string &
ParameterSectionData::getSectionName()
{
  return m_section;
}

enum axlf_section_kind &
ParameterSectionData::getSectionKind()
{
  return m_eKind;
}


const std::string &
ParameterSectionData::getFormatTypeAsStr()
{
  return m_formatTypeStr;
}
