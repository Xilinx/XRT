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

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>

namespace XUtil = XclBinUtilities;

int addressQualifierStrToInt(const std::string& addressQualifier)
{
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
  TYPESIZE(float),
  TYPESIZE(int8_t), TYPESIZE(uint8_t),
  TYPESIZE(int16_t), TYPESIZE(uint16_t),
  TYPESIZE(int32_t), TYPESIZE(uint32_t),
  TYPESIZE(int64_t), TYPESIZE(uint64_t),
  { "int", 8 }, { "unsigned int", 8 },
};

std::size_t getTypeSize(std::string typeStr, bool fixedKernel)
{
  // Remove all spaces
  std::string::iterator end_pos = std::remove(typeStr.begin(), typeStr.end(), ' ');
  typeStr.erase(end_pos, typeStr.end());

  if (typeStr.empty())
    throw std::runtime_error("The given type value is empty");

  {
    // Is this a pointer, if so then for:
    //    Fixed PS Kernels, the size is 8 Bytes
    //    PS Kernels, the size is 16 bytes
    if (typeStr.back() == '*')
      return fixedKernel ? 8 : 16;
  }

  // Get the scaler type size
  {
    const auto& iter = std::find_if(scalarTypes.begin(), scalarTypes.end(),
                                    [&typeStr](const std::pair<std::string, std::size_t>& element) {return element.first == typeStr;});

    if (iter != scalarTypes.end())
      return iter->second;
  }


  throw std::runtime_error("Unknown argument type: '" + typeStr + "'");
}

bool isScalar(const std::string& typeStr)
{
  const auto& iter = std::find_if(scalarTypes.begin(), scalarTypes.end(),
                                  [&typeStr](const std::pair<std::string, std::size_t>& element) {return element.first == typeStr;});

  return (iter != scalarTypes.end());
}

bool isGlobal(std::string typeStr)
{
  std::string::iterator end_pos = std::remove(typeStr.begin(), typeStr.end(), ' ');
  typeStr.erase(end_pos, typeStr.end());

  if (typeStr.empty() || typeStr.back() != '*')
    return false;

  return true;
}


