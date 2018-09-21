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

#include "XclBinUtilities.h"

#include "Section.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string.h>
#include <inttypes.h>
#include <vector>

namespace XUtil = XclBinUtilities;

static bool m_bVerbose = false;

void
XclBinUtilities::setVerbose(bool _bVerbose) {
  m_bVerbose = _bVerbose;
  TRACE("Verbosity enabled");
}

void
XclBinUtilities::TRACE(const std::string& _msg, bool _endl) {
  if (!m_bVerbose)
    return;

  std::cout << "Trace: " << _msg;

  if (_endl)
    std::cout << std::endl;
}


void
XclBinUtilities::TRACE_BUF(const std::string& _msg,
                           const char* _pData,
                           unsigned long _size) {
  if (!m_bVerbose)
    return;

  std::ostringstream buf;
  buf << "Trace: Buffer(" << _msg << ") Size: 0x" << std::hex << _size << std::endl;

  buf << std::hex << std::setfill('0');

  unsigned long address = 0;
  while (address < _size) {
    // We know we have data, create the address entry
    buf << "       " << std::setw(8) << address;

    // Read in 16 bytes (or less) at a time
    int bytesRead;
    unsigned char charBuf[16];

    for (bytesRead = 0; (bytesRead < 16) && (address < _size); ++bytesRead, ++address) {
      charBuf[bytesRead] = _pData[address];
    }

    // Show the hex codes
    for (int i = 0; i < 16; i++) {

      // Create a divider ever 8 bytes
      if (i % 8 == 0)
        buf << " ";

      // If we don't have data then display "nothing"
      if (i < bytesRead) {
        buf << " " << std::setw(2) << (unsigned)charBuf[i];
      } else {
        buf << "   ";
      }
    }

    // Bonus: Show printable characters
    buf << "  ";
    for (int i = 0; i < bytesRead; i++) {
      if ((charBuf[i] > 32) && (charBuf[i] <= 126)) {
        buf << charBuf[i];
      } else {
        buf << ".";
      }
    }

    buf << std::endl;
  }

  std::cout << buf.str() << std::endl;
}



void printTree(const boost::property_tree::ptree& pt, std::ostream& _buf = std::cout, int level = 0);



std::string indent(int _level) {
  std::string sIndent;

  for (int i = 0; i < _level; ++i)
    sIndent += "  ";

  return sIndent;
}

void printTree(const boost::property_tree::ptree& pt, std::ostream& _buf, int level) {
  if (pt.empty()) {
    _buf << "\"" << pt.data() << "\"";
  } else {
    if (level)
      _buf << std::endl;

    _buf << indent(level) << "{" << std::endl;

    for (boost::property_tree::ptree::const_iterator pos = pt.begin(); pos != pt.end();) {
      _buf << indent(level + 1) << "\"" << pos->first << "\": ";

      printTree(pos->second, _buf, level + 1);

      ++pos;

      if (pos != pt.end()) {
        _buf << ",";
      }

      _buf << std::endl;
    }

    _buf << indent(level) << " }";
  }

  if (level == 0)
    _buf << std::endl;
}


void
XclBinUtilities::TRACE_PrintTree(const std::string& _msg,
                                 const boost::property_tree::ptree& _pt) {
  if (!m_bVerbose)
    return;

  std::cout << "Trace: Property Tree (" << _msg << ")" << std::endl;

  std::ostringstream buf;
  printTree(_pt, buf);
  std::cout << buf.str();
}

void
XclBinUtilities::safeStringCopy(char* _destBuffer,
                                const std::string& _source,
                                unsigned int _bufferSize) {
  // Check the parameters
  if (_destBuffer == nullptr)
    return;

  if (_bufferSize == 0)
    return;

  // Initialize the destination buffer with zeros
  memset(_destBuffer, 0, _bufferSize);

  // Determine how many bytes to copy
  unsigned int bytesToCopy = _bufferSize - 1;

  if (_source.length() < bytesToCopy) {
    bytesToCopy = _source.length();
  }

  // Copy the string
  memcpy(_destBuffer, _source.c_str(), bytesToCopy);
}

unsigned int
XclBinUtilities::bytesToAlign(unsigned int _offset) {
  unsigned int bytesToAlign = (_offset & 0x7) ? 0x8 - (_offset & 0x7) : 0;

  return bytesToAlign;
}

static
uint64_t calculate_CheckSumSDBM(std::fstream& _istream,
                                unsigned int _bufferSize)
