/**
 * Copyright (C) 2021-2022 Xilinx, Inc
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

#include "ElfUtilities.h"

#include "XclBinUtilities.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>
#include <boost/version.hpp>

#if (BOOST_VERSION >= 106400)
  #include <boost/process/search_path.hpp>
#endif


namespace XUtil = XclBinUtilities;

static std::vector<std::string>
dataMineExportedFunctionsObjdump(const std::string& elfLibrary)
// Sample output being parsed:
// /proj/xcohdstaff1/stephenr/github/XRT/WIP/src/runtime_src/tools/xclbinutil/unittests/PSKernel/pskernel.so:     file format elf64-little
// DYNAMIC SYMBOL TABLE:
// 00000000000040b0  w   DF .text  000000000000001c  Base        std::_Vector_base<void*, std::allocator<void*> >::~_Vector_base()
// 00000000000040d0  w   DF .text  00000000000000a4  Base        std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_release()
// 00000000000040b0  w   DF .text  000000000000001c  Base        std::_Vector_base<void*, std::allocator<void*> >::~_Vector_base()
// 0000000000004090  w   DF .text  000000000000001c  Base        std::_Vector_base<unsigned long, std::allocator<unsigned long> >::~_Vector_base()
// 0000000000004090  w   DF .text  000000000000001c  Base        std::_Vector_base<unsigned long, std::allocator<unsigned long> >::~_Vector_base()
// 0000000000005a60  w   DF .text  0000000000000128  Base        void std::vector<void*, std::allocator<void*> >::_M_realloc_insert<void*&>(__gnu_cxx::__normal_iterator<void**, std::vector<void*, std::allocator<void*> > >, void*&)
// 0000000000003500 g    DF .text  00000000000005c4  Base        kernel0(float*, float*, float*, int, int, float, float*, xrtHandles*)
// 00000000000030f0 g    DF .text  000000000000040c  Base        kernel0_fini(xrtHandles*)
// 0000000000003ac4 g    DF .text  00000000000001e8  Base        kernel0_init(void*, unsigned char const*)
{

  // Call objdump to get the collection of functions
  boost::filesystem::path objdumpPath = "/usr/bin/objdump";    // Assume it is in a known location

#if (BOOST_VERSION >= 106400)
  objdumpPath = boost::process::search_path("objdump");

  const std::string expectedObjdumpPath = "/usr/bin/objdump";

  if (objdumpPath.string() != expectedObjdumpPath)
    std::cout << boost::format("Warning: Unexpected objdump path.\n"
                               "         Expected: %s\n"
                               "           Actual: %s\n") % expectedObjdumpPath % objdumpPath.string();
#endif

  const std::vector<std::string> cmdOptions = { "--wide", "--section=.text", "-T", "-C", elfLibrary };
  std::ostringstream os_stdout;
  std::ostringstream os_stderr;

  // Build the command line
  std::string cmdLine = objdumpPath.string();
  for (const auto& option : cmdOptions)
    cmdLine += " " + option;

  XUtil::TRACE("Cmd: " + cmdLine);
  XUtil::exec(objdumpPath, cmdOptions, true /*throw exception*/, os_stdout, os_stderr);

  XUtil::TRACE("Parsing results from the objdump cmd");
  std::vector<std::string> entries;
  std::string output = os_stdout.str();
  boost::split(entries, output, boost::is_any_of("\n"), boost::token_compress_on);

  // Look for the following function attributes in the '.text' section
  // g - Global
  // F - Function
  std::vector<std::string> kernelSignatures;

  for (const auto& entry : entries) {
    // Look for the ' .text' entry.
    auto textIndex = entry.find(" .text");
    if (textIndex == std::string::npos)
      continue;

    // Examine the flags looking for a global function.
    auto flagIndex = entry.find(" ");
    if (flagIndex == std::string::npos)
      throw std::runtime_error("Error: Could not find the start of the flag section: " + entry);

    std::string flags = entry.substr(flagIndex, textIndex - flagIndex);
    if (flags.find("g") == std::string::npos)
      continue;

    if (flags.find("F") == std::string::npos)
      continue;

    // Find and record the function signature
    auto baseIndex = entry.find("Base");
    if (baseIndex == std::string::npos)
      throw std::runtime_error("Error: Missing base entry: " + entry);

    std::string functionSig = entry.substr(baseIndex + sizeof("Base"));
    boost::algorithm::trim(functionSig);

    kernelSignatures.push_back(functionSig);
  }

  XUtil::TRACE("Finished populating kernel signatures");
  return kernelSignatures;
}

