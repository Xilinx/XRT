/**
 * Copyright (C) 2019,2022 Xilinx, Inc
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

#ifndef __FDTProperty_h_
#define __FDTProperty_h_

// ----------------------- I N C L U D E S -----------------------------------
#include <boost/property_tree/ptree.hpp>
#include <map>
#include <string>

// ------------ F O R W A R D - D E C L A R A T I O N S ----------------------
class DTCStringsBlock;

// -------------- C L A S S :   F D T P r o p e r t y ------------------------

class FDTProperty {
 public:
  enum class DataFormat {
    unknown,
    au8,
    au16,
    au32,
    au64,
    u16,
    u32,
    u64,
    u128,
    sz,
    asz,
  };

 public:
  typedef std::map<const std::string, DataFormat> PropertyNameFormat;

 private:
  FDTProperty();

 public:
  FDTProperty(const char* _pBuffer, const unsigned int _size, const DTCStringsBlock& _dtcStringsBlock, unsigned int& _bytesExamined, const PropertyNameFormat& _propertyNameFormat);
  FDTProperty(boost::property_tree::ptree::const_iterator& _iter, const PropertyNameFormat& _propertyNameFormat);
  virtual ~FDTProperty();

 public:
  void marshalToDTC(DTCStringsBlock& _dtcStringsBlock, std::ostream& _buf) const;
  void marshalToJSON(boost::property_tree::ptree& _dtcTree, const PropertyNameFormat& _propertyNameFormat) const;

 protected:
  static void runningBufferCheck(const unsigned int _bytesExamined, const unsigned int _size);
  static bool hasEnding(std::string const& _sFullString, std::string const& _sEndSubString);

  void au8MarshalToJSON(boost::property_tree::ptree& _ptTree) const;
  void au16MarshalToJSON(boost::property_tree::ptree& _ptTree) const;
  void au32MarshalToJSON(boost::property_tree::ptree& _ptTree) const;
  void au64MarshalToJSON(boost::property_tree::ptree& _ptTree) const;
  void aszMarshalToJSON(boost::property_tree::ptree& _ptTree) const;

  void u16MarshalToJSON(boost::property_tree::ptree& _ptTree) const;
  void u32MarshalToJSON(boost::property_tree::ptree& _ptTree) const;
  void u64MarshalToJSON(boost::property_tree::ptree& _ptTree) const;
  void u128MarshalToJSON(boost::property_tree::ptree& _ptTree) const;
  void szMarshalToJSON(boost::property_tree::ptree& _ptTree) const;

  void marshalDataFromJSON(boost::property_tree::ptree::const_iterator& _iter, const PropertyNameFormat& _propertyNameFormat);
  unsigned int writeDataWord(DataFormat _eDataFormat, char* _buffer, const std::string& _sData);

 protected:
  DataFormat getDataFormat(const std::string& _variableName) const;
  const std::string& getDataFormatPrettyName(DataFormat _eDataFormat) const;
  unsigned int getWordLength(DataFormat _eDataFormat) const;
  bool isDataFormatArray(DataFormat _eDataFormat) const;

 private:
  uint32_t m_dataLength;      // The length of the data buffer
  char* m_pDataBuffer;       // The databuffer
  std::string m_name;         // The name of the property
  DataFormat m_eDataFormat;    // The format of the property
};


#endif