void buildXMLKernelEntry(const boost::property_tree::ptree& ptKernel,
                         bool isFixedPS,
                         boost::property_tree::ptree& ptKernelXML)
{
  const std::string& kernelName = ptKernel.get<std::string>("name", "");
  if (kernelName.empty())
    throw std::runtime_error("Missing kernel name");

  // -- Build Kernel attributes
  boost::property_tree::ptree ptKernelAttributes;
  ptKernelAttributes.put("name", kernelName);
  ptKernelAttributes.put("language", "c");
  ptKernelAttributes.put("type", "dpu");
  ptKernelXML.add_child("<xmlattr>", ptKernelAttributes);

  // -- Build kernel arguments
  const boost::property_tree::ptree ptEmpty;
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

    size_t argSize = getTypeSize(argType, isFixedPS);

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
    const boost::property_tree::ptree& ptInstance = instance.second;

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
                           bool isFixedPS,
                           boost::property_tree::ptree& ptEmbeddedData)
{
  // -- Validate destination
  if (ptEmbeddedData.empty())
    throw std::runtime_error("Embedded Metadata section is empty");

  boost::property_tree::ptree ptEmpty;

  XUtil::TRACE_PrintTree("Embedded Data XML", ptEmbeddedData);

  // If the core node doesn't exist, create one
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

  // Create the kernel XML entry metedata
  XUtil::TRACE_PrintTree("Kernel", ptKernel);

  boost::property_tree::ptree ptKernelXML;
  buildXMLKernelEntry(ptKernel, isFixedPS, ptKernelXML);

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
                                const boost::property_tree::ptree& ptArgs,
                                std::vector<boost::property_tree::ptree>& memTopology,
                                std::vector<boost::property_tree::ptree>& connectivity)
{
  unsigned int argIndexID = 0;
  // Examine each argument for a memory connection.  If one is found then
  // a connection is made.
  for (const auto& arg: ptArgs) {
    const auto& ptArg = arg.second;

    // Determine if there should be a memory connection, if so add it
    static const std::string NOT_DEFINED = "<not defined>";
    std::string memoryConnection = ptArg.get<std::string>("memory-connection", NOT_DEFINED);

    // An empty memory connection indicates that we should connect it to a yet to be define PS kernel memory entry
    if (memoryConnection.empty()) {
      // Look for entry.  If it doesn't exist create one
      auto iter = std::find_if(memTopology.begin(), memTopology.end(), 
                               [](const boost::property_tree::ptree& pt)
                               {return pt.get<std::string>("m_type", "") == "MEM_PS_KERNEL";});
      if (iter == memTopology.end()) {
        XUtil::TRACE("MEM Entry of PS Kernel memory not found, creating one.");
        boost::property_tree::ptree ptMemData;
        ptMemData.put("m_type", "MEM_PS_KERNEL");
        ptMemData.put("m_used", "1");
        ptMemData.put("m_tag", "MEM_PS_KERNEL");
        ptMemData.put("m_base_address", "0x0");
        memTopology.push_back(ptMemData);
      }

      memoryConnection = "MEM_PS_KERNEL";
    }

    // Do we have a connection to perform?
    if (memoryConnection != NOT_DEFINED) {
      auto iter = std::find_if(memTopology.begin(), memTopology.end(), 
                               [memoryConnection](const boost::property_tree::ptree& pt)
                               {return pt.get<std::string>("m_tag", "") == memoryConnection;});

      if (iter == memTopology.end())
        throw std::runtime_error("Error: Memory tag '" + memoryConnection + "' not found in the MEM_TOPOLOGY section.");

      // We have a match
      unsigned int memIndex = std::distance(memTopology.begin(), iter);
      boost::property_tree::ptree ptEntry;
      ptEntry.put("arg_index", std::to_string(argIndexID));
      ptEntry.put("m_ip_layout_index", std::to_string(ipLayoutIndexID));
      ptEntry.put("mem_data_index", std::to_string(memIndex));
      connectivity.push_back(ptEntry);
    }
    ++argIndexID;
  }
}

void transformVectorToPtree(const std::vector<boost::property_tree::ptree>& vectorOfPtree,
                            const std::string& sectionName,
                            const std::string& arrayName,
                            boost::property_tree::ptree& ptRoot)
{
  boost::property_tree::ptree ptBase;
  ptBase.put("m_count", std::to_string(vectorOfPtree.size()));

  // Create an array of property trees
  boost::property_tree::ptree ptArray;
  for (const auto& entry : vectorOfPtree)
    ptArray.push_back(std::make_pair("", entry));

  ptBase.add_child(arrayName, ptArray);

  ptRoot.clear();
  ptRoot.add_child(sectionName, ptBase);
}


void
XclBinUtilities::addKernel(const boost::property_tree::ptree& ptKernel,
                           boost::property_tree::ptree& ptMemTopologyRoot,
                           boost::property_tree::ptree& ptIPLayoutRoot,
                           boost::property_tree::ptree& ptConnectivityRoot)
{
  XUtil::TRACE_PrintTree("IP_LAYOUT ROOT", ptIPLayoutRoot);

  const boost::property_tree::ptree ptEmpty;
  const std::string& kernelName = ptKernel.get<std::string>("name", "");
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
  for (const auto& instance: ptKernel.get_child("instances", ptEmpty)) {
    const auto& ptInstance = instance.second;

    // Make sure that the instance name is valid
    const std::string& instanceName = ptInstance.get<std::string>("name", "");
    if (instanceName.empty())
      throw std::runtime_error("Empty instance name for kernel: '" + kernelName + "'");

    const std::string ipLayoutName = kernelName + ":" + instanceName;

    // Validate that this PS kernel with this name doesn't already exist
    for (const auto& ipEntry : ipLayout) {
      if ((ipEntry.get<std::string>("m_type", "") == "PS_KERNEL") &&
          (ipEntry.get<std::string>("m_name", "") == ipLayoutName))
        throw std::runtime_error("PS Kernel instance name already exists: '" + ipLayoutName + "'");
    }

    // Create the new PS kernel instance and add it to the vector
    boost::property_tree::ptree ptIPEntry;
    ptIPEntry.put("m_type", "IP_PS_KERNEL");
    ptIPEntry.put("m_base_address", "not_used");
    ptIPEntry.put("m_name", ipLayoutName);
    ipLayout.push_back(ptIPEntry);
    const unsigned int ipLayoutIndex = static_cast<unsigned int>(ipLayout.size()) - 1;

    // -- For each PS Kernel Instance, connect any argument to its memory
    addArgsToMemoryConnections(ipLayoutIndex, ptKernel.get_child("arguments", ptEmpty), memTopology,  connectivity);
  }

  // Replace the original property tree
  transformVectorToPtree(ipLayout, "ip_layout", "m_ip_data", ptIPLayoutRoot);
  transformVectorToPtree(connectivity, "connectivity", "m_connection", ptConnectivityRoot);
  transformVectorToPtree(memTopology, "mem_topology", "m_mem_data", ptMemTopologyRoot);
}