static bool
isTag(const std::string& entry)
// <1><cc9>: Abbrev Number: 64 (DW_TAG_subprogram)
{
  if (entry.find("><") == std::string::npos)
    return false;

  if (entry.find(">:") == std::string::npos)
    return false;

  return true;
}

static unsigned long 
readHexString(const std::string& entry, 
              const size_t startPos)
{
  size_t endIndex = entry.find_first_not_of("abcdefABCDEF0123456789", startPos);
  size_t numberLength = ((endIndex == std::string::npos) ? entry.size() : endIndex) - startPos;
  std::string sNumber = entry.substr(startPos, numberLength);
  unsigned long number = 0;
  try {
    number = std::stoul(sNumber, nullptr, 16);
  } catch (...) {
    throw std::runtime_error("ERROR: Unable to convert hex string number '" + sNumber + "' to an unsigned long.");
  }
  return number;
}

using AbbrevCollection = std::vector<std::pair<unsigned long, boost::property_tree::ptree>>;

enum class DW_TAG {
  unknown,            // Could not determine the DW_TAG
  subprogram,         // DW_TAG_subprogram
  pointer_type,       // DW_TAG_pointer_type
  formal_parameter,   // DW_TAG_formal_parameter
  class_type,         // DW_TAG_class_type
  reference_type,     // DW_TAG_reference_type
  _typedef,           // DW_TAG_typedef
  base_type,          // DW_TAG_base_type
  const_type          // DW_TAG_const_type
};


// Collection of TAGs that is used to convert between human readable and enumeration value
static const std::vector<std::pair<enum DW_TAG, std::string>> 
DWTags = {
  { DW_TAG::unknown, "DW_TAG_unknown" },
  { DW_TAG::subprogram, "DW_TAG_subprogram" },
  { DW_TAG::pointer_type, "DW_TAG_pointer_type" },
  { DW_TAG::formal_parameter, "DW_TAG_formal_parameter" },
  { DW_TAG::class_type, "DW_TAG_class_type" },
  { DW_TAG::reference_type, "DW_TAG_reference_type" },
  { DW_TAG::_typedef, "DW_TAG_typedef" },
  { DW_TAG::base_type, "DW_TAG_base_type" },
  { DW_TAG::const_type, "DW_TAG_const_type" },
};

static enum DW_TAG 
get_DW_TAG(const std::string& tagString)
{
  for (const auto& entry : DWTags)
    if (tagString.find(entry.second) != std::string::npos)
      return entry.first;

  return DW_TAG::unknown;
}

static const std::string &
enum_DW_TAG_to_string(enum DW_TAG eTag)
{
  for (const auto& entry : DWTags)
    if (entry.first == eTag)
      return entry.second;

  return enum_DW_TAG_to_string(DW_TAG::unknown);
}

static std::string
get_DW_AT_value(const std::string& entry, const std::string& tag)
//    <434>   DW_AT_name        : (indirect string, offset: 0xa7): nullptr_t
{
  // If the tag is not found return an empty string
  if (entry.find(tag) == std::string::npos)
    return std::string();

  // Get the last value after the colon
  const std::string searchString = ":";
  size_t valueIndex = entry.find_last_of(searchString);
  if (valueIndex == std::string::npos)
    throw std::runtime_error("ERROR: Cannot find DW_AT value in the entry: '" + entry + "'");

  auto returnValue = entry.substr(valueIndex + searchString.size());

  // Remove leading and trailing spaces
  boost::algorithm::trim(returnValue);
  return returnValue;
}

