/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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
// (c) Copyright 2017-2018 Xilinx, Inc. All rights reserved.
//
// File Name: xclbindata.cxx
// ============================================================================

#include "xclbindata.h"


#include "xclbinutils.h"
#include <iostream>
#include <memory>
#include <stdlib.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/uuid/uuid.hpp>          // for uuid
#include <boost/uuid/uuid_io.hpp>       // for to_string
#include <boost/algorithm/string.hpp>


void printTree (boost::property_tree::ptree &pt, std::ostream &_buf = std::cout, int level = 0);



std::string indent(int _level) {
  std::string sIndent; 

  for (int i=0; i < _level; ++i) 
    sIndent += "  ";

  return sIndent; 
} 

void printTree (boost::property_tree::ptree &pt, std::ostream &_buf, int level) {
  if (pt.empty()) {
    _buf << "\""<< pt.data()<< "\"";
  }

  else {
    if (level) 
      _buf << std::endl; 

    _buf << indent(level) << "{" << std::endl;     

    for ( boost::property_tree::ptree::iterator pos = pt.begin(); pos != pt.end(); ) {
      _buf << indent(level+1) << "\"" << pos->first << "\": "; 

      printTree( pos->second, _buf, level + 1); 

      ++pos; 

      if (pos != pt.end()) {
        _buf << ","; 
      }

      _buf << std::endl;
    } 

   _buf << indent(level) << " }";     
  }

  if ( level == 0 )
    _buf << std::endl;
}


/*
 * You will see several lines with the following throughout this file:
 *   <...> - sizeof(axlf_section_header) //See top of file
 * These lines are present (subtracting the axlf_section_header) because
 * the 'axlf' data structure (xclbin.h) already contains one section header
 * just for communication purposes.  We subtract that section header before 
 * we start to write or read.
 */


XclBinData::XclBinData()
  : m_mode( FM_UNINITIALIZED )
  , m_numSections( 0 )
  , m_trace( false )
  , m_xclBinHead( (axlf){0} )
  , m_schemaVersion( (XclBinData::SchemaVersion) {0} )
{
  m_schemaVersion.major = 1;
  m_schemaVersion.minor = 0;
  m_schemaVersion.patch = 0;

  memset( &m_xclBinHead, 0, sizeof(axlf) );
}

XclBinData::~XclBinData() {
  if ( m_xclbinFile.is_open() )
    m_xclbinFile.close();
  m_sections.clear();
  m_sectionCounts.clear();
}

void  
XclBinData::align() 
{
  static char holePack[] = {(char)0, (char)0, (char)0, (char)0, (char)0, (char)0, (char)0, (char)0};

  long long current =  m_xclbinFile.tellp();
  unsigned hole = (current & 0x7) ? 0x8 - (current & 0x7) : 0;

  // If we are not align, then get aligned.
  if( hole ) {
    m_xclbinFile.write( holePack, hole ); 
    m_xclBinHead.m_header.m_length += hole;
    TRACE(XclBinUtil::format("Aligning by %d bytes.", hole ));
  }
}

bool 
XclBinData::initRead( const char* file )
{
  if ( m_mode != FM_UNINITIALIZED ) {
    std::cerr << "ERROR: The xclbin reader has already been initialized - calling '" << __FUNCTION__ << "' doesn't make sense.\n";
    return false;
  }
  m_mode = FM_READ;
  m_xclbinFile.open( file, std::ifstream::in | std::ifstream::binary );
  if ( ! m_xclbinFile.is_open() ) {
    std::cerr << "ERROR: Could not open " << file << " for reading\n";
    return false;
  }
  readHead( m_xclBinHead );
  return true;
}

void 
XclBinData::initWrite( const std::string &_file, int _numSections )
{
  if ( _numSections == 0 ) {
    std::string errMsg = "ERROR: No xclbin sections to write";
    throw std::runtime_error(errMsg);
  }

  if ( m_mode != FM_UNINITIALIZED ) {
    std::string errMsg = "INTERNAL ERROR: The xclbin writer has already been initialized.";
    throw std::runtime_error(errMsg);
  }

  m_mode = FM_WRITE;
  m_numSections = _numSections;
  m_xclbinFile.open( _file.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc );

  if ( ! m_xclbinFile.is_open() ) {
    std::string errMsg = "ERROR: Could not open '" + _file + "' for writing.";
    throw std::runtime_error(errMsg);
  }

  //reserve space for the header + sections. It will be written out later.
  m_xclbinFile.seekp( 0 );
  m_xclbinFile.write( (const char*) &m_xclBinHead, sizeof(axlf) );

  size_t sectionHeaderSize = m_numSections * sizeof(axlf_section_header) - sizeof(axlf_section_header) /*See top of file*/;
  char* zero  = new char[ sectionHeaderSize ]; 
  memset( zero, 0, sectionHeaderSize );
  m_xclbinFile.write( zero, sectionHeaderSize ); // using current puts pointer
  delete [] zero;
}


void 
XclBinData::finishWrite() 
{
  if ( m_mode != FM_WRITE ) {
    std::string errMsg = "INTERNAL ERROR: The xclbin writer was never initialized.";
    throw std::runtime_error(errMsg);
  }

  // Write out the head and set final section count
  m_xclbinFile.seekp( 0 );
  m_xclBinHead.m_header.m_length += sizeof(axlf) + m_numSections * sizeof(axlf_section_header) - sizeof(axlf_section_header) /*See top of file*/;


  TRACE_BUF("Structure AXLF", (char*) &m_xclBinHead, sizeof(axlf));
  m_xclbinFile.write( (const char*)&m_xclBinHead, sizeof(axlf) );

  // Write out the section headers
  m_xclbinFile.seekp( sizeof(axlf) - sizeof(axlf_section_header) /*See top of file*/);
  for( unsigned int i = 0; i < m_sections.size(); i++ )
    m_xclbinFile.write( (const char*)&m_sections.at( i ), sizeof(axlf_section_header) );

  m_xclbinFile.close();   
}

void 
XclBinData::addSection( axlf_section_header& sh, const char* data, size_t size )
{
  // Check that we are not adding more sections than were reserved
  if ( m_xclBinHead.m_header.m_numSections == m_numSections ) {
    std::string errMsg = "ERROR: Trying to add more sections than were reserved in memory with the initWrite() call.\n";
    throw std::runtime_error(errMsg);
  }

  // Make sure any new section is aligned
  align();

  // Add the new section
  sh.m_sectionSize = size;
  sh.m_sectionOffset = m_xclbinFile.tellp();
  m_sections.push_back(sh);

  writeSectionData( data, size );
  m_xclBinHead.m_header.m_numSections++;
}

bool 
XclBinData::writeSectionData( const char* data, size_t size )
{
  m_xclbinFile.write( data, size );
  m_xclBinHead.m_header.m_length += size;
  return true;
}

bool 
XclBinData::extractBinaryHeader( const char* file, const char* name )
{
  std::ifstream extractFrom;
  extractFrom.open( file, std::ifstream::in | std::ifstream::binary );
  if ( ! extractFrom.is_open() ) {
    std::cerr << "ERROR: Could not open " << file << " for reading\n";
    return false;
  }
  extractFrom.seekg( 0 );
  size_t headerSize = sizeof( axlf );
  unsigned char header[ headerSize ];
  extractFrom.read( (char*)&header, headerSize );
  extractFrom.close();

  std::string ext = ".bin";
  if ( XclBinUtil::stringEndsWith( name, ext.c_str() ) )
    ext = "";
  std::string outputFile = name + ext;

  std::ofstream writeTo;
  writeTo.open( outputFile.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc );
  if ( ! writeTo.is_open() ) {
    std::cerr << "ERROR: Could not open '" << outputFile.c_str() << "' for reading" << "\n";
    return false;
  }
  writeTo.write( (char*)&header, headerSize );
  writeTo.close();
  return true;
}

bool 
XclBinData::extractAll( const char* name )
{
  if ( m_mode != FM_READ ) {
    std::cerr << "ERROR: The xclbin reader was never initialized - calling '" << __FUNCTION__ << "' doesn't make sense (call initRead first).\n";
    return false;
  }

  // Prepare for extraction
  m_ptree_extract.clear();

  for( unsigned int i = 0; i < m_xclBinHead.m_header.m_numSections; ++i ) {
    if( ! extractSectionData( i, name ) ) 
      return false;
  }

  if ( m_ptree_extract.begin() != m_ptree_extract.end()) {
    addPTreeSchemaVersion(m_ptree_extract, m_schemaVersion);

    TRACE("Writing out JSON file.");
    TRACE_PrintTree("Root", m_ptree_extract);


    boost::property_tree::write_json("runtime_data.rtd", m_ptree_extract);
  }

  return true;
}




