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

// Copyright 2017 Xilinx, Inc. All rights reserved.


#include "xocl/config.h"
#include "xocl/core/platform.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "xocl/core/program.h"
#include "xocl/core/range.h"
#include "xocl/core/error.h"
#include "detail/context.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <fstream>

// should use some md5 checksum instead
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

#ifndef _WIN32
#include <crypt.h>

namespace {

static std::vector<char>
read_file(const std::string& filename)
{
  std::ifstream istr(filename,std::ios::binary|std::ios::ate);
  if (!istr)
    throw xocl::error(CL_BUILD_PROGRAM_FAILURE,"Cannot not open '" + filename + "' for reading");

  auto pos = istr.tellg();
  istr.seekg(0,std::ios::beg);

  std::vector<char> buffer(pos);
  istr.read (&buffer[0],pos);

  return buffer;
}

}

namespace xocl {

static void
validOrError(cl_context        context,
             cl_uint           count,
             const char **     strings,
             const size_t *    lengths,
             cl_int *          errcode_ret)
{
  if (!config::api_checks())
    return;

  detail::context::validOrError(context);

  // CL_INVALID_VALUE if count is zero or if strings or any entry in strings is NULL
  if (!count)
    throw xocl::error(CL_INVALID_VALUE,"count is zero");
  if (!strings)
    throw xocl::error(CL_INVALID_VALUE,"strings is nullptr");
  for (auto string : xocl::get_range(strings,strings+count))
    if (!string)
      throw xocl::error(CL_INVALID_VALUE,"string element is nullptr");

}

static cl_program
clCreateProgramWithSource(cl_context        context,
                          cl_uint           count,
                          const char **     strings,
                          const size_t *    lengths,
                          cl_int *          errcode_ret)
{
  if (!std::getenv("XCL_CONFORMANCE"))
    throw xocl::error(CL_INVALID_OPERATION,"clCreateProgramWithSource() is not supported, please use clCreateProgramWithBinary().");

  validOrError(context,count,strings,lengths,errcode_ret);

  // precondition: one device in context
  auto device = xocl(context)->get_device_if_one();
  if (!device)
    throw xocl::error(CL_INVALID_BINARY,"precondition check failed, multiple devices in context");

  // Concatenate all the provided fragments together
  std::string source;
  for (unsigned int i=0; i<count; ++i)
    source += (lengths && lengths[i])
               ? std::string(strings[i], strings[i] + lengths[i])
               : std::string(strings[i]);

  auto program = std::make_unique<xocl::program>(xocl::xocl(context),source);
  program->add_device(device);

  // hash source, checksumming would be better (portable)
  char salt[] = "$1$salt$encrypted";
  char* chash = crypt(source.c_str(),salt);
  chash+=8; //past $1$salt$
  XOCL_DEBUG(std::cout,"computed source hash: '",chash,"'\n");
  XOCL_DEBUG(std::cout,source,"\n");
  std::string hash(chash);

  // locate xclbin containing hash
  std::string filematch = xocl::conformance_get_xclbin(hash);
  if (filematch.empty())
    throw error(CL_INVALID_BINARY,"clCreateProgramWithSource: error no XCLBIN with matching hash");

  // may be more than one matching hashmatch if multiple kernels are
  // in prorgam and spread out over multiple binaries
  for (auto gprogram : xocl::get_global_programs()) {
    auto xp = xocl(gprogram);
    if ((xp->conformance_binaryfilename==filematch) && (xp->conformance_binaryhash==hash)) {
      gprogram->retain();
      return gprogram;
    }
  }


  auto xclbin = read_file(filematch);
  char* binary = &xclbin[0]; // char
  size_t length=xclbin.size();

  // hash match found clCreateProgramWithBinary and exit search
  cl_int err = CL_SUCCESS;
  cl_device_id cldevice = device; // cast before taking address
  cl_program binaryprogram =
    clCreateProgramWithBinary(context,1,&cldevice,&length,(const unsigned char **)&binary,nullptr,&err);
  if (err!=CL_SUCCESS)
    throw error(CL_INVALID_BINARY,"clCreateProgramWithSource: conformance failed to load binary with matching hash");

  auto bprogram = xocl::xocl(binaryprogram);
  bprogram->conformance_binaryfilename=filematch;
  bprogram->conformance_binaryhash=hash;
  bprogram->set_source(source);

  // Matching kernels must be renamed to remove the special naming
  // convention associated with comformance.
  bprogram->conformance_rename_kernel(hash);

  xocl::assign(errcode_ret,CL_SUCCESS);
  return bprogram;
}

} // xocl

#endif // ifndef _WIN32

cl_program
clCreateProgramWithSource(cl_context        context,
                          cl_uint           count,
                          const char **     strings,
                          const size_t *    lengths,
                          cl_int *          errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
#ifdef _WIN32
    throw xocl::error(CL_INVALID_OPERATION,"clCreateProgramWithSource() is not supported, please use clCreateProgramWithBinary().");
#else
    return xocl::clCreateProgramWithSource
      (context,count,strings,lengths,errcode_ret);
#endif
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    if (errcode_ret)
      *errcode_ret = ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    if (errcode_ret)
      *errcode_ret = CL_OUT_OF_HOST_MEMORY;
  }
  return nullptr;
}
