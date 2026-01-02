// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _XRT_COMMON_ELF_INT_H_
#define _XRT_COMMON_ELF_INT_H_

// This file defines implementation extensions to the XRT ELF APIs.
// It provides access to xrt::elf_impl class that is not
// directly exposed to end users.
#include "core/common/config.h"
#include "core/common/xclbin_parser.h"
#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/xrt_bo.h"
#include "elf_patcher.h"

#include "ert.h"

#include <elfio/elfio.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace xrt {

////////////////////////////////////////////////////////////////
// buf - wrapper for holding ELF section data
////////////////////////////////////////////////////////////////
struct buf
{
  std::vector<uint8_t> m_data;

  void
  append_section_data(const ELFIO::section* sec);

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

  void
  append_section_data(const uint8_t* userptr, size_t sz)
  {
    m_data.insert(m_data.end(), userptr, userptr + sz);
  }

  static const buf&
  get_empty_buf()
  {
    static const buf b = {};
    return b;
  }
};

// Aliases for different ELF section buffers
using instr_buf = buf;
using control_packet = buf;
using ctrlcode = buf; // represents control code for column or partition

// Alias for kernel argument type
using xarg = xrt_core::xclbin::kernel_argument;

// Forward declaration
class elf_impl;

////////////////////////////////////////////////////////////////
// Platform-specific configuration structures
// These structures hold references to ELF data needed by
// module_run classes.
////////////////////////////////////////////////////////////////

// Configuration for AIE2P platform
struct module_config_aie2p
{
  // Reference to instruction buffer data
  const instr_buf& instr_data;

  // Reference to control packet buffer (may be empty)
  const control_packet& ctrl_packet_data;

  // References to preemption save/restore buffers (may be empty)
  const buf& preempt_save_data;
  const buf& preempt_restore_data;

  // Size of scratch pad memory
  size_t scratch_pad_mem_size;

  // Control scratch pad memory size (0 if not present)
  size_t ctrl_scratch_pad_mem_size;

  // Reference to PDI symbols that need patching
  const std::unordered_set<std::string>& patch_pdi_symbols;

  // Reference to control packet preemption dynamic symbols
  const std::set<std::string>& ctrlpkt_pm_dynsyms;

  // Reference to control packet preemption buffers map
  const std::map<std::string, buf>& ctrlpkt_pm_bufs;

  // Flag indicating if preemption sections exist
  bool has_preemption;

  // Parent elf_impl pointer for accessing PDI buffers
  elf_impl* elf_parent;
};

// Configuration for AIE2PS/AIE4 platform
struct module_config_aie2ps
{
  // Reference to control codes for each column
  const std::vector<ctrlcode>& ctrlcodes;

  // Reference to control packet buffers map
  const std::map<std::string, buf>& ctrlpkt_bufs;

  // Reference to dump buffer for debug/trace
  const buf& dump_buf;

  // Parent elf_impl pointer for any mutable operations
  elf_impl* elf_parent;
};

// Variant type for platform-specific module configuration
using module_config = std::variant<module_config_aie2p, module_config_aie2ps>;

////////////////////////////////////////////////////////////////
// elf_impl - Base implementation class for xrt::elf
//
// This class is the internal implementation of xrt::elf.
// Derived classes (elf_aie2p, elf_aie2ps) provide platform-specific
// functionality. The declaration is exposed here to allow
// xrt_module.cpp to access parsed ELF data.
////////////////////////////////////////////////////////////////
class elf_impl
{
protected:
  // Below members are made protected to allow direct access in derived
  // classes for simplicity and avoids unnecessary boilerplate setters,
  // getters code
  // NOLINTBEGIN
  ELFIO::elfio m_elfio;
  xrt::elf::platform m_platform;

  /* Parsed ELF data structures */
  // lookup map for section index to group index
  std::map<uint32_t, uint32_t> m_section_to_group_map;

  // map of group id (ctrl code id) to vector of section indices
  std::map<uint32_t, std::vector<uint32_t>> m_group_to_sections_map;

  // lookup map for kernel + sub kernel name to grp idx(ctrl code id)
  std::map<std::string, uint32_t> m_kernel_name_to_id_map;

  // Kernel data collected during parsing (name -> args)
  // This is populated during group section parsing and used to build elf::kernel objects
  std::map<std::string, std::vector<xarg>> m_kernel_args_map;

  // Map that stores available subkernels/instances of a kernel
  // key - kernel name, value - vector of subkernel/instance names
  std::map<std::string, std::vector<std::string>> m_kernel_to_subkernels_map;

  // Final kernel objects built from m_kernel_args_map and m_kernel_to_subkernels_map
  std::vector<elf::kernel> m_kernels;

  /* Patcher related types and data - common between elf_aie2p and elf_aie2ps */
  // Alias for symbol_patcher for brevity
  using patcher = xrt_core::elf_patcher::symbol_patcher;
  using patcher_buf_type = xrt_core::elf_patcher::buf_type;

  // Map of argument name to patcher for each ctrl code id
  // key - ctrl code id, value - map of argument name to patcher
  std::map<uint32_t, std::map<std::string, patcher>> m_arg2patcher;

