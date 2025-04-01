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

namespace xrt {

// class elf_impl - Implementation
class elf_impl
{
  ELFIO::elfio m_elf;

  // Structure to hold symbol information of an
  // entry in .dynsym section
  struct symbol_info
  {
    std::string name;
    ELFIO::Elf64_Addr value{};
    ELFIO::Elf_Xword size{};
    unsigned char bind{};
    unsigned char type{};
    ELFIO::Elf_Half section_index{};
    unsigned char other{};
  };

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

  symbol_info
  get_symbol(ELFIO::section* section, const std::string& symbol_name) const
  {
    const ELFIO::symbol_section_accessor symbols(m_elf, section);
    for (unsigned int i = 0; i < symbols.get_symbols_num(); ++i) {
      symbol_info info;
      symbols.get_symbol(i, info.name, info.value, info.size, info.bind,
          info.type, info.section_index, info.other);
      if (info.name == symbol_name) {
        return info;
      }
    }

    throw std::runtime_error(symbol_name + " symbol not found in .dynsym");
  }

  size_t
  get_ctrl_scratchpad_mem_size() const
  {
    // Symbol 'scratch-pad-ctrl' in .dynsym section represents
    // control scratch pad memory, the symbol entry has the size info
    static const char* section_name = ".dynsym";
    static const char* symbol = "scratch-pad-ctrl";
    auto section = m_elf.sections[section_name];
    if (!section)
      throw std::runtime_error(".dynsym section not found");

    auto info = get_symbol(section, symbol); // throws if symbol not found
    return info.size;
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

size_t
program::
get_ctrl_scratchpad_mem_size() const
{
  return get_handle()->get_ctrl_scratchpad_mem_size();
}

} // namespace xrt::aie
  