static void 
if_exist_add_DW_AT(const std::string& entry,
                   const std::string& tag,
                   boost::property_tree::ptree& pt)
{
  const auto& tagValue = get_DW_AT_value(entry, tag);
  if (tagValue.empty())
    return;

  pt.put(tag, tagValue);
}

static unsigned long 
get_tag_offset(const std::string& entry)
// <1><c79>: Abbrev Number: 62 (DW_TAG_class_type)
{
  const std::string searchSubString = "><";

  size_t searchIndex = entry.find(searchSubString);
  if (searchIndex == std::string::npos)
    throw std::runtime_error("ERROR: Cannot find start head in the DW_TAG: '" + entry + "'");

  return readHexString(entry, searchIndex + searchSubString.size());
}


static void 
add_DWTAG_pointerType(size_t& index,
                      const std::vector<std::string>& dwarfEntries,
                      AbbrevCollection& argTags)
// <1><6f8>: Abbrev Number: 37 (DW_TAG_pointer_type)
//    <6f9>   DW_AT_byte_size   : 8
//    <6fa>   DW_AT_type        : <0x6db>
{
  const auto offset = get_tag_offset(dwarfEntries[index]);

  boost::property_tree::ptree ptDWTAG;
  ptDWTAG.put("DW_TAG", enum_DW_TAG_to_string(get_DW_TAG(dwarfEntries[index])));

  // Examine the DWTAG entries
  while ((++index < dwarfEntries.size()) &&
         (!isTag(dwarfEntries[index]))) {
    const auto& entry = dwarfEntries[index];
    if_exist_add_DW_AT(entry, "DW_AT_byte_size", ptDWTAG);
    if_exist_add_DW_AT(entry, "DW_AT_type", ptDWTAG);
  }
  argTags.emplace_back(offset, ptDWTAG);
}

static void 
add_DWTAG_referenceType(size_t& index,
                        const std::vector<std::string>& dwarfEntries,
                        AbbrevCollection& argTags)
// <1><bb4>: Abbrev Number: 55 (DW_TAG_reference_type)
//    <bb5>   DW_AT_byte_size   : 8
//    <bb6>   DW_AT_type        : <0x40a>
{
  const auto offset = get_tag_offset(dwarfEntries[index]);

  boost::property_tree::ptree ptDWTAG;
  ptDWTAG.put("DW_TAG", enum_DW_TAG_to_string(get_DW_TAG(dwarfEntries[index])));

  // Examine the DWTAG entries
  while ((++index < dwarfEntries.size()) &&
         (!isTag(dwarfEntries[index]))) {
    const auto& entry = dwarfEntries[index];
    if_exist_add_DW_AT(entry, "DW_AT_byte_size", ptDWTAG);
    if_exist_add_DW_AT(entry, "DW_AT_type", ptDWTAG);
  }
  argTags.emplace_back(offset, ptDWTAG);
}

static void 
add_DWTAG_typedef(size_t& index,
                  const std::vector<std::string>& dwarfEntries,
                  AbbrevCollection& argTags)
// <2><433>: Abbrev Number: 26 (DW_TAG_typedef)
//    <434>   DW_AT_name        : (indirect string, offset: 0xa7): nullptr_t
//    <438>   DW_AT_decl_file   : 4
//    <439>   DW_AT_decl_line   : 235
//    <43a>   DW_AT_type        : <0xba3>
{
  auto offset = get_tag_offset(dwarfEntries[index]);

  boost::property_tree::ptree ptDWTAG;
  ptDWTAG.put("DW_TAG", enum_DW_TAG_to_string(get_DW_TAG(dwarfEntries[index])));

  // Examine the DWTAG entries
  while ((++index < dwarfEntries.size()) &&
         (!isTag(dwarfEntries[index]))) {
    const auto& entry = dwarfEntries[index];
    if_exist_add_DW_AT(entry, "DW_AT_name", ptDWTAG);
    if_exist_add_DW_AT(entry, "DW_AT_type", ptDWTAG);
  }
  argTags.emplace_back(offset, ptDWTAG);
}

