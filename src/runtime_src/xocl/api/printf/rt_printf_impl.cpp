/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

// Copyright 2015 Xilinx, Inc. All rights reserved.

#include "rt_printf_impl.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
# pragma warning( disable : 4996 )
# define snprintf _snprintf
#endif


/////////////////////////////////////////////////////////////////////////

namespace {
  // Restores ostream flags to original when function goes out of scope
  struct IOS_FlagRestore
  {
    IOS_FlagRestore(std::ostream& os) : m_os(os), m_flags(os.flags()) {}
    ~IOS_FlagRestore() { m_os.flags(m_flags); }
    std::ostream& m_os;
    std::ios::fmtflags m_flags;
  };
}

/////////////////////////////////////////////////////////////////////////

namespace XCL {
namespace Printf {


ConversionSpec::ConversionSpec()
{
  setDefaults();
}

ConversionSpec::ConversionSpec(const std::string& str)
    : m_validSpec(false)
{
  parse(str);
}

ConversionSpec::~ConversionSpec()
{
  m_validSpec = false;
}

bool ConversionSpec::isFloatClass() const
{
  bool retval = false;
  const std::string values = "fFeEgGaA";
  if ( values.find(m_specifier) != std::string::npos ) {
    retval = true;
  }
  return retval;
}

bool ConversionSpec::isIntClass() const
{
  bool retval = false;
  const std::string values = "cdiouxXp";
  if ( values.find(m_specifier) != std::string::npos ) {
    retval = true;
  }
  return retval;
}

bool ConversionSpec::isStringClass() const
{
  return ( m_specifier == 's' );
}

bool ConversionSpec::isVector() const
{
  return (m_vectorSize > 1);
}

bool ConversionSpec::isPercent() const
{
  return (m_specifier == '%');
}

void ConversionSpec::dbgDump(std::ostream& str) const
{
  str << "ConversionSpec Dump:\n";
  str << "  m_validSpec     = " << m_validSpec << "\n";
  str << "  m_specifier     = '" << m_specifier << "'\n";
  str << "  m_fieldWidth    = " << m_fieldWidth << " val = " << m_fieldWidthValue << "\n";
  str << "  m_leftJustify   = " << m_leftJustify << "\n";
  str << "  m_padZero       = " << m_padZero << "\n";
  str << "  m_signPlus      = " << m_signPlus << "\n";
  str << "  m_prefixSpace   = " << m_prefixSpace << "\n";
  str << "  m_alternative   = " << m_alternative << "\n";
  str << "  m_precision     = " << m_precision << " val = " << m_precisionValue << "\n";
  str << "  m_vectorSize    = " << m_vectorSize << "\n";
}

void ConversionSpec::setDefaults()
{
  m_specifier = '\0';
  m_lengthModifier = CS_NONE;
  m_fieldWidth = false;
  m_fieldWidthValue = 0;
  m_leftJustify = 0;
  m_padZero = false;
  m_signPlus = false;
  m_prefixSpace = false;
  m_alternative = false;
  m_precision = false;
  m_precisionValue = 0;
  m_vectorSize = 1;
  m_validSpec = false;
}

void ConversionSpec::parse(const std::string& str)
{
  // Assumptions -
  //    1. str is a valid % printf conversion specifier
  // TODO: Better throwing when assumption 1 does not hold

  setDefaults();
  const char *p_mover = str.c_str();
  if ( *p_mover != '%' ) {
    // Invalid format string
    throwError("'%' not found at beginning of format specifier");
    return;
  }

  ++p_mover;
  bool parseDone = false;
  while ( !parseDone ) {
    char c = *p_mover;
    switch ( c )
    {
        case '%': {
                    m_specifier = c;
                    parseDone = true;
                    break;
                  }

        // Flags
        case '-': {
                    m_leftJustify = true;
                    break;
                  }
        case '+': {
                    m_signPlus = true;
                    break;
                  }
        case ' ': {
                    if ( !m_signPlus ) {
                      m_prefixSpace = true;
                      break;
                    }
                  }
        case '#': {
                    m_alternative = true;
                    break;
                  }
        case '0': {
                    m_padZero = true;
                    break;
                  }

        // Precision
        case '.': {
                    m_precision = true;
                    ++p_mover;
                    m_precisionValue = parseNumber(&p_mover);
                    if ( m_precisionValue == -1 ) {
                      // Adjust for legal cast "%.f" with precision but no number
                      m_precisionValue = 0;
                      --p_mover;
                    }
                    break;
                  }

        // Field width [1-9][0-9]*
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
                    m_fieldWidth = true;
                    m_fieldWidthValue = parseNumber(&p_mover);
                    if ( m_fieldWidthValue == -1 ) {
                      throwError("Bad field width argument during format parse");
                      return;
                    }
                    break;
                  }


        // length modifiers
        case 'h': {
                    // h, hh, hl
                    char nextCh = *(p_mover+1);
                    switch (nextCh) {
                      case 'h': {
                                  m_lengthModifier = ConversionSpec::CS_CHAR;
                                  ++p_mover;
                                  break;
                                }
                      case 'l': {
                                  m_lengthModifier = ConversionSpec::CS_INT_FLOAT;
                                  ++p_mover;
                                  break;
                                }
                      default:  {
                                  m_lengthModifier = ConversionSpec::CS_SHORT;
                                  break;
                                }
                    }
                    break;
                  }

        case 'l': {
                    m_lengthModifier = ConversionSpec::CS_LONG;
                    break;
                  }

        // vector
        case 'v': {
                    // must be 2,3,4,8,16
                    ++p_mover;
                    m_vectorSize = parseNumber(&p_mover);
                    switch ( m_vectorSize ) {
                      case 2:
                      case 3:
                      case 4:
                      case 8:
                      case 16: break;
                      default: {
                                 throwError("Bad vector size argument during format parse");
                                 return;
                               }
                    }
                    break;
                  }

        // Any valid conversion specifier type (ends specifier)
        case 'p':
        case 's':
        case 'c':
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'X':
        case 'x':
        case 'A':
        case 'a':
        case 'E':
        case 'e':
        case 'F':
        case 'f':
        case 'G':
        case 'g': {
                    m_specifier = c;
                    parseDone = true;
                    break;
                  }

        default:  {
                    if ( *p_mover == '\0' ) {
                      throwError("Premature format string termination during format parse");
                    }
                    else {
                      throwError("Unsupported specifier encountered during format parse");
                    }
                    return;
                  }
    }
    ++p_mover;
  }

