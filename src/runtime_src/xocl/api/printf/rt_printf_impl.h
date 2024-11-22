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

#ifndef __XILINX_RT_PRINTF_IMPL_H
#define __XILINX_RT_PRINTF_IMPL_H

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <stdint.h>


/////////////////////////////////////////////////////////////////////////
// rt_printf_impl.h 
//
// Printf implementation classes and functions. These are lower level
// printf classes that actually implement the format string parsing,
// conversion specifiers, and argument conversions based on the
// specifiers.
//
// Keep dependencies here only to standard c++ - this will get shared
// with the compiler side for format string parsing later.
/////////////////////////////////////////////////////////////////////////

namespace XCL {
namespace Printf {

/////////////////////////////////////////////////////////////////////////
// ConversionSpec - 
// This is a helper class that keeps track of everything in a single printf
// converson spec 
struct ConversionSpec {
    enum LengthModifier {
      CS_NONE,       // No modifier specified
      CS_CHAR,       // hh
      CS_SHORT,      // h
      CS_INT_FLOAT,  // hl *vec only*
      CS_LONG        // l
    };

    bool m_validSpec;
    char m_specifier; // long:d,i,o,u,x,X  double:f,F,e,E,g,G,a,A  char:c  char*:s ptr:p
    LengthModifier m_lengthModifier;  // %ld
    bool m_fieldWidth;    // %4d
    int m_fieldWidthValue;// %4d
    bool m_leftJustify;   // %-d
    bool m_padZero;       // %0d
    bool m_signPlus;      // %+d
    bool m_prefixSpace;   // % +d
    bool m_alternative;   // %#
    bool m_precision;     // %0.2f 
    int m_precisionValue; // %0.2f
    int m_vectorSize;     // 1,2,3,4,8,16  1=scalar, 2-16=vector

public:
    ConversionSpec();
    ConversionSpec(const std::string& str);
    ~ConversionSpec();

    // True if this represents a float, double, or vector of them
    // Basically any of %[fFeEgGaA]
    bool isFloatClass() const;

    // True if this represents a pointer, char, int, long, or vector 
    // of int or vector of float.
    // Basically any of %[cdiouxXp]
    bool isIntClass() const;

    // True if this represents a char*  (%s)
    bool isStringClass() const;

    // True if this is a vector of of floats or ints
    // %[2,3,4,8,16]v
    bool isVector() const;

    // percent is supported but not used as a conversion - it is
    // converted at the site into a % in the string
    bool isPercent() const;

    // You have a serious problem is this is not true...
    bool isValid() const { return m_validSpec; }

    void dbgDump(std::ostream& str = std::cout) const;

private:
    void setDefaults();
    void parse(const std::string& str);
    int parseNumber(const char** p_buf);
};

/////////////////////////////////////////////////////////////////////////
// Printf format string parser
//
// An opencl printf format string is in the following format:
//     Flags:                     [-+ 0]*
//     Field Width:               ([1-9][0-9]*)?
//     Precision:                 (.[0-9]*)?
//     Vector Specifier:          (v2|v3|v4|v8|v16)?
//     Length Modifier:           [hh||h|l]? | [hh|h|hl|l]  
//          *                     ^^^Scalar    ^^Vector (reqd)
//     Conversion Specifier:      [diouxXfFeEgGaAcsp%]
//
//     Special conversion specifier '%' must appear alone (as %% in
//     format string).
//
//     Length Modifier is required with vectors, optional with scalar types.
//
// IN:  Printf Format String
//
// OUT: Argument count
//      Conversion Specifier Vector
//      Split format string (split at conversion specifier locations)
//
class FormatString {

public:
   FormatString(const std::string& format);
   ~FormatString();

   // Return all conversion specifiers from the format string
   // NOTE: %% does not count as a specifier because it is automatically
   // rolled into the string
   void getSpecifiers(std::vector<ConversionSpec>& specVec) const;

   // Returns the format string split at the conversion specifier
   // locations - %% does not cause a split.
   //    splitStr.size() == specVec.size() + 1
   void getSplitFormatString(std::vector<std::string>& splitStr) const;

   bool isValid() const { return m_valid; }
   void dbgDump(std::ostream& str = std::cout) const;

private:
    void parse(const std::string& format);
    static size_t findNextConversion(const std::string& format, size_t pos);
    static size_t findConversionEnd(const std::string& format, size_t pos);
    static void replacePercent(std::string& str);

private:
    std::string m_format;
    bool m_valid;
    std::vector<ConversionSpec> m_specVec;
    std::vector<std::string> m_splitFormatString;

};

/////////////////////////////////////////////////////////////////////////
// A decoded printf argument. This is just a convenient way to quickly 
// store anything that a printf argument is allowed to be. Arguments are 
// promoted and stored in this structure.
// Probably a more efficient way to represent this union later like
// a boost variant.
//
// NOTE: This is not 100% to spec for vectors - the spec indicates there
// should be no promotion but we are promoting to the max 64 bit
// representation for vectors. I don't expect this to cause any issues
// but technically should change later when other infrastructure is
// complete...
struct PrintfArg 
{
    enum TypeInfo {
      AT_PTR, AT_STR, AT_INT, AT_UINT, AT_FLOAT, AT_INTVEC, AT_UINTVEC, AT_FLOATVEC
    };