bool 
XclBinData::extractSectionData( int sectionNum, const char* name ) 
{
  long long sectionOffset = sizeof(axlf) + sectionNum * sizeof(axlf_section_header) - sizeof(axlf_section_header) /*See top of file*/;
  m_xclbinFile.seekg( sectionOffset );
  axlf_section_header header;
  m_xclbinFile.read( (char*)&header, sizeof(axlf_section_header) );
  long long dataOffset = header.m_sectionOffset;
  m_xclbinFile.seekg( dataOffset );

  unsigned sectionSize = header.m_sectionSize;

  std::unique_ptr<char> data( new char[ sectionSize ] );
  m_xclbinFile.read( data.get(), sectionSize );

  std::string type;
  std::string ext;
  if ( m_sectionCounts.find( header.m_sectionKind ) == m_sectionCounts.end() )
    m_sectionCounts[ header.m_sectionKind ] = 0;

  m_sectionCounts[ header.m_sectionKind ] += 1;

  if ( header.m_sectionKind == BITSTREAM ) {
    type = "primary";
    ext = ".bit";
  }
  else if ( header.m_sectionKind == CLEARING_BITSTREAM ) {
    type = "secondary";
    ext = ".bit";
  }
  else if ( header.m_sectionKind == EMBEDDED_METADATA ) {
    type = "xclbin";
    ext = ".xml";
  }
  else if ( header.m_sectionKind == FIRMWARE ) {
    type = "mgmt";
    ext = ".bin";
  }
  else if (header.m_sectionKind == SCHED_FIRMWARE) {
    type = "sched";
    ext = ".bin";
  }
  else if ( header.m_sectionKind == DEBUG_DATA ) {
    type = "debug";
    ext = ".bin";
  }
  else if ( header.m_sectionKind == DNA_CERTIFICATE ) {
    type = "dna_certificate";
    ext = ".bin";
  }
  else if ( header.m_sectionKind == BUILD_METADATA ) {
    type = "build_metadata";
    ext = ".bin";
  }
  else if ( header.m_sectionKind == KEYVALUE_METADATA ) {
    type = "keyvalue_metadata";
    ext = ".bin";
  }
  else if ( header.m_sectionKind == USER_METADATA ) {
    type = "user_metadata";
    ext = ".bin";
  }
  else if ( header.m_sectionKind == MEM_TOPOLOGY ) {
    type = "mem_topology";
    ext = ".bin";
    extractMemTopologyData((char*) data.get(), sectionSize, m_ptree_extract);
  }
  else if ( header.m_sectionKind == CONNECTIVITY ) {
    type = "connectivity";
    ext = ".bin";
    extractConnectivityData((char*) data.get(), sectionSize, m_ptree_extract);
  }
  else if ( header.m_sectionKind == IP_LAYOUT ) {
    type = "ip_layout";
    ext = ".bin";
    extractIPLayoutData((char*) data.get(), sectionSize, m_ptree_extract);
  }
  else if ( header.m_sectionKind == DEBUG_IP_LAYOUT ) {
    type = "debug_ip_layout";
    ext = ".bin";
    extractDebugIPLayoutData((char*) data.get(), sectionSize, m_ptree_extract);
  }
  else if ( header.m_sectionKind == CLOCK_FREQ_TOPOLOGY ) {
    type = "clock_freq_topology";
    ext = ".bin";
    extractClockFreqTopology((char*) data.get(), sectionSize, m_ptree_extract);
  }
  else if ( header.m_sectionKind == MCS ) {
    extractAndWriteMCSImages((char*) data.get(), sectionSize);
    return true;
  }
  else if ( header.m_sectionKind == BMC ) {
    extractAndWriteBMCImages((char*) data.get(), sectionSize);
    return true;
  } else {
    static unsigned int uniqueCount = 1;
    type = "unknown(" + std::to_string(uniqueCount) + ")";
    ext = ".bin";
    ++uniqueCount;
  }

  


  std::string id = "";
  if ( m_sectionCounts[ header.m_sectionKind ] > 1 )
    id = std::string( "-" ) + std::to_string( m_sectionCounts[ header.m_sectionKind ] );

  std::string file = name + std::string( "-" ) + type + id + ext;

  std::fstream fs;
  fs.open( file, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc );
  if ( ! fs.is_open() ) {
    std::cerr << "ERROR: Could not open " << file << " for writing" << "\n";
    return false;
  }
  fs.write( data.get(), sectionSize );

  return true;
}

bool 
XclBinData::report()
{
  if ( m_mode != FM_READ ) {
    std::cerr << "ERROR: The xclbin reader was never initialized - calling '" << __FUNCTION__ << "' doesn't make sense (call initRead first).\n";
    return false;
  }

  if ( ! reportHead() ) {
    std::cerr << "ERROR: Failed to report 'top'\n";
    return false;
  }
  if ( ! reportHeader() ) {
    std::cerr << "ERROR: Failed to report 'header'\n";
    return false;
  }
  if ( ! reportSectionHeaders() ) {
    std::cerr << "ERROR: Failed to report 'section_headers'\n";
    return false;
  }
  return true;
}

bool 
XclBinData::readHead( axlf & xclBinHead ) 
{
  m_xclbinFile.seekg( 0 );
  m_xclbinFile.read( (char*)&xclBinHead, sizeof(axlf) );
  return true;
}

bool 
XclBinData::readHeader( axlf_section_header & header, int sectionNum )
{
  long long sectionOffset = sizeof(axlf) + sectionNum * sizeof(axlf_section_header) - sizeof(axlf_section_header) /*See top of file*/;
  m_xclbinFile.seekg( sectionOffset );
  m_xclbinFile.read( (char*)&header, sizeof(axlf_section_header) );
  return true;
}

bool 
XclBinData::reportHead() 
{
  std::cout << "Magic: " << m_xclBinHead.m_magic << "\n";
//  std::cout << "Cipher: ";
//  XclBinUtil::data2hex( std::cout, (const unsigned char*)&m_xclBinHead.m_cipher, sizeof(m_xclBinHead.m_cipher) );
//  std::cout << "\n";
  std::cout << "Key Block: ";
  XclBinUtil::data2hex( std::cout, (const unsigned char*)&m_xclBinHead.m_keyBlock, sizeof(m_xclBinHead.m_keyBlock) );
  std::cout << "\n";
  std::cout << "Unique ID: " << m_xclBinHead.m_uniqueId << "\n";
  return true;
}

std::string 
XclBinData::getUUIDAsString( const unsigned char (&_uuid)[16] )
{
  static_assert (sizeof(boost::uuids::uuid) == 16, "Error: UUID size mismatch");

  // Copy the values to the UUID structure
  boost::uuids::uuid uuid;
  memcpy((void *) &uuid, (void *) &_uuid, sizeof(boost::uuids::uuid));

  // Now decode it to a string we can work with
  return boost::uuids::to_string(uuid);
}


bool 
XclBinData::reportHeader() 
{
  std::cout << "xclbin1 Size:           " << m_xclBinHead.m_header.m_length << "\n";
  std::cout << "Version:                " << m_xclBinHead.m_header.m_versionMajor 
                                            << "." << m_xclBinHead.m_header.m_versionMinor 
                                            << "." << m_xclBinHead.m_header.m_versionPatch << "\n";
  std::cout << "Timestamp:              " << m_xclBinHead.m_header.m_timeStamp << "\n";
  std::cout << "Feature ROM Timestamp:  " << m_xclBinHead.m_header.m_featureRomTimeStamp << "\n";
  std::cout << "Mode:                   " << (int)m_xclBinHead.m_header.m_mode << "\n";
  std::cout << "  XCLBIN_FLAT:            " << XCLBIN_FLAT << "\n";
  std::cout << "  XCLBIN_PR:              " << XCLBIN_PR << "\n";
  std::cout << "  XCLBIN_HW_EMU:          " << XCLBIN_HW_EMU << "\n";
  std::cout << "  XCLBIN_SW_EMU:          " << XCLBIN_SW_EMU << "\n";
  std::cout << "  XCLBIN_MODE_MAX:        " << XCLBIN_MODE_MAX << "\n";
  std::cout << "Platform VBNV:          " << m_xclBinHead.m_header.m_platformVBNV << "\n";
  std::cout << "XSA uuid:               " << getUUIDAsString(m_xclBinHead.m_header.rom_uuid) << "\n";
  std::cout << "xclbin uuid:            " << getUUIDAsString(m_xclBinHead.m_header.uuid) << "\n";
  std::cout << "Debug Bin:              " << m_xclBinHead.m_header.m_debug_bin << "\n";
  std::cout << "Num of sections:        " << m_xclBinHead.m_header.m_numSections << "\n";
  return true;
}

bool 
XclBinData::reportSectionHeaders() 
{
  for(unsigned int i = 0; i <  m_xclBinHead.m_header.m_numSections; ++i) {
    std::cout << "\nReporting section header: " << i  << "\n";
    std::cout << "-----------------" << "\n";
    if(!reportSectionHeader(i)) {
      std::cout << "Failed to read 'section_header(" << i << ")'\n";
      return false;
    }
  }
  return true;
}

bool 
XclBinData::reportSectionHeader( int sectionNum )
{
  axlf_section_header header;
  readHeader( header, sectionNum );
  std::cout << "Section Name: " << header.m_sectionName << "\n";
  std::cout << "Section Size: " << header.m_sectionSize << "\n";
  std::cout << "Section Data Offset: " << header.m_sectionOffset << "\n";

  std::string kind = kindToString( (axlf_section_kind) header.m_sectionKind );
  std::cout << "Section Kind : " << kind << "\n";

  return true;
}

std::string
XclBinData::kindToString( axlf_section_kind kind )
{
  std::string type = "UNKNOWN";
  switch ( kind ) {
    case BITSTREAM:
      type = "BITSTREAM";
      break;
    case CLEARING_BITSTREAM:
      type = "CLEARING_BITSTREAM";
      break;
    case EMBEDDED_METADATA:
      type = "EMBEDDED_METADATA";
      break;
    case FIRMWARE:
      type = "FIRMWARE";
      break;
    case DEBUG_DATA:
      type = "DEBUG_DATA";
      break;
    case SCHED_FIRMWARE:
      type = "SCHED_FIRMWARE";
      break;
    case MEM_TOPOLOGY:
      type = "MEM_TOPOLOGY";
      break;
    case CONNECTIVITY:
      type = "CONNECTIVITY";
      break;
    case IP_LAYOUT:
      type = "IP_LAYOUT";
      break;
    case DEBUG_IP_LAYOUT:
      type = "DEBUG_IP_LAYOUT";
      break;
    case CLOCK_FREQ_TOPOLOGY:
      type = "CLOCK_FREQ_TOPOLOGY";
      break;
    case DESIGN_CHECK_POINT:
      type = "DESIGN_CHECK_POINT";
      break;
    case MCS:
      type = "MCS";
      break;
    default:
      break;
  }
  return type;
}


void
XclBinData::TRACE(const std::string &_msg, bool _endl)
{
  if ( !m_trace )
    return;

  std::cout << "Trace: " << _msg;

  if ( _endl )
    std::cout << std::endl;
}

void 
XclBinData::TRACE_PrintTree( const std::string &_msg, 
                             boost::property_tree::ptree &_pt)
{
  if ( !m_trace )
    return;

  std::cout << "Trace: Property Tree (" << _msg << ")" << std::endl;

  std::ostringstream buf;
  printTree( _pt, buf);
  std::cout << buf.str();
}


void 
XclBinData::TRACE_BUF(const std::string &_msg, 
                      const char * _pData, 
                      unsigned long _size)
{
  if ( !m_trace ) 
    return;

  std::ostringstream buf;
  buf << "Trace: Buffer(" << _msg << ") Size: 0x" << std::hex << _size << std::endl;
  
  buf << std::hex << std::setfill('0');

  unsigned long address = 0;
  while( address < _size )
  {
    // We know we have data, create the address entry
    buf << "       " << std::setw(8) << address;

    // Read in 16 bytes (or less) at a time
    int bytesRead;
    unsigned char charBuf[16];

    for( bytesRead = 0; (bytesRead < 16) && (address < _size); ++bytesRead, ++address ) {
      charBuf[bytesRead] = _pData[address];
    }

    // Show the hex codes
    for( int i = 0; i < 16; i++ ) {

      // Create a divider ever 8 bytes
      if ( i % 8 == 0 ) 
        buf << " ";

      // If we don't have data then display "nothing"
      if ( i < bytesRead ) {
        buf << " " << std::setw(2) << (unsigned) charBuf[i];
      } else {
        buf << "   ";
      }
    }

    // Bonus: Show printable characters
    buf << "  ";
    for( int i = 0; i < bytesRead; i++) {
      if( (charBuf[i] > 32) && (charBuf[i] <= 126) ) {
        buf << charBuf[i];
      } else {
        buf << ".";
      }
    }

    buf << std::endl;
  }

  std::cout << buf.str() << std::endl;
}


