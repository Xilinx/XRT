/**
 * Copyright (C) 2021 Xilinx, Inc
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


#include "KernelUtilities.h"

#include "XclBinUtilities.h"
#include <boost/format.hpp>

namespace XUtil = XclBinUtilities;


int addressQualifierStrToInt(const std::string& addressQualifier) {
  if (addressQualifier == "SCALAR")
    return 0;

  if (addressQualifier == "GLOBAL")
    return 1;

  if (addressQualifier == "CONSTANT")
    return 2;

  if (addressQualifier == "LOCAL")
    return 3;

  if (addressQualifier == "STREAM")
    return 4;

  throw std::runtime_error("Unknown address-qualifier value: '" + addressQualifier + "'");
}

#define TYPESIZE(VAR) {#VAR, sizeof(VAR)}
static std::vector<std::pair<std::string, std::size_t>> scalarTypes = {
  TYPESIZE(char), TYPESIZE(unsigned char),
  TYPESIZE(int8_t), TYPESIZE(uint8_t),
  TYPESIZE(int16_t), TYPESIZE(uint16_t),
  TYPESIZE(int32_t), TYPESIZE(uint32_t),
  TYPESIZE(int64_t), TYPESIZE(uint64_t)
};

static std::vector<std::pair<std::string, std::size_t>> globalTypes = {
  TYPESIZE(void*), 
  TYPESIZE(char*), TYPESIZE(unsigned char*), 
  TYPESIZE(int8_t*), TYPESIZE(uint8_t*),
  TYPESIZE(int16_t*), TYPESIZE(uint16_t*),
  TYPESIZE(int32_t*), TYPESIZE(uint32_t*),
  TYPESIZE(int64_t*), TYPESIZE(uint64_t*)
};

std::size_t getTypeSize(std::string typeStr) {
  // Remove all spaces
  std::string::iterator end_pos = std::remove(typeStr.begin(), typeStr.end(), ' ');
  typeStr.erase(end_pos, typeStr.end());

  if (typeStr.empty())
    throw std::runtime_error("The given type value is empty");

  // Is this a pointer, if so then it is 8 Bytes in size (as defined by the HW)
  {
    const auto &iter = std::find_if( globalTypes.begin(), globalTypes.end(),
      [&typeStr](const std::pair<std::string, std::size_t>& element){ return element.first == typeStr;} );
    if (iter != globalTypes.end())
      return 8;
  }

  // Is this a scalar type
  {
    const auto &iter = std::find_if( scalarTypes.begin(), scalarTypes.end(),
      [&typeStr](const std::pair<std::string, std::size_t>& element){ return element.first == typeStr;} );
    if (iter != globalTypes.end())
      return iter->second;
  }


  throw std::runtime_error("Unknown argument type: '" + typeStr + "'");
}


void buildXMLKernelEntry(const boost::property_tree::ptree& ptKernel,
                         boost::property_tree::ptree& ptKernelXML) {
  const boost::property_tree::ptree ptEmpty;

  const std::string & kernelName = ptKernel.get<std::string>("name", "");
  if (kernelName.empty())
    throw std::runtime_error("Missing kernel name");

  // -- Build Kernel attributes
  boost::property_tree::ptree ptKernelAttributes;
  ptKernelAttributes.put("name", kernelName);
  ptKernelAttributes.put("language", "c");
  ptKernelXML.add_child("<xmlattr>", ptKernelAttributes);

  // -- Build kernel arguments
  const boost::property_tree::ptree& ptArguments = ptKernel.get_child("arguments", ptEmpty);

  unsigned int argID = 0;
  for (const auto& argument : ptArguments) {
    const boost::property_tree::ptree& ptArgument = argument.second;
    boost::property_tree::ptree ptArgAttributes;

    // Argument name
    const std::string& name = ptArgument.get<std::string>("name", "");
    if (name.empty())
      throw std::runtime_error("Missing argument name");

    // Address qualifier
    const std::string& addressQualifier = ptArgument.get<std::string>("address-qualifier", "");
    if (addressQualifier.empty())
      throw std::runtime_error("Missing address qualifier");

    // Type & size
    const std::string& argType = ptArgument.get<std::string>("type", "");
    if (argType.empty())
      throw std::runtime_error("Missing argument type");

    unsigned int argSize = getTypeSize(argType);

    // Offset
    const std::string& offset = ptArgument.get<std::string>("offset", "");
    if (offset.empty())
      throw std::runtime_error("Missing offset value");

    // Add attributes in the following order.  Helps maintain readablity
    ptArgAttributes.put("name", name);
    ptArgAttributes.put("addressQualifier", std::to_string(addressQualifierStrToInt(addressQualifier)));
    ptArgAttributes.put("id", std::to_string(argID++));
    ptArgAttributes.put("size", (boost::format("0x%x") % argSize).str());
    ptArgAttributes.put("offset", offset);
    ptArgAttributes.put("hostOffset", "0x0");
    ptArgAttributes.put("hostSize", (boost::format("0x%x") % argSize).str());
    ptArgAttributes.put("type", argType);

    // Add the kernel argument
    boost::property_tree::ptree ptArg;
    ptArg.add_child("<xmlattr>", ptArgAttributes);
    ptKernelXML.add_child("arg", ptArg);
  }

  // - Build kernel instances
  const boost::property_tree::ptree& ptInstances = ptKernel.get_child("instances", ptEmpty);
  for (const auto& instance : ptInstances) {
    const boost::property_tree::ptree & ptInstance = instance.second;

    const std::string& instanceName = ptInstance.get<std::string>("name", "");
    if (instanceName.empty())
      throw std::runtime_error("Missing kernel instance name value");

    boost::property_tree::ptree ptInstanceAttribute;
    ptInstanceAttribute.put("name", instanceName);

    // Add the instance
    boost::property_tree::ptree ptInstanceXML;
    ptInstanceXML.add_child("<xmlattr>", ptInstanceAttribute);
    ptKernelXML.add_child("instance", ptInstanceXML);
  }
}


void
XclBinUtilities::addKernel(const boost::property_tree::ptree& ptKernel,
                           boost::property_tree::ptree& ptEmbeddedData) {
  // -- Validate destination
  if (ptEmbeddedData.empty())
    throw std::runtime_error("Embedded Metadata section is empty");

  boost::property_tree::ptree ptEmpty;

  XUtil::TRACE_PrintTree("Embedded Data XML", ptEmbeddedData);
  if (ptEmbeddedData.get_child("project.platform.device.core", ptEmpty).empty()) {
    std::cout << "Info: Embedded Metadata section is missing project.platform.device.core element, adding it.\n";
    boost::property_tree::ptree ptCore;

    boost::property_tree::ptree ptDevice;
    ptDevice.add_child("core", ptCore);

    boost::property_tree::ptree ptPlatform;
    ptPlatform.add_child("device", ptDevice);

    boost::property_tree::ptree ptProject;
    ptProject.add_child("platform", ptPlatform);

    ptEmbeddedData.add_child("project", ptProject);
  }

  boost::property_tree::ptree& ptCore = ptEmbeddedData.get_child("project.platform.device.core", ptEmpty);

  XUtil::TRACE_PrintTree("Kernel", ptKernel);

  boost::property_tree::ptree ptKernelXML;
  buildXMLKernelEntry(ptKernel, ptKernelXML);
  XUtil::TRACE_PrintTree("KernelXML", ptKernelXML);

  // Validate that there is no other kernels with the same name.
  const std::string& kernelName = ptKernelXML.get<std::string>("<xmlattr>.name");

  for (const auto& entry : ptCore) {
    if (entry.first != "kernel")
      continue;

    const auto& ptKernelEntry = entry.second;
    const std::string& kernelEntryName = ptKernelEntry.get<std::string>("<xmlattr>.name", "");

    if (kernelEntryName == kernelName)
      throw std::runtime_error("Kernel name already exists in the EMBEDDED_METADATA section: '" + kernelName + "'");
  }

  // Add the kernel
  XUtil::TRACE("Fix kernel '" + kernelName + "' added to EMBEDDED_METADATA");
  ptCore.add_child("kernel", ptKernelXML);
}

void addArgsToMemoryConnections(const unsigned int ipLayoutIndexID,
                                const boost::property_tree::ptree &ptArgs,
                                const std::vector<boost::property_tree::ptree> &memTopology,
                                std::vector<boost::property_tree::ptree> & connectivity) {
  unsigned int argIndexID = 0;
  for (const auto & arg: ptArgs) {
    const auto & ptArg = arg.second;

    // Determine if there should be a memory connection, if so add it
    const std::string & memoryConnection = ptArg.get<std::string>("memory-connection","");

    if (!memoryConnection.empty()) {
      for (unsigned int memIndex = 0; memIndex < memTopology.size(); memIndex++ ){
        if (memTopology[memIndex].get<std::string>("m_tag","") != memoryConnection)
          continue;

        // We have a match
        boost::property_tree::ptree ptEntry;
        ptEntry.put("arg_index", std::to_string(argIndexID));
        ptEntry.put("m_ip_layout_index", std::to_string(ipLayoutIndexID));
        ptEntry.put("mem_data_index", std::to_string(memIndex));
        connectivity.push_back(ptEntry);
        break;
      }
    }
    ++argIndexID;
  }
}
  
void transformVectorToPtree(const std::vector<boost::property_tree::ptree> & vectorOfPtree,
                            const std::string & sectionName,
                            const std::string & arrayName,
                            boost::property_tree::ptree & ptRoot) {
 
  boost::property_tree::ptree ptBase;
  ptBase.put("m_count", std::to_string(vectorOfPtree.size()));

  // Create an array of property trees
  boost::property_tree::ptree ptArray;
  for (const auto & entry : vectorOfPtree) 
    ptArray.push_back(std::make_pair("", entry));  

  ptBase.add_child(arrayName, ptArray);

  ptRoot.clear();
  ptRoot.add_child(sectionName, ptBase);
}


void
XclBinUtilities::addKernel(const boost::property_tree::ptree& ptKernel,
                           const boost::property_tree::ptree& ptMemTopologyRoot,
                           boost::property_tree::ptree& ptIPLayoutRoot,
                           boost::property_tree::ptree& ptConnectivityRoot) {

  XUtil::TRACE_PrintTree("IPLAYOUT ROOT", ptIPLayoutRoot);

  const boost::property_tree::ptree ptEmpty;
  const std::string & kernelName = ptKernel.get<std::string>("name", "");
  if (kernelName.empty())
    throw std::runtime_error("Missing kernel name"); 

  // Transform the sections into something more manageable
  boost::property_tree::ptree& ptIPLayout = ptIPLayoutRoot.get_child("ip_layout");
  std::vector<boost::property_tree::ptree> ipLayout = XUtil::as_vector<boost::property_tree::ptree>(ptIPLayout, "m_ip_data");

  const boost::property_tree::ptree& ptMemTopology = ptMemTopologyRoot.get_child("mem_topology");
  std::vector<boost::property_tree::ptree> memTopology = XUtil::as_vector<boost::property_tree::ptree>(ptMemTopology, "m_mem_data");

  boost::property_tree::ptree& ptConnectivity = ptConnectivityRoot.get_child("connectivity");
  std::vector<boost::property_tree::ptree> connectivity = XUtil::as_vector<boost::property_tree::ptree>(ptConnectivity, "m_connection");

  // -- Create the kernel instances
  for (const auto & instance: ptKernel.get_child("instances", ptEmpty)) {
    const auto & ptInstance = instance.second;

    // Make sure that the instance name is valid
    const std::string & instanceName = ptInstance.get<std::string>("name","");
    if (instanceName.empty()) 
      throw std::runtime_error("Empty instance name for kernel: '" + kernelName + "'");

    const std::string ipLayoutName = kernelName + ":" + instanceName;

    // Validate that this PS kernel with this name doesn't already exist
    for (const auto & ipEntry : ipLayout) {
      if ((ipEntry.get<std::string>("m_type","") == "PS_KERNEL") &&
          (ipEntry.get<std::string>("m_name","") == ipLayoutName))
        throw std::runtime_error("PS Kernel instance name already exists: '" + ipLayoutName + "'");
    }

    // Create the new PS kernel instance and add it to the vector
    boost::property_tree::ptree ptIPEntry;
    ptIPEntry.put("m_type", "IP_PS_KERNEL");
    ptIPEntry.put("m_base_address", "not_used");
    ptIPEntry.put("m_name", ipLayoutName);
    ipLayout.push_back(ptIPEntry);

    // -- For each PS Kernel Instance, connect any argument to its memory
    addArgsToMemoryConnections(ipLayout.size()-1, ptKernel.get_child("arguments", ptEmpty), memTopology,  connectivity);
  }

  // Replace the original property tree
  transformVectorToPtree(ipLayout, "ip_layout", "m_ip_data", ptIPLayoutRoot);
  transformVectorToPtree(connectivity, "connectivity", "m_connection", ptConnectivityRoot);
}

