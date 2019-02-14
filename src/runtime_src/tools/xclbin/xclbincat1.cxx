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

#include "xclbincat1.h"

#include "xclbin.h"
#include "xclbindata.h"
#include "xclbinutils.h"
#include <sys/stat.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <regex>
#include <vector>
 
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>          // for uuid
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>       // for to_string


namespace xclbincat1 {

  //Option Class...
  OptionParser::OptionParser()
    : m_help( false )
    , m_binaryHeader( "" )
    , m_output( "a.xclbin" )
    , m_verbose( false )
  {
  }

  OptionParser::~OptionParser()
  {
  }



  void
  OptionParser::printHelp( char* program )
  {
    std::cout << "Usage:   " << program << " [-option] [xclbin.xml] [a.xclbin]\n";
    std::cout << "option:  -h/--help             Print help\n" ;
    std::cout << "         -b/--bitstream    <file>         Add bitstream\n" ;
    std::cout << "         -c/--clearstream  <file>         Add clear bitstream\n" ;
    std::cout << "         -d/--debugdata    <file>         Add debug data\n" ;
    std::cout << "         -f/--firmware     <file>         Add firmware\n" ;
    std::cout << "         -k/--kvp          <key:value>    Set key-value pair (e.g. -k key:value)\n" ;
    std::cout << "         -m/--metadata     <file>         Add metadata (XML)\n" ;
    std::cout << "         -n/--binaryheader <file>         Add binary header file\n" ;
    std::cout << "         -r/--runtime_data <file>         Read 'rtd' formatted data segment(s)\n";
    std::cout << "         -o/--output                      Specify output filename (e.g. -o example.xclbin)\n" ;
    std::cout << "         -s/--segment_type <type> <file>  Specifies segment type and file. \n";
    std::cout << "                                          Valid segment types:  \n";
    std::cout << "                                             BITSTREAM, CLEAR_BITSTREAM, FIRMWARE, SCHEDULER,   \n";
    std::cout << "                                             BINARY_HEADER, METADATA, MEM_TOPOLOGY, CONNECTIVITY,\n"; 
    std::cout << "                                             IP_LAYOUT, DEBUG_IP_LAYOUT, MCS_PRIMARY, MCS_SECONDARY,\n";
    std::cout << "                                             BMC, and DEBUG_DATA.         \n";
  }



  typedef struct OptionParserSegmentTypeMapT {
    OptionParserSegmentTypeMapT( OptionParser::SegmentType _eSegmentType,
  			                         const std::string & _sSegmentType )
      : m_eSegmentType( _eSegmentType ), 
        m_sSegmentType( _sSegmentType ) {}
    OptionParser::SegmentType m_eSegmentType;
    std::string m_sSegmentType;
  } OptionParserSegmentTypeMap;
  
  static const OptionParserSegmentTypeMap OptionParserSTMap[] = {
    OptionParserSegmentTypeMap( OptionParser::ST_BITSTREAM, "BITSTREAM" ),
    OptionParserSegmentTypeMap( OptionParser::ST_CLEAR_BITSTREAM, "CLEAR_BITSTREAM" ),
    OptionParserSegmentTypeMap( OptionParser::ST_FIRMWARE, "FIRMWARE" ),
    OptionParserSegmentTypeMap( OptionParser::ST_SCHEDULER, "SCHEDULER" ),
    OptionParserSegmentTypeMap( OptionParser::ST_BINARY_HEADER, "BINARY_HEADER" ),
    OptionParserSegmentTypeMap( OptionParser::ST_META_DATA, "METADATA" ),
    OptionParserSegmentTypeMap( OptionParser::ST_MEM_TOPOLOGY, "MEM_TOPOLOGY" ),
    OptionParserSegmentTypeMap( OptionParser::ST_CONNECTIVITY, "CONNECTIVITY" ),
    OptionParserSegmentTypeMap( OptionParser::ST_IP_LAYOUT, "IP_LAYOUT" ),
    OptionParserSegmentTypeMap( OptionParser::ST_DEBUG_IP_LAYOUT, "DEBUG_IP_LAYOUT" ),
    OptionParserSegmentTypeMap( OptionParser::ST_DEBUG_DATA, "DEBUG_DATA" ),
    OptionParserSegmentTypeMap( OptionParser::ST_MCS_PRIMARY, "MCS_PRIMARY" ),
    OptionParserSegmentTypeMap( OptionParser::ST_MCS_SECONDARY, "MCS_SECONDARY" ),
    OptionParserSegmentTypeMap( OptionParser::ST_BMC, "BMC" ),
    OptionParserSegmentTypeMap( OptionParser::ST_BUILD_METADATA, "BUILD_METADATA" ),
    OptionParserSegmentTypeMap( OptionParser::ST_KEYVALUE_METADATA, "KEYVALUE_METADATA" ),
    OptionParserSegmentTypeMap( OptionParser::ST_USER_METADATA, "USER_METADATA" ),
    OptionParserSegmentTypeMap( OptionParser::ST_UNKNOWN, "UNKNOWN" )
  };

