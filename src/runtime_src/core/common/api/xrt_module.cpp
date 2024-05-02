// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_module.h
#define XRT_API_SOURCE         // exporting xrt_module.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/common/config_reader.h"
#include "experimental/xrt_module.h"
#include "experimental/xrt_elf.h"
#include "experimental/xrt_ext.h"

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
# define AIE_COLUMN_PAGE_SIZE 8192  // NOLINT
#endif

namespace
{

// Control code is padded to page size, where page size is
// 0 if no padding is required.   The page size should be
// embedded as ELF metadata in the future.
static constexpr size_t column_page_size = AIE_COLUMN_PAGE_SIZE;
static constexpr uint8_t Elf_Amd_Aie2p  = 69;
static constexpr uint8_t Elf_Amd_Aie2ps = 64;

// When Debug.dump_bo_from_elf is true in xrt.ini, instruction bo(s) from elf will be dumped
static const char* Debug_Bo_From_Elf_Feature = "Debug.dump_bo_from_elf";

struct buf
{
  std::vector<uint8_t> m_data;

  void
  append_section_data(ELFIO::section* sec)
  {
    auto sz = sec->get_size();
    auto sdata = sec->get_data();
    m_data.insert(m_data.end(), sdata, sdata + sz);
  }

  [[nodiscard]] size_t
  size() const
  {
    return m_data.size();
  }

  [[nodiscard]] const uint8_t*
  data() const
  {
    return m_data.data();
  }

  void
  append_section_data(const uint8_t* userptr, size_t sz)
  {
    m_data.insert(m_data.end(), userptr, userptr + sz);
  }

