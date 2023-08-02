// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_module.h
#define XRT_API_SOURCE         // exporting xrt_module.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "experimental/xrt_module.h"
#include "experimental/xrt_elf.h"

#include "xrt/xrt_bo.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_uuid.h"

#include "elf_int.h"
#include "module_int.h"
#include "core/common/debug.h"
#include "core/common/error.h"

#include <elfio/elfio.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>

#ifndef AIE_COLUMN_PAGE_SIZE
# define AIE_COLUMN_PAGE_SIZE 8192
#endif

static constexpr size_t column_page_size = AIE_COLUMN_PAGE_SIZE;

namespace {

// struct ctrlcode - represent control code for column or partition
//
// Manage ctrlcode data for a single column or partition with optional
// padding of pages per ELF spec.
struct ctrlcode
{
  std::vector<uint8_t> m_data;

  void
  append_section_data(ELFIO::section* sec)
  {
    auto sz = sec->get_size();
    auto sdata = sec->get_data();
    m_data.insert(m_data.end(), sdata, sdata + sz);
  }

  void
  append_section_data(const uint8_t* userptr, size_t sz)
  {
    m_data.insert(m_data.end(), userptr, userptr + sz);
  }

  void
  pad_to_page(int page)
  {
    if (!column_page_size)
      return;
    
    auto pad = (page + 1) * column_page_size;

    if (m_data.size() > pad)
      throw std::runtime_error("Invalid ELF section size");

    m_data.resize(pad);
  }

  size_t
  size() const
  {
    return m_data.size();
  }

  const uint8_t*
  data() const
  {
    return m_data.data();
  }
};

} // namespace

namespace xrt {

// class module_impl - Base class for different implementations
class module_impl
{
protected:
  xrt::uuid m_cfg_uuid;     // matching hw configuration id

public:
  module_impl(xrt::uuid cfg_uuid)
    : m_cfg_uuid(std::move(cfg_uuid))
  {}

  module_impl(const module_impl* parent)
    : m_cfg_uuid(parent->m_cfg_uuid)
  {}

  virtual
  ~module_impl()
  {}

  xrt::uuid
  get_cfg_uuid() const
  {
    return m_cfg_uuid;
  }

  // Get raw instruction buffer data for all columns or for
  // single partition.  The returned vector has the control
  // code as extracted from ELF or userptr.
  virtual const std::vector<ctrlcode>&
  get_data() const
  {
    throw std::runtime_error("Not supported");
  }

  virtual xrt::hw_context
  get_hw_context() const
  {
    return {};
  }

  // Get the address and size of the control code for all columns
  // or for a single partition.  The returned vector has elements
  // that are used when populating ert_dpu_data elements embedded
  // in an ert_packet.
  virtual const std::vector<std::pair<uint64_t, uint64_t>>&
  get_ctrlcode_addr_and_size() const
  {
    throw std::runtime_error("Not supported");
  }
};

// class module_elf - Elf provided by application
//
// Construct a module from a ELF file provided by the application.
// The ELF is mined for control code sections which are extracted
// into a vector of ctrlcode objects.  The ctrlcode objects are
// binary blobs that are used to populate the ert_dpu_data elements
// of an ert_packet.
class module_elf : public module_impl
{
  xrt::elf m_elf;
  std::vector<ctrlcode> m_ctrlcodes;

  // The ELF sections embed column and page information in their
  // names.  Extract the column and page information from the
  // section name, default to single column and page when nothing
  // is specified.
  static std::pair<uint32_t, uint32_t>
  get_column_and_page(const std::string& name)
  {
    constexpr auto first_dot = 9;  // .ctrltext.<col>.<page>
    auto dot1 = name.find_first_of(".", first_dot);
    auto dot2 = name.find_first_of(".", first_dot + 1);
    auto col = dot1 != std::string::npos 
      ? std::stoi(name.substr(dot1 + 1, dot2))
      : 0;
    auto page = dot2 != std::string::npos 
      ? std::stoi(name.substr(dot2 + 1))
      : 0;
    return {col, page};
  }