static void 
add_DWTAG_base_type(size_t& index,
                    const std::vector<std::string>& dwarfEntries,
                    AbbrevCollection& argTags)
// <1><592>: Abbrev Number: 36 (DW_TAG_base_type)
//    <593>   DW_AT_byte_size   : 4
//    <594>   DW_AT_encoding    : 5	(signed)
//    <595>   DW_AT_name        : int
{
  auto offset = get_tag_offset(dwarfEntries[index]);

  boost::property_tree::ptree ptDWTAG;
  ptDWTAG.put("DW_TAG", enum_DW_TAG_to_string(get_DW_TAG(dwarfEntries[index])));

  // Examine the DWTAG entries
  while ((++index < dwarfEntries.size()) &&
         (!isTag(dwarfEntries[index]))) {
    const auto& entry = dwarfEntries[index];
    if_exist_add_DW_AT(entry, "DW_AT_name", ptDWTAG);
    if_exist_add_DW_AT(entry, "DW_AT_byte_size", ptDWTAG);
  }
  argTags.emplace_back(offset, ptDWTAG);
}

static void 
add_DWTAG_class_type(size_t& index,
                     const std::vector<std::string>& dwarfEntries,
                     AbbrevCollection& argTags)
// <1><c79>: Abbrev Number: 62 (DW_TAG_class_type)
//    <c7a>   DW_AT_name        : (indirect string, offset: 0x623): xrtHandles
//    <c7e>   DW_AT_byte_size   : 1
//    <c7f>   DW_AT_decl_file   : 1
//    <c80>   DW_AT_decl_line   : 4
{
  auto offset = get_tag_offset(dwarfEntries[index]);

  boost::property_tree::ptree ptDWTAG;
  ptDWTAG.put("DW_TAG", enum_DW_TAG_to_string(get_DW_TAG(dwarfEntries[index])));

  // Examine the DWTAG entries
  while ((++index < dwarfEntries.size()) &&
         (!isTag(dwarfEntries[index]))) {
    const auto& entry = dwarfEntries[index];
    if_exist_add_DW_AT(entry, "DW_AT_name", ptDWTAG);
  }
  argTags.emplace_back(offset, ptDWTAG);
}

static void 
add_DWTAG_const_type(size_t& index,
                     const std::vector<std::string>& dwarfEntries,
                     AbbrevCollection& argTags)
// <1><c79>: Abbrev Number: 62 (DW_TAG_class_type)
//    <c7a>   DW_AT_name        : (indirect string, offset: 0x623): xrtHandles
//    <c7e>   DW_AT_byte_size   : 1
//    <c7f>   DW_AT_decl_file   : 1
//    <c80>   DW_AT_decl_line   : 4
{
  auto offset = get_tag_offset(dwarfEntries[index]);

  boost::property_tree::ptree ptDWTAG;
  ptDWTAG.put("DW_TAG", enum_DW_TAG_to_string(get_DW_TAG(dwarfEntries[index])));

  // Examine the DWTAG entries up either the end of the list or the next DWTAG
  while ((++index < dwarfEntries.size()) &&
         (!isTag(dwarfEntries[index]))) {
    if_exist_add_DW_AT(dwarfEntries[index], "DW_AT_type", ptDWTAG);
  }
  argTags.emplace_back(offset, ptDWTAG);
}

static const boost::property_tree::ptree&
get_dw_type(const std::string& typeOffset, 
            const AbbrevCollection& argTags)
// <0xcc3>
{
  size_t posOffset = typeOffset.find("<0x") == std::string::npos ? 0 : 3;
  unsigned long offset = readHexString(typeOffset, posOffset);
  std::cout << boost::format("Offset string: %s', value: 0x%x\n") % typeOffset % offset;

  auto it = std::find_if(argTags.begin(), argTags.end(),
                         [&offset](const std::pair<unsigned long, boost::property_tree::ptree>& element) {return element.first == offset;});

  const static boost::property_tree::ptree ptEmpty;
  if (it == argTags.end())
    return ptEmpty;

  return it->second;
}

