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

// #includes here - please keep these to a bare minimum!
#include "Section.h"
#include <boost/functional/factory.hpp>



class SectionVenderMetadata : public Section {
 public:
  SectionVenderMetadata();
  virtual ~SectionVenderMetadata();

 public:
  bool doesSupportAddFormatType(FormatType _eFormatType) const override;
  bool supportsSubSection(const std::string& _sSubSectionName) const override;
  bool subSectionExists(const std::string& _sSubSectionName) const override;
  void readXclBinBinary(std::istream& _istream, const struct axlf_section_header& _sectionHeader)  override;

 protected:
  void readSubPayload(const char* _pOrigDataSection, unsigned int _origSectionSize,  std::istream& _istream, const std::string& _sSubSection, enum Section::FormatType _eFormatType, std::ostringstream& _buffer) const override;
  void writeSubPayload(const std::string& _sSubSectionName, FormatType _eFormatType, std::fstream&  _oStream) const override;

 protected:
  void copyBufferUpdateMetadata(const char* _pOrigDataSection, unsigned int _origSectionSize,  std::istream& _istream, std::ostringstream& _buffer) const;
  void createDefaultImage(std::istream& _istream, std::ostringstream& _buffer) const;
  void writeObjImage(std::ostream& _oStream) const;
  void writeMetadata(std::ostream& _oStream) const;

 private:
  SectionVenderMetadata(const SectionVenderMetadata& obj) = delete;
  SectionVenderMetadata& operator=(const SectionVenderMetadata& obj) = delete;

 private:
  // Static initializer helper class
  static class _init {
   public:
    _init() { registerSectionCtor(VENDER_METADATA, "VENDER_METADATA", "", true, true, boost::factory<SectionVenderMetadata*>()); }
  } _initializer;
};

#endif