  OptionParser::SegmentType 
  OptionParser::getSegmentType(const std::string _sSegmentType)
  {
    for (const auto i : OptionParserSTMap ) {
      if ( (_sSegmentType.length() == i.m_sSegmentType.length()) &&
           (strcasecmp(_sSegmentType.c_str(), i.m_sSegmentType.c_str()) == 0) ) 
        return ( i.m_eSegmentType );
    }
    
    return ST_UNKNOWN;
  }

  int
  OptionParser::parseSegmentType(std::string _sSegmentType, std::string _sFile)
  {
    // Get and validate the segment
    SegmentType eSegmentType = getSegmentType( _sSegmentType );
    if ( eSegmentType == ST_UNKNOWN ) {
      std::cout << "ERROR: Unknown segment type: '" << _sSegmentType << "'\n";
      return 1;
    }

    // Validate the file exists
    struct stat buf;
    if ( stat(_sFile.c_str(), &buf) != 0) {
      std::cout << "ERROR: File does not exist: '" << _sFile << "'\n";
      return 1;
    }
    
    // TODO: Merge all of the individual string vectors into 1 collection
    switch ( eSegmentType ) {
      case ST_BITSTREAM:       
        m_bitstreams.push_back( _sFile );    
        break;

      case ST_CLEAR_BITSTREAM: 
        m_clearstreams.push_back( _sFile );  
        break;

      case ST_FIRMWARE:
        m_firmware.push_back( _sFile );
        break;

      case ST_SCHEDULER:
        m_scheduler.push_back( _sFile );
        break;

      case ST_BINARY_HEADER:
        if ( ! m_binaryHeader.empty() ) {
          std::cout << "ERROR: Only one binary header can be specified (-n/--binaryheader), second was detected: '" << _sFile << "'.\n";
          return 1;
        }
        m_binaryHeader = _sFile;
        break;

      case ST_META_DATA:
        m_metadata.push_back( _sFile );
        break;

      case ST_MEM_TOPOLOGY:
        if ( ! m_memTopology.empty() ) {
          std::cout << "ERROR: Only one MEM_TOPOLOGY section can be specified." << std::endl;
          return 1;
        }
        m_memTopology.push_back( _sFile );
        break;

      case ST_CONNECTIVITY:
        if ( ! m_connectivity.empty() ) {
          std::cout << "ERROR: Only one CONNECTIVITY section can be specified." << std::endl;
          return 1;
        }
        m_connectivity.push_back( _sFile );
        break;

      case ST_IP_LAYOUT:
        if ( ! m_ipLayout.empty() ) {
          std::cout << "ERROR: Only one IP_LAYOUT section can be specified." << std::endl;
          return 1;
        }
        m_ipLayout.push_back( _sFile );
        break;

      case ST_DEBUG_IP_LAYOUT:
        if ( ! m_debugIpLayout.empty() ) {
          std::cout << "ERROR: Only one DEBUG_IP_LAYOUT section can be specified." << std::endl;
          return 1;
        }
        m_debugIpLayout.push_back(_sFile);
        break;

      case ST_CLOCK_FREQ_TOPOLOGY:
        if ( ! m_clockFreqTopology.empty() ) {
          std::cout << "ERROR: Only one DEBUG_IP_LAYOUT section can be specified." << std::endl;
          return 1;
        }
        m_clockFreqTopology.push_back(_sFile);
        break;

      case ST_DEBUG_DATA:
        m_debugdata.push_back( _sFile );
        break;

      case ST_MCS_PRIMARY:
        m_mcs.emplace_back( _sFile, MCS_PRIMARY);
        break;

      case ST_MCS_SECONDARY:
        m_mcs.emplace_back( _sFile, MCS_SECONDARY);
        break;

      case ST_BMC:
        m_bmc.emplace_back( _sFile );
        break;

      case PDI:
      default:
        std::cout << "ERROR: Support missing for the following Segment Type: '" << _sSegmentType << "'" << std::endl;
        break;
    }
    
    return 0;  // All was good
  }

