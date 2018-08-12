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

// ============================================================================
// COPYRIGHT NOTICE
// (c) Copyright 2017 Xilinx, Inc. All rights reserved.
//
// File Name: xclbinutil.cxx
// ============================================================================

#include "xclbinutil.h"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>
#include <cinttypes>

std::string
XclBinUtil::getCurrentTimeStamp()
{
  time_t rawtime;
  struct tm* timeinfo;
  char buffer[ 100 ];
  time( & rawtime );
  timeinfo = localtime( & rawtime );
  strftime( buffer, sizeof( buffer ), "%d-%m-%Y %I:%M:%S", timeinfo );
  return std::string( buffer );
}

std::string
XclBinUtil::getBaseFilename( const std::string &_fullPath )
{
  std::string filename = _fullPath;

  const size_t rightMostSlash = filename.find_last_of( "\\/" );
  if ( rightMostSlash != std::string::npos )
    filename.erase( 0, rightMostSlash + 1 );

  const size_t rightMostDot = filename.rfind( '.' );
  if ( rightMostDot != std::string::npos )
    filename.erase( rightMostDot );

  return filename;
}

bool
XclBinUtil::cmdLineSearch( int argc, char** argv, const char* check )
{
  for ( int i = 1 ; i < argc ; ++i )
    if ( std::strcmp( argv[i], check ) == 0 )
      return true;
  return false;
}

bool
XclBinUtil::stringEndsWith( const char* str, const char* ending )
{
  if (( str == nullptr ) || ( ending == nullptr))
    return false;

  const size_t strLength = std::strlen( str );
  const size_t endLength = std::strlen( ending );

  if ( endLength > strLength )
    return false;

  const char* endOfStr = str + strLength - endLength;
  if ( std::strncmp( endOfStr, ending, endLength ) == 0 )
    return true;

  return false;
}

// Mapping Arguments, enables support for legacy switches...
// xclbincat -bitstream <bitstreambinaryfile.bin> [-clearstream <clearstreambinaryfile.bin>] <map.xml> <outputfile.xclbin>
// xclbincat -nobitstream <sharedlib.so> <map.xml> <outputfile.xclbin>
// xclbincat -clearstream <clearstreambinaryfile.bin> <outputfile.dsabin>
// xclbincat -bitstream <bitstreambinaryfile.bin> -clearstream <clearstreambinaryfile.bin> <outputfile.dsabin>
void
XclBinUtil::mapArgs(
      std::map< std::string, std::string > & decoder,
      int argc,
      char** argv,
      std::vector< std::string > & newArgv )
{
  for ( int i = 0; i < argc; i++ ) {
    std::string newArg = argv[ i ];
    if ( decoder.find( newArg ) != decoder.end() )
      newArg = decoder[ newArg ];
    if ( newArg.empty() )
      continue; // skip adding (delete) if the decoded value is empty
    newArgv.push_back( newArg );
  }
}

std::ostream &
XclBinUtil::data2hex(
    std::ostream & s,
    const unsigned char* value,
    size_t size )
{
  // Save and restore the original state of the incoming ostream, because we manipulate it
  std::ios originalState(nullptr);
  originalState.copyfmt(s);
  // Print 2 hex chars for every binary char, and reverse the order on char (byte) boundaries...
  for ( size_t i = 0; i < size; i++ )
    s << std::hex << std::setw(2) << std::setfill('0') << (int)(value[size - i - 1]) << std::dec;
  s.copyfmt(originalState);
  return s;
}

unsigned char
XclBinUtil::hex2char( const unsigned char* hex )
{
  unsigned char byte = *hex;
  if      ( byte >= '0' && byte <= '9' ) byte = byte - '0';
  else if ( byte >= 'a' && byte <= 'f' ) byte = byte - 'a' + 10;
  else if ( byte >= 'A' && byte <= 'F' ) byte = byte - 'A' + 10;
  return byte;
}


std::ostream &
XclBinUtil::hex2data(
    std::ostream & s,
    const unsigned char* value,
    size_t hexSize )
{
  size_t size = hexSize / 2;
  for ( size_t i = 0; i < size; i++ ) {
    unsigned char byte = hex2char( &value[ 2 * i ] );
    unsigned char nibble = hex2char( &value[ 2 * i + 1 ] );
    byte = (byte << 4) | (nibble & 0xF); // Shift the 1st nibble over and add the second nibble
    s.write( (const char*)&byte, sizeof(byte) );
  }
  return s;
}

uint64_t
XclBinUtil::stringToUInt64( std::string _sInteger)
{
  uint64_t value = 0;

  // Is it a hex value
  if ( ( _sInteger.length() > 2) &&
       ( _sInteger[0] == '0' ) &&
       ( _sInteger[1] == 'x' ) ) {
    if ( 1 == sscanf(_sInteger.c_str(), "%" PRIx64 "", &value ) ) {
      return value;
    }
  } else {
    if ( 1 == sscanf(_sInteger.c_str(), "%" PRId64 "", &value ) ) {
      return value;
    }
  }

  std::string errMsg = "ERROR: Invalid integer string in JSON file: '" + _sInteger + "'";
  throw std::runtime_error(errMsg);
}