  // If we survive to the end, everything is ok...
  m_validSpec = true;
}

int ConversionSpec::parseNumber(const char** p_buf)
{
  int retval = -1;
  char c = *(*p_buf);
  if ( isdigit(c) ) {
    retval = 0;
    while ( isdigit(c) ) {
      retval += (c - '0');
      ++(*p_buf);
      c = *(*p_buf);
      if ( isdigit(c) ) {
        retval *= 10;
      }
    }
    // adjust so we end up pointing at last digit
    --(*p_buf);
  }
  return retval;
}

/////////////////////////////////////////////////////////////////////////

PrintfArg::PrintfArg(void *val)
    : m_typeInfo(AT_PTR), ptr(val)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
}

PrintfArg::PrintfArg(const std::string& val)
    : m_typeInfo(AT_STR), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  str = val;
}

PrintfArg::PrintfArg(uint8_t val)
    : m_typeInfo(AT_UINT), ptr(nullptr)
    , int_arg(0), uint_arg(val), float_arg(0.0)
{
}

PrintfArg::PrintfArg(int16_t val)
    : m_typeInfo(AT_INT), ptr(nullptr)
    , int_arg(val), uint_arg(val), float_arg(0.0)
{
}

PrintfArg::PrintfArg(uint16_t val)
    : m_typeInfo(AT_UINT), ptr(nullptr)
    , int_arg(0), uint_arg(val), float_arg(0.0)
{
}

PrintfArg::PrintfArg(int32_t val)
    : m_typeInfo(AT_INT), ptr(nullptr)
    , int_arg(val), uint_arg(val), float_arg(0.0)
{
}

