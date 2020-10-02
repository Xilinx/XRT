/**
 * Copyright (C) 2019 Xilinx, Inc
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

#define XDP_SOURCE

#include "run_summary.h"

#include "core/common/config_reader.h"
#include "xocl/core/time.h"
#include "xdp/profile/writer/base_profile.h"

#include <chrono>
#include <iostream>
#include <stdlib.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#pragma warning(disable : 4996)
/* Disable warning for use of "getenv" */
#endif


RunSummary::RunSummary()
    : mSystemMetadata("")
    , mXclbinContainerName("")
{
  // Empty
}

RunSummary::~RunSummary() 
{
  // Empty
}

void RunSummary::addFile(const std::string & fileName, RunSummary::FileType eFileType )
{
  // Validate the input parameters
  if (fileName.empty() || (eFileType == FT_UNKNOWN)) {
    return;  
  }

  mFiles.emplace_back(fileName, eFileType);
}

void RunSummary::setProfileTree(std::shared_ptr<boost::property_tree::ptree> tree)
{
  mProfileTree = tree;
}

const std::string  
RunSummary::getFileTypeAsStr(enum RunSummary::FileType eFileType)
{
  switch (eFileType) {
    case FT_UNKNOWN: return "UNKNOWN";
    case FT_PROFILE: return "PROFILE";
    case FT_TRACE: return "TRACE";
    case FT_WDB: return "WAVEFORM_DATABASE";
    case FT_WDB_CONFIG: return "WAVEFORM_CONFIGURATION";
    case FT_POWER_PROFILE: return "XRT_POWER_PROFILE";
    case FT_KERNEL_PROFILE: return "KERNEL_PROFILE";
    case FT_KERNEL_TRACE: return "KERNEL_TRACE";
    case FT_VP_TRACE: return "VP_TRACE";
  }

  // Yeah, the code will never get here, but it makes for a clean flow
  return "UNKNOWN";
}

void RunSummary::extractSystemProfileMetadata(const axlf * pXclbinImage, 
                                              const std::string & xclbinContainerName)
{
  mXclbinContainerName = xclbinContainerName;
  mSystemMetadata.clear();

  // Make sure we have something to work with
  if (pXclbinImage == nullptr) {
    return;
  }

  // Find the System Metadata section
  const struct axlf_section_header *pSectionHeader = xclbin::get_axlf_section((const axlf*) pXclbinImage, SYSTEM_METADATA);
  if (pSectionHeader == nullptr) {
    return;  
  }

  // Point to the payload
  const unsigned char *pBuffer = (const unsigned char *) pXclbinImage + pSectionHeader->m_sectionOffset;

  // Convert the payload from 1 byte binary format to 2 byte hex ascii string representation
  std::ostringstream buf;

  for (unsigned int index = 0; index < pSectionHeader->m_sectionSize; ++index) {
    buf << std::hex << std::setw(2) << std::setfill('0') << (unsigned int) pBuffer[index];
  }

  mSystemMetadata = buf.str();

  // If we don't have a binary container name, obtain it from the system diagram metadata

  if (mXclbinContainerName.empty()) {
    try {
      std::stringstream ss;
      ss.write((const char*) pBuffer,  pSectionHeader->m_sectionSize);

      // Create a property tree and determine if the variables are all default values
      boost::property_tree::ptree pt;
      boost::property_tree::read_json(ss, pt);

      mXclbinContainerName = pt.get<std::string>("system_diagram_metadata.xclbin.generated_by.xclbin_name", "");
      if (!mXclbinContainerName.empty()) {
        mXclbinContainerName += ".xclbin";
      }
    } catch (...) {
      // Do nothing
    }
  }
}