void 
XclBinData::parseJSONFiles(const std::vector< std::string > & _files)
{
  for ( const std::string & file : _files ) {
    // Check for duplicate files being parsed
    if ( m_ptree_segments.find(file) != m_ptree_segments.end() ) {
      std::string errMsg = "ERROR: Duplicate file name previously parsed: '" + file + "'";
      throw std::runtime_error(errMsg);
    }

    // Check if the file can be read in
    std::ifstream fs;
    fs.open( file.c_str(), std::ifstream::in | std::ifstream::binary );

    if ( ! fs.is_open() ) {
      std::string errMsg = "ERROR: Could not open the file for reading: '" + file + "'";
      throw std::runtime_error(errMsg);
    }

    //  Add a new element to the collection and parse the jason file
    TRACE("Reading JSON File: '" + file + '"');
    boost::property_tree::ptree &pt = m_ptree_segments[file];
    boost::property_tree::read_json( fs, pt );
  }

  createBinaryImages();
}

enum MEM_TYPE
XclBinData::getMemType( std::string &_sMemType ) const
{
  if ( _sMemType == "MEM_DDR3" )
      return MEM_DDR3;

  if ( _sMemType == "MEM_DDR4" )
      return MEM_DDR4;

  if ( _sMemType == "MEM_DRAM" )
      return MEM_DRAM;

  if ( _sMemType == "MEM_HBM" )
      return MEM_HBM;

  if ( _sMemType == "MEM_BRAM" )
      return MEM_BRAM;

  if ( _sMemType == "MEM_URAM" )
      return MEM_URAM;

  if ( _sMemType == "MEM_STREAMING" )
      return MEM_STREAMING;

  if ( _sMemType == "MEM_PREALLOCATED_GLOB" )
      return MEM_PREALLOCATED_GLOB;

  if ( _sMemType == "MEM_ARE" )
      return MEM_ARE;

  if ( _sMemType == "MEM_STREAMING_CONNECTION" )
      return MEM_STREAMING_CONNECTION;

  std::string errMsg = "ERROR: Unknown memory type: '" + _sMemType + "'";
  throw std::runtime_error(errMsg);
}

void
XclBinData::createMemTopologyBinaryImage( boost::property_tree::ptree &_pt, 
                                          std::ostringstream &_buf)
{
   mem_topology memTopologyHdr = (mem_topology){0};

   // Read, store, and report mem_topology data
   memTopologyHdr.m_count = _pt.get<uint32_t>("m_count");

   TRACE("MEM_TOPOLOGY");
   TRACE(XclBinUtil::format("m_count: %d", memTopologyHdr.m_count));

   if ( memTopologyHdr.m_count == 0) {
     std::cout << "WARNING: Skipping MEM_TOPOLOGY section for count size is zero." << std::endl;
     return;
   }

   // Write out the entire structure except for the mem_data structure
   TRACE_BUF("mem_topology - minus mem_data", reinterpret_cast<const char*>(&memTopologyHdr), (sizeof(mem_topology) - sizeof(mem_data)));
   _buf.write(reinterpret_cast<const char*>(&memTopologyHdr), (sizeof(mem_topology) - sizeof(mem_data)) );


   // Read, store, and report mem_data segments
   unsigned int count = 0;
   boost::property_tree::ptree memDatas = _pt.get_child("m_mem_data");
   for (const auto& kv : memDatas) {
     mem_data memData = (mem_data){0};
     boost::property_tree::ptree ptMemData = kv.second;

     std::string sm_type = ptMemData.get<std::string>("m_type");
     memData.m_type = (uint8_t) getMemType( sm_type );
     memData.m_used = ptMemData.get<uint8_t>("m_used");

     boost::optional<std::string> sizeBytes = ptMemData.get_optional<std::string>("m_size");

     if ( sizeBytes.is_initialized() ) {
       memData.m_size = XclBinUtil::stringToUInt64(static_cast<std::string>(sizeBytes.get()));
       if ( (memData.m_size % 1024) != 0 )
          throw std::runtime_error(XclBinUtil::format("ERROR: The memory size (%ld) does not align to a 1K (1024 bytes) boundary.", memData.m_size));

       memData.m_size = memData.m_size / (uint64_t) 1024;
     }

     boost::optional<std::string> sizeKB = ptMemData.get_optional<std::string>("m_sizeKB");
     if ( sizeBytes.is_initialized() && sizeKB.is_initialized() ) 
       throw std::runtime_error(XclBinUtil::format("ERROR: 'm_size' (%s) and 'm_sizeKB' (%s) are mutually exclusive.", 
                                    static_cast<std::string>(sizeBytes.get()),
                                    static_cast<std::string>(sizeKB.get())));

     if ( sizeKB.is_initialized() ) 
       memData.m_size = XclBinUtil::stringToUInt64(static_cast<std::string>(sizeKB.get()));

     
     std::string sm_tag = ptMemData.get<std::string>("m_tag");
     if ( sm_tag.length() >= sizeof(mem_data::m_tag) ) {
       std::string errMsg = XclBinUtil::format("ERROR: The m_tag entry length (%d), exceeds the allocated space (%d).  Name: '%s'",
                                         (unsigned int) sm_tag.length(), (unsigned int) sizeof(mem_data::m_tag), sm_tag);
       throw std::runtime_error(errMsg);
     }

     // We already know that there is enough room for this string
     memcpy( memData.m_tag, sm_tag.c_str(), sm_tag.length() + 1);

     std::string sBaseAddress = ptMemData.get<std::string>("m_base_address");
     memData.m_base_address = XclBinUtil::stringToUInt64(sBaseAddress);

     TRACE(XclBinUtil::format("[%d]: m_type: %d, m_used: %d, m_size: 0x%lx, m_tag: '%s', m_base_address: 0x%lx",
               count,
               (unsigned int) memData.m_type,
               (unsigned int) memData.m_used, 
               memData.m_size, 
               memData.m_tag,
               memData.m_base_address));

     // Write out the entire structure 
     TRACE_BUF("mem_data", reinterpret_cast<const char*>(&memData), sizeof(mem_data));
    _buf.write(reinterpret_cast<const char*>(&memData), sizeof(mem_data) );
    count++;
  }

  // -- The counts should match --
  if ( count != (unsigned int) memTopologyHdr.m_count  ) {
    std::string errMsg = XclBinUtil::format("ERROR: Number of mem_data sections (%d) does not match expected encoded value: %d", 
                                      (unsigned int) count, (unsigned int) memTopologyHdr.m_count);
    throw std::runtime_error(errMsg);
  }
}

void
XclBinData::createConnectivityBinaryImage( boost::property_tree::ptree &_pt, 
                                           std::ostringstream &_buf)
{
   connectivity connectivityHdr = (connectivity){0};
 
   // Read, store, and report mem_topology data
   connectivityHdr.m_count = _pt.get<uint32_t>("m_count");

   TRACE("CONNECTIVITY");
   TRACE(XclBinUtil::format("m_count: %d", connectivityHdr.m_count));

   if ( connectivityHdr.m_count == 0) {
     std::cout << "WARNING: Skipping CONNECTIVITY section for count size is zero." << std::endl;
     return;
   }

   // Write out the entire structure except for the mem_data structure
   TRACE_BUF("connectivity - minus connection", reinterpret_cast<const char*>(&connectivityHdr), (sizeof(connectivity) - sizeof(connection)));
   _buf.write(reinterpret_cast<const char*>(&connectivityHdr), sizeof(connectivity) - sizeof(connection) );


   // Read, store, and report connection segments
   unsigned int count = 0;
   boost::property_tree::ptree connections = _pt.get_child("m_connection");
   for (const auto& kv : connections) {
     connection connectionHdr = (connection){0};
     boost::property_tree::ptree ptConnection = kv.second;

     connectionHdr.arg_index = ptConnection.get<int32_t>("arg_index");
     connectionHdr.m_ip_layout_index = ptConnection.get<int32_t>("m_ip_layout_index");
     connectionHdr.mem_data_index = ptConnection.get<int32_t>("mem_data_index");

     TRACE(XclBinUtil::format("[%d]: arg_index: %d, m_ip_layout_index: %d, mem_data_index: %d",
               count, (unsigned int) connectionHdr.arg_index, 
               (unsigned int) connectionHdr.m_ip_layout_index,
               (unsigned int) connectionHdr.mem_data_index));

     // Write out the entire structure 
     TRACE_BUF("connection", reinterpret_cast<const char*>(&connectionHdr), sizeof(connection));
    _buf.write(reinterpret_cast<const char*>(&connectionHdr), sizeof(connection) );
    count++;
  }

  // -- The counts should match --
  if ( count != (unsigned int) connectivityHdr.m_count  ) {
    std::string errMsg = XclBinUtil::format("ERROR: Number of connection sections (%d) does not match expected encoded value: %d",
                                      (unsigned int) count, (unsigned int) connectivityHdr.m_count);
    throw std::runtime_error(errMsg);
  }
}

enum IP_TYPE
XclBinData::getIPType( std::string &_sIPType ) const
{
  if ( _sIPType == "IP_MB" ) return IP_MB;
  if ( _sIPType == "IP_KERNEL" ) return IP_KERNEL;
  if ( _sIPType == "IP_DNASC" ) return IP_DNASC;
  if ( _sIPType == "IP_DDR4_CONTROLLER" ) return IP_DDR4_CONTROLLER;

  std::string errMsg = "ERROR: Unknown IP type: '" + _sIPType + "'";
  throw std::runtime_error(errMsg);
}


