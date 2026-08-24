// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _XRT_COMMON_ELF_INT_H_
#define _XRT_COMMON_ELF_INT_H_

// This file defines implementation extensions to the XRT ELF APIs.
// It provides access to xrt::elf_impl class that is not
// directly exposed to end users.
#include "core/common/config.h"
#include "core/common/span.h"
#include "core/common/xclbin_parser.h"
#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/xrt_bo.h"
#include "elf_patcher.h"

#include "ert.h"

#include "core/common/aiebu/src/cpp/include/aiebu/aiebu_decompress.h"

#include <elfio/elfio.hpp>

#include <cstdint>
#include <cstring>
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
//
// Stores non-owning pointers to ELFIO section objects.
// Compression is fully abstracted — aiebu determines whether
// decompression is needed via get_section_uncompressed_size() /
// copy_section_uncompressed_data().
// Padding stored separately to avoid copying section data.
////////////////////////////////////////////////////////////////
struct buf
{
  template <typename T> using span = xrt_core::span<T>;
private:
  // A view into ELFIO section data, possibly compressed.
  // For section-backed views, section and elf are non-null.
  // For padding views, section is null and padding holds the zero buffer.
  //
  // Lifetime of section/elf pointers:
  //   section points into m_elfio's internal section array; elf points to
  //   m_elfio itself (stored as &elf in append_section_data).  Both m_elfio
  //   and the buf objects (m_instr_buf_map, m_ctrl_packet_map, etc.) are
  //   members of the same elf_impl instance (m_elfio in the base class,
  //   buf maps in the derived classes elf_aie_gen2 / elf_aie_gen2_plus).
  //   This is why the raw pointers are safe: a buf cannot outlive its
  //   owning elf_impl because it is a member of it, so m_elfio is
  //   guaranteed to be alive whenever a view_entry in that buf is accessed.
  //   At the broader scope, module_impl holds a shared_ptr<elf_impl>
  //   ensuring elf_impl is not destroyed while the module is in use.
  //
  struct view_entry {
    const ELFIO::section* section = nullptr;   // section-backed view (may be compressed)
    const ELFIO::elfio* elf = nullptr;         // ELFIO owning the section (see lifetime note above)
    span<uint8_t> padding;                     // for zero-padding, points into m_padding_buffer
    std::size_t data_size = 0;                 // effective size (uncompressed if compressed)
  };

  // Non-owning views into ELFIO or external data
  std::vector<view_entry> m_views;

  // Padding buffer - only allocated when AIE2PS/AIE4 needs page alignment
  // Stored separately to avoid copying section data
  std::vector<uint8_t> m_padding_buffer;

public:
  buf() = default;

  // Append section data from an ELFIO section.
  // Compression handling is delegated to aiebu — XRT does not inspect
  // SHF_COMPRESSED or Chdr headers directly.  Decompression is deferred
  // until copy_to().
  void
  append_section_data(const ELFIO::section* sec, const ELFIO::elfio& elf)
  {
    if (!sec || sec->get_size() == 0)
      return;

    view_entry entry;
    entry.section = sec;
    entry.elf = &elf;
    entry.data_size = aiebu::get_section_uncompressed_size(sec, elf);
    m_views.push_back(entry);
  }

  // Overload for smart pointers (from ELFIO range-based for loops)
  void
  append_section_data(const std::unique_ptr<ELFIO::section>& sec,
                      const ELFIO::elfio& elf)
  {
    append_section_data(sec.get(), elf);
  }

  // Add padding to reach target size (for AIE2PS/AIE4 page alignment)
  // Only allocates memory for padding zeros, NOT for existing section data!
  void
  add_padding_to_size(size_t target_size)
  {
    size_t current = size();
    if (target_size <= current)
      return;
    
    size_t padding_size = target_size - current;
    m_padding_buffer.resize(padding_size, 0);
    view_entry entry;
    entry.padding = {m_padding_buffer.data(), m_padding_buffer.size()};
    entry.data_size = padding_size;
    m_views.push_back(entry);
  }

