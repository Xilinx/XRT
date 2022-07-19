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

#include "SectionClearBitstream.h"

#include <boost/functional/factory.hpp>

// Static Variables / Classes
SectionClearBitstream::init SectionClearBitstream::initializer;

SectionClearBitstream::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(CLEARING_BITSTREAM, "CLEARING_BITSTREAM", boost::factory<SectionClearBitstream*>());

  sectionInfo->supportedAddFormats.push_back(FormatType::raw);

  sectionInfo->supportedDumpFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}


