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
// File Name: xclbinsplit1.cxx
// ============================================================================


#include "xclbinsplit1.h"

#include "xclbindata.h"
#include "xclbinutils.h"
#include <getopt.h>
#include <iostream>

namespace xclbinsplit1 {

  //Option Class...
  OptionParser::OptionParser()
    : m_output( "split" )
    , m_input( "a.xclbin" )
    , m_binaryHeader( "header" )
    , m_verbose( false )
    , m_help( false )
  {
  }

  OptionParser::~OptionParser()
  {
  }

  void
  OptionParser::printHelp( char* program )
  {
    std::cout << "Usage:   " << program << " [-option] a.xclbin\n";
    std::cout << "option:  -h/--help             Print help\n";
    std::cout << "         -n/--binaryheader     Specify binary header filename (e.g. -n header > header.bin)\n";
    std::cout << "         -o/--output           Specify output filename (e.g. -o test > test-primary.bit)\n";
    std::cout << "         -i/--input            Specify input filename (e.g. example.xclbin)\n";
    std::cout << "         -v/--verbose          Verbose messaging\n";
  }

  int
  OptionParser::parse( int argc, char** argv )
  {
    int optCode;
    int optionIndex = 0;
    static struct option longOptions[] = 
    {
      // Remember to update call to getopt_long
      {"help",          no_argument,        0, 'h'},
      {"verbose",       no_argument,        0, 'v'},
      {"binaryheader",  required_argument,  0, 'n'},
      {"output",        required_argument,  0, 'o'},
      {"input",         required_argument,  0, 'i'},
      {0,               0,                  0, 0}
    };

    while ( 1 )
    {
      optCode = getopt_long( argc, argv, "hvn:o:i:", longOptions, &optionIndex );
      
      if ( optCode == -1 )
        break;

      switch ( optCode )
      {
        case 0:
          // getopt_long will handle, just keep going...
          break;
        case 'h':
          m_help = true;
          return 0;
          break;
        case 'i':
          m_input = optarg;
          break;
        case 'n':
          m_binaryHeader = optarg;
          break;
        case 'o':
          m_output = optarg;
          break;
        case 'v':
          m_verbose = true;
          break;
        case ':': // Missing option
          std::cerr << "ERROR: '" << argv[ 0 ] << "' option '-" << optopt << "' requires an argument.\n";
          return 1;
          break;
        case '?': // Invalid option
        default:
          std::cerr << "ERROR: Unrecognized option.\n" ;
          printHelp( argv[0] );
          return 1;
      }
    }
    int positionalStart = optind;
    int positionalCount = ( argc - positionalStart );
    if ( positionalCount >= 2 ) {
      std::cerr << "ERROR: Too many positional arguments provided (" << positionalCount << ").\n";
      printHelp( argv[0] );
      return 1;
    }
    if ( positionalCount >= 1 ) // last positional arg is xclbin output file
      m_input = argv[ optind + (positionalCount - 1) ];

    // Additional checks...
    if ( m_input.empty() ) {
      std::cerr << "ERROR: Input argument must be provided (either 1st positional or with '-i').\n";
      printHelp( argv[0] );
      return 1;
    }

    return 0;
  }
  
  bool extract( const OptionParser& parser ) 
  {
    const char* file = parser.m_input.c_str();
    const char* name = parser.m_output.c_str();
    const char* header = parser.m_binaryHeader.c_str();
    bool verbose = parser.m_verbose;

    XclBinData data;
    if ( verbose )
      data.enableTrace();

    data.initRead(file);
    if ( verbose && ( ! data.report() ) ) {
      std::cerr << "ERROR: Failed to read '" << file << "'\n";
      return false;
    }
    if ( ! data.extractAll( name ) ) {
      std::cerr << "ERROR: Failed to extract '" << file << "'\n";
      return false;
    }
    if ( ! data.extractBinaryHeader( file, header ) ) {
      std::cerr << "ERROR: Failed to extract binary data from '" << file << "'\n";
      return false;
    }

    return true;
  }

  int execute( int argc, char** argv ) 
  {
    OptionParser parser;
    int code = parser.parse( argc, argv );

    if ( code != 0 ) 
      return code;

    if ( parser.m_help ) {
      parser.printHelp( argv[0] );
      return 0;
    }

    if ( parser.m_verbose ) {
      std::cout << "Command line: " << argv[ 0 ];
      for ( int i = 1; i < argc; i++ )
        std::cout << " " << argv[ i ];
      std::cout << "\n";
      std::cout << "STARTED '" << argv[ 0 ] << "' at: '" << XclBinUtil::getCurrentTimeStamp().c_str() << "'\n";
    }

    if( ! extract( parser ) )
      return -1;

    if ( parser.m_verbose )
      std::cout << "COMPLETED '" << argv[ 0 ] << "' at: '" << XclBinUtil::getCurrentTimeStamp().c_str() << "'\n";

    return 0;
  }

}


