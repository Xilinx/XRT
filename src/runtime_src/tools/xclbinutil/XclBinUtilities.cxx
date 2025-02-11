/**
 * Copyright (C) 2018, 2020-2023 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
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

#include "Section.h"                           // TODO: REMOVE SECTION INCLUDE
#include "XclBinClass.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/uuid/uuid.hpp>          // for uuid
#include <boost/uuid/uuid_io.hpp>       // for to_string
#include <boost/version.hpp>
#include <filesystem>
#include <fstream>
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include <set>

#if (BOOST_VERSION >= 106400)

#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#pragma warning (disable : 4244) // Addresses Boost conversion Windows build warnings
#endif

#include <boost/asio/io_service.hpp>
#include <boost/process.hpp>
#include <boost/process/child.hpp>
#include <boost/process/env.hpp>
#include <boost/process/search_path.hpp>
#endif


#ifdef _WIN32
  #include <winsock2.h>
#else
  #include <arpa/inet.h>
#endif

namespace XUtil = XclBinUtilities;
namespace fs = std::filesystem;

static bool m_bVerbose = false;
static bool m_bQuiet = false;

void
XclBinUtilities::setVerbose(bool _bVerbose) {
  m_bVerbose = _bVerbose;
  TRACE("Verbosity enabled");
}

bool
XclBinUtilities::getVerbose() {
  return m_bVerbose;
}

void
XclBinUtilities::setQuiet(bool _bQuiet) {
  m_bQuiet = _bQuiet;
  TRACE(m_bQuiet ? "Quiet enabled" : "Quiet disabled");
}

bool
XclBinUtilities::isQuiet() {
  return m_bQuiet;
}

void XclBinUtilities::QUIET(const std::string &_msg) {
  if (m_bQuiet == false) {
    std::cout << _msg.c_str() << std::endl;
  }
}

void XclBinUtilities::QUIET(const boost::format& fmt) {
  QUIET(boost::str(fmt));
}


void
XclBinUtilities::TRACE(const std::string& _msg, bool _endl) {
  if (!m_bVerbose)
    return;

  std::cout << "Trace: " << _msg.c_str();

  if (_endl)
    std::cout << std::endl << std::flush;
}

void
XclBinUtilities::TRACE(const boost::format &fmt, bool endl) {
  TRACE(boost::str(fmt), endl);
}


void
XclBinUtilities::TRACE_BUF(const std::string& _msg,
                           const char* _pData,
                           uint64_t _size) {
  if (!m_bVerbose)
    return;

  std::ostringstream buf;
  buf << "Trace: Buffer(" << _msg << ") Size: 0x" << std::hex << _size << std::endl;

  buf << std::hex << std::setfill('0');

  uint64_t address = 0;
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

  std::ostringstream outputBuffer;
  boost::property_tree::write_json(outputBuffer, _pt, true /*Pretty print*/);
  std::cout << outputBuffer.str() << std::endl;
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

  if (_source.size() < bytesToCopy) {
    bytesToCopy = (unsigned int) _source.length();
  }

  // Copy the string
  memcpy(_destBuffer, _source.c_str(), bytesToCopy);
}

unsigned int
XclBinUtilities::bytesToAlign(uint64_t _offset) {
  unsigned int bytesToAlign = (_offset & 0x7) ? 0x8 - (_offset & 0x7) : 0;

  return bytesToAlign;
}

unsigned int
XclBinUtilities::alignBytes(std::ostream & _buf, unsigned int _byteBoundary)
{
  _buf.seekp(0, std::ios_base::end);
  uint64_t bufSize = (uint64_t) _buf.tellp();
  unsigned int bytesAdded = 0;

  if ((bufSize % _byteBoundary) != 0 ) {
    bytesAdded = _byteBoundary - (bufSize % _byteBoundary);
    for (unsigned int index = 0; index < bytesAdded; ++index) {
      char emptyByte = '\0';
      _buf.write(&emptyByte, sizeof(char));
    }
  }

  return bytesAdded;
}