  void
  pad_to_page(uint32_t page)
  {
    if (!column_page_size)
      return;

    auto pad = (page + 1) * column_page_size;

    if (m_data.size() > pad)
      throw std::runtime_error("Invalid ELF section size");

    m_data.resize(pad);
  }
};

using instr_buf = buf;
using control_packet = buf;
using ctrlcode = buf; // represent control code for column or partition

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
    shim_dma_base_addr_symbol_kind = 2, // patching scheme needed by AIE2PS firmware
    scalar_32bit_kind = 3,
    control_packet_48 = 4,              // patching scheme needed by firmware to patch dpu-sequence control packet
    shim_dma_48 = 5,                    // patching scheme needed by firmware to patch dpu-seuqnece instruction buffer
    tansaction_ctrlpkt_48 = 6,          // patching scheme needed by firmware to patch transaction buffer control packet
    tansaction_48 = 7,                  // patching scheme needed by firmware to patch transaction buffer
    unknown_symbol_kind = 8
  };

   enum class buf_type {
       ctrltext = 0,   // control code
       ctrldata,       // control packet
       preempt_save,   // preempt_save
       preempt_restore // preempt_restore
  };

  buf_type m_buf_type = buf_type::ctrltext;
  symbol_type m_symbol_type = symbol_type::shim_dma_48;

  // Offsets from base address of control code buffer object
  // The base address is passed in as a parameter to patch()
  std::vector<uint64_t> m_ctrlcode_offset;

  patcher(symbol_type type, std::vector<uint64_t> ctrlcode_offset, buf_type t)
    : m_buf_type(t)
    , m_symbol_type(type)
    , m_ctrlcode_offset(std::move(ctrlcode_offset))
  {}

  void
  patch32(uint32_t* bd_data_ptr, uint64_t patch)
  {
    uint64_t base_address = bd_data_ptr[0];
    base_address += patch;
    bd_data_ptr[0] = (uint32_t)(base_address & 0xFFFFFFFF);                           // NOLINT
  }

  void
  patch57(uint32_t* bd_data_ptr, uint64_t patch)
  {
    uint64_t base_address =
      ((static_cast<uint64_t>(bd_data_ptr[8]) & 0x1FF) << 48) |                       // NOLINT
      ((static_cast<uint64_t>(bd_data_ptr[2]) & 0xFFFF) << 32) |                      // NOLINT
      bd_data_ptr[1];

    base_address += patch;
    bd_data_ptr[1] = (uint32_t)(base_address & 0xFFFFFFFF);                           // NOLINT
    bd_data_ptr[2] = (bd_data_ptr[2] & 0xFFFF0000) | ((base_address >> 32) & 0xFFFF); // NOLINT
    bd_data_ptr[8] = (bd_data_ptr[8] & 0xFFFFFE00) | ((base_address >> 48) & 0x1FF);  // NOLINT
  }

  void
  patch_ctrl48(uint32_t* bd_data_ptr, uint64_t patch)
  {
    // This function is a copy&paste from IPU firmware
    constexpr uint64_t ddr_aie_addr_offset = 0x80000000;

    uint64_t base_address =
      ((static_cast<uint64_t>(bd_data_ptr[3]) & 0xFFF) << 32) |                       // NOLINT
      ((static_cast<uint64_t>(bd_data_ptr[2])));

    base_address = base_address + patch + ddr_aie_addr_offset;
    bd_data_ptr[2] = (uint32_t)(base_address & 0xFFFFFFFC);                           // NOLINT
    bd_data_ptr[3] = (bd_data_ptr[3] & 0xFFFF0000) | (base_address >> 32);            // NOLINT
  }

  void patch_shim48(uint32_t* bd_data_ptr, uint64_t patch)
  {
    // This function is a copy&paste from IPU firmware
    constexpr uint64_t ddr_aie_addr_offset = 0x80000000;

    uint64_t base_address =
      ((static_cast<uint64_t>(bd_data_ptr[2]) & 0xFFF) << 32) |                       // NOLINT
      ((static_cast<uint64_t>(bd_data_ptr[1])));

    base_address = base_address + patch + ddr_aie_addr_offset;
    bd_data_ptr[1] = (uint32_t)(base_address & 0xFFFFFFFC);                           // NOLINT
    bd_data_ptr[2] = (bd_data_ptr[2] & 0xFFFF0000) | (base_address >> 32);            // NOLINT
  }

  void
  patch(uint8_t* base, uint64_t patch, buf_type type)
  {
    if (type != m_buf_type)
      return;

    for (auto offset : m_ctrlcode_offset) {
      auto bd_data_ptr = reinterpret_cast<uint32_t*>(base + offset);
      switch (m_symbol_type) {
      case symbol_type::scalar_32bit_kind:
        patch32(bd_data_ptr, patch);
        break;
      case symbol_type::shim_dma_base_addr_symbol_kind:
        patch57(bd_data_ptr, patch);
        break;
      case symbol_type::control_packet_48:
        patch_ctrl48(bd_data_ptr, patch);
        break;
      case symbol_type::shim_dma_48:
        patch_shim48(bd_data_ptr, patch);
        break;
      case symbol_type::tansaction_ctrlpkt_48:
        patch_ctrl48(bd_data_ptr, patch);
        break;
      case symbol_type::tansaction_48:
        patch_shim48(bd_data_ptr, patch);
        break;
      default:
        throw std::runtime_error("Unsupported symbol type");
      }
    }
  }
};

  XRT_CORE_UNUSED void
  dump_bo(xrt::bo& bo, const std::string& filename)
  {
    if (!xrt_core::config::get_feature_toggle(Debug_Bo_From_Elf_Feature))
      return;

    std::ofstream ofs(filename, std::ios::out | std::ios::binary);
    if (!ofs.is_open())
      throw std::runtime_error("Failure opening file " + filename + " for writing!");

    auto buf = bo.map<char*>();
    ofs.write(buf, bo.size());
  }
} // namespace

namespace xrt
{

// class module_impl - Base class for different implementations
class module_impl
{
  xrt::uuid m_cfg_uuid;   // matching hw configuration id

public:
  explicit module_impl(xrt::uuid cfg_uuid)
    : m_cfg_uuid(std::move(cfg_uuid))
  {}

  explicit module_impl(const module_impl* parent)
    : m_cfg_uuid(parent->m_cfg_uuid)
  {}

  virtual ~module_impl() = default;

  module_impl() = delete;
  module_impl(const module_impl&) = delete;
  module_impl(module_impl&&) = delete;
  module_impl& operator=(const module_impl&) = delete;
  module_impl& operator=(module_impl&&) = delete;

  [[nodiscard]] xrt::uuid
  get_cfg_uuid() const
  {
    return m_cfg_uuid;
  }

  // Get raw instruction buffer data for all columns or for
  // single partition.  The returned vector has the control
  // code as extracted from ELF or userptr.
  [[nodiscard]] virtual const std::vector<ctrlcode>&
  get_data() const
  {
    throw std::runtime_error("Not supported");
  }

  [[nodiscard]] virtual const instr_buf&
  get_instr() const
  {
    throw std::runtime_error("Not supported");
  }