void
XclBinData::createIPLayoutBinaryImage( boost::property_tree::ptree &_pt, 
                                       std::ostringstream &_buf)
{
  // Initialize the memory to zero's
  ip_layout ipLayoutHdr = (ip_layout){0};  

  // Read, store, and report mem_topology data
  ipLayoutHdr.m_count = _pt.get<uint32_t>("m_count");

  if ( ipLayoutHdr.m_count == 0) {
    std::cout << "WARNING: Skipping IP_LAYOUT section for count size is zero." << std::endl;
    return;
  }

  TRACE("IP_LAYOUT");
  TRACE(XclBinUtil::format("m_count: %d", ipLayoutHdr.m_count));

  // Write out the entire structure except for the mem_data structure
  TRACE_BUF("ip_layout - minus ip_data", reinterpret_cast<const char*>(&ipLayoutHdr), (sizeof(ip_layout) - sizeof(ip_data)));
  _buf.write(reinterpret_cast<const char*>(&ipLayoutHdr), sizeof(ip_layout) - sizeof(ip_data) );


  // Read, store, and report connection segments
  unsigned int count = 0;
  boost::property_tree::ptree ipDatas = _pt.get_child("m_ip_data");
  for (const auto& kv : ipDatas) {
    ip_data ipDataHdr = (ip_data){0};      
    boost::property_tree::ptree ptIPData = kv.second;

    std::string sm_type = ptIPData.get<std::string>("m_type");
    ipDataHdr.m_type = getIPType( sm_type );

    std::string sProperties = ptIPData.get<std::string>("properties");
    ipDataHdr.properties = (uint32_t) XclBinUtil::stringToUInt64(sProperties);

    std::string sBaseAddress = ptIPData.get<std::string>("m_base_address");
    if ( sBaseAddress != "not_used" ) {
      ipDataHdr.m_base_address = XclBinUtil::stringToUInt64(sBaseAddress);
    }
    else {
      ipDataHdr.m_base_address = (uint64_t) -1;
    }

    std::string sm_name = ptIPData.get<std::string>("m_name");
    if ( sm_name.length() >= sizeof(ip_data::m_name) ) {
      std::string errMsg = XclBinUtil::format("ERROR: The m_name entry length (%d), exceeds the allocated space (%d).  Name: '%s'",
                                        (unsigned int) sm_name.length(), (unsigned int) sizeof(ip_data::m_name), sm_name);
      throw std::runtime_error(errMsg);
    }

    // We already know that there is enough room for this string
    memcpy( ipDataHdr.m_name, sm_name.c_str(), sm_name.length() + 1);

    TRACE(XclBinUtil::format("[%d]: m_type: %d, properties: 0x%x, m_base_address: 0x%lx, m_name: '%s'",
              count,
              (unsigned int) ipDataHdr.m_type,
              (unsigned int) ipDataHdr.properties,
              ipDataHdr.m_base_address,
              ipDataHdr.m_name));

    // Write out the entire structure 
    TRACE_BUF("ip_data", reinterpret_cast<const char*>(&ipDataHdr), sizeof(ip_data));
    _buf.write(reinterpret_cast<const char*>(&ipDataHdr), sizeof(ip_data) );
    count++;
  }

  // -- The counts should match --
  if ( count != (unsigned int) ipLayoutHdr.m_count  ) {
    std::string errMsg = XclBinUtil::format("ERROR: Number of connection sections (%d) does not match expected encoded value: %d",
                                      (unsigned int) count, (unsigned int) ipLayoutHdr.m_count);
    throw std::runtime_error(errMsg);
  }
}

enum DEBUG_IP_TYPE
XclBinData::getDebugIPType( std::string &_sDebugIPType ) const
{
  if ( _sDebugIPType == "LAPC" )
      return LAPC;

  if ( _sDebugIPType == "ILA" )
      return ILA;

  if ( _sDebugIPType == "AXI_MM_MONITOR" )
      return AXI_MM_MONITOR;

  if ( _sDebugIPType == "AXI_TRACE_FUNNEL" )
      return AXI_TRACE_FUNNEL;

  if ( _sDebugIPType == "AXI_MONITOR_FIFO_LITE" )
      return AXI_MONITOR_FIFO_LITE;

  if ( _sDebugIPType == "AXI_MONITOR_FIFO_FULL" )
      return AXI_MONITOR_FIFO_FULL;

  if ( _sDebugIPType == "ACCEL_MONITOR" )
      return ACCEL_MONITOR;

  if ( _sDebugIPType == "TRACE_S2MM" )
      return TRACE_S2MM;

  if ( _sDebugIPType == "AXI_DMA" )
      return AXI_DMA;

  if ( _sDebugIPType == "AXI_STREAM_MONITOR" )
      return AXI_STREAM_MONITOR;

  if ( _sDebugIPType == "AXI_STREAM_PROTOCOL_CHECKER" )
      return AXI_STREAM_PROTOCOL_CHECKER;

  if ( _sDebugIPType == "UNDEFINED" )
      return UNDEFINED;

  std::string errMsg = "ERROR: Unknown IP type: '" + _sDebugIPType + "'";
  throw std::runtime_error(errMsg);
}


void
XclBinData::createDebugIPLayoutBinaryImage( boost::property_tree::ptree &_pt, 
                                            std::ostringstream &_buf)
{
  // Initialize the memory to zero's
  debug_ip_layout debugIpLayoutHdr = (debug_ip_layout){0};  

  // Read, store, and report mem_topology data
  debugIpLayoutHdr.m_count = _pt.get<uint16_t>("m_count");

  TRACE("DEBUG_IP_LAYOUT");
  TRACE(XclBinUtil::format("m_count: %d", debugIpLayoutHdr.m_count));

  if ( debugIpLayoutHdr.m_count == 0) {
    std::cout << "WARNING: Skipping DEBUG_IP_LAYOUT section for count size is zero." << std::endl;
    return;
  }

  // Write out the entire structure except for the mem_data structure
  TRACE_BUF("debug_ip_layout - minus debug_ip_data", reinterpret_cast<const char*>(&debugIpLayoutHdr), (sizeof(debug_ip_layout) - sizeof(debug_ip_data)));
  _buf.write(reinterpret_cast<const char*>(&debugIpLayoutHdr), sizeof(debug_ip_layout) - sizeof(debug_ip_data) );
  
  // Read, store, and report connection segments
  unsigned int count = 0;
  boost::property_tree::ptree debugIpDatas = _pt.get_child("m_debug_ip_data");
  for (const auto& kv : debugIpDatas) {
    debug_ip_data debugIpDataHdr = (debug_ip_data){0};      
    boost::property_tree::ptree ptDebugIPData = kv.second;

    std::string sm_type = ptDebugIPData.get<std::string>("m_type");
    debugIpDataHdr.m_type = getDebugIPType( sm_type );

    uint16_t index = ptDebugIPData.get<uint16_t>("m_index");
    debugIpDataHdr.m_index_lowbyte = index & 0x00FF;
    debugIpDataHdr.m_index_highbyte = (index & 0xFF00) >> 8;

    debugIpDataHdr.m_properties = ptDebugIPData.get<uint8_t>("m_properties");

    // Optional value, will set to 0 if not set (as it was initialized)
    debugIpDataHdr.m_major = ptDebugIPData.get<uint8_t>("m_major", 0);
    // Optional value, will set to 0 if not set (as it was initialized)
    debugIpDataHdr.m_minor = ptDebugIPData.get<uint8_t>("m_minor", 0);

    std::string sBaseAddress = ptDebugIPData.get<std::string>("m_base_address");
    debugIpDataHdr.m_base_address = XclBinUtil::stringToUInt64(sBaseAddress);

    std::string sm_name = ptDebugIPData.get<std::string>("m_name");
    if ( sm_name.length() >= sizeof(debug_ip_data::m_name) ) {
      std::string errMsg = XclBinUtil::format("ERROR: The m_name entry length (%d), exceeds the allocated space (%d).  Name: '%s'",
                              (unsigned int) sm_name.length(), (unsigned int) sizeof(debug_ip_data::m_name), sm_name);
      throw std::runtime_error(errMsg);
    }

    // We already know that there is enough room for this string
    memcpy( debugIpDataHdr.m_name, sm_name.c_str(), sm_name.length() + 1);

    TRACE(XclBinUtil::format("[%d]: m_type: %d, m_index: %d (m_index_highbyte: 0x%x, m_index_lowbyte: 0x%x), m_properties: %d, m_major: %d, m_minor: %d, m_base_address: 0x%lx, m_name: '%s'", 
                             count,
                             (unsigned int) debugIpDataHdr.m_type,
                             index,
                             (unsigned int) debugIpDataHdr.m_index_highbyte,
                             (unsigned int) debugIpDataHdr.m_index_lowbyte,
                             (unsigned int) debugIpDataHdr.m_properties,
                             (unsigned int) debugIpDataHdr.m_major,
                             (unsigned int) debugIpDataHdr.m_minor,
                             debugIpDataHdr.m_base_address,
                             debugIpDataHdr.m_name));

    // Write out the entire structure 
    TRACE_BUF("debug_ip_data", reinterpret_cast<const char*>(&debugIpDataHdr), sizeof(debug_ip_data));
    _buf.write(reinterpret_cast<const char*>(&debugIpDataHdr), sizeof(debug_ip_data) );
    count++;
  }

  // -- The counts should match --
  if ( count != debugIpLayoutHdr.m_count  ) {
    std::string errMsg = XclBinUtil::format("ERROR: Number of connection sections (%d) does not match expected encoded value: %d",
                                      (unsigned int) count, (unsigned int) debugIpLayoutHdr.m_count);
    throw std::runtime_error(errMsg);
  }
}

enum CLOCK_TYPE
XclBinData::getClockType( std::string &_sClockType ) const
{
  if ( _sClockType == "UNUSED" )
      return CT_UNUSED;

  if ( _sClockType == "DATA" )
      return CT_DATA;

  if ( _sClockType == "KERNEL" )
      return CT_KERNEL;

  if ( _sClockType == "SYSTEM" )
      return CT_SYSTEM;

  std::string errMsg = "ERROR: Unknown Clock Type: '" + _sClockType + "'";
  throw std::runtime_error(errMsg);
}