void
XclBinUtilities::binaryBufferToHexString(const unsigned char* _binBuf,
                                         uint64_t _size,
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
    XUtil::TRACE(boost::format("InputString: %d (%s), BufferSize: %d") %  _inputString.length() % _inputString % _bufferSize);
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
XclBinUtilities::stringToUInt64(const std::string& _sInteger, bool _bForceHex) {
  // Is it a hex value
  if (_bForceHex) {
    return std::stoul(_sInteger, nullptr, 16);
  }

  return std::stoul(_sInteger, nullptr, 0); // Allow standard library to determine the base
}

void
XclBinUtilities::printKinds() {
  auto supportedKinds = Section::getSupportedKinds();
  std::cout << "All supported section names supported by this tool:\n";
  for (auto & entry : supportedKinds) {
    std::cout << "  " << entry << "\n";
  }
}

std::string
XclBinUtilities::getUUIDAsString( const unsigned char (&_uuid)[16] )
{
  static_assert (sizeof(boost::uuids::uuid) == 16, "Error: UUID size mismatch");

  // Copy the values to the UUID structure
  boost::uuids::uuid uuid;
  memcpy((void *) &uuid, (void *) &_uuid, sizeof(boost::uuids::uuid));

  // Now decode it to a string we can work with
  return boost::uuids::to_string(uuid);
}


bool
XclBinUtilities::findBytesInStream(std::fstream& _istream, const std::string& _searchString, unsigned int& _foundOffset) {
  _foundOffset = 0;

  std::iostream::pos_type savedLocation = _istream.tellg();

  unsigned int stringLength = (unsigned int) _searchString.length();
  unsigned int matchIndex = 0;

  char aChar;
  while (_istream.get(aChar)) {
    ++_foundOffset;
    if (aChar == _searchString[matchIndex++]) {
      if (matchIndex == stringLength) {
        _foundOffset -= stringLength;
        return true;
      }
    } else {
      matchIndex = 0;
    }
  }
  _istream.clear();
  _istream.seekg(savedLocation);

  return false;
}

static
const std::string &getSignatureMagicValue()
{
  // Magic Value: 5349474E-9DFF41C0-8CCB82A7-131CC9F3
  unsigned char magicChar[] = { 0x53, 0x49, 0x47, 0x4E,
                                0x9D, 0xFF, 0x41, 0xC0,
                                0x8C, 0xCB, 0x82, 0xA7,
                                0x13, 0x1C, 0xC9, 0xF3};

  static std::string sMagicString((char *) &magicChar[0], 16);

  return sMagicString;
}

bool
XclBinUtilities::getSignature(std::fstream& _istream, std::string& _sSignature,
                              std::string& _sSignedBy, unsigned int & _totalSize)
{
  _istream.seekg(0);
  // Find the signature
  unsigned int signatureOffset;
  if (!XclBinUtilities::findBytesInStream(_istream, getSignatureMagicValue(), signatureOffset)) {
    return false;
  }

  // We have a signature read it in
  XUtil::SignatureHeader signature = {};

  _istream.seekg(signatureOffset);
  _istream.read((char*)&signature, sizeof(XUtil::SignatureHeader));

  // Get signedBy
  if (signature.signedBySize != 0)
  {
    _istream.seekg(signatureOffset + signature.signedByOffset);
    std::unique_ptr<char[]> data( new char[ signature.signedBySize ] );
    _istream.read( data.get(), signature.signedBySize );
    _sSignedBy = std::string(data.get(), signature.signedBySize);
  }

  // Get the signature
  if (signature.signatureSize != 0)
  {
    _istream.seekg(signatureOffset + signature.signatureOffset);
    std::unique_ptr<char[]> data( new char[ signature.signatureSize ] );
    _istream.read( data.get(), signature.signatureSize );
    _sSignature = std::string(data.get(), signature.signatureSize);
  }

  _totalSize = signature.totalSignatureSize;
  return true;
}

void
XclBinUtilities::reportSignature(const std::string& _sInputFile)
{
  // Open the file for consumption
  XUtil::TRACE("Examining xclbin binary file for a signature: " + _sInputFile);
  std::fstream inputStream;
  inputStream.open(_sInputFile, std::ifstream::in | std::ifstream::binary);
  if (!inputStream.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + _sInputFile;
    throw std::runtime_error(errMsg);
  }

  std::string sSignature;
  std::string sSignedBy;
  unsigned int totalSize;
  if (!XUtil::getSignature(inputStream, sSignature, sSignedBy, totalSize)) {
    std::string errMsg = "ERROR: No signature found in file: " + _sInputFile;
    throw std::runtime_error(errMsg);
  }

  std::cout << sSignature << " " << totalSize << std::endl;
}

void
XclBinUtilities::removeSignature(const std::string& _sInputFile, const std::string& _sOutputFile)
{
  // Open the file for consumption
  XUtil::TRACE("Examining xclbin binary file for a signature: " + _sInputFile);
  std::fstream inputStream;
  inputStream.open(_sInputFile, std::ifstream::in | std::ifstream::binary);
  if (!inputStream.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + _sInputFile;
    throw std::runtime_error(errMsg);
  }

  // Find the signature
  unsigned int signatureOffset;
  if (!XclBinUtilities::findBytesInStream(inputStream, getSignatureMagicValue(), signatureOffset)) {
    std::string errMsg = "ERROR: No signature found in file: " + _sInputFile;
    throw std::runtime_error(errMsg);
  }

  // Open output file
  std::fstream outputStream;
  outputStream.open(_sOutputFile, std::ifstream::out | std::ifstream::binary);
  if (!outputStream.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for writing: " + _sOutputFile;
    throw std::runtime_error(errMsg);
  }

  // Copy the file contents (minus the signature)
  {
    // copy file
    unsigned int count = 0;
    inputStream.seekg(0);
    char aChar;
    while (inputStream.get(aChar) ) {
      outputStream << aChar;
      if (++count == signatureOffset) {
        break;
      }
    }
  }

  std::cout << "Signature successfully removed." << std::endl;
  outputStream.close();
}

void
createSignatureBufferImage(std::ostringstream& _buf, const std::string & _sSignature, const std::string & _sSignedBy)
{
  XUtil::SignatureHeader signature = {};
  std::string magicValue = getSignatureMagicValue();

  // Initialize the structure
  unsigned int runningOffset = sizeof(XUtil::SignatureHeader);
  memcpy(&signature.magicValue, magicValue.c_str(), sizeof(XUtil::SignatureHeader::magicValue));

  signature.signatureOffset = runningOffset;
  signature.signatureSize = (unsigned int) _sSignature.size();
  runningOffset += signature.signatureSize;

  signature.signedByOffset = runningOffset;
  signature.signedBySize = (unsigned int) _sSignedBy.size();
  runningOffset += signature.signedBySize;

  signature.totalSignatureSize = runningOffset;

  // Write out the data
  _buf.write(reinterpret_cast<const char*>(&signature), sizeof(XUtil::SignatureHeader));
  _buf.write(_sSignature.c_str(), _sSignature.size());
  _buf.write(_sSignedBy.c_str(), _sSignedBy.size());
}


void
XclBinUtilities::addSignature(const std::string& _sInputFile, const std::string& _sOutputFile,
                              const std::string& _sSignature, const std::string& _sSignedBy)
{
  // Error checks
  if (_sInputFile.empty()) {
    std::string errMsg = "ERROR: Missing file name to modify from.";
    throw std::runtime_error(errMsg);
  }

  // Open the file for consumption
  XUtil::TRACE("Examining xclbin binary file to determine if there is already a signature added: " + _sInputFile);
  std::fstream inputStream;
  inputStream.open(_sInputFile, std::ifstream::in | std::ifstream::binary);
  if (!inputStream.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for reading: " + _sInputFile;
    throw std::runtime_error(errMsg);
  }

  // See if there already is a signature, if so do nothing
  unsigned int signatureOffset;
  if (XclBinUtilities::findBytesInStream(inputStream, getSignatureMagicValue(), signatureOffset)) {
    std::string errMsg = "ERROR: The given file already has a signature added. File: " + _sInputFile;
    throw std::runtime_error(errMsg);
  }

  // Open output file
  std::fstream outputStream;
  outputStream.open(_sOutputFile, std::ifstream::out | std::ifstream::binary);
  if (!outputStream.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for writing: " + _sOutputFile;
    throw std::runtime_error(errMsg);
  }

  // Copy the file contents
  {
    // copy file
    inputStream.seekg(0);
    char aChar;
    while (inputStream.get(aChar)) {
      outputStream << aChar;
    }
  }

  // Tack on the signature
  std::ostringstream buffer;
  createSignatureBufferImage(buffer, _sSignature, _sSignedBy);
  outputStream.write(buffer.str().c_str(), buffer.str().size());

  outputStream.close();
}

void
XclBinUtilities::write_htonl(std::ostream & _buf, uint32_t _word32)
{
  uint32_t word32 = htonl(_word32);
  _buf.write((char *) &word32, sizeof(uint32_t));
}

// ----------------------------------------------------------------------------

// Connective entry plus supporting address metadata
typedef struct {
  unsigned int argIndex;        // Argument index
  unsigned int ipLayoutIndex;   // IP Layout Index
  unsigned int memIndex;        // Memory Index
  std::string memType;          // Type of memory being indexed
  uint64_t baseAddress;         // Base address of the memory
  uint64_t size;                // Size of the memory
  bool canGroup;                // Indicates if this entry can be grouped
} WorkingConnection;


// Creates a connection property_tree entry
static void addConnection( std::vector<boost::property_tree::ptree> & groupConnectivity,
                           unsigned int argIndex, unsigned int ipLayoutIndex, unsigned int memIndex)
{
  boost::property_tree::ptree ptConnection;
  ptConnection.put("arg_index", (boost::format("%d") % argIndex).str());
  ptConnection.put("m_ip_layout_index", (boost::format("%d") % ipLayoutIndex).str());
  ptConnection.put("mem_data_index", (boost::format("%d") % memIndex).str());

  groupConnectivity.push_back(ptConnection);
}

// Compare two property trees for equality
static bool
isEqual(const boost::property_tree::ptree & first,
        const boost::property_tree::ptree & second)
{
  // A simple way to determine if the trees are the same
  std::ostringstream outputBufferFirst;
  boost::property_tree::write_json(outputBufferFirst, first, false /*Pretty print*/);

  std::ostringstream outputBufferSecond;
  boost::property_tree::write_json(outputBufferSecond, second, false /*Pretty print*/);

  return (outputBufferFirst.str() == outputBufferSecond.str());
}

// Given the collection of connections, appends to the GROUP_TOPOLOGY and
// GROUP_CONNECTIVITY additional entries that represents grouped memories.
static void
createMemoryBankGroupEntries( std::vector<WorkingConnection> & workingConnections,
                              std::vector<boost::property_tree::ptree> & groupTopology,
                              std::vector<boost::property_tree::ptree> & groupConnectivity)
{
  // Sort our collection by: IP Layout Index, Argument Index, and Base address
  std::sort(workingConnections.begin(), workingConnections.end(),
            [](WorkingConnection &a, WorkingConnection &b) {
              if (a.ipLayoutIndex != b.ipLayoutIndex)     // Level 1: IP Layout Index
                return a.ipLayoutIndex < b.ipLayoutIndex;

              if (a.argIndex != b.argIndex)               // Level 2: Argument Index
                return a.argIndex < b.argIndex;

              return a.baseAddress < b.baseAddress;       // Level 3: Base addresses
            });

  // Assume that all of the memory connections cannot be grouped
  for (auto & entry : workingConnections)
    entry.canGroup = false;

  // Look at the memory connections and determine which are valid to group.
  for (unsigned int index = 0; index < workingConnections.size(); ++index) {
    const unsigned int startIndex = index;
    unsigned int endIndex = index;
    const uint64_t groupBaseAddress = workingConnections[startIndex].baseAddress;
    uint64_t groupSize = workingConnections[startIndex].size;

    // Peek at the next entry
    for (; endIndex + 1 < workingConnections.size(); ++endIndex) {
      const unsigned int peekIndex = endIndex + 1;
      const uint64_t nextBaseAddress = groupBaseAddress + groupSize;

      // The next memory must be:
      //    1) Continuous
      //    2) Same IP
      //    3) Same argument
      if ((nextBaseAddress != workingConnections[peekIndex].baseAddress) ||
          (workingConnections[startIndex].ipLayoutIndex != workingConnections[peekIndex].ipLayoutIndex) ||
          (workingConnections[startIndex].argIndex != workingConnections[peekIndex].argIndex))
        break;
      groupSize += workingConnections[peekIndex].size;
    }

    index = endIndex;

    // Group requirements:
    //    + Group size must be greater then 1
    //    + Only one (1) group per IP Argument pair
    //
    // Note: Because of how the collection is sorted, if there are multiple groups,
    //       they would be before or after the current group collection.
    if (startIndex == endIndex)
      continue;

    if ((startIndex != 0) &&
        (workingConnections[startIndex-1].ipLayoutIndex == workingConnections[startIndex].ipLayoutIndex) &&
        (workingConnections[startIndex-1].argIndex == workingConnections[startIndex].argIndex))
      continue;

    if ((endIndex < (workingConnections.size() - 1)) &&
        (workingConnections[endIndex+1].ipLayoutIndex == workingConnections[endIndex].ipLayoutIndex) &&
        (workingConnections[endIndex+1].argIndex == workingConnections[endIndex].argIndex))
      continue;

    // The is a valid group, mark it as so
    for (unsigned int goodIndex = startIndex; goodIndex <= endIndex; ++goodIndex)
      workingConnections[goodIndex].canGroup = true;
  }

  // Collect the groups
  for (unsigned int index = 0; index < workingConnections.size(); ++index) {
    const unsigned int startIndex = index;
    unsigned int endIndex = index;
    const uint64_t groupBaseAddress = workingConnections[startIndex].baseAddress;
    uint64_t groupSize = workingConnections[startIndex].size;

    // Peek at the next entry
    if (workingConnections[startIndex].canGroup) {
      for (; endIndex + 1 < workingConnections.size(); ++endIndex) {
        const unsigned int peekIndex = endIndex + 1;
        const uint64_t nextBaseAddress = groupBaseAddress + groupSize;

        if ((nextBaseAddress != workingConnections[peekIndex].baseAddress) ||
            (workingConnections[startIndex].ipLayoutIndex != workingConnections[peekIndex].ipLayoutIndex) ||
            (workingConnections[startIndex].argIndex != workingConnections[peekIndex].argIndex))
          break;
        groupSize += workingConnections[peekIndex].size;
      }
    }

    // Update to our next working index
    index = endIndex;

    // If range is 1 then no grouping is needed
    if (startIndex == endIndex) {
      addConnection(groupConnectivity, workingConnections[startIndex].argIndex, workingConnections[startIndex].ipLayoutIndex, workingConnections[startIndex].memIndex);
      continue;
    }

    // Create a group entry based on the first memory entry
    boost::property_tree::ptree ptGroupMemory = groupTopology[workingConnections[startIndex].memIndex];

    // Prepare a new entry
    {
      const boost::optional<std::string> sSizeBytes = ptGroupMemory.get_optional<std::string>("m_size");
      if (sSizeBytes.is_initialized())
        ptGroupMemory.put("m_size", (boost::format("0x%lx") % groupSize).str());
      else
        ptGroupMemory.put("m_sizeKB", (boost::format("0x%lx") % (groupSize / 1024)).str());

      // Add a tag value to indicate that this entry was the result of grouping memories
      std::vector<int> memIndexVector;
      for (unsigned int memIndex = startIndex; memIndex <= endIndex; ++memIndex) {
        memIndexVector.push_back(workingConnections[memIndex].memIndex);
      }

      // Sort the vector
      std::sort(memIndexVector.begin(), memIndexVector.end());

      // Iterate over the collection producing a more compress tag.
      // Contigious tag specified as start:end and non-contigious are seperated by ','
      // Ex. MBG[0,2,3,4,6,8,9] becomes MBG[0,2:4,6,8:9]
      std::string newTag = "MBG[";
      for (unsigned int idx = 0; idx < memIndexVector.size();)
      {
        auto s_index = idx;
        while ((memIndexVector[idx] + 1) == memIndexVector[idx + 1])
          idx++;

        newTag += std::to_string(memIndexVector[s_index]);
        if (s_index != idx) {
          newTag += ":";
          newTag += std::to_string(memIndexVector[idx]);
        }

        // If terminal then add ']' otherwise add ','
        newTag += (idx != memIndexVector.size() - 1) ? "," : "]";
        idx++;
      }

      // Record the new tag, honoring the size limitation
      ptGroupMemory.put("m_tag", newTag.substr(0, sizeof(mem_data::m_tag) - 1).c_str());
    }

    unsigned int groupMemIndex = 0; // Index where this group memory entry is located

    // See if this entry has already been added, if so use it
    for (groupMemIndex = 0; groupMemIndex < (unsigned int) groupTopology.size(); ++groupMemIndex) {
      if (isEqual(groupTopology[groupMemIndex], ptGroupMemory))
        break;
    }

    // Entry not found, add it to the array
    if (groupMemIndex == groupTopology.size())
      groupTopology.push_back(ptGroupMemory);

    // Create the connection entry
    addConnection(groupConnectivity, workingConnections[startIndex].argIndex, workingConnections[startIndex].ipLayoutIndex, groupMemIndex);
  }
}

static void const
validateMemoryBankGroupEntries( const unsigned int startGroupMemIndex,
                                const std::vector<boost::property_tree::ptree> & groupTopology,
                                const std::vector<boost::property_tree::ptree> & groupConnectivity)
{
  // Were there any memory groups added
  if (startGroupMemIndex >= groupTopology.size())
    return;

  // Validate a 1-to-1 relation between group connectivity to group topology group entry
  for (unsigned int index = 0; index < groupConnectivity.size(); ++index) {
    const unsigned int argIndex = groupConnectivity[index].get<unsigned int>("arg_index");
    const unsigned int ipLayoutIndex = groupConnectivity[index].get<unsigned int>("m_ip_layout_index");
    const unsigned int memIndex = groupConnectivity[index].get<unsigned int>("mem_data_index");

    // If the memory being examined is a group entry, then validate that there
    // are no other entries associated with connection
    if (memIndex >= startGroupMemIndex) {
      for (unsigned int searchIndex = 0; searchIndex < groupConnectivity.size(); ++searchIndex) {
        // Don't examine the reference entry
        if (searchIndex == index)
          continue;

        // We are looking for common IP and argument indexes
        if ((groupConnectivity[searchIndex].get<unsigned int>("arg_index") != argIndex) ||
            (groupConnectivity[searchIndex].get<unsigned int>("m_ip_layout_index") != ipLayoutIndex))
          continue;

        // Do we have a duplicate entry
        const unsigned int searchMemIndex = groupConnectivity[searchIndex].get<unsigned int>("mem_data_index");
        if (searchMemIndex == memIndex) {
          auto errMsg = boost::format("ERROR: Connection indexes at %d and %d in the GROUP_CONNECTIVITY section are duplicates of each other.") % index % searchIndex;
          throw std::runtime_error(errMsg.str());
        }

        // Memory connectivity is not continuous (when using grouped memories)
        auto errMsg = boost::format("ERROR: Invalid memory grouping (not continuous).\n"
                                    "       Connection:\n"
                                    "           arg_index       : %d\n"
                                    "           ip_layout_index : %d\n"
                                    "           mem_data_index  : %d (group)\n"
                                    "       is also connected to mem_data_index %d.\n") % argIndex % ipLayoutIndex % memIndex % searchMemIndex;
        throw std::runtime_error(errMsg.str());
      }
    }
  }
}

static void
transformMemoryBankGroupingCollections(const std::vector<boost::property_tree::ptree> & connectivity,
                                       std::vector<boost::property_tree::ptree> & groupTopology,
                                       std::vector<boost::property_tree::ptree> & groupConnectivity)
{
  // Memory types that can be grouped
  static const std::vector<std::string> validGroupTypes = { "MEM_HBM", "MEM_DDR3", "MEM_DDR4" };

  std::vector<WorkingConnection> possibleGroupConnections;

  // Examine the existing connections.  Collect the bank grouping candidates and
  // place those that are not in the groupConnectivitySection.
  for (auto & connection : connectivity) {
    const unsigned int argIndex = connection.get<unsigned int>("arg_index");
    const unsigned int ipLayoutIndex = connection.get<unsigned int>("m_ip_layout_index");
    const unsigned int memIndex = connection.get<unsigned int>("mem_data_index");

    // Determine if the connection is a valid grouping connection
    // Algorithm: Look at the memory type and if the memory is used
    auto memType = groupTopology[memIndex].get<std::string>("m_type");
    if (memType.compare("MEM_DRAM") == 0)
        memType = "MEM_HBM";

    if ((std::find( validGroupTypes.begin(), validGroupTypes.end(), memType) == validGroupTypes.end()) ||
        (groupTopology[memIndex].get<uint8_t>("m_used") == 0)) {
      addConnection(groupConnectivity, argIndex, ipLayoutIndex, memIndex);
      continue;
    }

    // This connection need to be evaluated
    // Collect information about the memory
    const uint64_t baseAddress = XUtil::stringToUInt64(groupTopology[memIndex].get<std::string>("m_base_address"));
    uint64_t sizeBytes = 0;
    boost::optional<std::string> sSizeBytes = groupTopology[memIndex].get_optional<std::string>("m_size");
    if (sSizeBytes.is_initialized())
      sizeBytes = XUtil::stringToUInt64(static_cast<std::string>(sSizeBytes.get()));
    else {
      boost::optional<std::string> sSizeKBytes = groupTopology[memIndex].get_optional<std::string>("m_sizeKB");
      if (sSizeKBytes.is_initialized())
        sizeBytes = XUtil::stringToUInt64(static_cast<std::string>(sSizeKBytes.get())) * 1024;
    }

    possibleGroupConnections.emplace_back( WorkingConnection{argIndex, ipLayoutIndex, memIndex, memType, baseAddress, sizeBytes, 0 /*canGroup*/} );
  }

  // Group the memories
  createMemoryBankGroupEntries(possibleGroupConnections, groupTopology, groupConnectivity);
}

// CR1176455: DRC to check if dpu_kernel_ids of AIE_PARTITION section matches with the m_kernel_ids of IP_LAYOUT section
// because the dpu_kernel_id is the key in mapping between CU and PDI.
bool
XclBinUtilities::checkAIEPartitionIPLayoutCompliance(XclBin & xclbin){
  // Get AIE_PARTITION metadata only when AIE_PARTITION section is just added
  std::set<std::string> allDpuKernelIDs;
  Section *pAIEPartition = xclbin.findSection(AIE_PARTITION);
  std::string jsonFile = pAIEPartition->getPathAndName();
  // If the aie partition metadata file is not found,
  // then AIE_PARTITION section has already been added hence no-op
  if(jsonFile.empty()){
    return true;
  }
  boost::property_tree::ptree pt;
  boost::property_tree::read_json(jsonFile, pt);
  const boost::property_tree::ptree& ptAIEPartition = pt.get_child("aie_partition");
  std::vector<boost::property_tree::ptree> ptPDIs = XUtil::as_vector<boost::property_tree::ptree>(ptAIEPartition, "PDIs");
  for (const auto& pdi : ptPDIs) {
    std::vector<boost::property_tree::ptree> ptCDOs = XUtil::as_vector<boost::property_tree::ptree>(pdi, "cdo_groups");
    for (const auto& cdo_group : ptCDOs) {
      std::vector<std::string> dpuKernelIDs = XUtil::as_vector_simple<std::string>(cdo_group, "dpu_kernel_ids");
      allDpuKernelIDs.insert(dpuKernelIDs.begin(), dpuKernelIDs.end());
    }
  }

  // Get IP_LAYOUT metadata
  Section *pIPLayout = xclbin.findSection(IP_LAYOUT);
  boost::property_tree::ptree ptIPLayout;
  pIPLayout->getPayload(ptIPLayout);
  boost::property_tree::ptree ptIPlayout = ptIPLayout.get_child("ip_layout");
  boost::property_tree::ptree ipDatas = ptIPlayout.get_child("m_ip_data");
  for (const auto& element : ipDatas) {
    boost::property_tree::ptree ptIPData = element.second;
    std::string sm_type = ptIPData.get<std::string>("m_type");
    std::string sSubType = ptIPData.get<std::string>("m_subtype", "");
    // Check if each m_kernel_id is present in the set of all dpu_kernel_ids
    if(sm_type == "IP_PS_KERNEL" && sSubType == "DPU"){
      std::string sKernelId = ptIPData.get<std::string>("m_kernel_id", "");
      if(allDpuKernelIDs.find(sKernelId) == allDpuKernelIDs.end()){
        XUtil::TRACE("There is no matching dpu_kernel_id in AIE_PARTITION for m_kernel_id " + sKernelId + " in IP_LAYOUT");
        return false;
      }
    }
  }
  return true;
}

void
XclBinUtilities::createMemoryBankGrouping(XclBin & xclbin)
{
  // -- DRC checks
  if (xclbin.findSection(ASK_GROUP_TOPOLOGY) != nullptr)
    throw std::runtime_error("ERROR: GROUP_TOPOLOGY section already exists.  Unable to auto create the GROUP_TOPOLOGY section for memory bank grouping.");

  if (xclbin.findSection(ASK_GROUP_CONNECTIVITY) != nullptr)
    throw std::runtime_error("ERROR: GROUP_CONNECTIVITY section already exists.  Unable to auto create the GROUP_CONNECTIVITY section for memory bank grouping.");

  // -- Create a copy of the MEM_TOPOLOGY section
  Section *pMemTopology = xclbin.findSection(MEM_TOPOLOGY);
  if (pMemTopology == nullptr)
    throw std::runtime_error("ERROR: MEM_TOPOLOGY section doesn't exist.  Unable to auto create the memory bank grouping sections.");

  boost::property_tree::ptree ptMemTopology;
  pMemTopology->getPayload(ptMemTopology);
  const auto memTopology = XUtil::as_vector<boost::property_tree::ptree>(ptMemTopology.get_child("mem_topology"), "m_mem_data");
  if ( memTopology.empty() ) {
    std::cout << "Info: MEM_TOPOLOGY section is empty.  No action will be taken to create the GROUP_TOPOLOGY section." << std::endl;
    return;
  }

  // Copy the data
  std::vector<boost::property_tree::ptree> groupTopology = memTopology;

  // -- If there is a connectivity section, then create the memory groupings
  std::vector<boost::property_tree::ptree> groupConnectivity;

  Section *pConnectivity = xclbin.findSection(CONNECTIVITY);
  if (pConnectivity != nullptr) {
    boost::property_tree::ptree ptConnectivity;
    pConnectivity->getPayload(ptConnectivity);
    const auto connectivity = XUtil::as_vector<boost::property_tree::ptree>(ptConnectivity.get_child("connectivity"), "m_connection");
    if ( connectivity.empty() ) {
      std::cout << "Info: CONNECTIVITY section is empty.  No action taken regarding creating the GROUP_CONNECTIVITY section." << std::endl;
    } else {
      // DRC: Validate the memory indexes
      for (unsigned int index = 0; index < connectivity.size(); ++index) {
        const unsigned int memIndex = connectivity[index].get<unsigned int>("mem_data_index");
        if (memIndex >= groupTopology.size()) {
          auto errMsg = boost::format("ERROR: Connectivity section 'mem_data_index' (%d) at index %d exceeds the number of 'mem_topology' elements (%d).  This is usually an indication of corruption in the xclbin archive.") % memIndex % index % groupTopology.size();
          throw std::runtime_error(errMsg.str());
        }
      }

      // Transform and group the memories
      transformMemoryBankGroupingCollections(connectivity, groupTopology, groupConnectivity);

      // Perform some DRC checks on the memory grouping and connectivity produced before marging to group connectivity
      validateMemoryBankGroupEntries((unsigned int) memTopology.size(), groupTopology, groupConnectivity);

      // Merge connectivity information into the group connectivity
      for (auto & connection : connectivity) {
        groupConnectivity.push_back(connection);
      }

      // Re-create the property tree, create and re-populate the Group Connectivity section, and add it.
      {
        boost::property_tree::ptree ptConnection;
        for (const auto & connection : groupConnectivity)
          ptConnection.push_back({"", connection});

        boost::property_tree::ptree ptGroupConnection;
        ptGroupConnection.add_child("m_connection", ptConnection);
        ptGroupConnection.put("m_count", groupConnectivity.size());

        boost::property_tree::ptree ptTop;
        ptTop.add_child("group_connectivity", ptGroupConnection);
        XUtil::TRACE_PrintTree("Group Connectivity", ptTop);

        Section* pGroupConnectivitySection = Section::createSectionObjectOfKind(ASK_GROUP_CONNECTIVITY);
        pGroupConnectivitySection->readJSONSectionImage(ptTop);
        xclbin.addSection(pGroupConnectivitySection);
      }
    }
  }

  // Re-create the property tree, create and re-populate the Group Topology section, and add it.
  {
    boost::property_tree::ptree ptMemData;
    for (const auto & mem_data : groupTopology)
      ptMemData.push_back({"", mem_data});

    boost::property_tree::ptree ptGroupTopology;
    ptGroupTopology.add_child("m_mem_data", ptMemData);
    ptGroupTopology.put("m_count", groupTopology.size());

    boost::property_tree::ptree ptTop;
    ptTop.add_child("group_topology", ptGroupTopology);
    XUtil::TRACE_PrintTree("Group Topology", ptTop);

    Section* pGroupTopologySection = Section::createSectionObjectOfKind(ASK_GROUP_TOPOLOGY);
    pGroupTopologySection->readJSONSectionImage(ptTop);
    xclbin.addSection(pGroupTopologySection);
  }
}

#ifndef _WIN32
// pdi_transform is only available on Linux
// pdi_transform is defined in libtransformcdo.a
extern "C" int pdi_transform(char* pdi_file, char* pdi_file_out, const char* out_file);

int transform_PDI_file(std::string fileName)
{
  // pdi_transform prints lots of messages, these messages go to
  // the third argument of pdi_transform
  // pdi_transform can either return 0 or ‐1
  int ret = pdi_transform(fileName.data(), fileName.data(), "transform_out");
  if (ret != 0) {
    std::string errMsg = "ERROR: --transform-pdi is specified, but pdi transformation failed, please make sure the pdi files are valid";
    throw std::runtime_error(errMsg);
  }

  return ret;
}

void
XclBinUtilities::transformAiePartitionPDIs(XclBin & xclbin)
{
  // find all sections with type "AIE_PARTITION" in xclbin
  // create a temp empty folder on disk, e.g. "ap_temp"
  // for each section
  //   dump the content (pdi files) to the temp folder
  //      <index name>/
  //          orig/
  //             aie_partition.json
  //             1.pdi, 2.pdi ...
  //   copy orig/ to transform/
  //   for each pdi in transform/, call pdi_transform API from aie-pdi-transform
  //      <index name>/
  //          transform/
  //             aie_partition.json
  //             transformed 1.pdi, 2.pdi ...
  //   construct the string for sections to be removed, add to container A
  //   construct PSD for the sections in <temp>/<index name>/transform, add to container B
  // }
  // for each section in container A
  //   removeSection(PSD)
  // for each PSD in container B
  //   addSection(PSD)
  // delete the temp dir

  // create a temp directory
  // fs::current_path or fs::temp_directory_path?
  std::string apJson = "aie_partition.json";
  // ap: aie_partition
  fs::path tempDir = fs::current_path() / "ap_temp";

  std::vector<std::string> removeSections;
  std::vector<std::string> addSections;
  std::vector<Section*> sections = xclbin.findSection(AIE_PARTITION, true);
  for (const auto& pSection : sections) {
    // get the section name, index
    std::string sSectionKind = pSection->getSectionKindAsString();
    std::string sSectionIndex = pSection->getSectionIndexName();
    std::string sSectionName = pSection->getName();
    // std::cout << "sSectionKind = " << sSectionKind << std::endl;
    // std::cout << "sSectionIndex = " << sSectionIndex << std::endl;
    // std::cout << "sSectionName = " << sSectionName << std::endl;

    // note sSectionIndex could be an empty string
    fs::path origDir = tempDir / sSectionIndex / "orig";
    fs::path transformDir = tempDir / sSectionIndex / "transform";
    fs::path origApJsonPath = origDir / apJson;
    fs::path tranApJsonPath = transformDir / apJson;
    // std::cout << "origDir = " << origDir.string() << std::endl;

    try {
      fs::create_directories(origDir);
    }
    catch (std::exception& e) { // Not using fs::filesystem_error since std::bad_alloc can throw too.
      // if we couldn't create directory, usually something fundamental is wrong (e.g. user has no permission)
      std::string errMsg = "ERROR: couldn't create directory: " + std::string(e.what());
      throw std::runtime_error(errMsg);
    }
    // std::cout << "Temporary directory created: " << origDir << std::endl;

    // construct the PSD for dumpSection
    std::string sDumpSection = sSectionKind + "[" + sSectionIndex + "]:JSON:" + origApJsonPath.string();
    // std::cout << "sDumpSection = " << sDumpSection << std::endl;
    ParameterSectionData dumpPsd(sDumpSection);
    xclbin.dumpSection(dumpPsd);

    std::string sRemoveSection = sSectionKind + "[" + sSectionIndex + "]";
    removeSections.push_back(sRemoveSection);

    // after dumpSection, <temp>/<index name>/orig/ should be populated with pdi files
    // copy it to <temp>/<index name>/transform/
    fs::copy(origDir, transformDir);

    // transform the pdi in the transform/ folder
    // Iterate over the files in the directory
    // std::cout << "Transform the pdi files in " << transformDir.string() << std::endl;
    // the order of the files returned by std::filesystem::directory_iterator() is
    // not specified, sort the files so that it is easier to compare the result between
    // runs
    std::vector<fs::directory_entry> files_in_directory;
    std::copy(fs::directory_iterator(transformDir), fs::directory_iterator(), std::back_inserter(files_in_directory));
    std::sort(files_in_directory.begin(), files_in_directory.end());

    for (const auto& entry : files_in_directory) {
      if (!fs::is_regular_file(entry) || entry.path().extension() != ".pdi")
        continue;

      // std::cout << "pdi file found: " << entry.path() << std::endl;
      // if pdi_transform fails, transform_PDI_file throws, so no need to
      // check the return value
      transform_PDI_file(entry.path().string());
      XUtil::TRACE("pdi file transformed: " + entry.path().string());
    }

    // construct the PSD for addSection
    std::string sAddSection = sSectionKind + "[" + sSectionIndex + "]:JSON:" + tranApJsonPath.string();
    // std::cout << "sAddSection = " << sAddSection << std::endl;
    addSections.push_back(sAddSection);
  }

  // remove the sections in removeSections
  for (const auto& sectionToRemove : removeSections)
    // std::cout << "remove section " << sectionToRemove << std::endl;
    xclbin.removeSection(sectionToRemove);

  // add the sections in transform folder
  for (const auto& sAddSection : addSections) {
    // std::cout << "add section " << sAddSection << std::endl;
    ParameterSectionData addPsd(sAddSection);
    xclbin.addSection(addPsd);
  }

  // delete the temp dir
  fs::remove_all(tempDir);
}
#endif


#if (BOOST_VERSION >= 106400)
int
XclBinUtilities::exec(const fs::path &cmd,
                      const std::vector<std::string> &args,
                      bool bThrow,
                      std::ostringstream & os_stdout,
                      std::ostringstream & os_stderr)
{
  //boost::process::ipstream ip_stdout;
  //boost::process::ipstream ip_stderr;
  std::future<std::string> data_stdout;
  std::future<std::string> data_stderr;

  boost::asio::io_service svc;
  boost::process::child runningProcess( cmd.string(),
                                        args,
                                        boost::process::std_out > data_stdout,
                                        boost::process::std_err > data_stderr,
                                        boost::this_process::environment(),
                                        svc);
  svc.run();
  runningProcess.wait();

  // Update the return buffers
  os_stdout << data_stdout.get();
  os_stderr << data_stderr.get();

  // Obtain the exit code from the running process
  int exitCode = runningProcess.exit_code();

  if (exitCode != 0) {
    auto errMsg = boost::format("Error: Shell command exited with a non-zero value (%d)\n"
                                "     Cmd: %s %s\n"
                                "  StdOut: %s\n"
                                "  StdErr: %s\n")
                                % exitCode
                                % cmd.string() % boost::algorithm::join(args, " ")
                                % os_stdout.str()
                                % os_stderr.str();
    if (bThrow)
      throw std::runtime_error(errMsg.str());
  }

  return exitCode;
}

#else
int
XclBinUtilities::exec(const fs::path &cmd,
                      const std::vector<std::string> &args,
                      bool bThrow,
                      std::ostringstream & os_stdout,
                      std::ostringstream & os_stderr)
{
  // Build the command line
  const std::string cmdLine = cmd.string() + " " + boost::algorithm::join(args, " ");

  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmdLine.c_str(), "r"), pclose);
  if (!pipe) {
    auto errMsg = boost::format("Error: Shell command failed\n"
                                "       Cmd: %s %s\n")
                                % cmd.string() % boost::algorithm::join(args, " ");

    if (bThrow)
      throw std::runtime_error(errMsg.str());

    return 1;
  }

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    result += buffer.data();

  os_stdout << result;

  return 0;
}


#endif


