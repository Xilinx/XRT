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

#ifndef __XclBin_h_
#define __XclBin_h_

#include <string>
#include <fstream>
#include <vector>
#include <boost/property_tree/ptree.hpp>

#include "xclbin.h"
#include "Section.h"

class XclBin {
 public:
  struct SchemaVersion {
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
  };

 public:
  XclBin();
  virtual ~XclBin();

 public:
  void readXclBinBinary(const std::string _binaryFileName, bool _bMigrate = false);
  void writeXclBinBinary(const std::string _binaryFileName);

 private:
  void readXclBinBinaryHeader(std::fstream& _istream);
  void readXclBinBinarySections(std::fstream& _istream);

  void findAndReadMirrorData(std::fstream& _istream, boost::property_tree::ptree& _mirrorData) const;
  void readXclBinaryMirrorImage(std::fstream& _istream, const boost::property_tree::ptree& _mirrorData);

  void writeXclBinBinaryMirrorData(std::fstream& _ostream, const boost::property_tree::ptree& _mirroredData) const;

  void addHeaderMirrorData(boost::property_tree::ptree& _pt_header);

  void addSection(Section* _pSection);

  // Should be in their own separate class
 private:
  void readXclBinHeader(const boost::property_tree::ptree& _ptHeader, struct axlf& _axlfHeader);
  void readXclBinSection(std::fstream& _istream, const boost::property_tree::ptree& _ptSection);
  void writeXclBinBinaryHeader(std::fstream& _ostream, boost::property_tree::ptree& _mirroredData);
  void writeXclBinBinarySections(std::fstream& _ostream, boost::property_tree::ptree& _mirroredData);


 protected:
  void addPTreeSchemaVersion(boost::property_tree::ptree& _pt, SchemaVersion const& _schemaVersion);
  void getSchemaVersion(boost::property_tree::ptree& _pt, SchemaVersion& _schemaVersion);

 private:
  std::vector<Section*> m_sections;
  axlf m_xclBinHeader;

 protected:
  SchemaVersion m_SchemaVersionMirrorWrite;


 private:
  // Purposefully private and undefined ctors...
  XclBin(const XclBin& obj);
  XclBin& operator=(const XclBin& obj);
};


#endif
