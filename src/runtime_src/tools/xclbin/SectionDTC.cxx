/**
 * Copyright (C) 2018 - 2019 Xilinx, Inc
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

#include "SectionDTC.h"
#include "DTC.h"

#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;

// Static Variables / Classes
SectionDTC::_init SectionDTC::_initializer;

SectionDTC::SectionDTC() {
  // Empty
}

SectionDTC::~SectionDTC() {
  // Empty
}

bool 
SectionDTC::doesSupportAddFormatType(FormatType _eFormatType) const
{
  if (( _eFormatType == FT_JSON ) ||
      ( _eFormatType == FT_RAW )) {
    return true;
  }
  return false;
}

bool 
SectionDTC::doesSupportDumpFormatType(FormatType _eFormatType) const
{
    if ((_eFormatType == FT_JSON) ||
        (_eFormatType == FT_HTML) ||
        (_eFormatType == FT_RAW))
    {
      return true;
    }

    return false;
}

void
SectionDTC::marshalToJSON(char* _pDataSection,
                          unsigned int _sectionSize,
                          boost::property_tree::ptree& _ptree) const 
{
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: DTC Image");

  boost::property_tree::ptree dtcTree;

  // Parse the DTC buffer
  if (_pDataSection != nullptr) {
      class DTC dtc(_pDataSection, _sectionSize);
      dtc.marshalToJSON(dtcTree);
  }

  // Create the JSON file
  _ptree.add_child("ip_shell_definitions", dtcTree);
  XUtil::TRACE_PrintTree("Ptree", _ptree);
}


void
SectionDTC::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                            std::ostringstream& _buf) const 
{
  const boost::property_tree::ptree& ptDTC = _ptSection.get_child("ip_shell_definitions");

  // Parse the DTC JSON file
  class DTC dtc(ptDTC);

  dtc.marshalToDTC(_buf);

  // Dump debug data
  std::string sBuf = _buf.str();
  XUtil::TRACE_BUF("DTC Buffer", sBuf.c_str(), sBuf.size());
}



void 
SectionDTC::appendToSectionMetadata(const boost::property_tree::ptree& _ptAppendData,
                                         boost::property_tree::ptree& _ptToAppendTo)
{
  XUtil::TRACE_PrintTree("To Append To", _ptToAppendTo);
  XUtil::TRACE_PrintTree("Append data", _ptAppendData);

  boost::property_tree::ptree &ipShellTree = _ptToAppendTo.get_child("ip_shell_definitions");
  for (auto childTree : _ptAppendData) {
    ipShellTree.add_child(childTree.first, childTree.second);
  }

  XUtil::TRACE_PrintTree("To Append To Done", _ptToAppendTo);
}