  int
  OptionParser::parse( int argc, char** argv )
  {
    int optCode;
    int optionIndex = 0;
    bool bDisablePositionalArguments = false;
    static struct option longOptions[] = 
    {
      {"help",            no_argument,        0, 'h'},
      {"verbose",         no_argument,        0, 'v'},
      {"bitstream",       required_argument,  0, 'b'},
      {"clearstream",     required_argument,  0, 'c'},
      {"debugdata",       required_argument,  0, 'd'},
      {"firmware",        required_argument,  0, 'f'},
      {"scheduler",       required_argument,  0, 'p'},
      {"kvp",             required_argument,  0, 'k'},
      {"metadata",        required_argument,  0, 'm'},
      {"binaryheader",    required_argument,  0, 'n'},
      {"output",          required_argument,  0, 'o'},
      {"segment_type",    required_argument,  0, 's'},
      {"runtime_data",    required_argument,  0, 'r'},
      {0,                 0,                  0, 0}
    };

    while ( 1 )
    {
      optCode = getopt_long( argc, argv, "hvb:c:r:d:f:p:k:m:n:o:s:", longOptions, &optionIndex );
      
      if ( optCode == -1 )
        break;

      switch ( optCode )
      {
        case 0:
          // getopt_long will handle, just keep going...
          break;

        // Help
        case 'h':  
          m_help = true;
          return 0;
          break;

        // Verbose
        case 'v':  
          m_verbose = true;
          break;

        case 'r':
          m_jsonfiles.push_back( optarg );
          break;

        case 'b':
          // Deprecate 
          m_bitstreams.push_back( optarg );
          break;
        case 'c':
          // Deprecate 
          m_clearstreams.push_back( optarg );
          break;
        case 'd':
          // Deprecate 
          m_debugdata.push_back( optarg );
          break;
        case 'f':
          // Deprecate 
          m_firmware.push_back( optarg );
          break;
        case 'p':
          // Deprecate 
          m_scheduler.push_back( optarg );
          break;

        // Key Value pair
        case 'k':
          {
            std::pair< std::string, std::string > keyValue;
            if ( ! getKeyValuePair( optarg, keyValue ) ) {
              std::cout << "ERROR: Parsing key-value pair (-k/--kvp) failed '" << optarg << "'.\n";
              printHelp( argv[0] );
              return 1;
            }
            m_keyValuePairs[ keyValue.first ] = keyValue.second;
          }
          break;

        // Meta Data
        case 'm':
          // Deprecate 
          m_metadata.push_back( optarg );
          bDisablePositionalArguments = true;
          break;

        // Binary Header
        case 'n':
          // Deprecate 
          if ( ! m_binaryHeader.empty() ) {
            std::cout << "ERROR: Only one binary header can be specified (-n/--binaryheader), second was detected: '" << optarg << "'.\n";
            printHelp( argv[0] );
            return 1;
          }
          m_binaryHeader = optarg;
          break;

        case 'o':
          m_output = optarg;
          bDisablePositionalArguments = true;
          break;

        // Segment Type
        case 's':
          {
            // Record the Segment Type
            std::string sSegmentType( optarg );   

            // Now get the file name
            std::string sFileName;
            if (optind < argc && *argv[optind] != '-') {
              sFileName=argv[optind];
              optind++;
            } else {
              std::cout << "ERROR: '-s/--segment_type' option requires TWO arguments; <type> <file>\n";
              printHelp( argv[0] );
              return 1;
            }

            // Parse the segment type if an error occured exit.
            if ( parseSegmentType(sSegmentType, sFileName) != 0 ) {
              printHelp( argv[0] );
              return 1;
            }
          }
          break;

        // Missing Option
        case ':': // Missing option
          std::cout << "ERROR: '" << argv[ 0 ] << "' option '-" << optopt << "' requires an argument.\n";
          return 1;
          break;

        // Invalid Option
        case '?': 
        default:
          std::cout << "ERROR: Unrecognized option.\n" ;
          printHelp( argv[0] );
          return 1;
      }
    }
    int positionalStart = optind;
    int positionalCount = ( argc - positionalStart );

    if ( positionalCount >= 3 ) {
      std::cout << "ERROR: Too many positional arguments provided.\n" ;
      printHelp( argv[0] );
      return 1;
    }

    if ( bDisablePositionalArguments && (positionalCount >= 1) ) {
      std::cout << "ERROR: Positional arguments are not supported with the use of the options '-m/--metadata' and '-o/--output'.\n";
      printHelp( argv[0] );
      return 1;
    }

    if ( positionalCount >= 2 ) // second to last positional arg, if present, is metadata
      m_metadata.push_back( argv[ optind + (positionalCount - 2) ] );

    if ( positionalCount >= 1 ) // last positional arg is xclbin output file
      m_output = argv[ optind + (positionalCount - 1) ];

    // Additional checks...
    if ( m_output.empty() ) {
      std::cout << "ERROR: Output argument must be provided (either last positional or with '-o').\n";
      printHelp( argv[0] );
      return 1;
    }

    return 0;
  }