PrintfArg::PrintfArg(uint32_t val)
    : m_typeInfo(AT_UINT), ptr(nullptr)
    , int_arg(0), uint_arg(val), float_arg(0.0)
{
}

PrintfArg::PrintfArg(int64_t val)
    : m_typeInfo(AT_INT), ptr(nullptr)
    , int_arg(val), uint_arg(val), float_arg(0.0)
{
}

PrintfArg::PrintfArg(uint64_t val)
    : m_typeInfo(AT_UINT), ptr(nullptr)
    , int_arg(0), uint_arg(val), float_arg(0.0)
{
}

PrintfArg::PrintfArg(double val)
    : m_typeInfo(AT_FLOAT), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(val)
{
}

PrintfArg::PrintfArg(const std::vector<int8_t>& vec)
    : m_typeInfo(AT_INTVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(intVec));
}

PrintfArg::PrintfArg(const std::vector<uint8_t>& vec)
    : m_typeInfo(AT_UINTVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(uintVec));
}

PrintfArg::PrintfArg(const std::vector<int16_t>& vec)
    : m_typeInfo(AT_INTVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(intVec));
}

PrintfArg::PrintfArg(const std::vector<uint16_t>& vec)
    : m_typeInfo(AT_UINTVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(uintVec));
}

PrintfArg::PrintfArg(const std::vector<int32_t>& vec)
    : m_typeInfo(AT_INTVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(intVec));
}

PrintfArg::PrintfArg(const std::vector<uint32_t>& vec)
    : m_typeInfo(AT_UINTVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(uintVec));
}

PrintfArg::PrintfArg(const std::vector<int64_t>& vec)
    : m_typeInfo(AT_INTVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(intVec));
}

PrintfArg::PrintfArg(const std::vector<uint64_t>& vec)
    : m_typeInfo(AT_UINTVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(uintVec));
}

PrintfArg::PrintfArg(const std::vector<float>& vec)
    : m_typeInfo(AT_FLOATVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(floatVec));
}

PrintfArg::PrintfArg(const std::vector<double>& vec)
    : m_typeInfo(AT_FLOATVEC), ptr(nullptr)
    , int_arg(0), uint_arg(0), float_arg(0.0)
{
  std::copy(vec.begin(), vec.end(), std::back_inserter(floatVec));
}

std::string PrintfArg::toString() const
{
  std::ostringstream oss;
  switch ( m_typeInfo ) {
    case AT_PTR: {
      oss << ptr;
      break;
    }
    case AT_STR: {
      oss << str;
      break;
    }
    case AT_INT: {
      oss << int_arg;
      break;
    }
    case AT_UINT: {
      oss << uint_arg;
      break;
    }
    case AT_FLOAT: {
      oss << float_arg;
      break;
    }
    case AT_INTVEC: {
      oss << "{";
      for (size_t i = 0; i < intVec.size(); ++i) {
        oss << intVec[i];
        if (i < intVec.size()-1) { oss << ","; }
      }
      oss << "}";
      break;
    }
    case AT_UINTVEC: {
      oss << "{";
      for (size_t i = 0; i < uintVec.size(); ++i) {
        oss << uintVec[i];
        if (i < uintVec.size()-1) { oss << ","; }
      }
      oss << "}";
      break;
    }
    case AT_FLOATVEC: {
      oss << "{";
      for (size_t i = 0; i < floatVec.size(); ++i) {
        oss << floatVec[i];
        if (i < floatVec.size()-1) { oss << ","; }
      }
      oss << "}";
      break;
    }
  }
  return oss.str();
}


/////////////////////////////////////////////////////////////////////////


FormatString::FormatString(const std::string& format)
    : m_format(format)
    , m_valid(false)
{
  parse(format);
}


FormatString::~FormatString()
{
  m_format = "";
  m_valid = false;
  m_specVec.clear();
  m_splitFormatString.clear();
}

void FormatString::getSpecifiers(std::vector<ConversionSpec>& specVec) const
{
  specVec = m_specVec;
}

void FormatString::getSplitFormatString(std::vector<std::string>& splitVec) const
{
  splitVec = m_splitFormatString;
}

