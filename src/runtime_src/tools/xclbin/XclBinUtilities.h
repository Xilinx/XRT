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

#ifndef __XclBinUtilities_h_
#define __XclBinUtilities_h_

// Include files
#include "xclbin.h"
#include <string>
#include <memory>
#include <boost/property_tree/ptree.hpp>


#include <iostream>
namespace XclBinUtilities {
//
template<typename ... Args>

std::string format(const std::string& format, Args ... args) {
  size_t size = 1 + snprintf(nullptr, 0, format.c_str(), args ...);
  std::unique_ptr<char[]> buf(new char[size]);
  snprintf(buf.get(), size, format.c_str(), args ...);
  
  return std::string(buf.get(), buf.get() + size);
}

struct SignatureHeader {
   unsigned char magicValue[16];   // Magic Signature Value 5349474E-9DFF41C0-8CCB82A7-131CC9F3
   unsigned char padding[8]  ;     // Future variables. Initialized to zero.
   unsigned int signedByOffset;    // The offset string by whom it was signed by
   unsigned int signedBySize;      // The size of the signature
   unsigned int signatureOffset;   // The offset string of the signature
   unsigned int signatureSize;     // The size of the signature
   unsigned int totalSignatureSize;// Total size of this structure and strings
};

void addSignature(const std::string& _sInputFile, const std::string& _sOutputFile, const std::string& _sSignature, const std::string& _sSignedBy);
void reportSignature(const std::string& _sInputFile);
void removeSignature(const std::string& _sInputFile, const std::string& _sOutputFile);
bool getSignature(std::fstream& _istream, std::string& _sSignature, std::string& _sSignedBy, unsigned int & _totalSize);

bool findBytesInStream(std::fstream& _istream, const std::string& _searchString, unsigned int& _foundOffset);
void setVerbose(bool _bVerbose);
void TRACE(const std::string& _msg, bool _endl = true);
void TRACE_PrintTree(const std::string& _msg, const boost::property_tree::ptree& _pt);
void TRACE_BUF(const std::string& _msg, const char* _pData, unsigned long _size);

void safeStringCopy(char* _destBuffer, const std::string& _source, unsigned int _bufferSize);
unsigned int bytesToAlign(unsigned int _offset);

void binaryBufferToHexString(const unsigned char* _binBuf, unsigned int _size, std::string& _outputString);
void hexStringToBinaryBuffer(const std::string& _inputString, unsigned char* _destBuf, unsigned int _bufferSize);
uint64_t stringToUInt64(const std::string& _sInteger);
void printKinds();
std::string getUUIDAsString( const unsigned char (&_uuid)[16] );
};

#endif
