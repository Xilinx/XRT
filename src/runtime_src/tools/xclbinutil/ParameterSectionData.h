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

#ifndef __ParameterSectionData_h_
#define __ParameterSectionData_h_

// ----------------------- I N C L U D E S -----------------------------------
#include "Section.h"
#include "xrt/detail/xclbin.h"
#include <string>

// ---------- C L A S S :   P a r a m e t e r S e c t i o n D a t a ---------

class ParameterSectionData {
 private:
  ParameterSectionData();

 public:
  ParameterSectionData(const std::string& _formattedString);
  ~ParameterSectionData();

 public:
  const std::string& getFile();
  Section::FormatType getFormatType();
  const std::string& getFormatTypeAsStr();
  const std::string& getSectionName();
  const std::string& getSubSectionName();
  const std::string& getSectionIndexName();
  axlf_section_kind& getSectionKind();
  const std::string& getOriginalFormattedString();

 protected:
  void transformFormattedString(const std::string& _formattedString);

 protected:
  Section::FormatType m_formatType;
  std::string m_formatTypeStr;
  std::string m_file;
  std::string m_section;
  std::string m_subSection;
  std::string m_sectionIndex;
  axlf_section_kind m_eKind;
  std::string m_originalString;
};

#endif