/*static*/
size_t FormatString::findNextConversion(const std::string& format, size_t pos)
{
  // Return position of the next '%' conversion specifier in the string
  bool done = false;
  size_t retval = std::string::npos;
  do {
    retval = format.find('%', pos);
    if ( retval == std::string::npos ) {
      return retval;
    }
    if ( (retval+1) < format.length() ) {
      if ( format[retval+1] == '%' ) {
        // Skip %% - just make part of the normal string
        pos = retval + 2;
      }
      else {
        done = true;
      }
    }
    else {
      done = true;
    }
  } while ( !done ) ;
  return retval;
}

/*static*/
size_t FormatString::findConversionEnd(const std::string& format, size_t pos)
{
  size_t retval = std::string::npos;
  size_t conversionEnd = pos + 1;
  bool validConversionEndFound = false;
  while ( conversionEnd < format.length() ) {
    std::string endOfFormatStr = "diouxXfFeEgGaAcsp";
    char c = format[conversionEnd];
    if ( endOfFormatStr.find(c, 0) != std::string::npos ) {
      validConversionEndFound = true;
      break;
    }
    else {
      ++conversionEnd;
    }
  }
  if ( validConversionEndFound ) {
    retval = conversionEnd;
  }
  return retval;
}

/*static*/
void FormatString::replacePercent(std::string& str)
{
  // Replace all instances of %% with % in a string
  size_t pos = str.find("%%");
  while ( pos != std::string::npos ) {
    str.replace(pos, 2, "%");
    pos = str.find("%%");
  }
}


void FormatString::parse(const std::string& format)
{
  m_specVec.clear();
  m_splitFormatString.clear();
  m_valid = true;

  size_t conversionBegin = findNextConversion(format, 0);
  size_t len = conversionBegin;
  std::string str = format.substr(0, len);
  replacePercent(str);
  m_splitFormatString.push_back(str);

  while (conversionBegin != std::string::npos) {
    // Now skip to end of conversion specifier identified by next instance of
    // [diouxXfFeEgGaAcsp]
    size_t conversionEnd = findConversionEnd(format, conversionBegin);

    if ( conversionEnd != std::string::npos ) {
      // Now conversionEnd is pointing at the end of the conversion and conversionBegin
      // is pointing at the beginning. Extract the conversion spec and store it.
      len = conversionEnd - conversionBegin + 1;
      std::string specStr = format.substr(conversionBegin, len);
      ConversionSpec spec(specStr);
      m_specVec.push_back(spec);

      // Find beginning of next conversion spec (if there is one) else std::string::npos
      conversionBegin = findNextConversion(format, conversionEnd+1);

      if ( conversionBegin == std::string::npos ) {
        // Last iteration - store end of string past conversion specifier
        auto sstr = format.substr(conversionEnd+1, std::string::npos);
        replacePercent(sstr);
        m_splitFormatString.push_back(sstr);
      }
      else {
        len = conversionBegin - conversionEnd - 1;
        auto sstr = format.substr(conversionEnd+1, len);
        replacePercent(sstr);
        m_splitFormatString.push_back(sstr);
      }
    }
    else {
      // Illegal - must be an end to a conversion
      m_valid = false;
      m_specVec.clear();
      m_splitFormatString.clear();
      return;
    }
  }
}

void FormatString::dbgDump(std::ostream& str) const
{
  str << "FormatString Dump:\n";
  str << "  m_format = " << m_format << "\n";
  str << "  m_valid  = " << m_valid << "\n";
  size_t idx = 0;
  str << "  STRING    : " << m_splitFormatString[idx] << "\n";
  for (idx = 1; idx < m_splitFormatString.size(); ++idx) {
      str << "  CONVERSION: %" << std::string(1, m_specVec[idx-1].m_specifier) << "\n";
      str << "  STRING    : " << m_splitFormatString[idx] << "\n";
  }
  str << "\n";
}

/////////////////////////////////////////////////////////////////////////

BufferPrintf::BufferPrintf()
  : m_currentOffset(0)
{
}