void
XclBinData::createClockFreqTopologyBinaryImage( boost::property_tree::ptree &_pt, 
                                                std::ostringstream &_buf)
{
  // Initialize the memory to zero's
  clock_freq_topology clockFreqTopologyHdr = (clock_freq_topology){0};  

  // Read, store, and report clock frequency topology data
  clockFreqTopologyHdr.m_count = _pt.get<uint16_t>("m_count");

  TRACE("CLOCK_FREQ_TOPOLOGY");
  TRACE(XclBinUtil::format("m_count: %d", clockFreqTopologyHdr.m_count));

  if ( clockFreqTopologyHdr.m_count == 0) {
    std::cout << "WARNING: Skipping CLOCK_FREQ_TOPOLOGY section for count size is zero." << std::endl;
    return;
  }

  // Write out the entire structure except for the mem_data structure
  TRACE_BUF("clock_freq_topology- minus clock_freq", reinterpret_cast<const char*>(&clockFreqTopologyHdr), (sizeof(clock_freq_topology) - sizeof(clock_freq)));
  _buf.write(reinterpret_cast<const char*>(&clockFreqTopologyHdr), sizeof(clock_freq_topology) - sizeof(clock_freq) );
  
  // Read, store, and report connection segments
  unsigned int count = 0;
  boost::property_tree::ptree clockFreqs = _pt.get_child("m_clock_freq");
  for (const auto& kv : clockFreqs) {
    clock_freq clockFreqHdr = (clock_freq){0};      
    boost::property_tree::ptree ptClockFreq = kv.second;

    clockFreqHdr.m_freq_Mhz = ptClockFreq.get<u_int16_t>("m_freq_Mhz");
    std::string sm_type = ptClockFreq.get<std::string>("m_type");
    clockFreqHdr.m_type = getClockType( sm_type );

    std::string sm_name = ptClockFreq.get<std::string>("m_name");
    if ( sm_name.length() >= sizeof(clock_freq::m_name) ) {
      std::string errMsg = XclBinUtil::format("ERROR: The m_name entry length (%d), exceeds the allocated space (%d).  Name: '%s'",
                                        (unsigned int) sm_name.length(), (unsigned int) sizeof(clock_freq::m_name), sm_name);
      throw std::runtime_error(errMsg);
    }

    // We already know that there is enough room for this string
    memcpy( clockFreqHdr.m_name, sm_name.c_str(), sm_name.length() + 1);

    TRACE(XclBinUtil::format("[%d]: m_freq_Mhz: %d, m_type: %d, m_name: '%s'", 
              count,
              (unsigned int) clockFreqHdr.m_freq_Mhz,
              (unsigned int) clockFreqHdr.m_type,
              clockFreqHdr.m_name));

    // Write out the entire structure 
    TRACE_BUF("clock_freq", reinterpret_cast<const char*>(&clockFreqHdr), sizeof(clock_freq));
    _buf.write(reinterpret_cast<const char*>(&clockFreqHdr), sizeof(clock_freq) );
    count++;
  }

  // -- The counts should match --
  if ( count != (unsigned int) clockFreqTopologyHdr.m_count  ) {
    std::string errMsg = XclBinUtil::format("ERROR: Number of connection sections (%d) does not match expected encoded value: %d",
                                      (unsigned int) count, (unsigned int) clockFreqTopologyHdr.m_count);
    throw std::runtime_error(errMsg);
  }
}


void
XclBinData::createBinaryImages()
{
  // Look at each PT tree
  for (auto &x : m_ptree_segments) {
    // Get the Property Tree associated with the JSON file
    boost::property_tree::ptree ptRoot = x.second;   
    TRACE("Examining the property tree from the JSON's file: '" + x.first + "'");

    TRACE("Property Tree: Root");
    TRACE_PrintTree("Root", ptRoot);

    // Iterate over the root entries
    for (boost::property_tree::ptree::iterator ptSegment = ptRoot.begin(); ptSegment != ptRoot.end(); ++ptSegment) {
      TRACE("Processing: '" + ptSegment->first + "'");
     
      // ---------------------------------------------------------------------
      if ( ptSegment->first == "schema_version" ) {
        TRACE("Examining the version schema in the JSON file: '" + x.first + "'");
        SchemaVersion schemaVersion;
        getSchemaVersion(ptSegment->second, schemaVersion);
        continue;
      } 

      // ---------------------------------------------------------------------
      // Segment: MEM_TOPOLOGY 
      if ( ptSegment->first == "mem_topology" ) {
        // Check to see if there are any before us
        if ( m_memTopologyBuf.tellp() > 0 ) {
          throw std::runtime_error("ERROR: Only 1 MEM_TOPOLOGY segment permitted.");
        }

        TRACE("Examining MEM_TOPOLOGY section in the JSON file: '" + x.first + "'");
        createMemTopologyBinaryImage(ptSegment->second, m_memTopologyBuf);
        continue;
      }

      // ---------------------------------------------------------------------
      // Segment: CONNECTIVITY 
      if ( ptSegment->first == "connectivity" ) {
        // Check to see if there are any before us
        if ( m_connectivityBuf.tellp() > 0 ) {
          throw std::runtime_error("ERROR: Only 1 CONNECTIVITY segment permitted.");
        }

        TRACE("Examining CONNECTIVITY section in the JSON file: '" + x.first + "'");
        createConnectivityBinaryImage(ptSegment->second, m_connectivityBuf);
        continue;
      }

      // ---------------------------------------------------------------------
      // Segment: IP_LAYOUT
      if ( ptSegment->first == "ip_layout" ) {
        // Check to see if there are any before us
        if ( m_ipLayoutBuf.tellp() > 0 ) {
          throw std::runtime_error("ERROR: Only 1 IP_LAYOUT segment permitted.");
        }

        TRACE("Examining IP_LAYOUT section in the JSON file: '" + x.first + "'");
        createIPLayoutBinaryImage(ptSegment->second, m_ipLayoutBuf);
        continue;
      }

      // ---------------------------------------------------------------------
      // Segment: DEBUG_IP_LAYOUT
      if ( ptSegment->first == "debug_ip_layout" ) {
        // Check to see if there are any before us
        if ( m_debugIpLayoutBuf.tellp() > 0 ) {
          throw std::runtime_error("ERROR: Only 1 DEBUG_IP_LAYOUT segment permitted.");
        }

        TRACE("Examining DEBUG_IP_LAYOUT section in the JSON file: '" + x.first + "'");
        createDebugIPLayoutBinaryImage(ptSegment->second, m_debugIpLayoutBuf);
        continue;
      }

      // ---------------------------------------------------------------------
      // Segment: CLOCK_FREQ_TOPOLOGY
      if ( ptSegment->first == "clock_freq_topology" ) {
        // Check to see if there are any before us
        if ( m_clockFreqTopologyBuf.tellp() > 0 ) {
          throw std::runtime_error("ERROR: Only 1 CLOCK_FREQ_TOPOLOGY segment permitted.");
        }

        TRACE("Examining CLOCK_FREQ_TOPOLOGY section in the JSON file: '" + x.first + "'");
        createClockFreqTopologyBinaryImage(ptSegment->second, m_clockFreqTopologyBuf);
        continue;
      }

      TRACE("Skipping section: " + ptSegment->first);
    }
  }
}


void 
XclBinData::addPTreeSchemaVersion( boost::property_tree::ptree &_pt, SchemaVersion const &_schemaVersion)
{

  TRACE("");
  TRACE("Adding Versioning Properties");

  boost::property_tree::ptree pt_schemaVersion;

  TRACE(XclBinUtil::format("major: %d, minor: %d, patch: %d", 
                     _schemaVersion.major, 
                     _schemaVersion.minor, 
                     _schemaVersion.patch));

  pt_schemaVersion.put("major", XclBinUtil::format("%d", _schemaVersion.major).c_str());
  pt_schemaVersion.put("minor", XclBinUtil::format("%d", _schemaVersion.minor).c_str());
  pt_schemaVersion.put("patch", XclBinUtil::format("%d", _schemaVersion.patch).c_str());
  _pt.add_child("schema_version", pt_schemaVersion);
}


void
XclBinData::getSchemaVersion(boost::property_tree::ptree &_pt, SchemaVersion &_schemaVersion)
{
  TRACE("SchemaVersion");

  _schemaVersion.major = _pt.get<unsigned int>("major");
  _schemaVersion.minor = _pt.get<unsigned int>("minor");
  _schemaVersion.patch = _pt.get<unsigned int>("patch");

  TRACE(XclBinUtil::format("major: %d, minor: %d, patch: %d", 
                     _schemaVersion.major,
                     _schemaVersion.minor,
                     _schemaVersion.patch));
}

unsigned int 
XclBinData::getJSONBufferSegmentCount() 
{
  unsigned int count = 0;

  if ( m_memTopologyBuf.tellp() > 0 ) 
    ++count;

  if ( m_connectivityBuf.tellp() > 0 ) 
    ++count;

  if ( m_ipLayoutBuf.tellp() > 0 ) 
    ++count;

  if ( m_debugIpLayoutBuf.tellp() > 0 ) 
    ++count;

  if ( m_clockFreqTopologyBuf.tellp() > 0 ) 
    ++count;

  return count;
}

const std::string 
XclBinData::getMemTypeStr(enum MEM_TYPE _memType) const
{
  switch ( _memType ) {
    case MEM_DDR3: return "MEM_DDR3";
    case MEM_DDR4: return "MEM_DDR4";
    case MEM_DRAM: return "MEM_DRAM";
    case MEM_HBM:  return "MEM_HBM";
    case MEM_BRAM: return "MEM_BRAM";
    case MEM_URAM: return "MEM_URAM";
    case MEM_STREAMING: return "MEM_STREAMING";
    case MEM_PREALLOCATED_GLOB: return "MEM_PREALLOCATED_GLOB";
    case MEM_ARE: return "MEM_ARE";
    case MEM_STREAMING_CONNECTION: return "MEM_STREAMING_CONNECTION";
  }

  return XclBinUtil::format("UNKNOWN (%d)", (unsigned int) _memType);
}

const std::string 
XclBinData::getMCSTypeStr(enum MCS_TYPE _mcsType) const
{
  switch ( _mcsType ) {
    case MCS_PRIMARY:   return "MCS_PRIMARY";
    case MCS_SECONDARY: return "MCS_SECONDARY";
    case MCS_UNKNOWN:
    default:
      return XclBinUtil::format("UNKNOWN (%d)", (unsigned int) _mcsType);
  }
}


