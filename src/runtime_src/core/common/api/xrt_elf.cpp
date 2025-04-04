// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_elf.h
#define XRT_API_SOURCE         // exporting xrt_elf.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "xrt/experimental/xrt_aie.h"
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

#include <boost/interprocess/streams/bufferstream.hpp>

namespace xrt {

// class elf_impl - Implementation
class elf_impl
{
  ELFIO::elfio m_elf;

public:
  explicit
  elf_impl(const std::string& fnm)
  {
    if (!m_elf.load(fnm))
      throw std::runtime_error(fnm + " is not found or is not a valid ELF file");

    if (xrt_core::config::get_xrt_debug()) {
      std::string message = "Loaded elf file " + fnm;
      xrt_core::message::send( xrt_core::message::severity_level::debug, "xrt_elf", message);
    }
  }

  explicit
  elf_impl(std::istream& stream)
  {
    if (!m_elf.load(stream))
      throw std::runtime_error("not a valid ELF stream");
  }

  explicit elf_impl(const void *data, size_t size)
  {
    // Uses the same approach as in aiebu reporter.cpp
    // ibufferstream allows reading from data without first copying over
    // the data to create the stream
    boost::interprocess::ibufferstream istr(static_cast<const char *>(data), size);
    if (!m_elf.load(istr))
      throw std::runtime_error("not valid ELF data");
  }

  const ELFIO::elfio&
  get_elfio() const
  {
    return m_elf;
  }

  xrt::uuid
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

  std::string
  get_note(const ELFIO::section* section, ELFIO::Elf_Word note_num) const
  {
    //ELFIO::note_section_accessor accessor(m_elf, section);
    ELFIO::note_section_accessor accessor(m_elf, const_cast<ELFIO::section*>(section));
    ELFIO::Elf_Word type = 0;
    std::string name;
    char* desc = nullptr;
    ELFIO::Elf_Word desc_size = 0;
    if (!accessor.get_note(note_num, type, name, desc, desc_size))
      throw std::runtime_error("Failed to get note, note not found\n");
    return std::string{desc, desc_size};
  }

  uint32_t
  get_partition_size() const
  {
    // Partition size is stored in as note 0 in .note.xrt.configuration section
    if (auto section = m_elf.sections[".note.xrt.configuration"])
      return std::stoul(get_note(section, 0));

    throw std::runtime_error("ELF is missing xrt configuration info");
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

uint32_t
get_partition_size(const xrt::elf& elf)
{
  return elf.get_handle()->get_partition_size();
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

elf::
elf(const void *data, size_t size)
    : detail::pimpl<elf_impl>{std::make_shared<elf_impl>(data, size)}
{}

xrt::uuid
elf::
get_cfg_uuid() const
{
  return handle->get_cfg_uuid();
}

} // namespace xrt

////////////////////////////////////////////////////////////////
// xrt::aie::program C++ API implementation (xrt_aie.h)
////////////////////////////////////////////////////////////////
namespace xrt::aie {

void
program::
valid_or_error()
{
  // Validate that the ELF file is a valid AIE program
}

program::size_type
program::
get_partition_size() const
{
  return get_handle()->get_partition_size();
}

} // namespace xrt::aie