// SDBM Hash Function
// This is the algorithm of choice which is used in the open source SDBM project
// ----------------------------------------------------------------------------
{
  uint64_t hash = 1;

  for (unsigned int index = 0; (index < _bufferSize) && _istream.good(); ++index) {
    unsigned char byte;
    _istream.read((char*)&byte, 1);

    hash = (byte)+(hash << 6) + (hash << 16) - hash;
  }

  return hash;
}

static
bool readCheckSumHeader(std::fstream& _istream, struct checksum& _checksum) {
  // Check to see if the stream is large enough to have a checksum header
  _istream.seekg(0, _istream.end);
  unsigned int streamSize = _istream.tellg();

  if (streamSize < sizeof(checksum)) {
    return false;
  }

  // Read checksum header
  _istream.seekg(-sizeof(checksum), _istream.end);
  _istream.read((char*)&_checksum, sizeof(checksum));

  // Check to see if header is good
  const static std::string magic = "XCHKSUM";
  if (magic.compare(_checksum.m_magic) != 0) {
    return false;
  }

  return true;
}


void
XclBinUtilities::createCheckSumImage(std::fstream& _istream,
                                     struct checksum& _checksum) {
  // Record size of file
  _istream.seekg(0, _istream.end);
  unsigned int bytesToExamine = _istream.tellg();

  // See if there already is a checksum file, if so, don't examine it
  struct checksum fileChecksumHeader;
  if (readCheckSumHeader(_istream, fileChecksumHeader)) {
    bytesToExamine -= sizeof(struct checksum);
  }

  // Prepare to create new checksum value
  _istream.seekg(0, _istream.beg);
  const static std::string magic = "XCHKSUM";
  XUtil::safeStringCopy((char*)&_checksum.m_magic, magic, sizeof(_checksum.m_magic));

  switch (_checksum.m_type) {
    case CST_SDBM:
      _checksum.m_64bit = calculate_CheckSumSDBM(_istream, bytesToExamine);
      XUtil::TRACE(XUtil::format("Calculated SDBM Hash Value: 0x%lx", _checksum.m_64bit));
      break;

    case CST_UNKNOWN:
    case CST_LAST:
    default:
      XUtil::TRACE("Unknown checksum. No action taken");
      break;
  }
  _istream.close();
}

bool
XclBinUtilities::validateImage(const std::string _sFileName) {
  // Error checks
  if (_sFileName.empty()) {
    std::string errMsg = "ERROR: Missing file name to read from.";
    throw std::runtime_error(errMsg);
  }

  // Open the file for consumption
  XUtil::TRACE("Reading xclbin binary file to determine checksum value: " + _sFileName);
  std::fstream fileStream;
  fileStream.open(_sFileName, std::ifstream::in | std::ifstream::binary);
  if (!fileStream.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + _sFileName;
    throw std::runtime_error(errMsg);
  }

  // Make sure we are at the beginning of the stream
  fileStream.seekg(0, fileStream.beg);

  // Does the file have a checksum header
  struct checksum fileChecksum = { 0 };
  if (readCheckSumHeader(fileStream, fileChecksum) == false) {
    std::cout << "Info: File does not contain a checksum header." << std::endl;
    return false;
  }

  // Calculate the checksum
  struct checksum calcChecksum = { 0 };
  calcChecksum.m_type = fileChecksum.m_type;
  createCheckSumImage(fileStream, calcChecksum);

  // Now look to see if they are the same
  switch ((enum CHECKSUM_TYPE)fileChecksum.m_type) {
    case CST_SDBM:
      std::cout << "Info: Checksum hash algorithm: SDBM" << std::endl;
      if (fileChecksum.m_64bit == calcChecksum.m_64bit) {
        std::cout << XUtil::format("Info: [VALID] The file checksum and calculated checksums match: 0x%lx", fileChecksum.m_64bit) << std::endl;
      } else {
        std::cout << XUtil::format("Info: [INVALID] The file checksum (0x%lx) does not match the calculated checksum (0x%lx)", fileChecksum.m_64bit, calcChecksum.m_64bit) << std::endl;
      }
      break;
    case CST_UNKNOWN:
    case CST_LAST:
    default:
      std::cout << "Info: Unknown checksum algorithm" << std::endl;
      return false;
      break;
  }
  return true;
}

