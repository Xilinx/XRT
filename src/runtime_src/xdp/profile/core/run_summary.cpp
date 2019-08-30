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

#include "run_summary.h"

#include "xdp/profile/writer/base_profile.h"

#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

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

      mXclbinContainerName = pt.get<std::string>("system_diagram_metadata.xsa.xclbin.generated_by.xclbin_name", "");
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
    ptSchema.put("minor", "0");
    ptSchema.put("patch", "0");
    ptRunSummary.add_child("schema_version", ptSchema);
  }

  // -- Add the files
  {
    boost::property_tree::ptree ptFiles;

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

