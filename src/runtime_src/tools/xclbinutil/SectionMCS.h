/**
 * Copyright (C) 2018-2022 Xilinx, Inc
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

#ifndef __SectionMCS_h_
#define __SectionMCS_h_

// ----------------------- I N C L U D E S -----------------------------------
#include "Section.h"

// --------------- C L A S S :   S e c t i o n M C S -------------------------
class SectionMCS : public Section {
 public:
  static enum MCS_TYPE getSubSectionEnum(const std::string &_sSubSectionName);
  static const std::string & getSubSectionName(enum MCS_TYPE eSubSection);

 public:
  bool subSectionExists(const std::string &_sSubSectionName) const override;

 protected:
  void getSubPayload(char* _pDataSection, unsigned int _sectionSize, std::ostringstream &_buf, const std::string &_sSubSectionName, enum Section::FormatType _eFormatType) const override;
  void marshalToJSON(char* _buffer, unsigned int _pDataSegment, boost::property_tree::ptree& _ptree) const override;
  void readSubPayload(const char* _pOrigDataSection, unsigned int _origSectionSize,  std::istream& _istream, const std::string & _sSubSection, enum Section::FormatType _eFormatType, std::ostringstream &_buffer) const override;
  void writeSubPayload(const std::string & _sSubSectionName, FormatType _eFormatType, std::fstream&  _oStream) const override;

  typedef std::pair< enum MCS_TYPE, std::ostringstream *> mcsBufferPair;
  void extractBuffers(const char* _pDataSection, unsigned int _sectionSize, std::vector<mcsBufferPair> &_mcsBuffers) const;
  void buildBuffer(const std::vector<mcsBufferPair> &_mcsBuffers, std::ostringstream &_buffer) const;

 private:
  // Static initializer helper class
  static class init {
   public:
    init();
  } initializer; 
};

#endif