void
transposeFunction(const std::string& functionSig, boost::property_tree::ptree& ptFunction)
// Example signatures:
//    kernel0(float*, float*, float*, int, int, float, float*, xrtHandles*)
//    kernel0_fini(xrtHandles*)
//    kernel0_init(void*, unsigned char const*)

{
  if (functionSig.empty())
    throw std::runtime_error("Error: No function found to transform to a property_tree.");

  // Discover the function name
  std::size_t startArgs = functionSig.find("(");
  if (startArgs == std::string::npos)
    throw std::runtime_error("Error: Function signature not formed correctly, missing opening parenthesis '(': '" + functionSig + "'");

  std::string functionName = functionSig.substr(0, startArgs);
  boost::algorithm::trim(functionName);
  ptFunction.put("name", functionName);
  ptFunction.put("signature", functionSig);

  // Infer the function type
  std::string functionType = "kernel";
  if (boost::algorithm::ends_with(functionName, "_init"))
    functionType = "init";

  if (boost::algorithm::ends_with(functionName, "_fini"))
    functionType = "fini";

  ptFunction.put("type", functionType);

  // Discover the arguments
  std::size_t endArgs = functionSig.find(")");
  if (startArgs == std::string::npos)
    throw std::runtime_error("Error: Function signature not formed correctly, missing ending parenthesis ')(': '" + functionSig + "'");

  startArgs += 1; // Remove the start parenthesis
  std::string sArgs(functionSig.substr(startArgs, endArgs - startArgs));
  boost::algorithm::trim(sArgs);

  std::vector<std::string> arguments;
  boost::split(arguments, sArgs, boost::is_any_of(","));

  boost::property_tree::ptree ptArgsArray;
  unsigned int argID = 0;
  for (auto& arg : arguments) {
    const std::string sArgDefaultName = "arg" + std::to_string(argID++);
    boost::property_tree::ptree ptArg;

    boost::algorithm::trim(arg);

    // Build up the argument
    ptArg.put("name", sArgDefaultName);
    ptArg.put("type", arg);

    ptArgsArray.push_back(std::make_pair("", ptArg));
  }

  ptFunction.add_child("args", ptArgsArray);
}



void
XclBinUtilities::transposeFunctions(const std::vector<std::string>& functionSigs,
                                    boost::property_tree::ptree& ptFunctions)
{
  // Prepare return value
  ptFunctions.clear();

  // Examine each of the function signatures
  boost::property_tree::ptree ptFunctionArray;
  for (const auto& entry  : functionSigs) {
    boost::property_tree::ptree ptFunction;
    transposeFunction(entry, ptFunction);
    ptFunctionArray.push_back(std::make_pair("", ptFunction));
  }

  ptFunctions.add_child("functions", ptFunctionArray);
}