  bool
  OptionParser::getKeyValuePair( 
      const std::string & kvString,
      std::pair< std::string, std::string > & keyValue )
  {
    // Everything from the beginning of the kvp up to the first ':' is considered the key
    // Everything after the first ':' to the end of the kvp is considered the value
    //    Notice: This implies that no key shall contain a ':'
    const std::regex expression( "^([^:]+):(.+)$" );
    std::smatch result;
    bool match = std::regex_search( kvString, result, expression );
    if ( match ) {
      keyValue.first = result[ 1 ];
      keyValue.second = result[ 2 ];
    } else {
      keyValue.first = "";
      keyValue.second = "";
    }
    return match;
  }

  const std::string getKindStr(axlf_section_kind _eKind)
  {
    switch ( _eKind ) {
      case BITSTREAM: return "BITSTREAM";
      case CLEARING_BITSTREAM: return "CLEARING_BITSTREAM";
      case EMBEDDED_METADATA: return "EMBEDDED_METADATA";
      case FIRMWARE: return "FIRMWARE";
      case DEBUG_DATA: return "DEBUG_DATA";
      case SCHED_FIRMWARE: return "SCHED_FIRMWARE";
      case MEM_TOPOLOGY: return "MEM_TOPOLOGY";
      case CONNECTIVITY: return "CONNECTIVITY";
      case IP_LAYOUT: return "IP_LAYOUT";
      case DEBUG_IP_LAYOUT: return "DEBUG_IP_LAYOUT";
      case CLOCK_FREQ_TOPOLOGY: return "CLOCK_FREQ_TOPOLOGY";
      case DESIGN_CHECK_POINT: return "DESIGN_CHECK_POINT";
      case MCS: return "MCS";
      case BMC: return "BMC";
      case BUILD_METADATA: return "BUILD_METADATA";
      case KEYVALUE_METADATA: return "KEYVALUE_METADATA";
      case USER_METADATA: return "USER_METADATA";
      case DNA_CERTIFICATE: return "DNA_CERTIFICATE";
      case PDI: return "PDI";

        break;
    }

    return "UNKNOWN";
  }

  void addSectionsWithType( XclBinData & _xclBinData, 
                            const std::vector< std::string > & _files,
                            axlf_section_kind _ekind )
  {
    // Cycle through each file
    for ( const std::string & file : _files ) {

      // -- Section Header --
      axlf_section_header header = (axlf_section_header){0};

      // -- Initialize Header --
      header.m_sectionKind = _ekind;
      std::string baseName = XclBinUtil::getBaseFilename( file );

      // Don't overflow the the sectionName and make sure that it is null terminated.
      if ( baseName.length() >= sizeof(header.m_sectionName)) {
        memcpy(header.m_sectionName, baseName.c_str(), sizeof(header.m_sectionName));
        memset(header.m_sectionName + sizeof( header.m_sectionName ) - 1, '\0', 1 );
      } else {
        memcpy(header.m_sectionName, baseName.c_str(), baseName.length() + 1);
      }

      // -- Determine if the file can be opened and its size --
      std::ifstream fs;
      fs.open( file.c_str(), std::ifstream::in | std::ifstream::binary );
      fs.seekg( 0, fs.end );
      header.m_sectionSize = fs.tellg();

      if ( ! fs.is_open() ) {
        std::string errMsg = "ERROR: Could not open the file for reading: '" + file + "'";
        throw std::runtime_error(errMsg);
      }
      
      // -- Read contents into memory buffer --
      std::unique_ptr<unsigned char> memBuffer( new unsigned char[ header.m_sectionSize ] );
      fs.clear();
      fs.seekg( 0, fs.beg );
      fs.read( (char*) memBuffer.get(), header.m_sectionSize );

      // -- Write contents out --
      std::cout << "INFO: Adding section [" << getKindStr(_ekind) << " (" << _ekind << ")] using: '" << (const char*)&header.m_sectionName << "' (" << (unsigned int)header.m_sectionSize << " Bytes)\n";
      _xclBinData.addSection( header, (const char*) memBuffer.get(), header.m_sectionSize );
    }
  }