  // Extract control code from ELF sections without assuming anything
  // about order of sections in the ELF file.  Build helper data
  // structures that manages the control code data for each column and
  // page, then create ctrlcode objects from the data.
  static std::vector<ctrlcode>
  initialize_column_ctrlcode(const ELFIO::elfio& elf)
  {
    // Elf sections for a single page
    struct column_page {
      ELFIO::section* ctrltext = nullptr;
      ELFIO::section* ctrldata = nullptr;
    };

    // Elf sections for a single column, the column control code is
    // divided into pages of some architecture defined size.
    struct column_sections {
      using page_index = uint32_t;
      std::map<page_index, column_page> pages;
    };

    // Elf ctrl code for a partition spanning multiple columns, where
    // each column has its own control code.  For architectures where
    // a partition is not divided into columns, there will be just one
    // entry in associative map.
    // col -> [page -> [ctrltext, ctrldata]]
    using column_index = uint32_t;
    std::map<column_index, column_sections> col_secs; 

    // Iterate sections in elf, collect ctrltext and ctrldata
    // per column and page
    for (const auto& sec : elf.sections) {
      auto name = sec->get_name();
      if (name.find(".ctrltext") != std::string::npos) {
        auto [col, page] = get_column_and_page(sec->get_name());
        col_secs[col].pages[page].ctrltext = sec.get();
      }
      else if (name.find(".ctrldata") != std::string::npos) {
        auto [col, page] = get_column_and_page(sec->get_name());
        col_secs[col].pages[page].ctrldata = sec.get();
      }
    }

    // Create column control code from the collected data
    // If page requirement, then pad to page size for page
    // of a column so that embedded processor can load a page
    // at a time.
    std::vector<ctrlcode> ctrlcodes;
    ctrlcodes.resize(col_secs.size());

    for (auto& [col, col_sec] : col_secs) {
      for (auto& [page, page_sec] : col_sec.pages) {
        if (page_sec.ctrltext)
          ctrlcodes[col].append_section_data(page_sec.ctrltext);

        if (page_sec.ctrldata)
          ctrlcodes[col].append_section_data(page_sec.ctrldata);

        ctrlcodes[col].pad_to_page(page);
      }
    }

    return ctrlcodes;
  }
  

public:
  module_elf(xrt::elf elf)
    : module_impl{elf.get_cfg_uuid()}
    , m_elf(std::move(elf))
    , m_ctrlcodes{initialize_column_ctrlcode(xrt_core::elf_int::get_elfio(m_elf))}
  {}

  const std::vector<ctrlcode>&
  get_data() const override
  {
    return m_ctrlcodes;
  }
};

// class module_userptr - Opaque userptr provided by application
class module_userptr : public module_impl
{
  std::vector<ctrlcode> m_ctrlcode;

  // Create a ctrlcode object from the userptr.
  static std::vector<ctrlcode>
  initialize_ctrlcode(const char* userptr, size_t sz)
  {
    std::vector<ctrlcode> ctrlcodes;
    ctrlcodes.resize(1);
    ctrlcodes[0].append_section_data(reinterpret_cast<const uint8_t*>(userptr), sz);
    return ctrlcodes;
  }

public:
  module_userptr(const char* userptr, size_t sz, const xrt::uuid& uuid)
    : module_impl{uuid}
    , m_ctrlcode{initialize_ctrlcode(userptr, sz)}
  {}

  module_userptr(const void* userptr, size_t sz, const xrt::uuid& uuid)
    : module_userptr(static_cast<const char*>(userptr), sz, uuid)
  {}

  const std::vector<ctrlcode>&
  get_data() const override
  {
    return m_ctrlcode;
  }
};

// class module_sram - Create an hwct specific (sram) module from parent
//
// Allocate a buffer object to hold the ctrlcodes for each column created
// by parent module.  The ctrlcodes are concatenated into a single buffer
// where buffer object address of offset for each column.
class module_sram : public module_impl
{
  std::shared_ptr<module_impl> m_parent;
  xrt::hw_context m_hwctx;