void 
XclBinData::extractMemTopologyData( char * _pDataSegment, 
                                    unsigned int _segmentSize,
                                    boost::property_tree::ptree & _ptree) 
{
  TRACE("");
  TRACE("Extracting: MEM_TOPOLOGY");
  TRACE_BUF("Segment Buffer", reinterpret_cast<const char*>(_pDataSegment), _segmentSize);
  
  // Do we have enough room to overlay the header structure
  if ( _segmentSize < sizeof(mem_topology) ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) is smaller than the size of the mem_topology structure (%d)", 
                                          _segmentSize, sizeof(mem_topology)));
  }

  mem_topology *pHdr = (mem_topology *)_pDataSegment;
  boost::property_tree::ptree mem_topology;

  TRACE(XclBinUtil::format("m_count: %d", pHdr->m_count));

  // Write out the entire structure except for the array structure
  TRACE_BUF("mem_topology", reinterpret_cast<const char*>(pHdr), (unsigned long) &(pHdr->m_mem_data[0]) - (unsigned long) pHdr);
  mem_topology.put("m_count", XclBinUtil::format("%d", (unsigned int) pHdr->m_count).c_str());

  unsigned int expectedSize = ((unsigned long) &(pHdr->m_mem_data[0]) - (unsigned long) pHdr)  + (sizeof(mem_data) * pHdr->m_count);

  if ( _segmentSize != expectedSize ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) does not match expected segments size (%d).", 
                                          _segmentSize, expectedSize));
  }

  boost::property_tree::ptree m_mem_data;
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree mem_data;

    TRACE(XclBinUtil::format("[%d]: m_type: %s, m_used: %d, m_sizeKB: 0x%lx, m_tag: '%s', m_base_address: 0x%lx", 
                       index,
                       getMemTypeStr((enum MEM_TYPE) pHdr->m_mem_data[index].m_type).c_str(),
                       (unsigned int) pHdr->m_mem_data[index].m_used,
                       pHdr->m_mem_data[index].m_size,
                       pHdr->m_mem_data[index].m_tag,
                       pHdr->m_mem_data[index].m_base_address));

    // Write out the entire structure 
    TRACE_BUF("mem_data", reinterpret_cast<const char*>(&(pHdr->m_mem_data[index])), sizeof(mem_data));

    mem_data.put("m_type", getMemTypeStr((enum MEM_TYPE) pHdr->m_mem_data[index].m_type).c_str());
    mem_data.put("m_used", XclBinUtil::format("%d", (unsigned int) pHdr->m_mem_data[index].m_used).c_str());
    mem_data.put("m_sizeKB", XclBinUtil::format("0x%lx", pHdr->m_mem_data[index].m_size).c_str());
    mem_data.put("m_tag", XclBinUtil::format("%s", pHdr->m_mem_data[index].m_tag).c_str());
    mem_data.put("m_base_address", XclBinUtil::format("0x%lx", pHdr->m_mem_data[index].m_base_address).c_str());

    m_mem_data.add_child("mem_data", mem_data);
  }

  mem_topology.add_child("m_mem_data", m_mem_data);

  _ptree.add_child("mem_topology", mem_topology);
  TRACE("-----------------------------");
}


void 
XclBinData::extractConnectivityData( char * _pDataSegment, 
                                     unsigned int _segmentSize,
                                     boost::property_tree::ptree & _ptree) 
{
  TRACE("");
  TRACE("Extracting: CONNECTIVITY");
  TRACE_BUF("Segment Buffer", reinterpret_cast<const char*>(_pDataSegment), _segmentSize);

  // Do we have enough room to overlay the header structure
  if ( _segmentSize < sizeof(connectivity) ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) is smaller than the size of the connectivity structure (%d)",
                                          _segmentSize, sizeof(connectivity)));
  }

  connectivity *pHdr = (connectivity *) _pDataSegment;
  boost::property_tree::ptree connectivity;

  TRACE(XclBinUtil::format("m_count: %d", (unsigned int) pHdr->m_count));

  // Write out the entire structure except for the array structure
  TRACE_BUF("connectivity", reinterpret_cast<const char*>(pHdr), (unsigned long) &(pHdr->m_connection[0]) - (unsigned long) pHdr);
  connectivity.put("m_count", XclBinUtil::format("%d", (unsigned int) pHdr->m_count).c_str());

  unsigned int expectedSize = ((unsigned long) &(pHdr->m_connection[0]) - (unsigned long) pHdr)  + (sizeof(connection) * pHdr->m_count);

  if ( _segmentSize != expectedSize ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) does not match expected segments size (%d).",
                                          _segmentSize, expectedSize));
  }

  boost::property_tree::ptree m_connection;
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree connection;


     TRACE(XclBinUtil::format("[%d]: arg_index: %d, m_ip_layout_index: %d, mem_data_index: %d",
               index,
               (unsigned int) pHdr->m_connection[index].arg_index,
               (unsigned int) pHdr->m_connection[index].m_ip_layout_index,
               (unsigned int) pHdr->m_connection[index].mem_data_index));

    // Write out the entire structure 
    TRACE_BUF("connection", reinterpret_cast<const char*>(&(pHdr->m_connection[index])), sizeof(connection));

    connection.put("arg_index", XclBinUtil::format("%d", (unsigned int) pHdr->m_connection[index].arg_index).c_str());
    connection.put("m_ip_layout_index", XclBinUtil::format("%d", (unsigned int) pHdr->m_connection[index].m_ip_layout_index).c_str());
    connection.put("mem_data_index", XclBinUtil::format("%d", (unsigned int) pHdr->m_connection[index].mem_data_index).c_str());

    m_connection.add_child("connection", connection);
  }

  connectivity.add_child("m_connection", m_connection);

  _ptree.add_child("connectivity", connectivity);
  TRACE("-----------------------------");
}

const std::string 
XclBinData::getIPTypeStr(enum IP_TYPE _ipType) const
{
  switch ( _ipType ) {
    case IP_MB: return "IP_MB";
    case IP_KERNEL: return "IP_KERNEL";
    case IP_DNASC: return "IP_DNASC";
    case IP_DDR4_CONTROLLER: return "IP_DDR4_CONTROLLER";
    case IP_MEM_DDR4: return "IP_MEM_DDR4";
    case IP_MEM_HBM: return "IP_MEM_DDR4";
  }

  return XclBinUtil::format("UNKNOWN (%d)", (unsigned int) _ipType);
}


void 
XclBinData::extractIPLayoutData( char * _pDataSegment, 
                                 unsigned int _segmentSize,
                                 boost::property_tree::ptree & _ptree) 
{
  TRACE("");
  TRACE("Extracting: IP_LAYOUT");
  TRACE_BUF("Segment Buffer", reinterpret_cast<const char*>(_pDataSegment), _segmentSize);

  // Do we have enough room to overlay the header structure
  if ( _segmentSize < sizeof(ip_layout) ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) is smaller than the size of the ip_layout structure (%d)",
                                          _segmentSize, sizeof(ip_layout)));
  }

  ip_layout *pHdr = (ip_layout *)_pDataSegment;
  boost::property_tree::ptree ip_layout;

  TRACE(XclBinUtil::format("m_count: %d", pHdr->m_count));

   // Write out the entire structure except for the array structure
  TRACE_BUF("ip_layout", reinterpret_cast<const char*>(pHdr), (unsigned long) &(pHdr->m_ip_data[0]) - (unsigned long) pHdr);
  ip_layout.put("m_count", XclBinUtil::format("%d", (unsigned int) pHdr->m_count).c_str());

  unsigned int expectedSize = ((unsigned long) &(pHdr->m_ip_data[0]) - (unsigned long) pHdr)  + (sizeof(ip_data) * pHdr->m_count);

  if ( _segmentSize != expectedSize ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) does not match expected segments size (%d).",
                                          _segmentSize, expectedSize));
  }

  boost::property_tree::ptree m_ip_data;
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree ip_data;

    TRACE(XclBinUtil::format("[%d]: m_type: %s, properties: 0x%x, m_base_address: 0x%lx, m_name: '%s'",
                       index,
                       getIPTypeStr((enum IP_TYPE) pHdr->m_ip_data[index].m_type).c_str(),
                       pHdr->m_ip_data[index].properties,
                       pHdr->m_ip_data[index].m_base_address,
                       pHdr->m_ip_data[index].m_name));

    // Write out the entire structure 
    TRACE_BUF("ip_data", reinterpret_cast<const char*>(&(pHdr->m_ip_data[index])), sizeof(ip_data));

    ip_data.put("m_type", getIPTypeStr((enum IP_TYPE) pHdr->m_ip_data[index].m_type).c_str());
    ip_data.put("properties", XclBinUtil::format("0x%x", pHdr->m_ip_data[index].properties).c_str());
    if ( pHdr->m_ip_data[index].m_base_address != ((uint64_t) -1) ) {
      ip_data.put("m_base_address", XclBinUtil::format("0x%lx", pHdr->m_ip_data[index].m_base_address).c_str());
    } else {
      ip_data.put("m_base_address", "not_used");
    }
    ip_data.put("m_name", XclBinUtil::format("%s", pHdr->m_ip_data[index].m_name).c_str());

    m_ip_data.add_child("ip_data", ip_data);
  }

  ip_layout.add_child("m_ip_data", m_ip_data);

  _ptree.add_child("ip_layout", ip_layout);
  TRACE("-----------------------------");
}

const std::string 
XclBinData::getDebugIPTypeStr(enum DEBUG_IP_TYPE _debugIpType) const
{
  switch ( _debugIpType ) {
    case UNDEFINED: return "UNDEFINED";
    case LAPC: return "LAPC";
    case ILA: return "ILA";
    case AXI_MM_MONITOR: return "AXI_MM_MONITOR";
    case AXI_TRACE_FUNNEL: return "AXI_TRACE_FUNNEL";
    case AXI_MONITOR_FIFO_LITE: return "AXI_MONITOR_FIFO_LITE";
    case AXI_MONITOR_FIFO_FULL: return "AXI_MONITOR_FIFO_FULL";
    case ACCEL_MONITOR: return "ACCEL_MONITOR";
    case AXI_DMA: return "AXI_DMA";
    case TRACE_S2MM: return "TRACE_S2MM";
    case AXI_STREAM_MONITOR: return "AXI_STREAM_MONITOR";
    case AXI_STREAM_PROTOCOL_CHECKER: return "AXI_STREAM_PROTOCOL_CHECKER";
  }

  return XclBinUtil::format("UNKNOWN (%d)", (unsigned int) _debugIpType);
}