  void addSectionBufferWithType( XclBinData & _xclBinData, 
                                 std::ostringstream &_buf,
                                 axlf_section_kind _ekind )
  {
    if ( _buf.tellp() == 0)
      return;

    // -- Section Header --
    axlf_section_header header = (axlf_section_header){0};

    // -- Initialize Header --
    header.m_sectionKind = _ekind;
    std::string baseName = "runtime_data";

    // Don't overflow the the sectionName and make sure that it is null terminated.
    if ( baseName.length() >= sizeof(header.m_sectionName)) {
      memcpy(header.m_sectionName, baseName.c_str(), sizeof(header.m_sectionName));
      memset(header.m_sectionName + sizeof( header.m_sectionName ) - 1, '\0', 1 );
    } else {
      memcpy(header.m_sectionName, baseName.c_str(), baseName.length() + 1);
    }

    header.m_sectionSize = _buf.tellp();


    // -- Read contents into memory buffer --
    std::unique_ptr<unsigned char> memBuffer( new unsigned char[ header.m_sectionSize ] );

    memcpy( (char*) memBuffer.get(), _buf.str().c_str(), header.m_sectionSize);

    // -- Write contents out --
    if ( (_ekind == MCS) || (_ekind == BMC) ) {
      std::cout << "INFO: Adding section [" << getKindStr(_ekind) << " (" << _ekind << ")] (" << (unsigned int)header.m_sectionSize << " Bytes)\n";
    } else {
      std::cout << "INFO: Adding section [" << getKindStr(_ekind) << " (" << _ekind << ")] using: '" << (const char *)&header.m_sectionName << "' (" << (unsigned int)header.m_sectionSize << " Bytes)\n";
    }
    _xclBinData.addSection( header, (const char*) memBuffer.get(), header.m_sectionSize );
  }

  void 
  populateXclbinUUID( XclBinData & data )
  {
    static_assert (sizeof(boost::uuids::uuid) == 16, "Error: UUID size mismatch");
    static_assert (sizeof(axlf_header::uuid) == 16, "Error: UUID size mismatch");

    boost::uuids::uuid uuid = boost::uuids::random_generator()();

    // Copy the values to the UUID structure
    memcpy((void *) &data.getHead().m_header.uuid, (void *)&uuid, sizeof(axlf_header::rom_uuid));
  }



  void
  populateDataWithDefaults( XclBinData & data )
  {
    const char* magic = "xclbin2\0";
    memcpy( data.getHead().m_magic, magic, sizeof(data.getHead().m_magic) );
    memset( data.getHead().m_cipher, 0xFF, sizeof(data.getHead().m_cipher) );
    memset( data.getHead().m_keyBlock, 0xFF, sizeof(data.getHead().m_keyBlock) );
    data.getHead().m_uniqueId = time( nullptr );
    data.getHead().m_header.m_timeStamp = time( nullptr );
    data.getHead().m_header.m_versionMajor = 0;
    data.getHead().m_header.m_versionMinor = 0;
    data.getHead().m_header.m_versionPatch = 2017;
    populateXclbinUUID(data);
  }

  bool
  populateMode( 
      const char* value,
      XclBinData & data )
  {
    bool populated = true;
    // This needs to be turned into a vector and used here and for error checking messages
    if ( strcmp( value, "flat" ) == 0 ) {
      data.getHead().m_header.m_mode = XCLBIN_FLAT;
    } else if ( strcmp( value, "hw_pr" ) == 0 ) {
      data.getHead().m_header.m_mode = XCLBIN_PR;
    } else if ( strcmp( value, "tandem" ) == 0 ) {
      data.getHead().m_header.m_mode = XCLBIN_TANDEM_STAGE2;
    } else if ( strcmp( value, "tandem_pr" ) == 0 ) {
      data.getHead().m_header.m_mode = XCLBIN_TANDEM_STAGE2_WITH_PR;
    } else if ( strcmp( value, "hw_emu" ) == 0 ) {
      data.getHead().m_header.m_mode = XCLBIN_HW_EMU;
    } else if ( strcmp( value, "sw_emu" ) == 0 ) {
      data.getHead().m_header.m_mode = XCLBIN_SW_EMU;
    } else {
      populated = false;
    }
    return populated;
  }

