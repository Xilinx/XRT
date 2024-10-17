// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#define XCL_DRIVER_DLL_EXPORT
#define XRT_API_SOURCE
#include "capture.h"
#include "logger.h"

#include <filesystem>
#include <iostream>

namespace xtx = xrt::tools::xbtracer;
namespace fs = std::filesystem;

// NOLINTNEXTLINE(cert-err58-cpp)
static const xtx::xrt_ftbl& dtbl = xtx::xrt_ftbl::get_instance();

/*
 *  Device class instrumented methods
 * */
namespace xrt {

elf::elf(const std::string& fnm)
{
  auto func = "xrt::elf::elf(const std::string&)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.elf.ctor_str, this, fnm);

  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, fnm);

  // Usage in xclbin constructor
  std::vector<unsigned char> buffer;
  xtx::read_file(fnm, buffer);

  xtx::membuf data(buffer.data(), buffer.size());
  XRT_TOOLS_XBT_FUNC_EXIT(func, "data", data);
}

elf::elf(std::istream& stream)
{
  auto func = "xrt::elf::elf(std::istream&)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.elf.ctor_ist, this, stream);

  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, &stream);

  stream.seekg(0, std::ios::end);
  std::streamsize size = stream.tellg();
  stream.seekg(0, std::ios::beg); // Go back to the start of the stream.

  // Create a vector and resize it to the size of the data
  std::vector<unsigned char> buffer(size);

  // Read the data from the stream into the vector's buffer
  stream.read(reinterpret_cast<char*>(buffer.data()), size);

  xtx::membuf data(buffer.data(), buffer.size());
  XRT_TOOLS_XBT_FUNC_EXIT(func, "data", data);
}

xrt::uuid elf::get_cfg_uuid() const
{
  auto func = "xrt::elf::get_cfg_uuid()";
  XRT_TOOLS_XBT_FUNC_ENTRY(func);
  uuid muuid;
  XRT_TOOLS_XBT_CALL_METD_RET(dtbl.elf.get_cfg_uuid, muuid);
  XRT_TOOLS_XBT_FUNC_EXIT_RET(func, muuid.to_string().c_str());
  return muuid;
}

}  // namespace xrt
