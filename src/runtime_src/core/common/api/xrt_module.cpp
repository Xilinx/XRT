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

#include <boost/format.hpp>
#include <elfio/elfio.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <map>
#include <set>
#include <string>

#ifndef AIE_COLUMN_PAGE_SIZE
# define AIE_COLUMN_PAGE_SIZE 8192
#endif

namespace {

// Control code is padded to page size, where page size is
// 0 if no padding is required.   The page size should be
// embedded as ELF metadata in the future.
static constexpr size_t column_page_size = AIE_COLUMN_PAGE_SIZE;

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

// struct patcher - patcher for a symbol
//
// Manage patching of a symbol in the control code.  The symbol
// type is used to determine the patching method.
//
// The patcher is created with an offset into a buffer object
// representing the contigous control code for all columns
// and pages. The base address of the buffer object is passed
// in as a parameter to patch().
struct patcher
{
  enum class symbol_type {
    uc_dma_remote_ptr_symbol_kind = 1,
    shim_dma_base_addr_symbol_kind = 2,
    scalar_32bit_kind = 3,
    unknown_symbol_kind = 4
  };

  symbol_type m_symbol_type;

  // Offset from base address of control code buffer object
  // The base address is passed in as a parameter to patch()
  uint64_t m_ctrlcode_offset;

  patcher(symbol_type type, uint64_t ctrlcode_offset)
    : m_symbol_type(type)
    , m_ctrlcode_offset(ctrlcode_offset)
  {}

  void patch32(uint32_t* bd_data_ptr, uint64_t patch)
  {
    uint64_t base_address = bd_data_ptr[0];
    base_address += patch;
    bd_data_ptr[0] = (uint32_t)(base_address & 0xFFFFFFFF);
  }

  void patch57(uint32_t* bd_data_ptr, uint64_t patch)
  {
    uint64_t base_address =
      ((static_cast<uint64_t>(bd_data_ptr[8]) & 0x1FF) << 48) |
      ((static_cast<uint64_t>(bd_data_ptr[2]) & 0xFFFF) << 32) |
      bd_data_ptr[1];

    base_address += patch;
    bd_data_ptr[1] = (uint32_t)(base_address & 0xFFFFFFFF);
    bd_data_ptr[2] = (bd_data_ptr[2] & 0xFFFF0000) | ((base_address >> 32) & 0xFFFF);
    bd_data_ptr[8] = (bd_data_ptr[8] & 0xFFFFFE00) | ((base_address >> 48) & 0x1FF);
  }

  void patch(uint8_t* base, uint64_t patch)
  {
    auto bd_data_ptr = reinterpret_cast<uint32_t*>(base + m_ctrlcode_offset);
    switch (m_symbol_type) {
    case symbol_type::scalar_32bit_kind:
      patch32(bd_data_ptr, patch);
      break;
    case symbol_type::shim_dma_base_addr_symbol_kind:
      patch57(bd_data_ptr, patch);
      break;
    default:
      throw std::runtime_error("Unsupported symbol type");
    };
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

  // Patch ctrlcode buffer object for global argument
  //
  // @param symbol - symbol name
  // @param bo - global argument to patch into ctrlcode
  virtual void
  patch(const std::string&, const xrt::bo&)
  {
    throw std::runtime_error("Not supported");
  }

  // Patch ctrlcode buffer object for scalar argument
  //
  // @param symbol - symbol name
  // @param value - patch value
  // @param size - size of patch value
  virtual void
  patch(const std::string&, const void*, size_t)
  {
    throw std::runtime_error("Not supported");
  }

  // Patch symbol in control code with patch value
  //
  // @param base - base address of control code buffer object
  // @param symbol - symbol name
  // @param patch - patch value
  // @Return true if symbol was patched, false otherwise  //
  virtual bool
  patch(uint8_t*, const std::string&, uint64_t)
  {
    throw std::runtime_error("Not supported");
  }

  // Get the number of patchers for arguments.  The returned
  // value is the number of arguments that must be patched before
  // the control code can be executed.
  virtual size_t
  number_of_arg_patchers() const
  {
    return 0;
  }

  // Check that all arguments have been patched and sync control code
  // buffer if necessary.  Throw if not all arguments have been patched.
  virtual void
  sync_if_dirty()
  {
    throw std::runtime_error("Not supported");
  }
};

// class module_elf - Elf provided by application
//
// Construct a module from a ELF file provided by the application.
//
// The ELF is mined for control code sections which are extracted
// into a vector of ctrlcode objects.  The ctrlcode objects are
// binary blobs that are used to populate the ert_dpu_data elements
// of an ert_packet.
//
// The ELF is also mined for relocation sections which are used to to
// patch the control code with the address of a buffer object or value
// of a scalar object used as an argument. The relocations are used to
// construct patcher objects for each argument.
class module_elf : public module_impl
{
  xrt::elf m_elf;
  std::vector<ctrlcode> m_ctrlcodes;
  std::map<std::string, patcher> m_arg2patcher;

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
    // entry in the associative map.
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