BufferPrintf::BufferPrintf(const MemBuffer& buf, const StringTable& table)
  : m_currentOffset(0)
{
  setBuffer(buf);
  setStringTable(table);
}

BufferPrintf::~BufferPrintf()
{
  m_currentOffset = 0;
  m_buf.clear();
  m_stringTable.clear();
}

BufferPrintf::BufferPrintf(const uint8_t* buf, size_t bufLen, const StringTable& table)
  : m_currentOffset(0)
{
  setBuffer(buf, bufLen);
  setStringTable(table);
}

void BufferPrintf::setBuffer(const uint8_t* buf, size_t bufLen)
{
  m_buf.resize(bufLen);
  std::copy(buf, buf+bufLen, m_buf.begin());
}

void BufferPrintf::setBuffer(const MemBuffer& buf)
{
  size_t bufLen = buf.size();
  // Currently bufLen must be 64-bit aligned
  if ( (bufLen % 8) != 0 ) {
    throwError("setBuffer - bufLen is not a multiple of 8 bytes");
  }
  m_buf.resize(bufLen);
  std::copy(buf.begin(), buf.end(), m_buf.begin());
}

void BufferPrintf::setStringTable(const StringTable& table)
{
  m_stringTable = table;
}

void BufferPrintf::print(std::ostream& os)
{
  moveToFirstRecord();
  while ( hasNextRecord() ) {
    std::string formatStr = getFormat();
    FormatString format(formatStr);
    if ( format.isValid() ) {
      std::vector<XCL::Printf::ConversionSpec> conversionVec;
      format.getSpecifiers(conversionVec);
      std::vector<PrintfArg> argVec;
      int argOffset = getFormatByteCount();
      for (auto& conversion : conversionVec ) {
        PrintfArg arg = buildArg(m_currentOffset + argOffset, conversion);
        argVec.push_back(arg);
        argOffset += getElementByteCount(conversion) * conversion.m_vectorSize;
        // HACK: Special handling for vec3 packed strangely from compiler
        //    float3 += 32 bits
        //    others += 64 bits
        if ( conversion.isVector() && conversion.m_vectorSize == 3) {
          if ( conversion.isFloatClass() ) {
            argOffset += 4;
          }
          else {
            argOffset += 8;
          }
        }
      }
      std::string result = string_printf(formatStr, argVec);
      os << result;
    }
    nextRecord();
  }
}

void BufferPrintf::dbgDump(std::ostream& os) const
{
  char tmpbuf[8];
  IOS_FlagRestore ios_flagRestore(os);
  os << "------- BUFFER DEBUG DUMP --------\n";
  os << "String table:" << "\n";
  for (auto& iter : m_stringTable ) {
    os << iter.first << "=" << escape(iter.second) << "\n";
  }
  os << "\nBuffer Contents:" << "\n";
  os << "ADDR    [0]                         [7]" << "\n";
  for ( size_t idx = 0; idx < m_buf.size(); ++idx ) {
    if ( idx > 0 && ((idx % 8) == 0) ) {
      os << "\n";
    }
    if ( (idx % 8) == 0 ) {
      os << std::dec << std::left << idx << ":\t";
    }

    std::sprintf(tmpbuf, "%02X", (int)m_buf[idx]);
    os << tmpbuf << "  ";
  }
  os << "\n";
  os << "----- END BUFFER DEBUG DUMP ------\n";
}

/*static*/
std::string BufferPrintf::escape(const std::string& s)
{
  std::string retval;
  for (char c : s ) {
    switch (c) {
      case '\\': {
        retval += "\\\\";
        break;
      }
      case '\r': {
        retval += "\\r";
        break;
      }
      case '\n': {
        retval += "\\n";
        break;
      }
      case '\t': {
        retval += "\\t";
        break;
      }
      default: {
        retval += std::string(1, c);
      }
    }
  }
  return retval;
}

/*static*/
int BufferPrintf::getElementByteCount(const ConversionSpec& conversion)
{
  if (conversion.isVector() && conversion.isFloatClass()) {
    return 4;
  }
  return 8;
}

