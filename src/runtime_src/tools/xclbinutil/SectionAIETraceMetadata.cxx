/**
 * Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
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

#include "SectionAIETraceMetadata.h"

#include "XclBinUtilities.h"
#include <boost/functional/factory.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace XUtil = XclBinUtilities;

// Static Variables / Classes
// ----------------------------------------------------------------------------
SectionAIETraceMetadata::init SectionAIETraceMetadata::initializer;

SectionAIETraceMetadata::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(AIE_TRACE_METADATA, "AIE_TRACE_METADATA", boost::factory<SectionAIETraceMetadata*>());

  sectionInfo->supportedAddFormats.push_back(FormatType::json);
  sectionInfo->supportedAddFormats.push_back(FormatType::raw);

  sectionInfo->supportedDumpFormats.push_back(FormatType::json);
  sectionInfo->supportedDumpFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

// ----------------------------------------------------------------------------

void
SectionAIETraceMetadata::marshalToJSON(char* _pDataSection,
                                  unsigned int _sectionSize,
                                  boost::property_tree::ptree& _ptree) const
{
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: AIE_TRACE_METADATA");

  std::vector<unsigned char> memBuffer(_sectionSize + 1);  // Extra byte for "null terminate" char
  memcpy((char*)memBuffer.data(), _pDataSection, _sectionSize);
  memBuffer[_sectionSize] = '\0';

  std::stringstream ss((char*)memBuffer.data());

  // TODO: Catch the exception (if any) from this call and produce a nice message
  XUtil::TRACE_BUF("AIE_TRACE_METADATA", (const char*)memBuffer.data(), _sectionSize + 1);
  try {
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(ss, pt);
    boost::property_tree::ptree& buildMetaData = pt.get_child("aie_metadata");
    _ptree.add_child("aie_trace_metadata", buildMetaData);
  } catch (const std::exception& e) {
    std::string msg("ERROR: Bad JSON format detected while marshaling AIE trace metadata (");
    msg += e.what();
    msg += ").";
    throw std::runtime_error(msg);
  }
}

void
SectionAIETraceMetadata::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                    std::ostringstream& _buf) const
{
  XUtil::TRACE("AIE_TRACE_METADATA");
  boost::property_tree::ptree ptWritable = _ptSection;
  boost::property_tree::write_json(_buf, ptWritable, false);
}


