/**
 * Copyright (C) 2019 Xilinx, Inc
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

// #includes here - please keep these to a bare minimum!
#include <string>
#include <boost/property_tree/ptree.hpp>

// ------------ F O R W A R D - D E C L A R A T I O N S ----------------------
// Forward declarations - use these instead whenever possible...
class DTCStringsBlock;

// ------------------- C L A S S :   S e c t i o n ---------------------------

class FDTProperty {

 public:
  FDTProperty(const char* _pBuffer, const unsigned int _size, const DTCStringsBlock & _dtcStringsBlock, unsigned int & _bytesExamined);
  FDTProperty(boost::property_tree::ptree::const_iterator & _iter);
  virtual ~FDTProperty();

 public:
  static bool isProperty(const std::string &_sName);

 public:
  void marshalToDTC(DTCStringsBlock & _dtcStringsBlock, std::ostream& _buf) const;
  void marshalToJSON(boost::property_tree::ptree &_dtcTree) const;

 protected: 
  enum DataFormat {
   DF_unknown,
   DF_au16,
   DF_u16,
   DF_u32,
   DF_u128,
   DF_sz,
   DF_au64,
   DF_au8,
  };

  unsigned int getWordLength(enum DataFormat _eDataFormat);
  DataFormat getDataFormat(const std::string _sVariableName);
  bool isDataFormatArray(enum DataFormat _eDataFormat);

  static void runningBufferCheck(const unsigned int _bytesExamined, const unsigned int _size);
  static bool hasEnding(std::string const &_sFullString, std::string const & _sEndSubString);

  void au16MarshalToJSON(boost::property_tree::ptree &_ptTree) const;
  void au8MarshalToJSON(boost::property_tree::ptree &_ptTree) const;
  void u16MarshalToJSON(boost::property_tree::ptree &_ptTree) const;
  void u32MarshalToJSON(boost::property_tree::ptree &_ptTree) const;
  void u128MarshalToJSON(boost::property_tree::ptree &_ptTree) const;
  void szMarshalToJSON(boost::property_tree::ptree &_ptTree) const;
  void au64MarshalToJSON(boost::property_tree::ptree &_ptTree) const;

  void marshalDataFromJSON(boost::property_tree::ptree::const_iterator & _iter);

 private:
  void writeDataWord(enum DataFormat _eDataFormat, char * _buffer, const std::string & _sData);

 private:
  // Purposefully private and undefined ctors...
  FDTProperty();
  FDTProperty(const FDTProperty& obj);
  FDTProperty& operator=(const FDTProperty& obj);

 private:
  unsigned int m_dataLength;  // The length of the data buffer
  char * m_pDataBuffer;       // The databuffer
  std::string m_name;         // The name of the property
};


#endif
