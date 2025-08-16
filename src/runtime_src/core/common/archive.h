// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrtcore_archive_h_
#define xrtcore_archive_h_

#include "core/include/xrt/detail/config.h"
#include "core/include/xrt/detail/pimpl.h"
#include <string>

namespace xrt_core {

// class archive - Simple API to extract archive member
//
// The archive (.a) must be a UNIX archive.  The archive
// is read using ARIO from the ELFIO project repository.
//
// The implementation is insulated to for minimal header
// file inclusion by clients.
class archive_impl;
class archive : public xrt::detail::pimpl<archive_impl>
{
public:
  // archive() - Create an archive object from a archive filename
  XRT_API_EXPORT
  explicit
  archive(const std::string& archive_filename);

  // data() - Get the data of an archive member as a string
  XRT_API_EXPORT
  std::string
  data(const std::string& archive_member) const;
};
  
}

#endif