void
validateSignature(std::vector<boost::property_tree::ptree> functions,
                  const std::vector<std::string>& expectedArgs,
                  const std::string& kernelName,
                  const std::string& kernelLibrary)
{
  if (functions.empty())
    return;

  const std::string functionType = functions[0].get<std::string>("type");

  if (functions.size() > 1) {
    std::vector<std::string> functionsFound;
    for (const auto& entry : functions)
      functionsFound.push_back(entry.get<std::string>("name"));
    throw std::runtime_error("Error: Only one " + functionType + " kernel supported in a library, multiple " + functionType + " kernels found.\n"
                             "Shared Library: '" + kernelLibrary + "'\n"
                             "       Kernels: " + boost::algorithm::join(functionsFound, ", "));
  }

  const boost::property_tree::ptree& ptFunction = functions[0];

  // Validate name
  const std::string expectedName = kernelName + "_" + ptFunction.get<std::string>("type");

  const std::string& name = ptFunction.get<std::string>("name");
  if (name != expectedName)
    throw std::runtime_error("Error: The " + ptFunction.get<std::string>("type") + " kernel does not have the same base name as the kernel.\n"
                             "Shared Library: '" + kernelLibrary + "'\n"
                             "      Expected: '" + expectedName + "'\n"
                             "        Actual: '" + name + "'");

  // Validate arguments
  std::vector<boost::property_tree::ptree> args = XUtil::as_vector<boost::property_tree::ptree>(ptFunction, "args");
  if (args.size() != expectedArgs.size())
    throw std::runtime_error("Error: " + ptFunction.get<std::string>("type") + " kernel signature argument count mismatch.\n"
                             "Shared Library: '" + kernelLibrary + "'\n"
                             "      Expected: '" + name + "(" + boost::algorithm::join(expectedArgs, ", ") + ")'\n"
                             "        Actual: '" + ptFunction.get<std::string>("signature", "") + "'");

  for (size_t index = 0; index < expectedArgs.size(); ++index) {
    if (expectedArgs[index] != args[index].get<std::string>("type", "")) {
      throw std::runtime_error("Error: Argument mismatch.\n"
                               "Shared Library: '" + kernelLibrary + "'\n"
                               "   Expected[" + std::to_string(index) + "]: '" + expectedArgs[index] + "'\n"
                               "     Actual[" + std::to_string(index) + "]: '" + args[index].get<std::string>("type", "") + "'");
    }
  }
}




void
XclBinUtilities::validateFunctions(const std::string& kernelLibrary, const boost::property_tree::ptree& ptFunctions)
{
  XUtil::TRACE_PrintTree("Validate ptFunctions", ptFunctions);

  std::vector<boost::property_tree::ptree> functions = XUtil::as_vector<boost::property_tree::ptree>(ptFunctions, "functions");

  // Collect the functions
  std::vector<boost::property_tree::ptree> initKernels;
  std::vector<boost::property_tree::ptree> finiKernels;
  std::vector<boost::property_tree::ptree> kernels;

  for (const auto& entry: functions) {
    const auto& functionType = entry.get<std::string>("type", "");

    if (functionType == "init") {
      initKernels.push_back(entry);
      continue;
    }

    if (functionType == "fini") {
      finiKernels.push_back(entry);
      continue;
    }

    if (functionType == "kernel") {
      kernels.push_back(entry);
      continue;
    }
  }

  // DRC check
  // -- Validate kernels
  if (kernels.empty())
    throw std::runtime_error("Error: No kernels found in the shared library: '" + kernelLibrary + "'");

  if (kernels.size() > 1) {
    std::vector<std::string> functionsFound;
    for (const auto& entry : kernels)
      functionsFound.push_back(entry.get<std::string>("name"));
    throw std::runtime_error("Error: Only one kernel supported in a library, multiple kernels found.\n"
                             "Shared Library: '" + kernelLibrary + "'\n"
                             "Kernels: " + boost::algorithm::join(functionsFound, ", "));
  }

  const std::string& kernelName = kernels[0].get<std::string>("name");

  // -- Validate _init function
  if (!initKernels.empty()) {
    static std::vector<std::string> argsExpected = { "void*", "unsigned char const*" };
    validateSignature(initKernels, argsExpected, kernelName, kernelLibrary);
  }

  // -- Validate _fini function
  if (!finiKernels.empty()) {
    static std::vector<std::string> argsExpected = { "xrtHandles*" };
    validateSignature(finiKernels, argsExpected, kernelName, kernelLibrary);
  }

  // Validate kernel's last argument
  std::vector<boost::property_tree::ptree> args = XUtil::as_vector<boost::property_tree::ptree>(kernels[0], "args");
  if (args.back().get<std::string>("type") != "xrtHandles*")
    throw std::runtime_error("Error: Last kernel argument isn't a xrtHandle pointer."
                             "Shared Library: '" + kernelLibrary + "'\n"
                             "Kernel Function: '" + kernels[0].get<std::string>("signature"));
}




