// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT error APIs as declared in
// core/include/experimental/xrt_version.h
#define XRT_API_SOURCE         // exporting xrt_version.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_version.h"
#include "version.h"

#ifdef major
# undef major
#endif

#ifdef minor
# undef minor
#endif

namespace xrt::version {

unsigned int
code()
{
  return XRT_VERSION_CODE;
}

unsigned int
major()
{
  return XRT_MAJOR(code());
}

unsigned int
minor()
{
  return XRT_MINOR(code());
}

unsigned int
patch()
{
  return XRT_PATCH;
}

unsigned int
build()
{
  return XRT_BUILD;
}

} // namespace xrt::version

////////////////////////////////////////////////////////////////
// xrt_message C API implmentations (xrt_version.h)
////////////////////////////////////////////////////////////////
unsigned int
xrtVersionCode()
{
  return xrt::version::code();
}

unsigned int
xrtVersionMajor()
{
  return xrt::version::major();
}

unsigned int
xrtVersionMinor()
{
  return xrt::version::minor();
}
  
unsigned int
xrtVersionPatch()
{
  return xrt::version::patch();
}

unsigned int
xrtVersionBuild()
{
  return xrt::version::build();
}