    PrintfArg(void *val);
    PrintfArg(const std::string& val);
    PrintfArg(int8_t val);
    PrintfArg(uint8_t val);
    PrintfArg(int16_t val);
    PrintfArg(uint16_t val);
    PrintfArg(int32_t val);
    PrintfArg(uint32_t val);
    PrintfArg(int64_t val);
    PrintfArg(uint64_t val);
    PrintfArg(double val);
    PrintfArg(const std::vector<int8_t>& vec);
    PrintfArg(const std::vector<uint8_t>& vec);
    PrintfArg(const std::vector<int16_t>& vec);
    PrintfArg(const std::vector<uint16_t>& vec);
    PrintfArg(const std::vector<int32_t>& vec);
    PrintfArg(const std::vector<uint32_t>& vec);
    PrintfArg(const std::vector<int64_t>& vec);
    PrintfArg(const std::vector<uint64_t>& vec);
    PrintfArg(const std::vector<float>& vec);
    PrintfArg(const std::vector<double>& vec);

    TypeInfo m_typeInfo;
    void *ptr;
    int64_t int_arg;
    uint64_t uint_arg;
    double float_arg;
    std::string str;
    std::vector<int64_t> intVec;
    std::vector<uint64_t> uintVec;
    std::vector<double> floatVec;

    std::string toString() const;
};

/////////////////////////////////////////////////////////////////////////
// BufferPrintf -
//
// Text printf output from a packed memory buffer of printf arguments.
// This takes a memory buffer and string table as input and prints
// the resulting text printf output to a stream.
//
// String table entries are numbered from 1..N (0 is reserved and
// 0xFFFFFFFFFFFFFFFF is reserved)
//
// Printf buffer records repeat in the following format for each
// printf to execute on the host:
//     FIELD       BITS   DESCRIPTION
//     Format_ID   64     ID of the format string in table
//     Arguments   N*64   Arguments, N is number of aguments
//     ...
//     Format_ID   64     ID of the format string in table
//     Arguments   N*64   Arguments, N is number of aguments
//     0xFF filling to end of buffer
//
// TODO: - Exception when invalid format_id is found in record
//
class BufferPrintf {

public:
    typedef std::vector<uint8_t> MemBuffer;
    typedef std::map<uint32_t,std::string> StringTable;

public:
    BufferPrintf();
    BufferPrintf(const MemBuffer& buf, const StringTable& table);
    BufferPrintf(const uint8_t* buf, size_t bufSize, const StringTable& table);

    ~BufferPrintf();

    void setBuffer(const uint8_t* buf, size_t bufLen);
    void setBuffer(const MemBuffer& buf);

    void setStringTable(const StringTable& table);
    
    // Print buffer contents to the outputstream
    void print(std::ostream& os = std::cout);

    void dbgDump(std::ostream& os = std::cout) const;

public:
    // Returns the bytes one element of this conversion will take in the buffer
    //   4: float vector element
    //   8: everything else
    static int getElementByteCount(const ConversionSpec& conversion);

    // Number of bytes in a format string ID
    static int getFormatByteCount() { return 8; }

private:
    // Set the current offset to the first record
    void moveToFirstRecord();

    // Returns offset of next record or -1 if no more records found
    int nextRecordOffset(int currentOffset) const;

    // Returns true if we have another record, false if no more exist
    bool hasNextRecord() const;

    // Move m_currentOffset to the next record start. Assumes we are either
    // on a record start position -or- we are positioned on the gap between
    // records (because we advanced to the first byte after the last record
    // which lies in the gap between work item segments).
    void nextRecord();

    // Extracts the Format for the current record
    std::string getFormat() const;

    // Extracts the Format_ID for the current record
    uint32_t getFormatID() const;

    // Find an ID in the string table and return the string 
    void lookup(int id, std::string& retval) const;

    // Extract a value from buffer
    uint64_t extractField(int idx, int byteCount) const;

    // Build up a printf argument given the conversion specifier
    // and memory buffer and string table
    PrintfArg buildArg(int bufIdx, ConversionSpec& conversion) const;
    
    // Convert escape sequences \n, \r, \t, \ to text representation
    // Newline replaced by string: "\n"
    // Single slash replaced by string: "\\", etc etc
    // Used to print a string table without drawing special characters to the screen
    static std::string escape(const std::string& s);

private:
    // currentOffset always points at the current format string
    int m_currentOffset;
    MemBuffer m_buf;
    StringTable m_stringTable;
};


/////////////////////////////////////////////////////////////////////////
// UTILITY FUNCTIONS

// Perform a conversion given a single printf argument and return the string 
// representation of the result. This is called repeatedly for each arg
// during string_printf to build the complete output string.
std::string convertArg(PrintfArg& arg, ConversionSpec& conversion);

// Given format string and args, create and return a string (similar to sprintf). 
// This exercises the round trip internal printf and is used to test breaking down
// a format and printing arguments.
std::string string_printf(const std::string& formatStr, const std::vector<PrintfArg>& args);

// Throws an exception with the given error message. Put as a utility function 
// because I am not sure on the exception throwing and error reporting standards
// so for now I simply throw a std::runtime_exception.
void throwError(const std::string& errorMsg);

// Size of a local work item printf buffer - must match in compiler and runtime
unsigned int getWorkItemPrintfBufferSize();

// Returns the printf buffer size in bytes. By spec this should be 1MB/kernel for 
// the full profile. 
size_t getPrintfBufferSize(const std::array<size_t,3>& globalSize,
                           const std::array<size_t,3>& localSize);

/////////////////////////////////////////////////////////////////////////

} // namespace Printf
} //namespace XCL

#endif



