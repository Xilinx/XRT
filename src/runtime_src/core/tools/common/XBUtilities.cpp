/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "XBUtilities.h"
#include "core/common/error.h"
#include "core/common/utils.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"

#include "common/system.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>
#include <boost/tokenizer.hpp>
#include <boost/format.hpp>

// System - Include Files
#include <iostream>
#include <map>

#ifdef _WIN32
#pragma warning( disable : 4189 )
#pragma comment(lib, "Ws2_32.lib")
/* need to link the lib for the following to work */
#define be32toh ntohl
#define PALIGN(p, a) (const char*)NULL
#endif

#ifdef __GNUC__
#define ALIGN(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((char *)(ALIGN((unsigned long)(p), (a))))
#endif
#define GET_CELL(p)     (p += 4, *((const uint32_t *)(p-4)))

// ------ C O N S T A N T   V A R I A B L E S ---------------------------------
static const uint32_t FDT_BEGIN_NODE = 0x1;
static const uint32_t FDT_PROP = 0x3;
static const uint32_t FDT_END = 0x9;

// ------ L O C A L  F U N C T I O N S  A N D  S T R U C T S ------------------
struct fdt_header {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
};


// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBUtilities;

// ------ S T A T I C   V A R I A B L E S -------------------------------------
static bool m_bVerbose = false;
static bool m_bTrace = false;
static bool m_disableEscapeCodes = false;
static bool m_bShowHidden = false;


// ------ F U N C T I O N S ---------------------------------------------------
void 
XBUtilities::setVerbose(bool _bVerbose)
{
  bool prevVerbose = m_bVerbose;

  if ((prevVerbose == true) && (_bVerbose == false)) 
    verbose("Disabling Verbosity");

  m_bVerbose = _bVerbose;

  if ((prevVerbose == false) && (_bVerbose == true)) 
    verbose("Enabling Verbosity");
}

void 
XBUtilities::setTrace(bool _bTrace)
{
  if (_bTrace) 
    trace("Enabling Tracing");
  else 
    trace("Disabling Tracing");

  m_bTrace = _bTrace;
}


void 
XBUtilities::setShowHidden(bool _bShowHidden)
{
  if (_bShowHidden) 
    trace("Hidden commands and options will be shown.");
  else 
    trace("Hidden commands and options will be hidden");

  m_bShowHidden = _bShowHidden;
}

bool
XBUtilities::getShowHidden()
{
  return m_bShowHidden;
}

void 
XBUtilities::disable_escape_codes(bool _disable) 
{
  m_disableEscapeCodes = _disable;
}

bool 
XBUtilities::is_esc_enabled() {
  return m_disableEscapeCodes;
}


void 
XBUtilities::message_(MessageType _eMT, const std::string& _msg, bool _endl)
{
  static std::map<MessageType, std::string> msgPrefix = {
    { MT_MESSAGE, "" },
    { MT_INFO, "Info: " },
    { MT_WARNING, "Warning: " },
    { MT_ERROR, "Error: " },
    { MT_VERBOSE, "Verbose: " },
    { MT_FATAL, "Fatal: " },
    { MT_TRACE, "Trace: " },
    { MT_UNKNOWN, "<type unknown>: " },
  };

  // A simple DRC check
  if (_eMT > MT_UNKNOWN) {
    _eMT = MT_UNKNOWN;
  }

  // Verbosity is not enabled
  if ((m_bVerbose == false) && (_eMT == MT_VERBOSE)) {
      return;
  }

  // Tracing is not enabled
  if ((m_bTrace == false) && (_eMT == MT_TRACE)) {
      return;
  }

  std::cout << msgPrefix[_eMT] << _msg;

  if (_endl == true) {
    std::cout << std::endl;
  }
}

void 
XBUtilities::message(const std::string& _msg, bool _endl) 
{ 
  message_(MT_MESSAGE, _msg, _endl); 
}

void 
XBUtilities::info(const std::string& _msg, bool _endl)    
{ 
  message_(MT_INFO, _msg, _endl); 
}