static void
evaluate_DW_TAG_type(const std::string& typeOffset,
                     const AbbrevCollection& argTags,
                     boost::property_tree::ptree& ptArgument)
{
  const boost::property_tree::ptree& ptTag = get_dw_type(typeOffset, argTags);
  if (ptTag.empty())
    throw std::runtime_error("ERROR: No cache value found for: '" + typeOffset + "'");

  enum DW_TAG dwTag = get_DW_TAG(ptTag.get<std::string>("DW_TAG"));

  switch (dwTag) {
    case DW_TAG::pointer_type: {
        evaluate_DW_TAG_type(ptTag.get<std::string>("DW_AT_type"), argTags, ptArgument);
        ptArgument.put("byte-size", ptTag.get<std::string>("DW_AT_byte_size"));
        // Add pointer
        std::string argType = ptArgument.get<std::string>("type", "") + "*";
        ptArgument.put<std::string>("type", argType);
        ptArgument.put("address-qualifier", "GLOBAL");
        break;
      }
    case DW_TAG::class_type:
      ptArgument.put("type", ptTag.get<std::string>("DW_AT_name"));
      break;

    case DW_TAG::_typedef:
      ptArgument.put("type", ptTag.get<std::string>("DW_AT_name"));
      break;

    case DW_TAG::base_type:
      ptArgument.put("type", ptTag.get<std::string>("DW_AT_name"));
      ptArgument.put("byte-size", ptTag.get<std::string>("DW_AT_byte_size"));
      break;

    case DW_TAG::const_type: {
        evaluate_DW_TAG_type(ptTag.get<std::string>("DW_AT_type"), argTags, ptArgument);
        // Add const
        std::string argType = "const " + ptArgument.get<std::string>("type", "");
        ptArgument.put<std::string>("type", argType);
        break;
      }
    default:
      throw std::runtime_error("ERROR: DW enum not supported: " + enum_DW_TAG_to_string(dwTag));
      break;
  }
}

static void
add_formal_parameter(size_t& index,
                     const std::vector<std::string>& dwarfEntries,
                     const AbbrevCollection& argTags,
                     boost::property_tree::ptree& ptArgument)
{
  while ((++index < dwarfEntries.size()) &&
         (!isTag(dwarfEntries[index]))) {
    const auto& entry = dwarfEntries[index];

    if (entry.find("DW_AT_name") != std::string::npos)
      ptArgument.put("name", get_DW_AT_value(entry, "DW_AT_name"));

    if (entry.find("DW_AT_type") != std::string::npos)
      evaluate_DW_TAG_type(get_DW_AT_value(entry, "DW_AT_type"), argTags, ptArgument);
  }
  // If not defined then the value is a SCALAR value
  ptArgument.put("address-qualifier", ptArgument.get<std::string>("address-qualifier", "SCALAR"));
}

static std::string
create_function_signature(const boost::property_tree::ptree& ptArgsArray)
{
  std::string signature;

  // Build the signature
  for (const auto& arg : ptArgsArray) {
    if (!signature.empty())
      signature += ", ";
    signature += arg.second.get<std::string>("type");
    signature += " ";
    signature += arg.second.get<std::string>("name");
  }

  return "(" + signature + ")";
}

static void
add_DWTAG_subprogram(size_t& index,
                     const std::vector<std::string>& dwarfEntries,
                     const AbbrevCollection& argTags,
                     const std::vector<std::string> exportedFunctions,
                     boost::property_tree::ptree& ptFunctionArray)