  bool 
  populateFeatureRomTimestamp(
      const char* value,
      XclBinData & data )
  {
    bool populated = true;
    data.getHead().m_header.m_featureRomTimeStamp = (uint64_t)std::atoi( value );
    return populated;
  }

  void 
  populateDSAUUID( std::string _sUUID,
                   XclBinData & data )
  {
    static_assert (sizeof(boost::uuids::uuid) == 16, "Error: UUID size mismatch");
    static_assert (sizeof(axlf_header::rom_uuid) == 16, "Error: UUID size mismatch");

    // Convert string to UUID value
    std::stringstream ss(_sUUID);
    boost::uuids::uuid uuid;
    ss >> uuid;

    // Copy the values to the UUID structure
    memcpy((void *) &data.getHead().m_header.rom_uuid, (void *)&uuid, sizeof(axlf_header::rom_uuid));
  }

  bool 
  populateVBNV(
      const char* value,
      XclBinData & data )
  {
    bool populated = true;
    std::string strSize( value );
    size_t structSize = sizeof( data.getHead().m_header.m_platformVBNV );
    strncpy( (char*)data.getHead().m_header.m_platformVBNV, value, structSize - 1 );
    data.getHead().m_header.m_platformVBNV[ structSize - 1 ] = '\0'; // if we truncated, make sure it is still null terminated
    return populated;
  }


  void populateFromBinaryHeader( const OptionParser & _parser,
                                 XclBinData & writeData )
  {
    std::ifstream extractFrom;
    extractFrom.open( _parser.m_binaryHeader.c_str(), std::ifstream::in | std::ifstream::binary | std::ifstream::ate );

    if ( ! extractFrom.is_open() ) {
      std::string errMsg = "ERROR: Could not open '" + _parser.m_binaryHeader + "' for reading.";
      throw std::runtime_error(errMsg);
    }

    size_t fileSize = extractFrom.tellg();
    size_t headerSize = sizeof( axlf );

    if ( fileSize != headerSize ) {
      std::ostringstream errMsgBuf;
      errMsgBuf << "ERROR: Binary header size (" << headerSize << ") and axlf structure size (" << fileSize << ") do not match.";
      throw std::runtime_error(errMsgBuf.str());
    }

    extractFrom.seekg( 0 );
    unsigned char header[ headerSize ];
    extractFrom.read( (char*)&header, headerSize );
    extractFrom.close();

    memcpy( &writeData.getHead(), &header, headerSize );
    // We must reset the section count as this is updated when sections are added...
    writeData.getHead().m_header.m_numSections = 0;
    writeData.getHead().m_header.m_length = 0;
  }