  // Constants for parsing rela addend field
  // rela->addend have offset to base-bo-addr info along with schema
  // [0:3] bit are used for patching schema, [4:31] used for base-bo-addr
  static constexpr uint32_t addend_shift = 4;
  static constexpr uint32_t addend_mask = ~((uint32_t)0) << addend_shift;
  static constexpr uint32_t schema_mask = ~addend_mask;
  // NOLINTEND

  // Protected constructor - takes already-loaded ELFIO
  explicit
  elf_impl(ELFIO::elfio&& elfio);

  // Parse .group sections in the ELF file and populate all maps
  void
  parse_group_sections();

private:
  ////////////////////////////////////////////////////////////////
  // Private helper structures and methods
  ////////////////////////////////////////////////////////////////

  // Symbol information extracted from .symtab section
  struct symbol_info {
    std::string name;
    unsigned char type = 0;
    ELFIO::Elf_Half section_index = UINT16_MAX;
  };

  // Get symbol information from .symtab at given index
  symbol_info
  get_symbol_from_symtab(uint32_t sym_index) const;

  // Extract kernel name from demangled signature
  static std::string
  extract_kernel_name(const std::string& signature);

  // Check if kernel already exists in m_kernel_args_map
  bool
  kernel_exists(const std::string& kernel_name) const;

  // Add kernel arguments to m_kernel_args_map during parsing
  void
  add_kernel_info(const std::string& kernel_name, const std::string& signature);

  // Build elf::kernel objects from collected kernel data
  void
  finalize_kernels();

  // Parse .symtab section to extract kernel and subkernel information
  std::pair<std::string, std::string>
  get_kernel_subkernel_from_symtab(uint32_t sym_index);

  // Initialize maps for legacy ELF without .group sections
  void
  init_legacy_section_maps();

  // Parse a single .group section and update maps
  void
  parse_single_group_section(const ELFIO::section* section);

public:
  virtual ~elf_impl() = default;

  // Get raw ELFIO object reference
  const ELFIO::elfio&
  get_elfio() const
  {
    return m_elfio;
  }

  // Get configuration UUID from ELF
  xrt::uuid
  get_cfg_uuid() const;

  // Extract section data by name
  std::vector<uint8_t>
  get_section(const std::string& sname);

  // Get note data from ELF section
  std::string
  get_note(const ELFIO::section* section, ELFIO::Elf_Word note_num) const;

  // Get partition size from ELF notes
  uint32_t
  get_partition_size() const;

  // Check if this is a full ELF (contains all info for hw context)
  bool
  is_full_elf() const;

  // Get OS ABI from ELF header
  uint8_t
  get_os_abi() const
  {
    return m_elfio.get_os_abi();
  }

  // Get platform type
  xrt::elf::platform
  get_platform() const
  {
    return m_platform;
  }

  // Get list of kernels from ELF
  const std::vector<elf::kernel>&
  get_kernels() const
  {
    return m_kernels;
  }

  // Get ABI version as (major, minor) pair
  // Version byte format: upper nibble = major, lower nibble = minor
  std::pair<uint8_t, uint8_t>
  get_abi_version() const;

  // Check if ELF uses .group sections (version-dependent)
  virtual bool
  is_group_elf() const = 0;

  // Get module configuration for a specific control code id
  // Returns variant containing platform-specific config structure
  // for the platform. Derived classes override to provide their
  // specific configuration.
  virtual module_config
  get_module_config(uint32_t ctrl_code_id) = 0;


  // PDI buffer accessors
  // These remain as virtual methods since PDI buffers may be
  // created lazily and cached
  // Get PDI buffer data for a symbol
  virtual const buf&
  get_pdi(const std::string& symbol) const
  {
    throw std::runtime_error("get_pdi not supported on this platform");
  }

  // Get/create PDI buffer object for a symbol
  virtual xrt::bo&
  get_pdi_bo(const std::string& symbol)
  {
    throw std::runtime_error("get_pdi_bo not supported on this platform");
  }

  // Get control code id from kernel name
  // Looks up kernel + subkernel name in the kernel name to id map
  virtual uint32_t
  get_ctrlcode_id(const std::string& name) const = 0;

  // Get argument to patcher map for patching symbols
  const std::map<uint32_t, std::map<std::string, patcher>>&
  get_arg2patcher() const
  {
    return m_arg2patcher;
  }

  // Get number of arg patchers for a ctrl code id
  // Throws exception if no arg patchers found for given ctrl code id
  size_t
  number_of_arg_patchers(uint32_t ctrl_code_id) const
  {
    if (auto it = m_arg2patcher.find(ctrl_code_id); it != m_arg2patcher.end())
      return it->second.size();

    throw std::runtime_error(
        std::string{"Unable to get arg patchers for ctrl code id: " + std::to_string(ctrl_code_id)});
  }

  // Get the ERT command opcode in ELF flow
  virtual ert_cmd_opcode
  get_ert_opcode() const = 0;
};

} // namespace xrt

namespace xrt_core::elf_int {

// ELFs with no multi control code support use below id as
// grp index or control code id
static constexpr uint32_t no_ctrl_code_id = UINT32_MAX;

// Get kernel properties and arguments from elf::kernel object
std::pair<xrt_core::xclbin::kernel_properties, std::vector<xrt::xarg>>
get_kernel_properties_and_args(std::shared_ptr<xrt::elf_impl> elf_impl,
                               const std::string& kernel_name);

} // namespace xrt_core::elf_int

#endif