  // Get total size across all views.
  // For compressed sections, returns the uncompressed size (what copy_to will produce).
  size_t
  size() const
  {
    size_t total = 0;
    for (const auto& v : m_views)
      total += v.data_size;

    return total;
  }

  // Copy all views to destination buffer.
  // Compressed sections are decompressed directly into dest via aiebu.
  // Uncompressed sections and padding are memcpy'd.
  void
  copy_to(xrt_core::span<uint8_t> dest) const
  {
    if (dest.size() < size())
      throw std::runtime_error(
        "buf::copy_to: dest size (" + std::to_string(dest.size())
        + ") < buf size (" + std::to_string(size()) + ")");

    auto* dst = dest.data();
    for (const auto& v : m_views) {
      if (v.section)
        aiebu::copy_section_uncompressed_data(v.section, *v.elf, dst, v.data_size);
      else
        std::memcpy(dst, v.padding.data(), v.data_size);

      dst += v.data_size;
    }
  }

  // Get data pointer - only works for single uncompressed view (zero-copy).
  // Throws if the section is SHF_COMPRESSED — compressed bytes are not valid
  // instruction data. Use copy_to() for compression-safe access.
  const uint8_t*
  data() const
  {
    if (m_views.size() == 1 && m_views[0].section) {
      if (m_views[0].section->get_flags() & ELFIO::SHF_COMPRESSED)
        throw std::runtime_error(
          "buf::data() called on compressed section — use copy_to() instead");
      return reinterpret_cast<const uint8_t*>(m_views[0].section->get_data());
    }

    // Multiple views: cannot provide direct pointer
    // Caller should use copy_to() instead
    throw std::runtime_error(
      "Cannot get direct pointer from buffer with multiple views. "
      "Use copy_to() to copy data instead.");
  }

