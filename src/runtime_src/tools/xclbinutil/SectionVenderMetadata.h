/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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

#ifndef __SectionVenderMetadata_h_
#define __SectionVenderMetadata_h_

// ----------------------- I N C L U D E S -----------------------------------
#include "Section.h"

// -------- C L A S S :   S e c t i o n V e n d e r M e t a d a t a ----------
class SectionVenderMetadata : public Section {
 public:
  bool subSectionExists(const std::string& _sSubSectionName) const override;
  void readXclBinBinary(std::istream& _istream, const struct axlf_section_header& _sectionHeader) override;

 protected:
  void readSubPayload(const char* _pOrigDataSection, unsigned int _origSectionSize,  std::istream& _istream, const std::string& _sSubSection, Section::FormatType _eFormatType, std::ostringstream& _buffer) const override;
  void writeSubPayload(const std::string& _sSubSectionName, FormatType _eFormatType, std::fstream&  _oStream) const override;

 protected:
  void copyBufferUpdateMetadata(const char* _pOrigDataSection, unsigned int _origSectionSize,  std::istream& _istream, std::ostringstream& _buffer) const;
  void createDefaultImage(std::istream& _istream, std::ostringstream& _buffer) const;
  void writeObjImage(std::ostream& _oStream) const;
  void writeMetadata(std::ostream& _oStream) const;

 private:
  // Static initializer helper class
  static class init {
   public:
    init();
  } initializer;
};

#endif
