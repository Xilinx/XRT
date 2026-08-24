// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _XRT_COMMON_ELF_INT_H_
#define _XRT_COMMON_ELF_INT_H_

// This file defines implementation extensions to the XRT ELF APIs.
// It provides access to xrt::elf_impl class that is not
// directly exposed to end users.
#include "core/common/aiebu/src/cpp/include/aiebu/elf.h"

// TRANSITIONAL: elfio.hpp is included here solely to satisfy external
// submodules that access ELFIO types through elf_impl::get_elfio().
// Once those dependencies are updated to use get_aiebu_elf() instead,
// this include and get_elfio() on elf_impl will be removed in Step 1.3.
#include <elfio/elfio.hpp>

#include "core/common/config.h"
#include "core/common/xclbin_parser.h"
#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/xrt_bo.h"
#include "elf_patcher.h"

#include "ert.h"

#include <cstdint>
#include <functional>
#include <map>
#include <unordered_set>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace xrt_core { class device; }

namespace xrt {

// Alias for kernel argument type
using xarg = xrt_core::xclbin::kernel_argument;

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
  aiebu::elf m_elf;
  xrt::elf::platform m_platform;
  std::string m_path; // file path from which elf was loaded, empty if loaded from stream/buffer

  // Final kernel objects
  std::vector<elf::kernel> m_kernels;

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

  // NOLINTEND

  // elf_impl() - constructor
  //
  // @elf:  Parsed aiebu::elf object
  // @path: file path if loaded from a file, empty otherwise
  elf_impl(aiebu::elf&& elf, std::string path);

public:
  virtual ~elf_impl() = default;

  // Base class managed through shared_ptr - no copy/move
  elf_impl(const elf_impl&) = delete;
  elf_impl(elf_impl&&) = delete;
  elf_impl& operator=(const elf_impl&) = delete;
  elf_impl& operator=(elf_impl&&) = delete;

  // Get the aiebu::elf object (replaces get_elfio() for Step 1.3 call sites)
  const aiebu::elf&
  get_aiebu_elf() const
  {
    return m_elf;
  }