// <1><c92>: Abbrev Number: 64 (DW_TAG_subprogram)
//    <c93>   DW_AT_external    : 1
//    <c93>   DW_AT_name        : (indirect string, offset: 0x3e8): kernel_vcu_decoder_fini
//    <c97>   DW_AT_decl_file   : 1
//    <c98>   DW_AT_decl_line   : 26
//    <c99>   DW_AT_type        : <0x592>
//    <c9d>   DW_AT_low_pc      : 0x7b0
//    <ca5>   DW_AT_high_pc     : 0x8
//    <cad>   DW_AT_frame_base  : 1 byte block: 9c 	(DW_OP_call_frame_cfa)
//    <caf>   DW_AT_GNU_all_call_sites: 1
//    <caf>   DW_AT_sibling     : <0xcc3>
// <2><cb3>: Abbrev Number: 65 (DW_TAG_formal_parameter)
//    <cb4>   DW_AT_name        : (indirect string, offset: 0x3e0): handles
//    <cb8>   DW_AT_decl_file   : 1
//    <cb9>   DW_AT_decl_line   : 26
//    <cba>   DW_AT_type        : <0xcc3>
//    <cbe>   DW_AT_location    : 0x0 (location list)
{
  // -- Get function metadata
  boost::property_tree::ptree ptFunction;
  const size_t subProgramIndex = index;

  // Examine all of the subprogram key/value pairs
  while ((++index < dwarfEntries.size()) &&
         (!isTag(dwarfEntries[index]))) {
    const auto& entry = dwarfEntries[index];
    if (entry.find("DW_AT_name") != std::string::npos)
      ptFunction.put("name", get_DW_AT_value(entry, "DW_AT_name"));
  }

  const auto& functionName = ptFunction.get<std::string>("name", "");

  if (functionName.empty())
    throw std::runtime_error("ERROR: Could not find the function name for the sub-program. Index: " + std::to_string(subProgramIndex));

  // See if this function is visible
  if (std::find(exportedFunctions.begin(), exportedFunctions.end(), functionName) == exportedFunctions.end())
    return;

  // Infer the function type
  std::string functionType = "kernel";
  if (boost::algorithm::ends_with(functionName, "_init"))
    functionType = "init";

  if (boost::algorithm::ends_with(functionName, "_fini"))
    functionType = "fini";

  ptFunction.put("type", functionType);

  // -- Find and add the arguments
  boost::property_tree::ptree ptArgsArray;

  // Look for all of the arguments
  while ((index < dwarfEntries.size()) &&
         (isTag(dwarfEntries[index])) &&
         (get_DW_TAG(dwarfEntries[index]) == DW_TAG::formal_parameter)) {

    // Examine this argument
    boost::property_tree::ptree ptArg;
    add_formal_parameter(index, dwarfEntries, argTags, ptArg);
    ptArgsArray.push_back(std::make_pair("", ptArg));
  }

  if (!ptArgsArray.empty()) {
      // If the function type is a kernel, the last argument should not have an ID
      if (functionType == "kernel")
        ptArgsArray.back().second.put("use-id", 0);

    ptFunction.add_child("args", ptArgsArray);
  }

  ptFunction.put("signature", create_function_signature(ptArgsArray));

  // The the kernel to the collection
  ptFunctionArray.push_back(std::make_pair("", ptFunction));
}