  void populateDataFromKvp( const OptionParser & _parser,
                            XclBinData & _data )
  {
    std::stringstream ss( std::ios_base::in | std::ios_base::out | std::ios_base::binary );
    for ( std::pair< std::string, std::string > kvp : _parser.m_keyValuePairs ) {
      const std::string & key = kvp.first;
      const std::string & value = kvp.second;
      ss.str("");
      ss.clear();
      XclBinUtil::hex2data( ss, (const unsigned char*)value.c_str(), value.size() ); 
      if ( strcmp( key.c_str(), "cipher" ) == 0 ) {
        memset( _data.getHead().m_cipher, 0, sizeof(_data.getHead().m_cipher) );
        ss >> std::hex >> _data.getHead().m_cipher;
      } else if ( strcmp( key.c_str(), "keyBlock" ) == 0 ) {
        memset( _data.getHead().m_keyBlock, 0, sizeof(_data.getHead().m_keyBlock) );
        ss >> std::hex >> _data.getHead().m_keyBlock;
      } else if ( strcmp( key.c_str(), "uniqueId" ) == 0 ) {
        ss >> std::hex >> _data.getHead().m_uniqueId;
      } else if ( strcmp( key.c_str(), "timestamp" ) == 0 ) {
        ss >> std::hex >> _data.getHead().m_header.m_timeStamp;
      } else if ( strcmp( key.c_str(), "featureRomTimestamp" ) == 0 ) {
        populateFeatureRomTimestamp( value.c_str(), _data );
      } else if ( strcmp( key.c_str(), "version" ) == 0 ) {
        std::vector<std::string> tokens;
        boost::split(tokens, value, boost::is_any_of("."));
        if ( tokens.size() != 3 ) {
          std::ostringstream errMsgBuf;
          errMsgBuf << "ERROR: The version value (" << value << "') is not in the form <major>.<minor>.<patch>.  For example: 2.1.0\n";
          throw std::runtime_error(errMsgBuf.str());
        }
        _data.getHead().m_header.m_versionMajor = (uint8_t) std::stoi(tokens[0]);
        _data.getHead().m_header.m_versionMinor = (uint8_t) std::stoi(tokens[1]);
        _data.getHead().m_header.m_versionPatch = (uint16_t) std::stoi(tokens[2]);
      } else if ( strcmp( key.c_str(), "mode" ) == 0 ) {
        if ( ! populateMode( value.c_str(), _data ) ) {
          std::ostringstream errMsgBuf;
          errMsgBuf << "ERROR: Invalid mode value specified: '" << value << "' supported values are: 'flat', 'hw_pr', 'tandem', 'tandem_pr', 'hw_emu', 'sw_emu'\n";
          throw std::runtime_error(errMsgBuf.str());
        }
      } else if ( strcmp( key.c_str(), "platformId" ) == 0 ) {
        ss >> std::hex >> _data.getHead().m_header.rom.m_platformId;
      } else if ( strcmp( key.c_str(), "platformVBNV" ) == 0 ) {
        populateVBNV( value.c_str(), _data );
      } else if ( strcmp( key.c_str(), "featureId" ) == 0 ) {
        ss >> std::hex >> _data.getHead().m_header.rom.m_featureId;
      } else if ( strcmp( key.c_str(), "nextAxlf" ) == 0 ) {
        ss >> std::hex >> _data.getHead().m_header.m_next_axlf;
      } else if ( strcmp( key.c_str(), "debugBin" ) == 0 ) {
        ss >> std::hex >> _data.getHead().m_header.m_debug_bin;
      } else if ( strcmp( key.c_str(), "dsaUUID" ) == 0 ) {
        populateDSAUUID(value, _data);
      } else {
        std::cout << "WARNING: Unknown key '" << key.c_str() << "' will be ignored from key-value pair switch (-k).\n";
      }
    }
  }

