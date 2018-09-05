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


namespace XclBinUtilities {
//
template<typename ... Args>

std::string format(const std::string& format, Args ... args) {
  size_t size = 1 + snprintf(nullptr, 0, format.c_str(), args ...);
  std::unique_ptr<char[]> buf(new char[size]);
  snprintf(buf.get(), size, format.c_str(), args ...);
  
  return std::string(buf.get(), buf.get() + size);
}

void setVerbose(bool _bVerbose);
void TRACE(const std::string& _msg, bool _endl = true);
void TRACE_PrintTree(const std::string& _msg, const boost::property_tree::ptree& _pt);
void TRACE_BUF(const std::string& _msg, const char* _pData, unsigned long _size);

void safeStringCopy(char* _destBuffer, const std::string& _source, unsigned int _bufferSize);
unsigned int bytesToAlign(unsigned int _offset);

bool validateImage(const std::string _sFileName);
void addCheckSumImage(const std::string _sFileName, enum CHECKSUM_TYPE _eChecksumType);

void createCheckSumImage(std::fstream& _istream, struct checksum& _checksum);

void binaryBufferToHexString(unsigned char* _binBuf, unsigned int _size, std::string& _outputString);
void hexStringToBinaryBuffer(const std::string& _inputString, unsigned char* _destBuf, unsigned int _bufferSize);
uint64_t stringToUInt64(const std::string& _sInteger);
};

#endif
