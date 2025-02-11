/**
 * Copyright (C) 2018, 2020-2022 Xilinx, Inc. All rights reserved.
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

#ifndef __XclBinUtilities_h_
#define __XclBinUtilities_h_

// Include files
#include "xrt/detail/xclbin.h"

#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <sstream>
#include <stdint.h>
#include <string>
#include <vector>


class XclBin;

// Custom exception with payloads
typedef enum {
  xet_runtime = 1,           // Generic Runtime error (1)
  xet_missing_section = 100, // Section is missing
} XclBinExceptionType;

namespace XclBinUtilities {

template <typename T>
std::vector<T> as_vector(boost::property_tree::ptree const& pt,
                         boost::property_tree::ptree::key_type const& key)
{
    std::vector<T> r;

    boost::property_tree::ptree::const_assoc_iterator it = pt.find(key);

    if( it != pt.not_found()) {
      for (auto& item : pt.get_child(key)) {
        r.push_back(item.second);
      }
    }
    return r;
}

// This template will eventually replace "as_vector"
// The issue is that the code needs to be refactored to use this new template
template <typename T>
std::vector<T> as_vector_simple(const boost::property_tree::ptree& pt,
                                const boost::property_tree::ptree::key_type& key)
{
  static const boost::property_tree::ptree ptEmpty;
  std::vector<T> r;

  for (auto& item : pt.get_child(key, ptEmpty))
      r.push_back(item.second.get_value<T>());
  return r;
}


class XclBinUtilException : public std::runtime_error {
  private:
    std::string m_msg;
    std::string m_file;
    int m_line;
    std::string m_function;
    XclBinExceptionType m_eExceptionType;

public:
    XclBinUtilException(XclBinExceptionType _eExceptionType,
                        const std::string & _msg,
                        const char * _function = "<not_defined>",
                        const char * _file = __FILE__,
                        int _line = __LINE__)
    : std::runtime_error(_msg)
    , m_msg(_msg)
    , m_file(_file)
    , m_line(_line)
    , m_function(_function)
    , m_eExceptionType(_eExceptionType) {
      // Empty
    }

    ~XclBinUtilException()
    {
      // Empty
    }

    // Use are version of what() and not runtime_error's
    const char* what() const noexcept override{
      return m_msg.c_str();
    }

    const char *file() const {
      return m_file.c_str();
    }

    int line() const throw() {
      return m_line;
    }

    const char *function() const {
      return m_function.c_str();
    }

    XclBinExceptionType exceptionType() const {
      return m_eExceptionType;
    }
};

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
bool getVerbose();
void setQuiet(bool _bQuiet);
bool isQuiet();

void QUIET(const std::string& _msg);
void QUIET(const boost::format & fmt);
void TRACE(const std::string& _msg, bool _endl = true);
void TRACE(const boost::format & fmt, bool _endl = true);
void TRACE_PrintTree(const std::string& _msg, const boost::property_tree::ptree& _pt);
void TRACE_BUF(const std::string& _msg, const char* _pData, uint64_t _size);

void safeStringCopy(char* _destBuffer, const std::string& _source, unsigned int _bufferSize);
unsigned int bytesToAlign(uint64_t _offset);
unsigned int alignBytes(std::ostream & _buf, unsigned int _byteBoundary);

void binaryBufferToHexString(const unsigned char* _binBuf, uint64_t _size, std::string& _outputString);
void hexStringToBinaryBuffer(const std::string& _inputString, unsigned char* _destBuf, unsigned int _bufferSize);
uint64_t stringToUInt64(const std::string& _sInteger, bool _bForceHex = false);
void printKinds();
std::string getUUIDAsString( const unsigned char (&_uuid)[16] );

int exec(const std::filesystem::path &cmd, const std::vector<std::string> &args, bool bThrow, std::ostringstream & os_stdout, std::ostringstream & os_stderr);
void write_htonl(std::ostream & _buf, uint32_t _word32);

bool checkAIEPartitionIPLayoutCompliance(XclBin & xclbin);
void createMemoryBankGrouping(XclBin & xclbin);

// temporary for 2024.1, https://jira.xilinx.com/browse/SDXFLO-6890
void transformAiePartitionPDIs(XclBin & xclbin);
};

#endif