  [[nodiscard]] virtual const control_packet&
  get_ctrlpkt() const
  {
    throw std::runtime_error("Not supported");
  }

  [[nodiscard]] virtual xrt::hw_context
  get_hw_context() const
  {
    return {};
  }

  // Get the address and size of the control code for all columns
  // or for a single partition.  The returned vector has elements
  // that are used when populating ert_dpu_data elements embedded
  // in an ert_packet.
  [[nodiscard]] virtual const std::vector<std::pair<uint64_t, uint64_t>>&
  get_ctrlcode_addr_and_size() const
  {
    throw std::runtime_error("Not supported");
  }

  [[nodiscard]] virtual const uint8_t&
  get_os_abi() const
  {
    throw std::runtime_error("Not supported");
  }

  // Patch ctrlcode buffer object for global argument
  //
  // @param symbol - symbol name
  // @param bo - global argument to patch into ctrlcode
  // @param buf_type - whether it is control-code, control-packet, preempt-save or preempt-restore
  virtual void
  patch_instr(const std::string&, const xrt::bo&, patcher::buf_type)
  {
    throw std::runtime_error("Not supported ");
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
  // @param buf_type - whether it is control-code, control-packet, preempt-save or preempt-restore
  // @Return true if symbol was patched, false otherwise  //
  virtual bool
  patch(uint8_t*, const std::string&, uint64_t, patcher::buf_type)
  {
    throw std::runtime_error("Not supported");
  }

  // Get the number of patchers for arguments.  The returned
  // value is the number of arguments that must be patched before
  // the control code can be executed.
  [[nodiscard]] virtual size_t
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
  uint8_t m_os_abi;
  std::vector<ctrlcode> m_ctrlcodes;
  std::map<std::string, patcher> m_arg2patcher;
  instr_buf m_instr_buf;
  control_packet m_ctrl_packet;

  // The ELF sections embed column and page information in their
  // names.  Extract the column and page information from the
  // section name, default to single column and page when nothing
  // is specified.
  static std::pair<uint32_t, uint32_t>
  get_column_and_page(const std::string& name)
  {
    constexpr size_t first_dot = 9;  // .ctrltext.<col>.<page>
    auto dot1 = name.find_first_of(".", first_dot);
    auto dot2 = name.find_first_of(".", first_dot + 1);
    auto col = dot1 != std::string::npos
      ? std::stoi(name.substr(dot1 + 1, dot2))
      : 0;
    auto page = dot2 != std::string::npos
      ? std::stoi(name.substr(dot2 + 1))
      : 0;
    return { col, page };
  }

  // Extract instruction buffer from ELF sections without assuming anything
  // about order of sections in the ELF file.
  instr_buf
  initialize_instr_buf(const ELFIO::elfio& elf)
  {
    instr_buf instrbuf;

    for (const auto& sec : elf.sections) {
      auto name = sec->get_name();
      // Instruction buffer is in .ctrltext section.
      if (name.find(".ctrltext") != std::string::npos) {
        instrbuf.append_section_data(sec.get());
        break;
      }
    }

    return instrbuf;
  }

  // Extract control-packet buffer from ELF sections without assuming anything
  // about order of sections in the ELF file.
  control_packet
  initialize_ctrl_packet(const ELFIO::elfio& elf)
  {
    control_packet ctrlpacket;

    for (const auto& sec : elf.sections) {
      auto name = sec->get_name();
      // Control Packet is in .ctrldata section
      if (name.find(".ctrldata") != std::string::npos) {
        ctrlpacket.append_section_data(sec.get());
        break;
      }
    }

    return ctrlpacket;
  }

  // Extract control code from ELF sections without assuming anything
  // about order of sections in the ELF file.  Build helper data
  // structures that manages the control code data for each column and
  // page, then create ctrlcode objects from the data.
  std::vector<ctrlcode>
  initialize_column_ctrlcode(const ELFIO::elfio& elf)
  {
    // Elf sections for a single page
    struct column_page
    {
      ELFIO::section* ctrltext = nullptr;
      ELFIO::section* ctrldata = nullptr;
    };

    // Elf sections for a single column, the column control code is
    // divided into pages of some architecture defined size.
    struct column_sections
    {
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

  std::map<std::string, patcher>
  initialize_arg_patchers(const ELFIO::elfio& elf, const instr_buf& instrbuf, const control_packet& ctrlpkt)
  {
    auto dynsym = elf.sections[".dynsym"];
    auto dynstr = elf.sections[".dynstr"];

    std::map<std::string, patcher> arg2patchers;

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
        auto section = elf.sections[sym->st_shndx];
        if (!section)
          throw std::runtime_error("Invalid section index " + std::to_string(sym->st_shndx));

        auto secname = section->get_name();
        auto offset = rela->r_offset;
        size_t sec_size = 0;
        patcher::buf_type buf_type;
        if (secname.compare(".ctrltext") == 0) {
          sec_size = instrbuf.size();
          buf_type = patcher::buf_type::ctrltext;
        }
        else if (secname.compare(".ctrldata") == 0) {
          sec_size = ctrlpkt.size();
          buf_type = patcher::buf_type::ctrldata;
        }
        else
          throw std::runtime_error("Invalid section name " + secname);

        if (offset >= sec_size)
          throw std::runtime_error("Invalid offset " + std::to_string(offset));

        std::string argnm{ symname, symname + std::min(strlen(symname), dynstr->get_size()) };

        if (auto search = arg2patchers.find(argnm); search != arg2patchers.end())
          search->second.m_ctrlcode_offset.emplace_back(offset);
        else {
          auto symbol_type = static_cast<patcher::symbol_type>(rela->r_addend);
          std::vector<uint64_t> offsets;
          offsets.push_back(offset);
          arg2patchers.emplace(std::move(argnm), patcher{ symbol_type, std::move(offsets), buf_type });
        }
      }
    }

    return arg2patchers;
  }

  std::map<std::string, patcher>
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
        auto column_ctrlcode_offset = page * column_page_size + rela->r_offset + 16; // NOLINT magic number 16??
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
        std::string argnm{ symname, symname + std::min(strlen(symname), dynstr->get_size()) };
        auto symbol_type = static_cast<patcher::symbol_type>(rela->r_addend);

        patcher::buf_type buf_type = patcher::buf_type::ctrltext;
        std::vector<uint64_t> offsets;
        offsets.push_back(ctrlcode_offset);
        arg2patcher.emplace(std::move(argnm), patcher{ symbol_type, offsets, buf_type});
      }
    }

