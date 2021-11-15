/**
 * Copyright (C) 2018 - 2021 Xilinx, Inc
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

#include "SectionEmbeddedMetadata.h"

#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;

#include <boost/property_tree/xml_parser.hpp>

// Static Variables / Classes
SectionEmbeddedMetadata::_init SectionEmbeddedMetadata::_initializer;

SectionEmbeddedMetadata::SectionEmbeddedMetadata() {
  // Empty
}

SectionEmbeddedMetadata::~SectionEmbeddedMetadata() {
  // Empty
}

void
SectionEmbeddedMetadata::marshalToJSON(char* _pDataSection,
                                       unsigned int _sectionSize,
                                       boost::property_tree::ptree& _ptree) const {
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: EMBEDDED_METADATA");
  XUtil::TRACE_BUF("Section Buffer", reinterpret_cast<const char*>(_pDataSection), _sectionSize);

  // Determine if there is something to do, if not then leave
  if (_sectionSize == 0)
    return;

  std::stringstream ss;
  ss.write((char*) _pDataSection, _sectionSize);
  boost::property_tree::read_xml(ss, _ptree, boost::property_tree::xml_parser::trim_whitespace);
}

void
SectionEmbeddedMetadata::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                         std::ostringstream& _buf) const {

  XUtil::TRACE("Writing XML\n");
  boost::property_tree::xml_writer_settings<std::string> settings(' ', 2);
  boost::property_tree::write_xml(_buf, _ptSection, settings);
}