static void
buildKernelMetadataFromDWARF(const std::vector<std::string>& dwarfEntries,
                             const std::vector<std::string> exportedFunctions,
                             boost::property_tree::ptree& ptFunctions)
{
  // Note: The add_DWTAG* methods will always return to the next index.
  //       No need to perform an increment after they return.

  // -- Collect all of the argument references
  AbbrevCollection argTags;

  size_t index = 0;
  while (index < dwarfEntries.size()) {
    const auto& entry = dwarfEntries[index];
    if (!isTag(entry)) {
      ++index;
      continue;
    }

    switch (get_DW_TAG(entry)) {
      case DW_TAG::pointer_type:
        add_DWTAG_pointerType(index, dwarfEntries, argTags);
        break;

      case DW_TAG::reference_type:
        add_DWTAG_referenceType(index, dwarfEntries, argTags);
        break;

      case DW_TAG::_typedef:
        add_DWTAG_typedef(index, dwarfEntries, argTags);
        break;

      case DW_TAG::base_type:
        add_DWTAG_base_type(index, dwarfEntries, argTags);
        break;

      case DW_TAG::class_type:
        add_DWTAG_class_type(index, dwarfEntries, argTags);
        break;

      case DW_TAG::const_type:
        add_DWTAG_const_type(index, dwarfEntries, argTags);
        break;

      default:
        ++index;
        break;
    }
  }

  // -- Captured tags
  for (auto& entry: argTags) {
    std::string hexEntry = boost::str(boost::format("Tag cache: 0x%x") % entry.first);
    XUtil::TRACE_PrintTree(hexEntry, entry.second);
  }

  // -- Collect and transpose the functions
  boost::property_tree::ptree ptFunctionArray;
  index = 0;
  while (index < dwarfEntries.size()) {
    const auto& entry = dwarfEntries[index];
    if (!isTag(entry)) {
      ++index;
      continue;
    }

    XUtil::TRACE("Examining Tag: " + entry);
    switch (get_DW_TAG(entry)) {
      case DW_TAG::subprogram:
        add_DWTAG_subprogram(index, dwarfEntries, argTags, exportedFunctions, ptFunctionArray);
        break;
      default:
        ++index;
        break;
    }
  }

  ptFunctions.add_child("functions", ptFunctionArray);
}

static void
dataMineExportedFunctionsReadElf(const std::string& elfLibrary,
                                 std::vector<std::string>& dwarfEntries)
{
  // Call readelf to get the collection of functions
  boost::filesystem::path readElfPath = "/usr/bin/readelf";    // Assume it is in a known location

#if (BOOST_VERSION >= 106400)
  readElfPath = boost::process::search_path("readelf");

  const std::string expectedReadElfPath = "/usr/bin/readelf";

  if (readElfPath.string() != expectedReadElfPath)
    std::cout << boost::format("Warning: Unexpected readelf path.\n"
                               "         Expected: %s\n"
                               "           Actual: %s\n") % expectedReadElfPath % readElfPath.string();
#endif

  const std::vector<std::string> cmdOptions = { "--wide", "-wi", elfLibrary };
  std::ostringstream os_stdout;
  std::ostringstream os_stderr;

  // Build the command line
  std::string cmdLine = readElfPath.string();
  for (const auto& option : cmdOptions)
    cmdLine += " " + option;

  XUtil::TRACE("cmd: " + cmdLine);
  XUtil::exec(readElfPath, cmdOptions, true /*throw exception*/, os_stdout, os_stderr);

  XUtil::TRACE("Parsing results from the readelf cmd");
  std::string output = os_stdout.str();
  boost::split(dwarfEntries, output, boost::is_any_of("\n"), boost::token_compress_on);
}

static void
drcCheckExportedFunctions(const std::vector<std::string> exportedFunctions)
{
  // Examine the exported functions.  If any have a signature, this indicates that
  // C++ mangling is enabled.
  for (auto entry :exportedFunctions) {
    // A signature starts with a '('.  For example:  kernel0_fini(xrtHandles*)
    if (entry.find("(") != std::string::npos) {
      std::string errMsg = boost::str(boost::format("ERROR: C++ mangled functions are not supported, please export the function.  Offending function: '%s'"));
      throw std::runtime_error(errMsg);
    }
  }
}

void
XclBinUtilities::dataMineExportedFunctionsDWARF(const std::string& elfLibrary, boost::property_tree::ptree& ptFunctions)
{
  // Retrieve the collection of exported functions
  const std::vector<std::string> exportedFunctions = dataMineExportedFunctionsObjdump(elfLibrary);
  drcCheckExportedFunctions(exportedFunctions);

  // Retrieve the DWARF text database.
  // Note: Next release the shared library's elf file should be examined directly
  std::vector<std::string> dwarfEntries;
  dataMineExportedFunctionsReadElf(elfLibrary, dwarfEntries);
  buildKernelMetadataFromDWARF(dwarfEntries, exportedFunctions, ptFunctions);

  XUtil::TRACE_PrintTree("Kernel candidates", ptFunctions);
}