    return arg2patcher;
  }

  bool
  patch(uint8_t* base, const std::string& argnm, uint64_t patch, patcher::buf_type type) override
  {
    auto it = m_arg2patcher.find(argnm);
    if (it == m_arg2patcher.end())
      return false;

    it->second.patch(base, patch, type);
    return true;
  }

  [[nodiscard]] const uint8_t&
  get_os_abi() const override
  {
    return m_os_abi;
  }

public:
  explicit module_elf(xrt::elf elf)
    : module_impl{ elf.get_cfg_uuid() }
    , m_elf(std::move(elf))
    , m_os_abi{ xrt_core::elf_int::get_elfio(m_elf).get_os_abi() }
  {
    if (m_os_abi == Elf_Amd_Aie2ps) {
      m_ctrlcodes = initialize_column_ctrlcode(xrt_core::elf_int::get_elfio(m_elf));
      m_arg2patcher = initialize_arg_patchers(xrt_core::elf_int::get_elfio(m_elf), m_ctrlcodes);
    }
    else if (m_os_abi == Elf_Amd_Aie2p) {
      m_instr_buf = initialize_instr_buf(xrt_core::elf_int::get_elfio(m_elf));
      m_ctrl_packet = initialize_ctrl_packet(xrt_core::elf_int::get_elfio(m_elf));
      m_arg2patcher = initialize_arg_patchers(xrt_core::elf_int::get_elfio(m_elf), m_instr_buf, m_ctrl_packet);
    }
  }

  [[nodiscard]] const std::vector<ctrlcode>&
  get_data() const override
  {
    return m_ctrlcodes;
  }

  [[nodiscard]] const instr_buf&
  get_instr() const override
  {
    return m_instr_buf;
  }

  [[nodiscard]] const control_packet&
  get_ctrlpkt() const override
  {
    return m_ctrl_packet;
  }

  [[nodiscard]] size_t
  number_of_arg_patchers() const override
  {
    return m_arg2patcher.size();
  }
};

// class module_userptr - Opaque userptr provided by application
class module_userptr : public module_impl
{
  std::vector<ctrlcode> m_ctrlcode;
  instr_buf m_instr_buf;
  control_packet m_ctrl_pkt;

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
    : module_impl{ uuid }
    , m_ctrlcode{ initialize_ctrlcode(userptr, sz) }
  {}

