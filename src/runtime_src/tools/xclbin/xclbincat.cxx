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
// File Name: xclbincat.cxx
// ============================================================================

//#include "xclbincat0.h"
#include "xclbincat1.h"
#include "xclbinutil.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include <iostream>

int main_( int argc, char** argv )
{
  bool xclbin0 = XclBinUtil::cmdLineSearch( argc, argv, "-legacy_xclbin" );
  int returnValue = 0;
  if ( xclbin0 ) {
    std::cout << "** LEGACY XCLBINCAT FLOW IS NOT LONGER SUPPORTED: '" << argv[ 0 ] << "' **\n";
    return -1;

    // Legacy option mapping...
//    std::vector< std::string > newArgv;
//    std::map< std::string, std::string > decoder;
//    decoder.emplace( "-legacy_xclbin", "" ); // delete
//    XclBinUtil::mapArgs( decoder, argc, argv, newArgv );
//
//    std::vector< char* > newArgvChar;
//    for ( std::string & arg : newArgv )
//      newArgvChar.push_back( const_cast<char*>( arg.c_str() ) );
//
//    returnValue = xclbincat0::execute( newArgvChar.size(), &newArgvChar[0] );
  } else {
    // Legacy option mapping...
    std::vector< std::string > newArgv;
    std::map< std::string, std::string > decoder;
    decoder.emplace( "-xclbin1", "" ); // delete
    decoder.emplace( "-clearstream", "--clearstream" );
    decoder.emplace( "-bitstream", "--bitstream" );
    decoder.emplace( "-nobitstream", "--bitstream" );
    decoder.emplace( "-dwarfFile", "--debugdata" );
    decoder.emplace( "-ipiMappingFile", "--debugdata" );
    XclBinUtil::mapArgs( decoder, argc, argv, newArgv );

    std::vector< char* > newArgvChar;
    for ( std::string & arg : newArgv )
      newArgvChar.push_back( const_cast<char*>( arg.c_str() ) );

    returnValue = xclbincat1::execute( newArgvChar.size(), &newArgvChar[0] );
  }
  return returnValue;
}

int main( int argc, char** argv )
{
  try {
    return main_( argc, argv );
  } catch ( std::exception &e ) {
    std::string msg = e.what();
    if ( msg.empty() )
      std::cerr << "ERROR: Caught an internal exception no message information is available.\n";
    else
      std::cerr << "ERROR: Caught an internal exception...\n" << msg.c_str() << "\n";
  } catch ( ... ) {
    std::cerr << "ERROR: Caught an internal exception no exception information is available.\n";
  }
  return -1;
}