  // Temporary escape hatch — forwards to aiebu::elf::get_elfio().
  // TODO Step 1.3: remove once xrt_kernel.cpp and xdp elf_helper.cpp
  // are updated to use get_aiebu_elf() directly.
  const ELFIO::elfio&
  get_elfio() const
  {
    return m_elf.get_elfio();
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

  // Get partition size from ELF notes
  uint32_t
  get_partition_size() const;

  // Check if this is a full ELF (contains all info for hw context)
  bool
  is_full_elf() const
  {
    return m_elf.is_full_elf();
  }

  // Get OS ABI from ELF header
  uint8_t
  get_os_abi() const
  {
    return m_elf.get_os_abi();
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
  get_abi_version() const
  {
    return m_elf.get_abi_version();
  }

  // Check if ELF uses .group sections (version-dependent)
  virtual bool
  is_group_elf() const = 0;

  // Get control code id from kernel name — identical for all platforms.
  uint32_t
  get_ctrlcode_id(const std::string& name) const
  {
    return m_elf.get_ctrlcode_id(name);
  }

  // Get the ERT command opcode in ELF flow
  virtual ert_cmd_opcode
  get_ert_opcode() const = 0;

  // Get patcher configs for a specific ctrl code id.
  // Returns const pointer into m_arg2patcher — zero-copy, used on hot path.
  const std::map<std::string, patcher_config>*
  get_patcher_configs(uint32_t ctrl_code_id) const
  {
    auto it = m_arg2patcher.find(ctrl_code_id);
    if (it != m_arg2patcher.end())
      return &it->second;
    return nullptr;
  }

  // ---- All buffer accessors forward directly to aiebu::elf ----
  // module_run calls these with the ctrl_code_id it already holds,
  // allocates a BO of the returned size, then copies into the BO mapping.

  // gen2: instruction buffer
  size_t get_instr_buf_size(uint32_t id) const { return m_elf.get_instr_buf_size(id); }
  void   copy_instr_buf(uint32_t id, aiebu::detail::span<std::byte> d) const { m_elf.copy_instr_buf(id, d); }

  // gen2: control packet buffer
  size_t get_ctrl_packet_size(uint32_t id) const { return m_elf.get_ctrl_packet_size(id); }
  void   copy_ctrl_packet(uint32_t id, aiebu::detail::span<std::byte> d) const { m_elf.copy_ctrl_packet(id, d); }

  // gen2: preemption save / restore buffers
  size_t get_preempt_save_size(uint32_t id) const { return m_elf.get_preempt_save_size(id); }
  void   copy_preempt_save(uint32_t id, aiebu::detail::span<std::byte> d) const { m_elf.copy_preempt_save(id, d); }
  size_t get_preempt_restore_size(uint32_t id) const { return m_elf.get_preempt_restore_size(id); }
  void   copy_preempt_restore(uint32_t id, aiebu::detail::span<std::byte> d) const { m_elf.copy_preempt_restore(id, d); }
  bool   has_preemption() const { return m_elf.has_preemption(); }

  // gen2: PDI buffers
  // O(1) lookup of PDI symbols for a ctrl-code-id; no copy.
  const std::unordered_set<std::string>&
  get_pdi_symbols(uint32_t ctrl_code_id) const { return m_elf.get_pdi_symbols(ctrl_code_id); }

  size_t get_pdi_size(const std::string& sym) const { return m_elf.get_pdi_size(sym); }
  void   copy_pdi(const std::string& sym, aiebu::detail::span<std::byte> d) const { m_elf.copy_pdi(sym, d); }

  // gen2: ctrlpkt preemption buffers
  const std::set<std::string>& get_ctrlpkt_pm_dynsyms() const { return m_elf.get_ctrlpkt_pm_dynsyms(); }
  size_t get_ctrlpkt_pm_buf_size(const std::string& sym) const { return m_elf.get_ctrlpkt_pm_buf_size(sym); }
  void   copy_ctrlpkt_pm_buf(const std::string& sym, aiebu::detail::span<std::byte> d) const { m_elf.copy_ctrlpkt_pm_buf(sym, d); }

  // gen2: ctrl scratch pad memory size
  size_t get_ctrl_scratch_pad_mem_size() const { return m_elf.get_ctrl_scratch_pad_mem_size(); }

  // gen2plus: column ctrl-code buffers
  size_t get_column_count(uint32_t id) const { return m_elf.get_column_count(id); }
  size_t get_ctrlcode_col_size(uint32_t id, uint32_t col) const { return m_elf.get_ctrlcode_size(id, col); }
  void   copy_ctrlcode_col(uint32_t id, uint32_t col, aiebu::detail::span<std::byte> d) const { m_elf.copy_ctrlcode(id, col, d); }

  // gen2plus: ctrlpkt buffers
  // Prefer for_each_ctrlpkt() for iteration — avoids the vector<string> heap allocation.
  std::vector<std::string> get_ctrlpkt_section_names(uint32_t id) const { return m_elf.get_ctrlpkt_section_names(id); }
  size_t get_ctrlpkt_size(uint32_t id, const std::string& name) const { return m_elf.get_ctrlpkt_size(id, name); }
  void   copy_ctrlpkt(uint32_t id, const std::string& name, aiebu::detail::span<std::byte> d) const { m_elf.copy_ctrlpkt(id, name, d); }

  void
  for_each_ctrlpkt(uint32_t id,
                   const std::function<void(const std::string&, size_t)>& f) const
  { m_elf.for_each_ctrlpkt(id, f); }

  // gen2plus: dump buffer
  size_t get_dump_buf_size(uint32_t id) const { return m_elf.get_dump_buf_size(id); }
  void   copy_dump_buf(uint32_t id, aiebu::detail::span<std::byte> d) const { m_elf.copy_dump_buf(id, d); }
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

// Package a raw AIE coredump blob into an ET_CORE ELF with metadata.
// Metadata (timestamp, firmware version, device info, context status) is built
// internally by querying the device.  The AIE architecture is derived from the
// ELF's OS/ABI byte.
//
// uuid: UUID to embed in the coredump metadata.  Pass the UUID of the specific
//   ELF that caused the fault (e.g. the timed-out run's ELF).  Pass empty string
//   when no single ELF is attributable — e.g. a partition-level dump triggered
//   from the public API where multiple ELFs may be loaded.
std::vector<char>
make_aie_coredump_elf(const xrt::elf& elf, const std::vector<char>& blob,
                      const xrt_core::device* device, uint32_t slot,
                      const std::string& uuid = "");

} // namespace xrt_core::elf_int

#endif