int BufferPrintf::nextRecordOffset(int currentOffset) const
{
  // Given a currentOffset, return a pointer to the next valid record
  // If we are already at the start of a valid record, just return
  // the current offset. If we are not at the beginning, step across
  // to the next record
  int segmentSize = getWorkItemPrintfBufferSize();
  int offset = currentOffset;
  if ( offset < 0 ) {
    return -1;
  }
  if ( offset >= (int)m_buf.size() ) {
    return -1;
  }
  // format entry of 0xFFFFFFFFFFFFFFFF or 0x0 means this work item is finished
  uint64_t val = extractField(offset, 8);
  bool endOfWorkItem = (val == 0xFFFFFFFFFFFFFFFF ) || (val == 0x0000000000000000);
  if ( endOfWorkItem ) {
    offset = ((offset+segmentSize-1) / segmentSize) * segmentSize;
    if ( offset >= (int)m_buf.size() ) {
      return -1;
    }
    val = extractField(offset, 8);
    endOfWorkItem = (val == 0xFFFFFFFFFFFFFFFF ) || (val == 0x0000000000000000);
    while ( endOfWorkItem ) {
      // Round up to next segment - in this loop we are guaranteed the
      // offset is aligned on a segmentSize segment start so just quickly
      // step one to the next looking for a valid format field to print.
      offset += segmentSize;
      if ( offset >= (int)m_buf.size() ) {
        return -1;
      }
      val = extractField(offset, 8);
      endOfWorkItem = (val == 0xFFFFFFFFFFFFFFFF ) || (val == 0x0000000000000000);
    }
  }
  return offset;
}

bool BufferPrintf::hasNextRecord() const
{
  bool eof = (nextRecordOffset(m_currentOffset) == -1);
  return ( !eof );
}


void BufferPrintf::moveToFirstRecord()
{
  if ( hasNextRecord() ) {
    m_currentOffset = nextRecordOffset(0);
  }
}

void BufferPrintf::nextRecord()
{
  // Assume we are pointing at a valid record and want to move to the next one
  if (! hasNextRecord() ) {
    throwError("nextRecord - No next record");
  }

  std::string formatStr = getFormat();
  FormatString format(formatStr);
  if ( !format.isValid() ) {
    std::string msg = "nextRecord - Invalid format: ";
    msg += formatStr;
    throwError(msg);
  }
  std::vector<ConversionSpec> conversionVec;
  format.getSpecifiers(conversionVec);
  // skip format ID
  m_currentOffset += getFormatByteCount();
  // Skip all arguments
  for (auto& conversion : conversionVec) {
    m_currentOffset += getElementByteCount(conversion) * conversion.m_vectorSize;
    // HACK: Special handling for vec3 packed strangely from compiler
    //    float3 += 32 bits
    //    others += 64 bits
    if ( conversion.isVector() && conversion.m_vectorSize == 3) {
      if ( conversion.isFloatClass() ) {
        m_currentOffset += 4;
      }
      else {
        m_currentOffset += 8;
      }
    }
  }
  m_currentOffset = nextRecordOffset(m_currentOffset);
}

std::string BufferPrintf::getFormat() const
{
  uint32_t id = getFormatID();
  std::string retval;
  lookup(id, retval);
  return retval;
}

uint32_t BufferPrintf::getFormatID() const
{
  uint32_t id = (uint32_t)extractField(m_currentOffset, getFormatByteCount());
  return id;
}

void BufferPrintf::lookup(int id, std::string& retval) const
{
  auto found = m_stringTable.find(id);
  if ( found != m_stringTable.end() ) {
    retval = found->second;
  }
  else {
    std::ostringstream oss;
    oss << "BufferPrintf lookup() - id " << id << " does not exist in the string table";
    throwError(oss.str());
  }
}

uint64_t BufferPrintf::extractField(int idx, int byteCount) const
{
  uint64_t val = 0;
  for (int i = byteCount-1; i >= 0; --i) {
    val <<= 8;
    val |= m_buf[idx+i];
  }
  return val;
}

