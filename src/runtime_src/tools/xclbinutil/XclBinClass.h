/**
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef __XclBin_h_
#define __XclBin_h_

#include <string>
#include <fstream>
#include <vector>
#include <boost/property_tree/ptree.hpp>

#include "xrt/detail/xclbin.h"
#include "ParameterSectionData.h"

class Section;

class XclBin {
 public:
  struct SchemaVersion {
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
  };

 public:
  XclBin();
  ~XclBin();

 public:
  void reportInfo(std::ostream &_ostream, const std::string & _sInputFile, bool _bVerbose) const;
  void printSections(std::ostream &_ostream) const;
  bool checkForValidSection();
  bool checkForPlatformVbnv();
  void readXclBinBinary(const std::string &_binaryFileName, bool _bMigrate = false);
  void writeXclBinBinary(const std::string &_binaryFileName, bool _bSkipUUIDInsertion);
  void removeSection(const std::string & _sSectionToRemove);
  void addSection(ParameterSectionData &_PSD);
  void addReplaceSection(ParameterSectionData &_PSD);
  void addMergeSection(ParameterSectionData &_PSD);
  void addSections(ParameterSectionData &_PSD);
  void appendSections(ParameterSectionData &_PSD);
  void replaceSection(ParameterSectionData &_PSD);
  void dumpSection(ParameterSectionData &_PSD);
  void dumpSections(ParameterSectionData &_PSD);
  void setKeyValue(const std::string & _keyValue);
  void removeKey(const std::string & _keyValue);
  void addSection(Section* _pSection);
  void addPsKernel(const std::string &encodedString);
  void addKernels(const std::string &jsonFile);
  void updateInterfaceuuid();

  public:
    // Helper method to take given encoded keyValue and break it down to its individual values (e.g., domain, key, and value)
    static void getKeyValueComponents(const std::string & _keyValue, std::string & _domain, std::string & _key, std::string & _value);
    static std::string findKeyAndGetValue(const std::string & _searchDomain, const std::string & _searchKey, const std::vector<std::string> & _keyValues);

 public:
  Section *findSection(axlf_section_kind _eKind, const std::string & _indexName = "") const;
  std::vector<Section*> findSection(axlf_section_kind _eKind, bool _ignoreIndex, const std::string & _indexName = "") const;

 private:
  void updateHeaderFromSection(Section *_pSection);
  void readXclBinBinaryHeader(std::fstream& _istream);
  void readXclBinBinarySections(std::fstream& _istream);

  void findAndReadMirrorData(std::fstream& _istream, boost::property_tree::ptree& _mirrorData) const;
  void readXclBinaryMirrorImage(std::fstream& _istream, const boost::property_tree::ptree& _mirrorData);

  void writeXclBinBinaryMirrorData(std::ostream& _ostream, const boost::property_tree::ptree& _mirroredData) const;

  void addHeaderMirrorData(boost::property_tree::ptree& _pt_header);

  void addSubSection(ParameterSectionData &_PSD);
  void dumpSubSection(ParameterSectionData &_PSD);

  void removeSection(const Section* _pSection);

  void updateUUID();

  void initializeHeader(axlf &_xclBinHeader);

  // Should be in their own separate class
 private:
  void readXclBinHeader(const boost::property_tree::ptree& _ptHeader, struct axlf& _axlfHeader);
  void readXclBinSection(std::fstream& _istream, const boost::property_tree::ptree& _ptSection);
  void writeXclBinBinaryHeader(std::ostream& _ostream, boost::property_tree::ptree& _mirroredData);
  void writeXclBinBinarySections(std::ostream& _ostream, boost::property_tree::ptree& _mirroredData);


 protected:
  void addPTreeSchemaVersion(boost::property_tree::ptree& _pt, SchemaVersion const& _schemaVersion);
  void getSchemaVersion(boost::property_tree::ptree& _pt, SchemaVersion& _schemaVersion);

 private:
  std::vector<Section*> m_sections;
  axlf m_xclBinHeader;

 protected:
  SchemaVersion m_SchemaVersionMirrorWrite;
};


#endif