  static std::map<std::string, patcher>
  initialize_arg_patchers(const ELFIO::elfio& elf, const std::vector<ctrlcode>& ctrlcodes)
  {
    auto dynsym = elf.sections[".dynsym"];
    auto dynstr = elf.sections[".dynstr"];

    std::map<std::string, patcher> arg2patcher;

    for (const auto& sec : elf.sections) {
      auto name = sec->get_name();
      if (name.find(".rela.dyn") == std::string::npos)
        continue;

      // Iterate over all relocations and construct a patcher for each
      // relocation that refers to a symbol in the .dynsym section.
      auto begin = reinterpret_cast<const ELFIO::Elf32_Rela*>(sec->get_data());
      auto end = begin + sec->get_size() / sizeof(const ELFIO::Elf32_Rela);
      for (auto rela = begin; rela != end; ++rela) {
        auto symidx = ELFIO::get_sym_and_type<ELFIO::Elf32_Rela>::get_r_sym(rela->r_info);

        auto dynsym_offset = symidx * sizeof(ELFIO::Elf32_Sym);
        if (dynsym_offset >= dynsym->get_size())
          throw std::runtime_error("Invalid symbol index " + std::to_string(symidx));
        auto sym = reinterpret_cast<const ELFIO::Elf32_Sym*>(dynsym->get_data() + dynsym_offset);

        auto dynstr_offset = sym->st_name;
        if (dynstr_offset >= dynstr->get_size())
          throw std::runtime_error("Invalid symbol name offset " + std::to_string(dynstr_offset));
        auto symname = dynstr->get_data() + dynstr_offset;

        // Get control code section referenced by the symbol, col, and page
        auto ctrl_sec = elf.sections[sym->st_shndx];
        if (!ctrl_sec)
          throw std::runtime_error("Invalid section index " + std::to_string(sym->st_shndx));
        auto [col, page] = get_column_and_page(ctrl_sec->get_name());

        auto column_ctrlcode_size = ctrlcodes.at(col).size();
        auto column_ctrlcode_offset = page * column_page_size + rela->r_offset + 16; // magic number 16??
        if (column_ctrlcode_offset >= column_ctrlcode_size)
          throw std::runtime_error("Invalid ctrlcode offset " + std::to_string(column_ctrlcode_offset));

        // The control code for all columns will be represented as one
        // contiguous buffer object.  The patcher will need to know
        // the offset into the buffer object for the particular column
        // and page being patched.  Past first [0, col) columns plus
        // page offset within column and the relocation offset within
        // the page.
        uint64_t ctrlcode_offset = 0;
        for (uint32_t i = 0; i < col; ++i)
          ctrlcode_offset += ctrlcodes.at(i).size();
        ctrlcode_offset += column_ctrlcode_offset;

        // Construct the patcher for the argument with the symbol name
        std::string argnm{symname, symname + std::min(strlen(symname), dynstr->get_size())};
        auto symbol_type = static_cast<patcher::symbol_type>(rela->r_addend);
        arg2patcher.emplace(std::move(argnm), patcher{symbol_type, ctrlcode_offset});
      }
    }

    return arg2patcher;
  }
  
  bool
  patch(uint8_t* base, const std::string& argnm, uint64_t patch) override
  {
    auto it = m_arg2patcher.find(argnm);
    if (it == m_arg2patcher.end())
      return false;

    it->second.patch(base, patch);
    return true;
  }

public:
  module_elf(xrt::elf elf)
    : module_impl{elf.get_cfg_uuid()}
    , m_elf(std::move(elf))
    , m_ctrlcodes{initialize_column_ctrlcode(xrt_core::elf_int::get_elfio(m_elf))}
    , m_arg2patcher{initialize_arg_patchers(xrt_core::elf_int::get_elfio(m_elf), m_ctrlcodes)}
  {}

  const std::vector<ctrlcode>&
  get_data() const override
  {
    return m_ctrlcodes;
  }

  size_t
  number_of_arg_patchers() const override
  {
    return m_arg2patcher.size();
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

  // Arguments patched in the ctrlcode buffer object
  // Must match number of argument patchers in parent module
  std::set<std::string> m_patched_args;

  // Dirty bit to indicate that patching was done prior to last
  // buffer sync to device.
  bool m_dirty {false};

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

  void
  patch_value(const std::string& argnm, uint64_t value)
  {
    if (!m_parent->patch(m_buffer.map<uint8_t*>(), argnm, value))
      return;
    
    m_patched_args.insert(argnm);
    m_dirty = true;
  }

  void
  patch(const std::string& argnm, const xrt::bo& bo) override
  {
    patch_value(argnm, bo.address());
  }

  void
  patch(const std::string& argnm, const void* value, size_t size) override
  {
    if (size > 8)
      throw std::runtime_error{"patch_value() only supports 64-bit values or less"};

    patch_value(argnm, *static_cast<const uint64_t*>(value));
  }

  // Check that all arguments have been patched and sync the buffer
  // to device if it is dirty.
  void
  sync_if_dirty() override
  {
    if (m_patched_args.size() != m_parent->number_of_arg_patchers()) {
      auto fmt = boost::format("ctrlcode requires %d patched arguments, but only %d are patched")
        % m_parent->number_of_arg_patchers() % m_patched_args.size();
      throw std::runtime_error{fmt.str()};
    }

    if (!m_dirty)
      return;
    
    m_buffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    m_dirty = false;
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

void
patch(const xrt::module& module, const std::string& argnm, const xrt::bo& bo)
{
  module.get_handle()->patch(argnm, bo);
}

void
patch(const xrt::module& module, const std::string& argnm, const void* value, size_t size)
{
  module.get_handle()->patch(argnm, value, size);
}

void
sync(const xrt::module& module)
{
  module.get_handle()->sync_if_dirty();
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