void 
XBUtilities::warning(const std::string& _msg, bool _endl) 
{ 
  message_(MT_WARNING, _msg, _endl); 
}

void 
XBUtilities::error(const std::string& _msg, bool _endl)
{ 
  message_(MT_ERROR, _msg, _endl); 
}

void 
XBUtilities::verbose(const std::string& _msg, bool _endl) 
{ 
  message_(MT_VERBOSE, _msg, _endl); 
}

void 
XBUtilities::fatal(const std::string& _msg, bool _endl)   
{ 
  message_(MT_FATAL, _msg, _endl); 
}

void 
XBUtilities::trace(const std::string& _msg, bool _endl)   
{ 
  message_(MT_TRACE, _msg, _endl); 
}



void 
XBUtilities::trace_print_tree(const std::string & _name, 
                              const boost::property_tree::ptree & _pt)
{
  if (m_bTrace == false) {
    return;
  }

  XBUtilities::trace(_name + " (JSON Tree)");

  std::ostringstream buf;
  boost::property_tree::write_json(buf, _pt, true /*Pretty print*/);
  XBUtilities::message(buf.str());
}

void 
XBUtilities::wrap_paragraph( const std::string & _unformattedString, 
                             unsigned int _indentWidth, 
                             unsigned int _columnWidth, 
                             bool _indentFirstLine,
                             std::string &_formattedString)
{
  // Set return variables to a now state
  _formattedString.clear();

  if (_indentWidth >= _columnWidth) {
    std::string errMsg = boost::str(boost::format("Internal Error: %s paragraph indent (%d) is greater than or equal to the column width (%d) ") % __FUNCTION__ % _indentWidth % _columnWidth);
    throw std::runtime_error(errMsg);
  }

  const unsigned int paragraphWidth = _columnWidth - _indentWidth;

  std::string::const_iterator lineBeginIter = _unformattedString.begin();
  const std::string::const_iterator paragraphEndIter = _unformattedString.end();

  unsigned int linesProcessed = 0;

  while (lineBeginIter != paragraphEndIter)  
  {
    // Remove leading spaces
    if ((linesProcessed > 0) && 
        (*lineBeginIter == ' ')) {
      lineBeginIter++;
      continue;
    }

    // Determine the end-of-the line to be examined
    std::string::const_iterator lineEndIter = lineBeginIter;
    auto remainingChars = std::distance(lineBeginIter, paragraphEndIter);
    if (remainingChars < paragraphWidth)
      lineEndIter += remainingChars;
    else
      lineEndIter += paragraphWidth;

    // Not last line
    if (lineEndIter != paragraphEndIter) {
      // Find a break between the words
      std::string::const_iterator lastSpaceIter = find(std::reverse_iterator<std::string::const_iterator>(lineEndIter),
                                                       std::reverse_iterator<std::string::const_iterator>(lineBeginIter), ' ').base();

      // See if we have gone to the beginning, if not then break the line
      if (lastSpaceIter != lineBeginIter) {
        lineEndIter = lastSpaceIter;
      }
    }
    
    // Add new line
    if (linesProcessed > 0)
      _formattedString += "\n";

    // Indent the line
    if ((linesProcessed > 0) || 
        (_indentFirstLine == true)) {
      for (size_t index = _indentWidth; index > 0; index--)
      _formattedString += " ";
    }

    // Write out the line
    _formattedString.append(lineBeginIter, lineEndIter);

    lineBeginIter = lineEndIter;              
    linesProcessed++;
  }
}   