  // Create std::string from views (for debug/trace).
  // Decompresses compressed views into the result string.
  std::string
  to_string() const
  {
    std::string result;
    result.resize(size());
    copy_to({reinterpret_cast<uint8_t*>(result.data()), result.size()});
    return result;
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
struct module_config_aie_gen2
{
  // NOLINTBEGIN
  // Reference members are safe here: module_run holds shared_ptr<elf_impl>
  // ensuring data lifetime, and these configs are temporary parameter bundles
  // used only during construction.

  // Reference to instruction buffer data
  const instr_buf& instr_data;

  // Reference to control packet buffer (may be empty)
  const control_packet& ctrl_packet_data;

  // References to preemption save/restore buffers (may be empty)
  const buf& preempt_save_data;
  const buf& preempt_restore_data;

  // Size of scratch pad memory per column
  size_t scratch_pad_mem_size;

  // Control scratch pad memory size (0 if not present)
  size_t ctrl_scratch_pad_mem_size;

  // Reference to PDI symbols that need patching
  const std::unordered_set<std::string>& patch_pdi_symbols;

  // Reference to control packet preemption dynamic symbols
  const std::set<std::string>& ctrlpkt_pm_dynsyms;

  // Reference to control packet preemption buffers map
  const std::map<std::string, buf>& ctrlpkt_pm_bufs;
  // NOLINTEND

  // Flag indicating if preemption sections exist
  bool has_preemption;

  // Parent elf_impl pointer for accessing PDI buffers
  elf_impl* elf_parent;
};

// Configuration for AIE2PS/AIE4 platform
struct module_config_aie_gen2_plus
{
  // NOLINTBEGIN
  // Reference members are safe here: module_run holds shared_ptr<elf_impl>
  // ensuring data lifetime, and these configs are temporary parameter bundles
  // used only during construction.

  // Reference to control codes for each column
  const std::vector<ctrlcode>& ctrlcodes;

  // Reference to control packet buffers map
  const std::map<std::string, buf>& ctrlpkt_bufs;

  // Reference to dump buffer for debug/trace
  const buf& dump_buf;

  // Size of scratch pad memory per column
  size_t scratch_pad_mem_size;
  // NOLINTEND

  // Parent elf_impl pointer for any mutable operations
  elf_impl* elf_parent;
};

// Variant type for platform-specific module configuration
using module_config = std::variant<module_config_aie_gen2, module_config_aie_gen2_plus>;

////////////////////////////////////////////////////////////////
// elf_impl - Base implementation class for xrt::elf
//
// This class is the internal implementation of xrt::elf.
// Derived classes (elf_aie_gen2, elf_aie_gen2_plus) provide platform-specific
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
  std::string m_path; // file path from which elf was loaded, empty if loaded from stream/buffer

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

  // Map for custom sections
  // key - custom section name, value - custom section data
  std::map<std::string, detail::span<const char>> m_custom_section_map;

  /* Patcher related types and data - common between all platforms */
  // Aliases for patcher types
  using patcher_config = xrt_core::elf_patcher::patcher_config;
  using patch_config = xrt_core::elf_patcher::patch_config;
  using patcher_buf_type = xrt_core::elf_patcher::buf_type;
  using patcher_symbol_type = xrt_core::elf_patcher::symbol_type;

  // Map of argument name to patcher config for each ctrl code id
  // key - ctrl code id, value - map of argument name to patcher config
  // Stores static configuration only
  std::map<uint32_t, std::map<std::string, patcher_config>> m_arg2patcher;

  // Constants for parsing rela addend field
  // rela->addend have offset to base-bo-addr info along with schema
  // [0:3] bit are used for patching schema, [4:31] used for base-bo-addr
  static constexpr uint32_t addend_shift = 4;
  static constexpr uint32_t addend_mask = ~((uint32_t)0) << addend_shift;
  static constexpr uint32_t schema_mask = ~addend_mask;

  // NOLINTEND

  // elf_impl() - constructor
  // 
  // @elfio:  In memory ELFIO object
  // @platform: ?
  // @path: file path if ELFIO was loaded from from a file, empty otherwise
  elf_impl(ELFIO::elfio&& elfio, elf::platform platform, std::string path);

  // Parse sections in the ELF and populate internal maps
  void
  parse_sections();

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

  // Parse custom sections and populate corresponding map
  void
  parse_custom_sections(const std::vector<uint32_t>& custom_section_ids);

public:
  virtual ~elf_impl() = default;

  // Base class managed through shared_ptr - no copy/move
  elf_impl(const elf_impl&) = delete;
  elf_impl(elf_impl&&) = delete;
  elf_impl& operator=(const elf_impl&) = delete;
  elf_impl& operator=(elf_impl&&) = delete;

  // Get raw ELFIO object reference.
  // Compressed sections (.ctrltext*, .ctrldata*, .ctrlpkt*) contain raw compressed
  // bytes with SHF_COMPRESSED set. Callers that need section data should use
  // buf::append_section_data() + copy_to() which delegate to aiebu for decompression.
  const ELFIO::elfio&
  get_elfio() const
  {
    return m_elfio;
  }

  // Get the filename this ELF was loaded from (empty if loaded from buffer/stream)
  const std::string&
  get_filename() const
  {
    return m_path;
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
  get_pdi(const std::string&) const
  {
    throw std::runtime_error("get_pdi not supported on this platform");
  }

  // Get control code id from kernel name
  // Looks up kernel + subkernel name in the kernel name to id map
  virtual uint32_t
  get_ctrlcode_id(const std::string& name) const = 0;

  // Get patcher configs for a specific ctrl code id
  // Returns const pointer to shared configs owned by elf_impl (avoids copying)
  // module_run creates symbol_patcher objects from these configs
  const std::map<std::string, patcher_config>*
  get_patcher_configs(uint32_t ctrl_code_id) const
  {
    auto it = m_arg2patcher.find(ctrl_code_id);
    if (it != m_arg2patcher.end())
      return &it->second;
    return nullptr;
  }

  // Get the ERT command opcode in ELF flow
  virtual ert_cmd_opcode
  get_ert_opcode() const = 0;

  // Get custom section data by name
  // Returns span of custom section data
  detail::span<const char>
  get_custom_section(const std::string& name) const;
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

// get_filename() - Return the filename this ELF was loaded from
// Empty string if ELF was loaded from buffer/stream
std::string
get_filename(const xrt::elf_impl* elf_impl);

} // namespace xrt_core::elf_int

#endif
