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

std::vector<std::string> 
dataMineExportedFunctionsObjdump(const std::string &elfLibrary) {
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

  const std::vector<std::string> cmdOptions = {"--wide", "--section=.text", "-T", "-C", elfLibrary};
  std::ostringstream os_stdout;
  std::ostringstream os_stderr;
  XUtil::exec(objdumpPath, cmdOptions, true /*throw exception*/, os_stdout, os_stdout);
                                                                                      
  std::vector<std::string> entries;
  std::string output = os_stdout.str();
  boost::split(entries, output, boost::is_any_of("\n"), boost::token_compress_on);

  // Look for the following function attributes in the '.text' section
  // g - Global
  // F - Function
  std::vector<std::string> kernelSignatures;

  for (const auto &entry : entries) {
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
  return kernelSignatures;
}



std::vector<std::string> 
XclBinUtilities::dataMineExportedFunctions(const std::string &elfLibrary) {
  if ( !boost::filesystem::exists( elfLibrary ) )
    throw std::runtime_error("Error: The PS library file does not exist: '" + elfLibrary + "'");

  // Data mine the ELF file
  std::vector<std::string> kernelSignatures = dataMineExportedFunctionsObjdump(elfLibrary);

  if (kernelSignatures.empty())
    throw std::runtime_error("Error: No global exported functions were found in the library: '" + elfLibrary + "'");

  return kernelSignatures;
}

