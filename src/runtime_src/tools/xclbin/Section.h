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

#ifndef __Section_h_
#define __Section_h_

// ----------------------- I N C L U D E S -----------------------------------

// #includes here - please keep these to a bare minimum!
#include "xclbin.h"

#include <string>
#include <fstream>
#include <map>
#include <functional>

#include <boost/property_tree/ptree.hpp>

// ------------ F O R W A R D - D E C L A R A T I O N S ----------------------
// Forward declarations - use these instead whenever possible...

// ------------------- C L A S S :   S e c t i o n ---------------------------

/**
 * Section:
 *
 *    This class represents the base class for a given Section in the xclbin
 *    archive.  
*/

class Section {
 public:
  enum FormatType{
    FT_RAW,
    FT_JSON,
    FT_HTML
  };

 public:

 public:
  virtual ~Section();

 public:
  static void getKinds(std::vector< std::string > & kinds);
  static Section* createSectionObjectOfKind(enum axlf_section_kind _eKind);
  static bool translateSectionKindStrToKind(const std::string &_sKindStr, enum axlf_section_kind &_eKind);

 public:
  enum axlf_section_kind getSectionKind() const;
  const std::string& getSectionKindAsString() const;

 public:
  // Xclbin Binary helper methods - child classes can override them if they choose
  virtual void readXclBinBinary(std::fstream& _istream, const axlf_section_header& _sectionHeader);
  virtual void readXclBinBinary(std::fstream& _istream, const boost::property_tree::ptree& _ptSection);
  void readXclBinBinary(std::fstream& _istream, enum FormatType _eFormatType);

  virtual void initXclBinSectionHeader(axlf_section_header& _sectionHeader);
  virtual void writeXclBinSectionBuffer(std::fstream& _ostream);
  void dumpContents(std::fstream& _ostream, enum FormatType _eFormatType);

  void addMirrorPayload(boost::property_tree::ptree& _pt) const;

 protected:
  // Child class option to create an JSON metadata
  virtual void marshalToJSON(char* _pDataSection, unsigned int _sectionSize, boost::property_tree::ptree& _ptree) const;
  virtual void marshalFromJSON(const boost::property_tree::ptree& _ptSection, std::ostringstream& _buf) const;

 protected:
  Section();

 protected:
  typedef std::function<Section*()> Section_factory;
  static void registerSectionCtor(enum axlf_section_kind _eKind, const std::string& _sKindStr, Section_factory _Section_factory);

 protected:
  enum axlf_section_kind m_eKind;
  std::string m_sKindName;

  char* m_pBuffer;
  unsigned int m_bufferSize;
  std::string m_name;

 private:
  static std::map<enum axlf_section_kind, std::string> m_mapIdToName;
  static std::map<std::string, enum axlf_section_kind> m_mapNameToId;
  static std::map<enum axlf_section_kind, Section_factory> m_mapIdToCtor;

 private:
  // Purposefully private and undefined ctors...
  Section(const Section& obj);
  Section& operator=(const Section& obj);
};

#endif
