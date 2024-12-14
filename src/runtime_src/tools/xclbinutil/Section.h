/**
 * Copyright (C) 2018 - 2022 Xilinx, Inc
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

#ifndef __Section_h_
#define __Section_h_

// ----------------------- I N C L U D E S -----------------------------------

// #includes here - please keep these to a bare minimum!
#include "xrt/detail/xclbin.h"

#include <boost/property_tree/ptree.hpp>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
// ------------------- C L A S S :   S e c t i o n ---------------------------

class Section {
  typedef std::function<Section*()> Section_factory;

 public:
  enum class FormatType {
    undefined,
    unknown,
    raw,
    json,
    html,
    txt
  };

 protected:
  class SectionInfo {
    SectionInfo() = delete;

   public:
    SectionInfo(axlf_section_kind eKind, std::string sectionName, Section_factory sectionCtor);

   public:
    axlf_section_kind eKind;         // The section enumeration value
    std::string name;                     // Name of the section
    Section_factory sectionCtor;          // Section constructor
    std::string nodeName;                 // JSON node name
    bool supportsSubSections;             // Support subsections
    bool supportsIndexing;                // Supports indexing
    std::vector<FormatType> supportedAddFormats;  // Supported add format
    std::vector<FormatType> supportedDumpFormats; // Supported dump formats
    std::vector<std::string> subSections; // Supported subsections
  };

 public:
  virtual ~Section();

 private:
  static std::vector<std::unique_ptr<SectionInfo>>& getSectionTypes();

 protected:
  static void addSectionType(std::unique_ptr<SectionInfo> sectionInfo);

 public:
  static std::vector<std::string> getSupportedKinds();
  static Section* createSectionObjectOfKind(axlf_section_kind eKind, const std::string sIndexName = "");
  static void translateSectionKindStrToKind(const std::string& sKind, axlf_section_kind& eKind);
  static axlf_section_kind getKindOfJSON(const std::string& nodeName);
  static std::string getJSONOfKind(axlf_section_kind eKind);
  static FormatType getFormatType(const std::string& sFormatType);
  static bool supportsSubSections(axlf_section_kind& eKind);
  static bool supportsSectionIndex(axlf_section_kind& eKind);
  static bool doesSupportAddFormatType(axlf_section_kind eKind, FormatType eFormatType);
  static bool doesSupportDumpFormatType(axlf_section_kind eKind, FormatType eFormatType);
  static bool supportsSubSectionName(axlf_section_kind eKind, const std::string& sSubSectionName);

 public:
  virtual bool subSectionExists(const std::string& sSubSectionName) const;

 public:
  axlf_section_kind getSectionKind() const;
  const std::string& getSectionKindAsString() const;
  std::string getName() const;
  unsigned int getSize() const;
  const std::string& getSectionIndexName() const;

 public:
  // Xclbin Binary helper methods - child classes can override them if they choose
  virtual void readXclBinBinary(std::istream& _istream, const struct axlf_section_header& _sectionHeader);
  virtual void readXclBinBinary(std::istream& _istream, const boost::property_tree::ptree& _ptSection);
  void readJSONSectionImage(const boost::property_tree::ptree& _ptSection);
  void readPayload(std::istream& _istream, FormatType _eFormatType);
  void printHeader(std::ostream& _ostream) const;
  bool getSubPayload(std::ostringstream& _buf, const std::string& _sSubSection, Section::FormatType _eFormatType) const;
  void readSubPayload(std::istream& _istream, const std::string& _sSubSection, Section::FormatType _eFormatType);
  virtual void initXclBinSectionHeader(axlf_section_header& _sectionHeader);
  virtual void writeXclBinSectionBuffer(std::ostream& _ostream) const;
  virtual void appendToSectionMetadata(const boost::property_tree::ptree& _ptAppendData, boost::property_tree::ptree& _ptToAppendTo);

  void dumpContents(std::ostream& _ostream, FormatType _eFormatType) const;
  void dumpSubSection(std::fstream& _ostream, std::string _sSubSection, FormatType _eFormatType) const;

  void getPayload(boost::property_tree::ptree& _pt) const;
  void purgeBuffers();
  void setName(const std::string& _sSectionName);
  void setPathAndName(const std::string& _pathAndName);
  const std::string& getPathAndName() const;

 protected:
  // Child class option to create an JSON metadata
  virtual void marshalToJSON(char* _pDataSection, unsigned int _sectionSize, boost::property_tree::ptree& _ptree) const;
  virtual void marshalFromJSON(const boost::property_tree::ptree& _ptSection, std::ostringstream& _buf) const;
  virtual void getSubPayload(char* _pDataSection, unsigned int _sectionSize, std::ostringstream& _buf, const std::string& _sSubSection, Section::FormatType _eFormatType) const;
  virtual void readSubPayload(const char* _pOrigDataSection, unsigned int _origSectionSize,  std::istream& _istream, const std::string& _sSubSection, Section::FormatType _eFormatType, std::ostringstream& _buffer) const;
  virtual void writeSubPayload(const std::string& _sSubSectionName, FormatType _eFormatType, std::fstream&  _oStream) const;

 protected:
  Section();

 protected:
  axlf_section_kind m_eKind;
  std::string m_sKindName;
  std::string m_sIndexName;

  char* m_pBuffer;
  unsigned int m_bufferSize;
  std::string m_name;

  std::string m_pathAndName;

 private:
  Section(const Section& obj) = delete;
  Section& operator=(const Section& obj) = delete;
};

#endif
