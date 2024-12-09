// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_elf.h
#define XRT_API_SOURCE         // exporting xrt_elf.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "xrt/experimental/xrt_elf.h"
#include "xrt/xrt_uuid.h"

#include "elf_int.h"
#include "core/common/config_reader.h"
#include "core/common/error.h"
#include "core/common/message.h"

#include <elfio/elfio.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace xrt {

// class elf_impl - Implementation
class elf_impl
{
  ELFIO::elfio m_elf;
public:
  explicit elf_impl(const std::string& fnm)
  {
    if (!m_elf.load(fnm))
      throw std::runtime_error(fnm + " is not found or is not a valid ELF file");

    if (xrt_core::config::get_xrt_debug()) {
      std::string message = "Loaded elf file " + fnm;
      xrt_core::message::send( xrt_core::message::severity_level::debug, "xrt_elf", message);
    }
  }

  explicit elf_impl(std::istream& stream)
  {
    if (!m_elf.load(stream))
      throw std::runtime_error("not a valid ELF stream");
  }

  [[nodiscard]] const ELFIO::elfio&
  get_elfio() const
  {
    return m_elf;
  }

  [[nodiscard]] xrt::uuid
  get_cfg_uuid() const
  {
    return {}; // tbd
  }

  std::vector<uint8_t>
  get_section(const std::string& sname)
  {
    auto sec = m_elf.sections[sname];
    if (!sec)
      throw std::runtime_error("Failed to find section: " + sname);

    auto data = sec->get_data();
    std::vector<uint8_t> vec(data, data + sec->get_size());
    return vec;
  }
};

} // namespace xrt

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal elf APIs
////////////////////////////////////////////////////////////////
namespace xrt_core::elf_int {

// Extract section data from ELF file
std::vector<uint8_t>
get_section(const xrt::elf& elf, const std::string& sname)
{
  return elf.get_handle()->get_section(sname);
}

const ELFIO::elfio&
get_elfio(const xrt::elf& elf)
{
  return elf.get_handle()->get_elfio();
}

} // xrt_core::elf_int

////////////////////////////////////////////////////////////////
// xrt_elf C++ API implementation (xrt_elf.h)
////////////////////////////////////////////////////////////////
namespace xrt {

elf::
elf(const std::string& fnm)
  : detail::pimpl<elf_impl>{std::make_shared<elf_impl>(fnm)}
{}

elf::
elf(std::istream& stream)
  : detail::pimpl<elf_impl>{std::make_shared<elf_impl>(stream)}
{}

xrt::uuid
elf::
get_cfg_uuid() const
{
  return handle->get_cfg_uuid();
}

} // namespace xrt