void RunSummary::writeContent()
{
  //  Determine if there are files, if not then exit
  if (mFiles.empty()) {
    return;
  }

  boost::property_tree::ptree ptRunSummary;

  // -- Create and add the schema version
  {
    boost::property_tree::ptree ptSchema;
    ptSchema.put("major", "1");
    ptSchema.put("minor", "1");
    ptSchema.put("patch", "0");
    ptRunSummary.add_child("schema_version", ptSchema);
  }

  {
#ifdef _WIN32
    auto pid = _getpid() ;
#else
    auto pid = (getpid()) ;
#endif
    auto timestamp = (std::chrono::system_clock::now()).time_since_epoch() ;
    auto value =
      std::chrono::duration_cast<std::chrono::milliseconds>(timestamp) ;
    uint64_t timeMsec = value.count() ;

    boost::property_tree::ptree ptGeneration ;
    ptGeneration.put("source", "ocl") ;
    ptGeneration.put("PID", std::to_string(pid)) ;
    ptGeneration.put("timestamp", std::to_string(timeMsec)) ;
    ptRunSummary.add_child("generation", ptGeneration) ;
  }

  // -- Add the files
  {
    boost::property_tree::ptree ptFiles;

    // If the waveform data is available add it to the report
    char* pWdbFile = getenv("VITIS_WAVEFORM_WDB_FILENAME"); 
    if (pWdbFile != nullptr) {
      boost::property_tree::ptree ptFile;
      ptFile.put("name", pWdbFile);
      ptFile.put("type", getFileTypeAsStr(FT_WDB).c_str());
      ptFiles.push_back(std::make_pair("", ptFile));
      // Also need to add the config file that will be written next to the
      // waveform database. This is needed to open the WDB. The name is the
      // the same, but the extension is changed from .wdb to .wcfg.
      std::string configName(pWdbFile);
      configName = configName.substr(0, configName.rfind('.'));
      configName += ".wcfg";
      ptFile.put("name", configName.c_str());
      ptFile.put("type", getFileTypeAsStr(FT_WDB_CONFIG).c_str());
      ptFiles.push_back(std::make_pair("", ptFile));
    }

    // If kernel profile and trace files are available add them to the report
    // NOTE: HW emulation only
    char* pKernelProfileFile = getenv("VITIS_KERNEL_PROFILE_FILENAME");
    if (pKernelProfileFile != nullptr) {
      boost::property_tree::ptree ptFile;
      ptFile.put("name", pKernelProfileFile);
      ptFile.put("type", getFileTypeAsStr(FT_KERNEL_PROFILE).c_str());
      ptFiles.push_back(std::make_pair("", ptFile));
    }
    char* pKernelTraceFile = getenv("VITIS_KERNEL_TRACE_FILENAME");
    if (pKernelTraceFile != nullptr) {
      boost::property_tree::ptree ptFile;
      ptFile.put("name", pKernelTraceFile);
      ptFile.put("type", getFileTypeAsStr(FT_KERNEL_TRACE).c_str());
      ptFiles.push_back(std::make_pair("", ptFile));
    }

    // If VART profiling is turned on, then add the generated file
    if (xrt_core::config::get_vitis_ai_profile()) {
      boost::property_tree::ptree ptFile;
      ptFile.put("name", "vart_trace.csv");
      ptFile.put("type", getFileTypeAsStr(FT_VP_TRACE).c_str());
      ptFiles.push_back(std::make_pair("", ptFile));
    }

    // Add each files
    for (auto file : mFiles) {
      boost::property_tree::ptree ptFile;
      ptFile.put("name", file.first.c_str());
      ptFile.put("type", getFileTypeAsStr(file.second).c_str());
      ptFiles.push_back(std::make_pair("", ptFile));
    }

    // Add the files array to the run summary
    ptRunSummary.add_child("files", ptFiles);
    boost::property_tree::ptree ptFile;
  }

  // Add the system diagram payload.
  if (!mSystemMetadata.empty()) {
    boost::property_tree::ptree ptSystemDiagram;
    ptSystemDiagram.put("payload_16bitEnc", mSystemMetadata.c_str());
    ptRunSummary.add_child("system_diagram", ptSystemDiagram);
  }

  // Add profile data if available.
  if (mProfileTree) {
    ptRunSummary.add_child("profile", *mProfileTree);
  }
   
  // Open output file
  std::string outputFile = mXclbinContainerName.empty() ? "xclbin" : mXclbinContainerName;
  outputFile += ".run_summary";

  std::fstream outputStream;
  outputStream.open(outputFile, std::ifstream::out | std::ifstream::binary);
  if (!outputStream.is_open()) {
    std::string errMsg = "ERROR: Unable to open the file for writing: " + outputFile;
    // std::cerr << errMsg << std::endl;
    return;
  }

  boost::property_tree::write_json(outputStream, ptRunSummary, true /*Pretty print*/);
  outputStream.close();
}

