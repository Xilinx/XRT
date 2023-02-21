/**
 * Copyright (C) 2023 Xilinx, Inc
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

#include "SectionIPMetadata.h"

#include "XclBinUtilities.h"
#include <boost/format.hpp>
#include <boost/functional/factory.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace XUtil = XclBinUtilities;

// Static Variables / Classes
SectionIPMetadata::init SectionIPMetadata::initializer;

SectionIPMetadata::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(IP_METADATA, "IP_METADATA", boost::factory<SectionIPMetadata*>());

  sectionInfo->supportedAddFormats.push_back(FormatType::raw);

  sectionInfo->supportedDumpFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

void
SectionIPMetadata::marshalToJSON(char* pDataSection,
                                 unsigned int sectionSize,
                                 boost::property_tree::ptree& ptree) const
{
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: IP_METADATA");
  XUtil::TRACE_BUF("IP_METADATA Section Buffer", reinterpret_cast<const char*>(pDataSection), sectionSize);

  // Determine if there is something to do, if not then leave
  if (sectionSize == 0)
    return;

  std::stringstream ss;
  ss.write(pDataSection, sectionSize);

  try {
    boost::property_tree::read_json(ss, ptree);
  } catch (const std::exception& e) {
    const std::string errMsg = (boost::format("ERROR: Bad JSON format detected while marshaling IP_METADATA (%s).") % e.what()).str();
    throw std::runtime_error(errMsg);
  }
}

void
SectionIPMetadata::marshalFromJSON(const boost::property_tree::ptree& ptSection,
                                   std::ostringstream& buf) const
{
  XUtil::TRACE("IP_METADATA");
  // write the property tree to buf
  boost::property_tree::write_json(buf, ptSection, false);
}

