/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc
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

#ifndef __SectionAIEPartitions_h_
#define __SectionAIEPartitions_h_

// ----------------------- I N C L U D E S -----------------------------------
#include "Section.h"

// -------- C L A S S :   S e c t i o n A I E P a r t i t i o n --------------
class SectionAIEPartition : public Section {
 public:
  bool subSectionExists(const std::string& sSubSectionName) const override;
  void readXclBinBinary(std::istream& _istream, const struct axlf_section_header& _sectionHeader) override;

 protected:
  void readSubPayload(const char* pOrigDataSection, unsigned int origSectionSize,  std::istream& iStream, const std::string& sSubSection, Section::FormatType eFormatType, std::ostringstream& buffer) const override;
  void writeSubPayload(const std::string&  sSubSectionName, FormatType eFormatType, std::fstream&  oStream) const override;

 private:
  // Static initializer helper class
  static class init {
   public:
    init();
  } initializer;
};

#endif