void
XclBinUtilities::addCheckSumImage(const std::string _sFileName,
                                  enum CHECKSUM_TYPE _eChecksumType) {
  // Error checks
  if (_sFileName.empty()) {
    std::string errMsg = "ERROR: Missing file name to modify from";
    throw std::runtime_error(errMsg);
  }

  // Open the file for consumption
  XUtil::TRACE("Examining xclbin binary file to determine checksum value: " + _sFileName);
  std::fstream fileStream;
  fileStream.open(_sFileName, std::ifstream::in | std::ifstream::binary);
  if (!fileStream.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + _sFileName;
    throw std::runtime_error(errMsg);
  }

  // Make sure we are at the beginning of the stream
  fileStream.seekg(0, fileStream.beg);

  // Does the file have a checksum header
  struct checksum fileChecksum = { 0 };
  if (readCheckSumHeader(fileStream, fileChecksum) == true) {
    std::string errMsg = "Error: The given file already has a checksum header.  No action taken.";
    throw std::runtime_error(errMsg);
  }

  // Calculate the checksum
  struct checksum calcChecksum = { 0 };
  calcChecksum.m_type = (uint8_t)_eChecksumType;
  createCheckSumImage(fileStream, calcChecksum);
  fileStream.close();

  fileStream.open(_sFileName, std::ifstream::out | std::ifstream::binary | std::ifstream::app);
  if (!fileStream.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for writing: " + _sFileName;
    throw std::runtime_error(errMsg);
  }

  fileStream.seekg(0, fileStream.end);
  fileStream.write((const char*)&calcChecksum, sizeof(checksum));
  fileStream.close();
  XUtil::TRACE("Checksum header added");
}


void
XclBinUtilities::binaryBufferToHexString(unsigned char* _binBuf,
                                         unsigned int _size,
                                         std::string& _outputString) {
  // Initialize output data
  _outputString.clear();

  // Check the data
  if ((_binBuf == nullptr) || (_size == 0)) {
    return;
  }

  // Convert the binary data to an ascii string representation
  std::ostringstream buf;

  for (unsigned int index = 0; index < _size; ++index) {
    buf << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)_binBuf[index];
  }

  _outputString = buf.str();
}


unsigned char
hex2char(const unsigned char _nibbleChar) {
  unsigned char nibble = _nibbleChar;

  if      (_nibbleChar >= '0' && _nibbleChar <= '9') nibble = _nibbleChar - '0';
  else if (_nibbleChar >= 'a' && _nibbleChar <= 'f') nibble = _nibbleChar - 'a' + 10;
  else if (_nibbleChar >= 'A' && _nibbleChar <= 'F') nibble = _nibbleChar - 'A' + 10;

  return nibble;
}


void
XclBinUtilities::hexStringToBinaryBuffer(const std::string& _inputString,
                                         unsigned char* _destBuf,
                                         unsigned int _bufferSize) {
  // Check the data
  if ((_destBuf == nullptr) || (_bufferSize == 0) || _inputString.empty()) {
    std::string errMsg = "Error: hexStringToBinaryBuffer - Invalid parameters";
    throw std::runtime_error(errMsg);
  }

  if (_inputString.length() != _bufferSize * 2) {
    std::string errMsg = "Error: hexStringToBinaryBuffer - Input string is not the same size as the given buffer";
    XUtil::TRACE(XUtil::format("InputString: %d (%s), BufferSize: %d", _inputString.length(), _inputString.c_str(), _bufferSize));
    throw std::runtime_error(errMsg);
  }

  // Initialize buffer
  // Note: We know that the string is even in length
  unsigned int bufIndex = 0;
  for (unsigned int index = 0;
       index < _inputString.length();
       index += 2, ++bufIndex) {
    _destBuf[bufIndex] = (hex2char(_inputString[index]) << 4) + (hex2char(_inputString[index + 1]));
  }
}

uint64_t
XclBinUtilities::stringToUInt64(const std::string& _sInteger) {
  uint64_t value = 0;

  // Is it a hex value
  if ((_sInteger.length() > 2) &&
      (_sInteger[0] == '0') &&
      (_sInteger[1] == 'x')) {
    if (1 == sscanf(_sInteger.c_str(), "%" PRIx64 "", &value)) {
      return value;
    }
  } else {
    if (1 == sscanf(_sInteger.c_str(), "%" PRId64 "", &value)) {
      return value;
    }
  }

  std::string errMsg = "ERROR: Invalid integer string in JSON file: '" + _sInteger + "'";
  throw std::runtime_error(errMsg);
}

void
XclBinUtilities::printKinds() {
  std::vector< std::string > kinds;
  Section::getKinds(kinds);
  std::cout << "All available section names:\n";
  for (auto & kind : kinds) {
    std::cout << "  " << kind << "\n";
  }
}

