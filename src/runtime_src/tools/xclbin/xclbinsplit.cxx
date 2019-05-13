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
// (c) Copyright 2015-2017 Xilinx, Inc. All rights reserved.
//
// File Name: xclbinsplit.cxx
// ============================================================================

//#include "xclbinsplit0.h"
#include "xclbinsplit1.h"
#include "xclbinutils.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

int main_( int argc, char** argv )
{
  bool xclbin0 = XclBinUtil::cmdLineSearch( argc, argv, "-legacy_xclbin" );
  int returnValue = 0;
  if ( xclbin0 ) {
    std::cout << "** LEGACY XCLBINSPLIT FLOW IS NO LONGER SUPPORTED: '" << argv[ 0 ] << "' **\n";
    return -1;

//    // Legacy option mapping...
//    std::vector< std::string > newArgv;
//    std::map< std::string, std::string > decoder;
//    decoder.emplace( "-legacy_xclbin", "" ); // delete
//    XclBinUtil::mapArgs( decoder, argc, argv, newArgv );
//
//    std::vector< char* > newArgvChar;
//    for ( std::string & arg : newArgv )
//      newArgvChar.push_back( const_cast<char*>( arg.c_str() ) );
//
//    returnValue = xclbinsplit0::execute( newArgvChar.size(), &newArgvChar[0] );
  } else {
    // Legacy option mapping...
    std::vector< std::string > newArgv;
    std::map< std::string, std::string > decoder;
    decoder.emplace( "-xclbin1", "" ); // delete
    XclBinUtil::mapArgs( decoder, argc, argv, newArgv );

    std::vector< char* > newArgvChar;
    for ( std::string & arg : newArgv )
      newArgvChar.push_back( const_cast<char*>( arg.c_str() ) );

    returnValue = xclbinsplit1::execute( newArgvChar.size(), &newArgvChar[0] );
  }
  return returnValue;
}

int main( int argc, char** argv )
{
  std::cout << std::endl;
  std::cout << "**** DEPRICATION WARNING ****"                                             << std::endl;
  std::cout << "xclbincat and xclbinsplit utilities are replaced by xclbinutil."           << std::endl; 
  std::cout << "You are recommended to use xclbinutil instead."                            << std::endl;
  std::cout << std::endl;
  std::cout << "The xclbincat and xclbinsplit utilities will be obsoleted and removed in " << std::endl; 
  std::cout << "the next software release."                                                << std::endl;
  std::cout << std::endl;

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