void
XclBinUtilities::createPSKernelMetadata(unsigned long numInstances,
                                        const boost::property_tree::ptree& ptFunctions,
                                        const std::string& kernelLibrary,
                                        boost::property_tree::ptree& ptPSKernels)
{
  // Find the PS kernel entry
  std::vector<boost::property_tree::ptree> functions = XUtil::as_vector<boost::property_tree::ptree>(ptFunctions, "functions");


  boost::property_tree::ptree ptKernelArray;
  for (const auto& ptFunction : functions) {
    if (ptFunction.get<std::string>("type", "") != "kernel")
      continue;

    // Build up the PSKernel property tree
    boost::property_tree::ptree ptKernel;
    ptKernel.put("name", ptFunction.get<std::string>("name"));

    // Gather arguments
    std::vector<boost::property_tree::ptree> args = XUtil::as_vector<boost::property_tree::ptree>(ptFunction, "args");
    boost::property_tree::ptree ptArgArray;
    uint64_t offset = 0;
    for (const auto& entry : args) {
      boost::property_tree::ptree ptArg;
      ptArg.put("name", entry.get<std::string>("name"));

      const std::string sType = entry.get<std::string>("type");

      std::string addressQualifier;
      if (isScalar(sType))
        addressQualifier = "SCALAR";

      if (isGlobal(sType)) {
        ptArg.put("memory-connection", "");    // Empty string indicates auto connection
        addressQualifier = "GLOBAL";
      }

      if (addressQualifier.empty())
        throw std::runtime_error("Error: Unknown kernel argument type.\n"
                                 "Shared Library: '" + kernelLibrary + "'\n"
                                 "Kernel Function: '" + ptFunction.get<std::string>("signature") + "\n"
                                 "Argument Name: '" + entry.get<std::string>("name") + "\n"
                                 "Argument Type: '" + entry.get<std::string>("type"));

      ptArg.put("address-qualifier", addressQualifier);
      ptArg.put("type", sType);
      ptArg.put("offset", boost::str(boost::format("0x%x") % offset));
      offset += getTypeSize(sType, false /*fixedKernel*/);

      ptArgArray.push_back(std::make_pair("", ptArg));   // Used to make an array of objects
    }
    ptKernel.add_child("arguments", ptArgArray);

    // Add the instances
    boost::property_tree::ptree ptInstanceArray;
    for (unsigned long instance = 0; instance < numInstances; ++instance) {
      boost::property_tree::ptree ptInstance;
      ptInstance.put("name", std::to_string(instance));
      ptInstanceArray.push_back(std::make_pair("", ptInstance));
    }
    ptKernel.add_child("instances", ptInstanceArray);

    ptKernelArray.push_back(std::make_pair("", ptKernel));
  }

  // DRC check, make sure we have some kernels
  if (ptKernelArray.empty())
    throw std::runtime_error("Error: No PS kernels found. Shared Library: '" + kernelLibrary + "'");

  // Build the kernels array node
  boost::property_tree::ptree ptKernels;
  ptKernels.add_child("kernels", ptKernelArray);

  // Build the ps-kernel node
  ptPSKernels.add_child("ps-kernels", ptKernels);

  XUtil::TRACE_PrintTree("PS Kernel Entries", ptPSKernels);
}