PrintfArg BufferPrintf::buildArg(int bufIdx, ConversionSpec& conversion) const
{
  int elementBytes = getElementByteCount(conversion);
  if ( conversion.isIntClass() ) {
    if ( conversion.isVector() ) {
      std::vector<uint64_t> vec;
      for ( int i = 0; i < conversion.m_vectorSize; ++i ) {
        uint64_t val = extractField(bufIdx + i*elementBytes, elementBytes);
        vec.push_back(val);
      }
      PrintfArg arg(vec);
      return arg;
    }
    else {
      uint64_t val = extractField(bufIdx, elementBytes);
      PrintfArg arg(val);
      return arg;
    }
  }
  else if ( conversion.isFloatClass() ) {
    if ( conversion.isVector() ) {
      std::vector<float> vec;
      for ( int i = 0; i < conversion.m_vectorSize; ++i ) {
        float val = 0;
        uint8_t *ptr = (uint8_t*)&val;
        for ( int ii = elementBytes-1; ii >= 0; --ii ) {
          ptr[ii] = m_buf[bufIdx+ii+(i*elementBytes)];
        }
        vec.push_back(val);
      }
      PrintfArg arg(vec);
      return arg;
    }
    else {
      double val = 0;
      uint8_t *ptr = (uint8_t*)&val;
      for ( int i = elementBytes-1; i >= 0; --i ) {
        ptr[i] = m_buf[bufIdx+i];
      }
      PrintfArg arg(val);
      return arg;
    }
  }
  else if ( conversion.isStringClass() ) {
    /*uint64_t idx = */ (void)extractField(bufIdx, elementBytes);
    std::string str = "";
    // Temporary error - remove when %s works
    std::cout << std::endl << "ERROR: Printf conversion specifier '%s' is not allowed" << std::endl;
//    lookup(idx, str);
    PrintfArg arg(str);
    return arg;
  }
  // TODO: probably best to throw an exception here...
  PrintfArg arg(0);
  return arg;
}


/////////////////////////////////////////////////////////////////////////

std::string convertArg(const PrintfArg& arg, const ConversionSpec& conversion)
{
  std::string retval = "";
  char formatStr[32];
  strcpy(formatStr, "%");
  if (conversion.m_leftJustify)
    strcat(formatStr, "-");
  if (conversion.m_signPlus)
    strcat(formatStr, "+");
  if (conversion.m_prefixSpace)
    strcat(formatStr, " ");
  if (conversion.m_alternative)
    strcat(formatStr, "#");
  if (conversion.m_padZero)
    strcat(formatStr, "0");
  if (conversion.m_fieldWidth) {
    char *buf = formatStr + strlen(formatStr);
    sprintf(buf, "%d", conversion.m_fieldWidthValue);
  }
  if (conversion.m_precision) {
    char *buf = formatStr + strlen(formatStr);
    sprintf(buf, ".%d", conversion.m_precisionValue);
  }
  switch ( conversion.m_lengthModifier ) {
    case ConversionSpec::CS_CHAR: {
      strcat(formatStr, "hh");
      break;
    }
    case ConversionSpec::CS_SHORT: {
      strcat(formatStr, "h");
      break;
    }
    case ConversionSpec::CS_INT_FLOAT: {
      // TODO: Vec Only...
      //strcat(formatStr, "hl");
      break;
    }
    case ConversionSpec::CS_LONG: {
      // HACK: LONG only supported for non vectors now...
      if ( conversion.m_vectorSize == 1 ) {
        strcat(formatStr, "l");
      }
      break;
    }
    default:
      break;
  }

  strcat(formatStr, " ");
  formatStr[strlen(formatStr)-1] = conversion.m_specifier;
  // TODO: later make this dynamically size... for now 1024 should be sufficient
  int bufLen = 1024;
  char *printBuf = new char[bufLen];
  switch ( arg.m_typeInfo ) {
    case PrintfArg::AT_PTR: {
      snprintf(printBuf, bufLen, formatStr, arg.ptr);
      retval = printBuf;
      break;
    }
    case PrintfArg::AT_STR: {
      snprintf(printBuf, bufLen, formatStr, arg.str.c_str());
      retval = printBuf;
      break;
    }
    case PrintfArg::AT_INT: {
      snprintf(printBuf, bufLen, formatStr, arg.int_arg);
      retval = printBuf;
      break;
    }
    case PrintfArg::AT_UINT: {
      snprintf(printBuf, bufLen, formatStr, arg.uint_arg);
      retval = printBuf;
      break;
    }
    case PrintfArg::AT_FLOAT: {
      snprintf(printBuf, bufLen, formatStr, arg.float_arg);
      retval = printBuf;
      break;
    }
    case PrintfArg::AT_INTVEC: {
      size_t comma = 0;
      for (auto val : arg.intVec) {
        if (comma++) retval += ",";
        snprintf(printBuf, bufLen, formatStr, val);
        retval += printBuf;
      }
      break;
    }
    case PrintfArg::AT_UINTVEC: {
      size_t comma = 0;
      for (auto val : arg.uintVec) {
        if (comma++) retval += ",";
        snprintf(printBuf, bufLen, formatStr, val);
        retval += printBuf;
      }
      break;
    }
    case PrintfArg::AT_FLOATVEC: {
      size_t comma = 0;
      for (auto val : arg.floatVec) {
        if (comma++) retval += ",";
        snprintf(printBuf, bufLen, formatStr, val);
        retval += printBuf;
      }
      break;
    }
  }
  delete[] printBuf;
  return retval;
}