void 
XclBinData::extractDebugIPLayoutData( char * _pDataSegment, 
                                      unsigned int _segmentSize,
                                      boost::property_tree::ptree & _ptree) 
{
  TRACE("");
  TRACE("Extracting: DEBUG_IP_LAYOUT");
  TRACE_BUF("Segment Buffer", reinterpret_cast<const char*>(_pDataSegment), _segmentSize);

  // Do we have enough room to overlay the header structure
  if ( _segmentSize < sizeof(debug_ip_layout) ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) is smaller than the size of the debug_ip_layout structure (%d)", 
                                                _segmentSize, sizeof(debug_ip_layout)));
  }

  debug_ip_layout *pHdr = (debug_ip_layout *)_pDataSegment;
  boost::property_tree::ptree debug_ip_layout;

  TRACE(XclBinUtil::format("m_count: %d", (uint32_t) pHdr->m_count));

  // Write out the entire structure except for the array structure
  TRACE_BUF("ip_layout", reinterpret_cast<const char*>(pHdr), (unsigned long) &(pHdr->m_debug_ip_data[0]) - (unsigned long) pHdr);
  debug_ip_layout.put("m_count", XclBinUtil::format("%d", (unsigned int) pHdr->m_count).c_str());

  debug_ip_data mydata = (debug_ip_data){0};
  

  TRACE(XclBinUtil::format("Size of debug_ip_data: %d\nSize of mydata: %d", 
                           sizeof(debug_ip_data),
                           sizeof(mydata)));
  unsigned int expectedSize = ((unsigned long) &(pHdr->m_debug_ip_data[0]) - (unsigned long) pHdr)  + (sizeof(debug_ip_data) * (uint32_t) pHdr->m_count);

  if ( _segmentSize != expectedSize ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) does not match expected segments size (%d).", 
                                                _segmentSize, expectedSize));
  }


  boost::property_tree::ptree m_debug_ip_data;
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree debug_ip_data;

    uint16_t m_virtual_index = (((uint16_t) pHdr->m_debug_ip_data[index].m_index_highbyte) << 8) + (uint16_t) pHdr->m_debug_ip_data[index].m_index_lowbyte;

    TRACE(XclBinUtil::format("[%d]: m_type: %d, m_index: %d (m_index_highbyte: 0x%x, m_index_lowbyte: 0x%x), m_properties: %d, m_major: %d, m_minor: %d, m_base_address: 0x%lx, m_name: '%s'", 
                             index,
                             getDebugIPTypeStr((enum DEBUG_IP_TYPE) pHdr->m_debug_ip_data[index].m_type).c_str(),
                             (unsigned int) m_virtual_index,
                             (unsigned int) pHdr->m_debug_ip_data[index].m_index_highbyte,
                             (unsigned int) pHdr->m_debug_ip_data[index].m_index_lowbyte,
                             (unsigned int) pHdr->m_debug_ip_data[index].m_properties,
                             (unsigned int) pHdr->m_debug_ip_data[index].m_major,
                             (unsigned int) pHdr->m_debug_ip_data[index].m_minor,
                             pHdr->m_debug_ip_data[index].m_base_address,
                             pHdr->m_debug_ip_data[index].m_name));

    // Write out the entire structure 
    TRACE_BUF("debug_ip_data", reinterpret_cast<const char*>(&pHdr->m_debug_ip_data[index]), sizeof(debug_ip_data));

    debug_ip_data.put("m_type", getDebugIPTypeStr((enum DEBUG_IP_TYPE) pHdr->m_debug_ip_data[index].m_type).c_str());
    debug_ip_data.put("m_index", XclBinUtil::format("%d", (unsigned int) m_virtual_index).c_str());
    debug_ip_data.put("m_properties", XclBinUtil::format("%d", (unsigned int) pHdr->m_debug_ip_data[index].m_properties).c_str());
    debug_ip_data.put("m_major", XclBinUtil::format("%d", (unsigned int) pHdr->m_debug_ip_data[index].m_major).c_str());
    debug_ip_data.put("m_minor", XclBinUtil::format("%d", (unsigned int) pHdr->m_debug_ip_data[index].m_minor).c_str());
    debug_ip_data.put("m_base_address", XclBinUtil::format("0x%lx",  pHdr->m_debug_ip_data[index].m_base_address).c_str());
    debug_ip_data.put("m_name", XclBinUtil::format("%s", pHdr->m_debug_ip_data[index].m_name).c_str());

    m_debug_ip_data.add_child("debug_ip_data", debug_ip_data);
  }

  debug_ip_layout.add_child("m_debug_ip_data", m_debug_ip_data);

  _ptree.add_child("debug_ip_layout", debug_ip_layout);
  TRACE("-----------------------------");
}

const std::string 
XclBinData::getClockTypeStr(enum CLOCK_TYPE _clockType) const
{
  switch ( _clockType ) {
    case CT_UNUSED: return "UNUSED";
    case CT_DATA:   return "DATA";
    case CT_KERNEL: return "KERNEL";
    case CT_SYSTEM: return "SYSTEM";
  }

  return XclBinUtil::format("UNKNOWN (%d) CLOCK_TYPE", (unsigned int) _clockType);
}


void 
XclBinData::extractClockFreqTopology( char * _pDataSegment, 
                                      unsigned int _segmentSize,
                                      boost::property_tree::ptree & _ptree) 
{
  TRACE("");
  TRACE("Extracting: ClockFreqTopology");
  TRACE_BUF("Segment Buffer", reinterpret_cast<const char*>(_pDataSegment), _segmentSize);

  // Do we have enough room to overlay the header structure
  if ( _segmentSize < sizeof(clock_freq_topology) ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) is smaller than the size of the clock_freq_topology structure (%d)",
                                                _segmentSize, sizeof(clock_freq_topology)));
  }

  clock_freq_topology *pHdr = (clock_freq_topology *) _pDataSegment;
  boost::property_tree::ptree clock_freq_topology;

  TRACE(XclBinUtil::format("m_count: %d", (uint32_t) pHdr->m_count));

  // Write out the entire structure except for the array structure
  TRACE_BUF("clock_freq", reinterpret_cast<const char*>(pHdr), (unsigned long) &(pHdr->m_clock_freq[0]) - (unsigned long) pHdr);
  clock_freq_topology.put("m_count", XclBinUtil::format("%d", (unsigned int) pHdr->m_count).c_str());

  clock_freq mydata = (clock_freq){0};
  
  TRACE(XclBinUtil::format("Size of clock_freq: %d\nSize of mydata: %d", 
                            sizeof(clock_freq),
                            sizeof(mydata)));
  unsigned int expectedSize = ((unsigned long) &(pHdr->m_clock_freq[0]) - (unsigned long) pHdr)  + (sizeof(clock_freq) * (uint32_t) pHdr->m_count);

  if ( _segmentSize != expectedSize ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) does not match expected segments size (%d).", 
                                                _segmentSize, expectedSize));
  }


  boost::property_tree::ptree m_clock_freq;
  for (int index = 0; index < pHdr->m_count; ++index) {
    boost::property_tree::ptree clock_freq;

    TRACE(XclBinUtil::format("[%d]: m_freq_Mhz: %d, m_type: %s, m_name: '%s'", 
                             index,
                             (unsigned int) pHdr->m_clock_freq[index].m_freq_Mhz,
                             getClockTypeStr((enum CLOCK_TYPE) pHdr->m_clock_freq[index].m_type).c_str(),
                             pHdr->m_clock_freq[index].m_name));

    // Write out the entire structure 
    TRACE_BUF("clock_freq", reinterpret_cast<const char*>(&pHdr->m_clock_freq[index]), sizeof(clock_freq));

    clock_freq.put("m_freq_Mhz", XclBinUtil::format("%d", (unsigned int) pHdr->m_clock_freq[index].m_freq_Mhz).c_str());
    clock_freq.put("m_type", getClockTypeStr((enum CLOCK_TYPE) pHdr->m_clock_freq[index].m_type).c_str());
    clock_freq.put("m_name", XclBinUtil::format("%s", pHdr->m_clock_freq[index].m_name).c_str());

    m_clock_freq.add_child("clock_freq", clock_freq);
  }

  clock_freq_topology.add_child("m_clock_freq", m_clock_freq);

  _ptree.add_child("clock_freq_topology", clock_freq_topology);
  TRACE("-----------------------------");
}

void
XclBinData::createMCSSegmentBuffer(const std::vector< std::pair< std::string, enum MCS_TYPE> > & _mcs)
{
  // Must have something to work with
  int count = _mcs.size();
  if ( count == 0 )
    return;

  mcs mcsHdr = (mcs) {0};
  mcsHdr.m_count = (int8_t) count;

  TRACE("MCS");
  TRACE(XclBinUtil::format("m_count: %d", (int) mcsHdr.m_count));

  // Write out the entire structure except for the mcs structure
  TRACE_BUF("mcs - minus mcs_chunk", reinterpret_cast<const char*>(&mcsHdr), (sizeof(mcs) - sizeof(mcs_chunk)));
  m_mcsBuf.write(reinterpret_cast<const char*>(&mcsHdr), (sizeof(mcs) - sizeof(mcs_chunk)) );

  // Calculate The mcs_chunks data
  std::vector< mcs_chunk > mcsChunks;
  {
    uint64_t currentOffset = ((sizeof(mcs) - sizeof(mcs_chunk)) + 
                              (sizeof(mcs_chunk) * count));

    for ( auto mcsEntry : _mcs) {
      mcs_chunk mcsChunk = (mcs_chunk) {0};
      mcsChunk.m_type = mcsEntry.second;   // Record the MCS type

      // -- Determine if the file can be opened and its size --
      std::ifstream fs;
      fs.open( mcsEntry.first.c_str(), std::ifstream::in | std::ifstream::binary );
      fs.seekg( 0, fs.end );
      mcsChunk.m_size = fs.tellg();
      mcsChunk.m_offset = currentOffset;
      currentOffset += mcsChunk.m_size;

      if ( ! fs.is_open() ) {
        std::string errMsg = "ERROR: Could not open the file for reading: '" + mcsEntry.first + "'";
        throw std::runtime_error(errMsg);
      }

      fs.close();
      mcsChunks.push_back(mcsChunk);
    }
  }

  // Finish building the buffer
  // First the array
  {
    int index = 0;
    for (auto mcsChunk : mcsChunks) {
      TRACE(XclBinUtil::format("[%d]: m_type: %d, m_offset: 0x%lx, m_size: 0x%lx",
                               index++,
                               mcsChunk.m_type,
                               mcsChunk.m_offset,
                               mcsChunk.m_size));
      TRACE_BUF("mcs_chunk", reinterpret_cast<const char*>(&mcsChunk), sizeof(mcs_chunk));
      m_mcsBuf.write(reinterpret_cast<const char*>(&mcsChunk), sizeof(mcs_chunk) );
    }
  }

  // Second the data
  {
    int index = 0;
    for ( auto mcsEntry : _mcs) {
      // -- Determine if the file can be opened and its size --
      std::ifstream fs;
      fs.open( mcsEntry.first.c_str(), std::ifstream::in | std::ifstream::binary );
      fs.seekg( 0, fs.end );
      uint64_t mcsSize = fs.tellg();

      if ( ! fs.is_open() ) {
        std::string errMsg = "ERROR: Could not open the file for reading: '" + mcsEntry.first + "'";
        throw std::runtime_error(errMsg);
      }

      // -- Read contents into memory buffer --
      std::unique_ptr<unsigned char> memBuffer( new unsigned char[ mcsSize ] );
      fs.clear();
      fs.seekg( 0, fs.beg );
      fs.read( (char*) memBuffer.get(), mcsSize );
      fs.close();

      TRACE(XclBinUtil::format("[%d]: Adding file - size: 0x%lx, file: %s",
                               index++,
                               mcsSize,
                               mcsEntry.first.c_str()));
      m_mcsBuf.write(reinterpret_cast<const char*>(memBuffer.get()), mcsSize );
    }
  }
}