void 
XBUtilities::wrap_paragraphs( const std::string & _unformattedString, 
                              unsigned int _indentWidth, 
                              unsigned int _columnWidth, 
                              bool _indentFirstLine,
                              std::string &_formattedString) 
{
  // Set return variables to a now state
  _formattedString.clear();

  if (_indentWidth >= _columnWidth) {
    std::string errMsg = boost::str(boost::format("Internal Error: %s paragraph indent (%d) is greater than or equal to the column width (%d) ") % __FUNCTION__ % _indentWidth % _columnWidth);
    throw std::runtime_error(errMsg);
  }

  typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
  boost::char_separator<char> sep{"\n", "", boost::keep_empty_tokens};
  tokenizer paragraphs{_unformattedString, sep};

  tokenizer::const_iterator iter = paragraphs.begin();
  while (iter != paragraphs.end()) {
    std::string formattedParagraph;
    wrap_paragraph(*iter, _indentWidth, _columnWidth, _indentFirstLine, formattedParagraph);
    _formattedString += formattedParagraph;
    _indentFirstLine = true; // We wish to indent all lines following the first

    ++iter;

    // Determine if a '\n' should be added
    if (iter != paragraphs.end()) 
      _formattedString += "\n";
  }
}

void
XBUtilities::collect_devices( const std::set<std::string> &_deviceBDFs,
                              bool _inUserDomain,
                              xrt_core::device_collection &_deviceCollection)
{
  // -- If the collection is empty then do nothing
  if (_deviceBDFs.empty())
    return;

  // -- Collect all of devices if the "all" option is used...anywhere in the collection
  if (_deviceBDFs.find("all") != _deviceBDFs.end()) {
    xrt_core::device::id_type total = 0;
    try {
      // If there are no devices in the server a runtime exception is thrown in  mgmt.cpp probe()
      total = (xrt_core::device::id_type) xrt_core::get_total_devices(_inUserDomain /*isUser*/).first;
    } catch (...) { 
      /* Do nothing */ 
    }

    // No devices found
    if (total == 0)
      return;

    // Now collect the devices and add them to the collection
    for(xrt_core::device::id_type index = 0; index < total; ++index) {
      if(_inUserDomain)
        _deviceCollection.push_back( xrt_core::get_userpf_device(index) );
      else 
        _deviceCollection.push_back( xrt_core::get_mgmtpf_device(index) );
    }

    return;
  }

  // -- Collect the devices by name
  for (const auto & deviceBDF : _deviceBDFs) {
  	auto index = xrt_core::utils::bdf2index(deviceBDF, _inUserDomain);         // Can throw
    if(_inUserDomain)
        _deviceCollection.push_back( xrt_core::get_userpf_device(index) );
      else 
        _deviceCollection.push_back( xrt_core::get_mgmtpf_device(index) );
  }
}

bool 
XBUtilities::can_proceed()
{
  bool proceed = false;
  std::string input;

  std::cout << "Are you sure you wish to proceed? [Y/n]: ";
  std::getline( std::cin, input );

  // Ugh, the std::transform() produces windows compiler warnings due to 
  // conversions from 'int' to 'char' in the algorithm header file
  boost::algorithm::to_lower(input);
  //std::transform( input.begin(), input.end(), input.begin(), [](unsigned char c){ return std::tolower(c); });
  //std::transform( input.begin(), input.end(), input.begin(), ::tolower);

  // proceeds for "y", "Y" and no input
  proceed = ((input.compare("y") == 0) || input.empty());
  if (!proceed)
    std::cout << "Action canceled." << std::endl;
  return proceed;
}

boost::property_tree::ptree
XBUtilities::get_available_devices(bool inUserDomain) 
{
  xrt_core::device_collection deviceCollection;
  collect_devices(std::set<std::string> {"all"}, inUserDomain, deviceCollection);
  boost::property_tree::ptree pt;
  for (const auto & device : deviceCollection) {
    boost::property_tree::ptree pt_dev;
    pt_dev.put("bdf", xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device)));
    pt_dev.put("vbnv", xrt_core::device_query<xrt_core::query::rom_vbnv>(device));
    pt_dev.put("id", xrt_core::query::rom_time_since_epoch::to_string(xrt_core::device_query<xrt_core::query::rom_time_since_epoch>(device)));
    pt_dev.put("is_ready", "true"); //to-do: sysfs node but on windows?
    pt.push_back(std::make_pair("", pt_dev));
  }
  return pt;
}

