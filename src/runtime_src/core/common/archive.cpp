// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#define XCL_DRIVER_DLL_EXPORT  // in same dll as exported xrt apis
#define XRT_API_SOURCE         // in same dll as coreutil
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil

#include "archive.h"

#ifdef _WIN32
# pragma warning( push )
# pragma warning(disable : 4389 4458)
#endif

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#include <ario/ario.hpp>

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

#ifdef _WIN32
# pragma warning( pop )
#endif

namespace xrt_core {

class archive_impl
{
  ARIO::ario m_archive;

public:
  archive_impl(const std::string& archive_filename)
  {
    m_archive.load(archive_filename);
  }

  std::string
  data(const std::string& archive_member) const
  {
    return m_archive.members[archive_member].data();
  }
}; // class archive_impl

////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////
archive::
archive(const std::string& archive_filename)
  : xrt::detail::pimpl<archive_impl>(std::make_shared<archive_impl>(archive_filename))
{}
  
std::string
archive::
data(const std::string& archive_member) const
{
  return handle->data(archive_member);
}

} // xrt_core