  module_userptr(const void* userptr, size_t sz, const xrt::uuid& uuid)
    : module_userptr(static_cast<const char*>(userptr), sz, uuid)
  {}

  [[nodiscard]] const std::vector<ctrlcode>&
  get_data() const override
  {
    return m_ctrlcode;
  }

  [[nodiscard]] const instr_buf&
  get_instr() const override
  {
    return m_instr_buf;
  }

  [[nodiscard]] const control_packet&
  get_ctrlpkt() const override
  {
    return m_ctrl_pkt;
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
  xrt::bo m_instr_buf;
  xrt::bo m_ctrlpkt_buf;

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
  bool m_dirty{ false };

  // For separated multi-column control code, compute the ctrlcode
  // buffer object address of each column (used in ert_dpu_data).
  void
  fill_column_bo_address(const std::vector<ctrlcode>& ctrlcodes)
  {
    m_column_bo_address.clear();
    auto base_addr = m_buffer.address();
    for (const auto& ctrlcode : ctrlcodes) {
      m_column_bo_address.push_back({ base_addr, ctrlcode.size() }); // NOLINT
      base_addr += ctrlcode.size();
    }
  }

  void
  fill_bo_addresses()
  {
    m_column_bo_address.clear();
    m_column_bo_address.push_back({ m_instr_buf.address(), m_instr_buf.size() }); // NOLINT
    if (m_ctrlpkt_buf) {
      m_column_bo_address.push_back({ m_ctrlpkt_buf.address(), m_ctrlpkt_buf.size() }); // NOLINT
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

  void
  fill_instr_buf(xrt::bo& bo, const instr_buf& instrbuf)
  {
    auto ptr = bo.map<char*>();
    std::memcpy(ptr, instrbuf.data(), instrbuf.size());
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  }

  void
  fill_ctrlpkt_buf(xrt::bo& bo, const control_packet& ctrlpktbuf)
  {
    auto ptr = bo.map<char*>();
    std::memcpy(ptr, ctrlpktbuf.data(), ctrlpktbuf.size());
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  }

  void
  create_instr_buf(const module_impl* parent)
  {
    XRT_PRINTF("-> module_sram::create_instr_buf()\n");
    const auto& data = parent->get_instr();
    size_t sz = data.size();

    if (sz == 0) {
      std::cout << "instr buf is empty" << std::endl;
      return;
    }

    // create bo combined size of all ctrlcodes
    m_instr_buf = xrt::bo{ m_hwctx, sz, xrt::bo::flags::cacheable, 1 /* fix me */ };

    // copy instruction into bo
    fill_instr_buf(m_instr_buf, data);

#ifdef _DEBUG
    dump_bo(m_instr_buf, "instrBo.bin");
#endif

    if (m_ctrlpkt_buf) {
      patch_instr("control-packet", m_ctrlpkt_buf, patcher::buf_type::ctrltext);
    }
    XRT_PRINTF("<- module_sram::create_instr_buf()\n");
  }

  void
  create_ctrlpkt_buf(const module_impl* parent)
  {
    XRT_PRINTF("-> module_sram::create_ctrlpkt_buf()\n");
    const auto& data = parent->get_ctrlpkt();
    size_t sz = data.size();

    if (sz == 0) {
      XRT_PRINTF("ctrlpkt buf is empty\n");
    }
    else {
      // create bo combined size of all ctrlcodes
      // m_ctrlpkt_buf = xrt::bo{m_hwctx, sz, xrt::bo::flags::host_only, 0};
      m_ctrlpkt_buf = xrt::ext::bo{ m_hwctx, sz };

      // copy instruction into bo
      fill_ctrlpkt_buf(m_ctrlpkt_buf, data);

#ifdef _DEBUG
      dump_bo(m_ctrlpkt_buf, "ctrlpktBo.bin");
#endif

      XRT_PRINTF("<- module_sram::create_ctrlpkt_buffer()\n");
    }
  }

  // Create the instruction buffer object and fill it with column
  // ctrlcodes.
  void
  create_instruction_buffer(const module_impl* parent)
  {
    XRT_PRINTF("-> module_sram::create_instruction_buffer()\n");
    const auto& data = parent->get_data();

    // create bo combined size of all ctrlcodes
    size_t sz = std::accumulate(data.begin(), data.end(), static_cast<size_t>(0), [](auto acc, const auto& ctrlcode) {
      return acc + ctrlcode.size();
      });
    if (sz == 0) {
      std::cout << "instruction buf is empty" << std::endl;
      return;
    }

    m_buffer = xrt::bo{ m_hwctx, sz, xrt::bo::flags::cacheable, 1 /* fix me */ };

    // copy ctrlcodes into bo
    fill_instruction_buffer(m_buffer, data);

    XRT_PRINTF("<- module_sram::create_instruction_buffer()\n");
  }

  virtual void
  patch_instr(const std::string& argnm, const xrt::bo& bo, patcher::buf_type type) override
  {
    patch_instr_value(argnm, bo.address(), type);
  }

  void
  patch_value(const std::string& argnm, uint64_t value)
  {
    bool patched = false;
    if (m_parent->get_os_abi() == Elf_Amd_Aie2p) {
      // patch control-packet buffer
      if (m_ctrlpkt_buf) {
        if (m_parent->patch(m_ctrlpkt_buf.map<uint8_t*>(), argnm, value, patcher::buf_type::ctrldata))
          patched = true;
      }

      // patch instruction buffer
      if (m_parent->patch(m_instr_buf.map<uint8_t*>(), argnm, value, patcher::buf_type::ctrltext))
          patched = true;
    }
    else if (m_parent->patch(m_buffer.map<uint8_t*>(), argnm, value, patcher::buf_type::ctrltext))
      patched = true;

    if (patched) {
      m_patched_args.insert(argnm);
      m_dirty = true;
    }
  }

  void
  patch_instr_value(const std::string& argnm, uint64_t value, patcher::buf_type type)
  {
    if (!m_parent->patch(m_instr_buf.map<uint8_t*>(), argnm, value, type))
        return;

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
    if (size > 8) // NOLINT
      throw std::runtime_error{ "patch_value() only supports 64-bit values or less" };

    patch_value(argnm, *static_cast<const uint64_t*>(value));
  }

  // Check that all arguments have been patched and sync the buffer
  // to device if it is dirty.
  void
  sync_if_dirty() override
  {
    if (!m_dirty)
      return;

    auto os_abi = m_parent.get()->get_os_abi();
    if (os_abi == Elf_Amd_Aie2ps) {
      if (m_patched_args.size() != m_parent->number_of_arg_patchers()) {
        auto fmt = boost::format("ctrlcode requires %d patched arguments, but only %d are patched")
            % m_parent->number_of_arg_patchers() % m_patched_args.size();
        throw std::runtime_error{ fmt.str() };
      }
      m_buffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }
    else if (os_abi == Elf_Amd_Aie2p) {
      m_instr_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
#ifdef _DEBUG
      dump_bo(m_instr_buf, "instrBoPatched.bin");
#endif
      if (m_ctrlpkt_buf) {
        m_ctrlpkt_buf.sync(XCL_BO_SYNC_BO_TO_DEVICE);
#ifdef _DEBUG
        dump_bo(m_ctrlpkt_buf, "ctrlpktBoPatched.bin");
#endif
        }
    }

    m_dirty = false;
  }

public:
  module_sram(std::shared_ptr<module_impl> parent, xrt::hw_context hwctx)
    : module_impl{ parent->get_cfg_uuid() }
    , m_parent{ std::move(parent) }
    , m_hwctx{ std::move(hwctx) }
  {
    auto os_abi = m_parent.get()->get_os_abi();

    if (os_abi == Elf_Amd_Aie2p) {
      // make sure to create control-packet buffer frist because we may
      // need to patch control-packet address to instruction buffer
      create_ctrlpkt_buf(m_parent.get());
      create_instr_buf(m_parent.get());
      fill_bo_addresses();
    }
    else if (os_abi == Elf_Amd_Aie2ps) {
      create_instruction_buffer(m_parent.get());
      fill_column_bo_address(m_parent->get_data());
    }
  }

  [[nodiscard]] const std::vector<std::pair<uint64_t, uint64_t>>&
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
namespace xrt
{
module::
module(const xrt::elf& elf)
: detail::pimpl<module_impl>{ std::make_shared<module_elf>(elf) }
{}

module::
module(void* userptr, size_t sz, const xrt::uuid& uuid)
: detail::pimpl<module_impl>{ std::make_shared<module_userptr>(userptr, sz, uuid) }
{}

module::
module(const xrt::module& parent, const xrt::hw_context& hwctx)
: detail::pimpl<module_impl>{ std::make_shared<module_sram>(parent.handle, hwctx) }
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