std::vector<char>
XBUtilities::get_axlf_section(const std::string& filename, axlf_section_kind kind)
{
  std::ifstream in(filename);
  if (!in.is_open())
    throw std::runtime_error(boost::str(boost::format("Can't open %s") % filename));

  // Read axlf from dsabin file to find out number of sections in total.
  axlf a;
  size_t sz = sizeof (axlf);
  in.read(reinterpret_cast<char *>(&a), sz);
  if (!in.good())
    throw std::runtime_error(boost::str(boost::format("Can't read axlf from %s") % filename));

  // Reread axlf from dsabin file, including all sections headers.
  // Sanity check for number of sections coming from user input file
  if (a.m_header.m_numSections > 10000)
    throw std::runtime_error("Incorrect file passed in");

  sz = sizeof (axlf) + sizeof (axlf_section_header) * (a.m_header.m_numSections - 1);

  std::vector<char> top(sz);
  in.seekg(0);
  in.read(top.data(), sz);
  if (!in.good())
    throw std::runtime_error(boost::str(boost::format("Can't read axlf and section headers from %s") % filename));

  const axlf *ap = reinterpret_cast<const axlf *>(top.data());
  auto section = ::xclbin::get_axlf_section(ap, kind);
  if (!section)
    throw std::runtime_error("Section not found");

  std::vector<char> buf(section->m_sectionSize);
  in.seekg(section->m_sectionOffset);
  in.read(buf.data(), section->m_sectionSize);

  return buf;
}

std::vector<std::string>
XBUtilities::get_uuids(const void *dtbuf)
{
  std::vector<std::string> uuids;
  struct fdt_header *bph = (struct fdt_header *)dtbuf;
  uint32_t version = be32toh(bph->version);
  uint32_t off_dt = be32toh(bph->off_dt_struct);
  const char *p_struct = (const char *)dtbuf + off_dt;
  uint32_t off_str = be32toh(bph->off_dt_strings);
  const char *p_strings = (const char *)dtbuf + off_str;
  const char *p, *s;
  uint32_t tag;
  int sz;

  p = p_struct;
  uuids.clear();
  while ((tag = be32toh(GET_CELL(p))) != FDT_END) {
    if (tag == FDT_BEGIN_NODE) {
      s = p;
      p = PALIGN(p + strlen(s) + 1, 4);
      continue;
    }
    if (tag != FDT_PROP)
      continue;

    sz = be32toh(GET_CELL(p));
    s = p_strings + be32toh(GET_CELL(p));
    if (version < 16 && sz >= 8)
      p = PALIGN(p, 8);

    if (!strcmp(s, "logic_uuid")) {
      uuids.insert(uuids.begin(), std::string(p));
    }
    else if (!strcmp(s, "interface_uuid")) {
      uuids.push_back(std::string(p));
    }
    
    p = PALIGN(p + sz, 4);
  }
  return uuids;  
}

static const std::map<std::string, reset_type> reset_map = {
    { "hot", reset_type::hot },
    { "kernel", reset_type::kernel },
    { "ert", reset_type::ert },
    { "ecc", reset_type::ecc },
    { "soft_kernel", reset_type::soft_kernel }
  };

XBUtilities::reset_type
XBUtilities::str_to_enum_reset(const std::string& str)
{
  auto it = reset_map.find(str);
  if (it != reset_map.end())
    return it->second;
  throw xrt_core::error(str + " is invalid. Please specify a valid reset type");
}

static std::string
precision(double value, int p)
{
  std::stringstream stream;
  stream << std::fixed << std::setprecision(p) << value;
  return stream.str();
}

std::string
XBUtilities::format_base10_shiftdown3(uint64_t value)
{
  return precision(static_cast<double>(value) / 1000.0, 3);
}

std::string
XBUtilities::format_base10_shiftdown6(uint64_t value)
{
  return precision(static_cast<double>(value) / 1000000.0, 6);
}
