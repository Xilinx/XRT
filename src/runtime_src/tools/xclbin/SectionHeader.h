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

#ifndef __SectionHeader_h_
#define __SectionHeader_h_

#include <string>
#include <fstream>
#include "xclbin.h"

class SectionHeader {
 public:
  SectionHeader();
  virtual ~SectionHeader();

 public:
  void readXclBinBinarySection(std::fstream& _istream, unsigned int _section);

 private:
  enum axlf_section_kind m_eType;  /* Type of section */
  std::string m_name;              /* Name of section (not really used in the xclbin anymore */

 private:
  // Purposefully private and undefined ctors...
  SectionHeader(const SectionHeader& obj);
  SectionHeader& operator=(const SectionHeader& obj);
};


#endif