std::string string_printf(const std::string& formatStr, const std::vector<PrintfArg>& args)
{
  std::vector<ConversionSpec> specVec;
  std::vector<std::string> splitVec;
  FormatString formatString(formatStr);
  if ( formatString.isValid() == false ) {
    std::ostringstream oss;
    oss << "Error - invalid format string '" << formatStr;
    throwError(oss.str());
    return "";
  }
  formatString.getSplitFormatString(splitVec);
  formatString.getSpecifiers(specVec);

  if ( args.size() != specVec.size() ) {
    std::ostringstream oss;
    oss << "Error - Format string conversion specifier count " << specVec.size() << " does not match argument count of " << args.size();
    throwError(oss.str());
    return "";
  }

  std::ostringstream oss;
  if ( splitVec.size() > 0 ) {
    oss << splitVec[0];
  }
  for ( size_t idx = 1; idx < splitVec.size(); ++idx ) {
    auto& arg = args[idx-1];
    ConversionSpec& conversion = specVec[idx-1];
    oss << convertArg(arg, conversion);
    oss << splitVec[idx];
  }
  std::string retval = oss.str();
  return retval;
}

void throwError(const std::string& errorMsg)
{
  throw std::runtime_error(errorMsg);
}

unsigned int getWorkItemPrintfBufferSize()
{
  return 2048;
}

size_t getPrintfBufferSize(const std::array<size_t,3>& globalSize, const std::array<size_t,3>& localSize)
{
  static bool msg_printed = false;
  size_t totalLocal = 1;
  size_t totalGlobal = 1;
  for ( auto g : globalSize ) {
    totalGlobal *= g;
  }
  for ( auto l : localSize ) {
    totalLocal *= l;
  }
  size_t workgroupCount = std::max(static_cast<size_t>(1), (totalGlobal / totalLocal) );

  size_t workgroupBufferSize = totalLocal * getWorkItemPrintfBufferSize();
  size_t retval = workgroupCount * workgroupBufferSize;
  char *p_bufEnv = getenv("XCL_PRINTF_BUFFER_SIZE");
  if (p_bufEnv != NULL) {
    retval = atol(p_bufEnv);
  }
  if ( getenv("XCL_PRINTF_DEBUG") ) {
    std::cout << "DEBUG: Workgroup_Count=" << workgroupCount << "  Workgroup_Buffer_Size=" << workgroupBufferSize << std::endl;
    std::cout << "DEBUG: Global_Size=" << totalGlobal << "  Local_Size=" << totalLocal << std::endl;
    std::cout << "DEBUG: XCL_PRINTF_BUFFER_SIZE=" << retval << std::endl;
    if ( !msg_printed ) {
      msg_printed = true;
    }
  }
  return retval;
}


} // namespace Printf
} //namespace XCL

/////////////////////////////////////////////////////////////////////////