  int _execute( int argc, char** argv )
  {
    // Parse the command line
    OptionParser parser;
    if ( parser.parse( argc, argv ) )
      return 1;

    if ( parser.m_help ) {
      parser.printHelp( argv[0] );
      return 0;
    }

    // Echo command line
    {
      std::ostringstream buf; 
      buf << "Command line: " << argv[ 0 ];
      for ( int i = 1; i < argc; i++ )
        buf << " " << argv[ i ];

      if ( parser.isVerbose() )
        std::cout << buf.str() << std::endl;
    }
    
    XclBinData data;
    if ( parser.isVerbose() )
      data.enableTrace();

    populateDataWithDefaults(data);

    if ( parser.m_binaryHeader.empty() ) {
      populateDataFromKvp( parser, data );
    } else {
      populateFromBinaryHeader( parser, data );
    }

    // Parse the JSON files
    data.parseJSONFiles( parser.m_jsonfiles );
    
    // Check duplicate segments
    if ( (parser.m_memTopology.size() > 0) && (data.m_memTopologyBuf.str().length() > 0) )
      throw std::runtime_error("ERROR: Only one MEM_TOPOLOGY data segment is permitted.\n");
    if ( (parser.m_connectivity.size() > 0) && (data.m_connectivityBuf.str().length() > 0) )
      throw std::runtime_error("ERROR: Only one CONNECTIVITY data segment is permitted.\n");
    if ( (parser.m_ipLayout.size() > 0) && (data.m_ipLayoutBuf.str().length() > 0) )
      throw std::runtime_error("ERROR: Only one IP_LAYOUT data segment is permitted.\n");
    if ( (parser.m_debugIpLayout.size() > 0) && (data.m_debugIpLayoutBuf.str().length() > 0) )
      throw std::runtime_error("ERROR: Only one DEBUG_IP_LAYOUT data segment is permitted.\n");
    if ( (parser.m_clockFreqTopology.size() > 0) && (data.m_clockFreqTopologyBuf.str().length() > 0) )
      throw std::runtime_error("ERROR: Only one CLOCK_FREQ_TOPOLOGY data segment is permitted.\n");
    if ( (parser.m_clockFreqTopology.size() > 0) && (data.m_clockFreqTopologyBuf.str().length() > 0) )
      throw std::runtime_error("ERROR: Only one CLOCK_FREQ_TOPOLOGY data segment is permitted.\n");

    // Count the number of MCS PRIMARY entries.  If more than 1 then error out.
    if (std::count_if(parser.m_mcs.begin(), parser.m_mcs.end(), [](std::pair< std::string, int >  pairEntry) { return (pairEntry.second == MCS_PRIMARY); }) > 1)
      throw std::runtime_error("ERROR: Only one MCS_PRIMARY data segment is permitted.\n");

   // Count the number of MCS SECONDARY entries.  If more than 1 then error out.
    if (std::count_if(parser.m_mcs.begin(), parser.m_mcs.end(), [](std::pair< std::string, int >  pairEntry) { return (pairEntry.second == MCS_SECONDARY); }) > 1)
      throw std::runtime_error("ERROR: Only one MCS_SECONDARY data segment is permitted.\n");

    if ( parser.m_bmc.size() > 1 )
      throw std::runtime_error("ERROR: Only one BMC image segment is permitted.\n");

    data.createMCSSegmentBuffer(parser.m_mcs);
    data.createBMCSegmentBuffer(parser.m_bmc);

    // Determine the number of sections that will be written out
    int sectionTotal = 0;
    sectionTotal += parser.m_bitstreams.size();
    sectionTotal += parser.m_clearstreams.size();
    sectionTotal += parser.m_metadata.size();
    sectionTotal += parser.m_debugdata.size();
    sectionTotal += parser.m_firmware.size();
    sectionTotal += parser.m_scheduler.size();
    sectionTotal += parser.m_connectivity.size();
    sectionTotal += parser.m_memTopology.size();
    sectionTotal += parser.m_ipLayout.size();
    sectionTotal += parser.m_bmc.size();
    sectionTotal += data.getJSONBufferSegmentCount();
    if (parser.m_mcs.size() > 0) ++sectionTotal;

    if ( parser.isVerbose() )
      std::cout << "INFO: Creating xclbin (with '" << sectionTotal << "' sections): '" << parser.m_output.c_str() << "'\n";

    data.initWrite( parser.m_output, sectionTotal );
    addSectionsWithType( data, parser.m_bitstreams, BITSTREAM );
    addSectionsWithType( data, parser.m_clearstreams, CLEARING_BITSTREAM );
    addSectionsWithType( data, parser.m_metadata, EMBEDDED_METADATA );
    addSectionsWithType( data, parser.m_firmware, FIRMWARE );
    addSectionsWithType( data, parser.m_scheduler, SCHED_FIRMWARE );
    addSectionsWithType( data, parser.m_memTopology, MEM_TOPOLOGY );
    addSectionsWithType( data, parser.m_connectivity, CONNECTIVITY );
    addSectionsWithType( data, parser.m_ipLayout, IP_LAYOUT );
    addSectionsWithType( data, parser.m_debugIpLayout, DEBUG_IP_LAYOUT );
    addSectionsWithType( data, parser.m_clockFreqTopology, CLOCK_FREQ_TOPOLOGY );
    addSectionsWithType( data, parser.m_debugdata, DEBUG_DATA );

    addSectionBufferWithType( data, data.m_memTopologyBuf, MEM_TOPOLOGY );
    addSectionBufferWithType( data, data.m_connectivityBuf, CONNECTIVITY );
    addSectionBufferWithType( data, data.m_ipLayoutBuf, IP_LAYOUT );
    addSectionBufferWithType( data, data.m_debugIpLayoutBuf, DEBUG_IP_LAYOUT );
    addSectionBufferWithType( data, data.m_clockFreqTopologyBuf, CLOCK_FREQ_TOPOLOGY );
    addSectionBufferWithType( data, data.m_mcsBuf, MCS );
    addSectionBufferWithType( data, data.m_bmcBuf, BMC );

    data.finishWrite();

    std::cout << "Successfully completed '" << argv[ 0 ] << "'" << std::endl;
    
    return 0;
  }


  int execute( int argc, char** argv )
  {
    int returnCode = 0;
    try {
      returnCode = _execute(argc, argv);
    } 
    catch ( const std::runtime_error & e) {
      std::cout << e.what() << std::endl;
      returnCode = 1;
    } 
    catch (...) {
      std::cout << "Caught an unknown exception" << std::endl;
      returnCode = 1;
    }

    return returnCode;
  }



} // namespace xclbincat1