  // The instruction buffer object contains the ctrlcodes for each
  // column.  The ctrlcodes are concatenated into a single buffer
  // padded at page size specific to hardware.
  xrt::bo m_buffer;

  // Column bo address is the address of the ctrlcode for each column
  // in the (sram) buffer object.  The first ctrlcode is at the base
  // address (m_buffer.address()) of the buffer object.  The addresses
  // are used in ert_dpu_data payload to identify the ctrlcode for
  // each column.
  std::vector<std::pair<uint64_t, uint64_t>> m_column_bo_address;

  // For separated multi-column control code, compute the ctrlcode
  // buffer object address of each column (used in ert_dpu_data).
  void
  fill_column_bo_address(const std::vector<ctrlcode>& ctrlcodes)
  {
    m_column_bo_address.clear();
    auto base_addr = m_buffer.address();
    for (const auto& ctrlcode : ctrlcodes) {
      m_column_bo_address.push_back({base_addr, ctrlcode.size()});
      base_addr += ctrlcode.size();
    }
  }

  // Fill the instruction buffer object with the ctrlcodes for each
  // column and sync the buffer to device.
  void
  fill_instruction_buffer(xrt::bo& bo, const std::vector<ctrlcode>& ctrlcodes)
  {
    auto ptr = bo.map<char*>();
    for (const auto& ctrlcode : ctrlcodes) {
      std::memcpy(ptr, ctrlcode.data(), ctrlcode.size());
      ptr += ctrlcode.size();
    }
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  }

  // Create the instruction buffer object and fill it with column
  // ctrlcodes.
  void
  create_instruction_buffer(const module_impl* parent)
  {
    XRT_PRINTF("-> module_sram::create_instruction_buffer()\n");
    auto data = parent->get_data();

    // create bo combined size of all ctrlcodes
    size_t sz = std::accumulate(data.begin(), data.end(), static_cast<size_t>(0), [](auto acc, const auto& ctrlcode) {
      return acc + ctrlcode.size();
    });
    m_buffer = xrt::bo{m_hwctx, sz, xrt::bo::flags::cacheable, 1 /* fix me */};

    // copy ctrlcodes into bo
    fill_instruction_buffer(m_buffer, data);

    XRT_PRINTF("<- module_sram::create_instruction_buffer()\n");
  }

public:
  module_sram(std::shared_ptr<module_impl> parent, xrt::hw_context hwctx)
    : module_impl{parent->get_cfg_uuid()}
    , m_parent{std::move(parent)}
    , m_hwctx{std::move(hwctx)}  
  {
    create_instruction_buffer(m_parent.get());
    fill_column_bo_address(m_parent->get_data());
  }

  const std::vector<std::pair<uint64_t, uint64_t>>&
  get_ctrlcode_addr_and_size() const override
  {
    return m_column_bo_address;
  }
};

} // namespace xrt

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal module APIs
////////////////////////////////////////////////////////////////
namespace xrt_core::module_int {

const std::vector<std::pair<uint64_t, uint64_t>>&
get_ctrlcode_addr_and_size(const xrt::module& module)
{
  return module.get_handle()->get_ctrlcode_addr_and_size();
}

} // xrt_core::module_int

////////////////////////////////////////////////////////////////
// xrt_module C++ API implementation (xrt_module.h)
////////////////////////////////////////////////////////////////
namespace xrt {

module::
module(const xrt::elf& elf)
  : detail::pimpl<module_impl>{std::make_shared<module_elf>(elf)}
{}

module::
module(void* userptr, size_t sz, const xrt::uuid& uuid)
  : detail::pimpl<module_impl>{std::make_shared<module_userptr>(userptr, sz, uuid)}
{}

module::
module(const xrt::module& parent, const xrt::hw_context& hwctx)
  : detail::pimpl<module_impl>{std::make_shared<module_sram>(parent.handle, hwctx)}
{}

xrt::uuid
module::
get_cfg_uuid() const
{
  return handle->get_cfg_uuid();
}

xrt::hw_context
module::
get_hw_context() const
{
  return handle->get_hw_context();
}

} // namespace xrt