void
XclBinData::createBMCSegmentBuffer(const std::vector< std::string > & _bmc)
{
  // Must have something to work with
  int count = _bmc.size();
  if ( count == 0 )
    return;

  bmc bmcHdr = (bmc) {0};

  TRACE("BMC");
  
  // Determine if the file can be opened and its size
  std::string filePath = _bmc[0];
  {
    std::ifstream fs;
    fs.open( filePath.c_str(), std::ifstream::in | std::ifstream::binary );
    fs.seekg( 0, fs.end );
    bmcHdr.m_size = fs.tellg();
    bmcHdr.m_offset = sizeof(bmc);

    if ( ! fs.is_open() ) {
      std::string errMsg = "ERROR: Could not open the file for reading: '" + _bmc[0] + "'";
      throw std::runtime_error(errMsg);
    }
    fs.close();
  }
  
  // Break down the file name into its basic parts
  {
    // Assume that there isn't a path
    std::string baseFileName = filePath;      

    // Remove the path (if there is one)
    {
      size_t pos = filePath.find_last_of("/");
      if ( pos != std::string::npos) {        
        baseFileName.assign(filePath.begin() + pos + 1, filePath.end());
      }
    }

    // Remove the extension (if there is one)
    {
      const std::string ext(".txt");
      if ( (baseFileName != ext) &&
           (baseFileName.size() > ext.size()) &&
           (baseFileName.substr(baseFileName.size() - ext.size()) == ext)) {
        baseFileName = baseFileName.substr(0, baseFileName.size() - ext.size());
      }
    }

    // Tokenize the base name
    std::vector <std::string> tokens;
    boost::split(tokens, baseFileName, boost::is_any_of("-"));

    TRACE("BaseName: " + baseFileName);

    if ( tokens.size() != 4 ) {
      std::string errMsg = XclBinUtil::format("ERROR: Unexpected number of tokens (found %d, expected 4) parsing the file: %s",
                                              tokens.size(), baseFileName);
      throw std::runtime_error(errMsg);
    }

    // Token 0 - Image Name
    if ( tokens[0].length() >= sizeof(bmc::m_image_name) ) {
      std::string errMsg = XclBinUtil::format("ERROR: The m_image_name entry length (%d), exceeds the allocated space (%d).  Name: '%s'",
                                              (unsigned int) tokens[0].length(), (unsigned int) sizeof(bmc::m_image_name), tokens[0].c_str());
      throw std::runtime_error(errMsg);
    }
    memcpy( bmcHdr.m_image_name, tokens[0].c_str(), tokens[0].length() + 1);

    // Token 1 - Device Name
    if ( tokens[1].length() >= sizeof(bmc::m_device_name) ) {
      std::string errMsg = XclBinUtil::format("ERROR: The m_device_name entry length (%d), exceeds the allocated space (%d).  Name: '%s'",
                                              (unsigned int) tokens[1].length(), (unsigned int) sizeof(bmc::m_device_name), tokens[1].c_str());
      throw std::runtime_error(errMsg);
    }
    memcpy( bmcHdr.m_device_name, tokens[1].c_str(), tokens[1].length() + 1);

    // Token 2 - Version
    if ( tokens[2].length() >= sizeof(bmc::m_version) ) {
      std::string errMsg = XclBinUtil::format("ERROR: The m_version entry length (%d), exceeds the allocated space (%d).  Version: '%s'",
                                              (unsigned int) tokens[2].length(), (unsigned int) sizeof(bmc::m_version), tokens[2].c_str());
      throw std::runtime_error(errMsg);
    }
    memcpy( bmcHdr.m_version, tokens[2].c_str(), tokens[2].length() + 1);

    // Token 3 - MD5 Value
    if ( tokens[3].length() >= sizeof(bmc::m_md5value) ) {
      std::string errMsg = XclBinUtil::format("ERROR: The m_md5value entry length (%d), exceeds the allocated space (%d).  Value: '%s'",
                                              (unsigned int) tokens[3].length(), (unsigned int) sizeof(bmc::m_md5value), tokens[3].c_str());
      throw std::runtime_error(errMsg);
    }
    memcpy( bmcHdr.m_md5value, tokens[3].c_str(), tokens[3].length() + 1);
  }

  TRACE(XclBinUtil::format("m_offset: 0x%lx, m_size: 0x%lx, m_image_name: '%s', m_device_name: '%s', m_version: '%s', m_md5Value: '%s'", 
                           bmcHdr.m_offset,
                           bmcHdr.m_size,
                           bmcHdr.m_image_name,
                           bmcHdr.m_device_name,
                           bmcHdr.m_version,
                           bmcHdr.m_md5value));

  TRACE_BUF("bmc", reinterpret_cast<const char*>(&bmcHdr), sizeof(bmc));

  // Create the buffer
  m_bmcBuf.write(reinterpret_cast<const char*>(&bmcHdr), sizeof(bmc));

  // Write Data
  {
    std::ifstream fs;
    fs.open( filePath.c_str(), std::ifstream::in | std::ifstream::binary );

    if ( ! fs.is_open() ) {
      std::string errMsg = "ERROR: Could not open the file for reading: '" + filePath + "'";
      throw std::runtime_error(errMsg);
    }

    std::unique_ptr<unsigned char> memBuffer( new unsigned char[ bmcHdr.m_size ] );
    fs.clear();
    fs.read( (char*) memBuffer.get(), bmcHdr.m_size );
    fs.close();

    m_bmcBuf.write(reinterpret_cast<const char*>(memBuffer.get()), bmcHdr.m_size );
  }
}


void 
XclBinData::extractAndWriteMCSImages( char * _pDataSegment, 
                                      unsigned int _segmentSize) 
{
  TRACE("");
  TRACE("Extracting: MCS");

  // Do we have enough room to overlay the header structure
  if ( _segmentSize < sizeof(mcs) ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) is smaller than the size of the mcs structure (%d)",
                                                _segmentSize, sizeof(mcs)));
  }

  mcs *pHdr = (mcs *) _pDataSegment;

  TRACE(XclBinUtil::format("m_count: %d", (uint32_t) pHdr->m_count));
  TRACE_BUF("mcs", reinterpret_cast<const char*>(pHdr), (unsigned long) &(pHdr->m_chunk[0]) - (unsigned long) pHdr);
  
  // Do we have something to extract.  Note: This should never happen.  
  if ( pHdr->m_count == 0 ) {
    TRACE("m_count is zero, nothing to extract");
    return;
  }

  // Check to make sure that the array did not exceed its bounds
  unsigned int arraySize = ((unsigned long) &(pHdr->m_chunk[0]) - (unsigned long) pHdr)  + (sizeof(mcs_chunk) * pHdr->m_count);

  if ( arraySize > _segmentSize ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: m_chunk array size (0x%lx) exceeds segment size (0x%lx).", 
                                            arraySize, _segmentSize));
  }

  // Examine and extract the data
  for (int index = 0; index < pHdr->m_count; ++index) {
    TRACE(XclBinUtil::format("[%d]: m_type: %s, m_offset: 0x%lx, m_size: 0x%lx", 
                       index,
                       getMCSTypeStr((enum MCS_TYPE) pHdr->m_chunk[index].m_type).c_str(),
                       pHdr->m_chunk[index].m_offset,
                       pHdr->m_chunk[index].m_size));

    TRACE_BUF("m_chunk", reinterpret_cast<const char*>(&(pHdr->m_chunk[index])), sizeof(mcs_chunk));

    std::string fileName;
    switch ( pHdr->m_chunk[index].m_type ) {
      case MCS_PRIMARY: fileName = "primary.mcs"; break;
      case MCS_SECONDARY: fileName = "secondary.mcs"; break;
      default: fileName = XclBinUtil::format("unknown_idx_%d.mcs", index);
    }
   
    char * ptrImageBase = _pDataSegment + pHdr->m_chunk[index].m_offset;

    // Check to make sure that the MCS image is partially looking good
    if ( (unsigned long) ptrImageBase > ((unsigned long) _pDataSegment) + _segmentSize ) {
      throw std::runtime_error(XclBinUtil::format("ERROR: MCS image %d start offset exceeds MCS segment size.", index));
    }

    if ( ((unsigned long) ptrImageBase) + pHdr->m_chunk[index].m_size > ((unsigned long) _pDataSegment) + _segmentSize ) {
      throw std::runtime_error(XclBinUtil::format("ERROR: MCS image %d size exceeds the MCS segment size.", index));
    }

    std::fstream fs;
    fs.open( fileName, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc );
    if ( ! fs.is_open() ) {
      std::cerr << "ERROR: Could not open " << fileName << " for writing" << "\n";
      continue;
    }
    fs.write( ptrImageBase, pHdr->m_chunk[index].m_size );
  }
}

void 
XclBinData::extractAndWriteBMCImages( char * _pDataSegment, 
                                      unsigned int _segmentSize) 
{
  TRACE("");
  TRACE("Extracting: BMC");

  // Do we have enough room to overlay the header structure
  if ( _segmentSize < sizeof(bmc) ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: Segment size (%d) is smaller than the size of the bmc structure (%d)",
                                                _segmentSize, sizeof(bmc)));
  }

  bmc *pHdr = (bmc *) _pDataSegment;

  TRACE_BUF("bmc", reinterpret_cast<const char*>(pHdr), sizeof(bmc));
  
  TRACE(XclBinUtil::format("m_offset: 0x%lx, m_size: 0x%lx, m_image_name: '%s', m_device_name: '%s', m_version: '%s', m_md5Value: '%s'", 
                           pHdr->m_offset,
                           pHdr->m_size,
                           pHdr->m_image_name,
                           pHdr->m_device_name,
                           pHdr->m_version,
                           pHdr->m_md5value));

  unsigned int expectedSize = pHdr->m_offset + pHdr->m_size;

  // Check to see if array size  
  if ( expectedSize > _segmentSize ) {
    throw std::runtime_error(XclBinUtil::format("ERROR: bmc section size (0x%lx) exceeds the given segment size (0x%lx).", 
                                            expectedSize, _segmentSize));
  }

  std::string fileName = XclBinUtil::format("%s-%s-%s-%s.txt",
                                            pHdr->m_image_name,
                                            pHdr->m_device_name,
                                            pHdr->m_version,
                                            pHdr->m_md5value);

  TRACE("Writing BMC File: '" + fileName + "'");

  std::fstream fs;
  fs.open( fileName, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc );
  if ( ! fs.is_open() ) {
    std::cerr << "ERROR: Could not open " << fileName << " for writing" << "\n";
    return;
  }

  char * ptrImageBase = _pDataSegment + pHdr->m_offset;
  fs.write( ptrImageBase, pHdr->m_size );
}



