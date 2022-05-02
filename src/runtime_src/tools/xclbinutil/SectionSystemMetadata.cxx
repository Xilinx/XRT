/**
 * Copyright (C) 2018, 2022 Xilinx, Inc
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

#include "SectionSystemMetadata.h"

#include "XclBinUtilities.h"
#include <boost/format.hpp>
#include <boost/functional/factory.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace XUtil = XclBinUtilities;

// Static Variables / Classes
SectionSystemMetadata::init SectionSystemMetadata::initializer;

SectionSystemMetadata::init::init() 
{ 
    auto sectionInfo = std::make_unique<SectionInfo>(SYSTEM_METADATA, "SYSTEM_METADATA", boost::factory<SectionSystemMetadata*>()); 

    addSectionType(std::move(sectionInfo));
}

void
SectionSystemMetadata::marshalToJSON(char* _pDataSection,
                                     unsigned int _sectionSize,
                                     boost::property_tree::ptree& _ptree) const {
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: SYSTEM_METADATA");
  XUtil::TRACE_BUF("SYSTEM_METADATA Section Buffer", reinterpret_cast<const char*>(_pDataSection), _sectionSize);

  // Determine if there is something to do, if not then leave
  if (_sectionSize == 0)
    return;

  std::stringstream ss;
  ss.write(_pDataSection, _sectionSize);

  try {
    boost::property_tree::read_json(ss, _ptree);
  } catch (const std::exception & e) {
    const std::string errMsg = (boost::format("ERROR: Bad JSON format detected while marshaling SYSTEM_METADATA (%s).") % e.what()).str();
    throw std::runtime_error(errMsg);
  }
}

void
SectionSystemMetadata::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                         std::ostringstream& _buf) const {

   XUtil::TRACE("SYSTEM_METADATA");
   boost::property_tree::write_json(_buf, _ptSection, false );
}

