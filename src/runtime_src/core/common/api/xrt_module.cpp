// Copyright (C) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_module.h
#define XRT_API_SOURCE         // exporting xrt_module.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/time.h"
#include "xrt/experimental/xrt_module.h"
#include "xrt/experimental/xrt_aie.h"
#include "xrt/experimental/xrt_elf.h"
#include "xrt/experimental/xrt_ext.h"

#include "xrt/xrt_bo.h"
#include "xrt/xrt_hw_context.h"

#include "xrt/detail/ert.h"

#include "bo_int.h"
#include "elf_int.h"
#include "hw_context_int.h"
#include "module_int.h"
#include "elf_patcher.h"
#include "core/common/debug.h"
#include "core/common/dlfcn.h"
#include "core/common/aiebu/src/cpp/dtrace/dtrace.h"

#include <boost/format.hpp>
#include <elfio/elfio.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <unordered_set>

#ifdef _WIN32
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <cxxabi.h>
#endif

namespace {

namespace xbi = xrt_core::bo_int;

XRT_CORE_UNUSED void
dump_bo(xrt::bo& bo, const std::string& filename)
{
  std::ofstream ofs(filename, std::ios::out | std::ios::binary);
  if (!ofs.is_open())
    throw std::runtime_error("Failure opening file " + filename + " for writing!");

  auto buf = bo.map<char*>();
  ofs.write(buf, static_cast<std::streamsize>(bo.size()));
}
} // namespace

namespace xrt {
class module_impl
{
protected:
  std::shared_ptr<xrt::elf_impl> m_elf_impl; // NOLINT

public:
  explicit
  module_impl(const xrt::elf& elf)
    : m_elf_impl(elf.get_handle())
  {}

  virtual ~module_impl() = default;

  // Base class managed through shared_ptr - no copy/move
  module_impl(const module_impl&) = delete;
  module_impl(module_impl&&) = delete;
  module_impl& operator=(const module_impl&) = delete;
  module_impl& operator=(module_impl&&) = delete;

  virtual xrt::hw_context
  get_hw_context() const
  {
    return {};
  }

  // Run-level dtrace control file
  virtual void
  set_dtrace_control_file(const std::string&)
  {
    throw std::runtime_error("Not supported");
  }

  std::shared_ptr<xrt::elf_impl>
  get_elf_handle() const
  {
    return m_elf_impl;
  }

  // Fill in ERT command payload in ELF flow
  // The payload is after extra_cu_mask and before CU arguments.
  // Returns the current point of the ERT command payload
  virtual uint32_t*
  fill_ert_dpu_data(uint32_t*) const
  {
    throw std::runtime_error("Not supported");
  }

  // Patch argument in control code
  virtual void
  patch(const std::string&, size_t, uint64_t)
  {
    throw std::runtime_error("Not supported");
  }

  // Check that all arguments have been patched and sync control code
  // buffer if necessary. Throw if not all arguments have been patched.
  virtual void
  sync_if_dirty()
  {
    throw std::runtime_error("Not supported");
  }

  // Dump dynamic trace buffer (optional)
  virtual void
  dump_dtrace_buffer(uint32_t)
  {
  //Placeholder has no dtrace
  }

  // Get control scratchpad buffer object
  virtual xrt::bo
  get_ctrl_scratchpad_bo()
  {
    throw std::runtime_error("Not supported");
  }

  // Get patch buffer size based on buffer type
  virtual size_t
  get_patch_buf_size(xrt_core::elf_patcher::buf_type) const
  {
    throw std::runtime_error("Not supported");
  }

  // This function is used for patching buffers at shim level
  // It is used with internal test cases that verifies shim functionality.
  virtual void
  patch(uint8_t*, size_t, const std::vector<std::pair<std::string, uint64_t>>*,
        xrt_core::elf_patcher::buf_type)
  {
    throw std::runtime_error("Not supported");
  }
};

// module that is associated with a hardware context
// created during xrt::run object creation
class module_run : public module_impl
{
protected:
  // NOLINTBEGIN
  // Protected members allow derived classes direct access without boilerplate
  // getters/setters. This is a controlled inheritance hierarchy within this file.

  xrt::hw_context m_hwctx;
  uint32_t m_ctrl_code_id;

  // Alias for patcher types
  using patcher_config = xrt_core::elf_patcher::patcher_config;
  using symbol_patcher = xrt_core::elf_patcher::symbol_patcher;

  // Pointer to shared patcher configs
  // This is created during ELF parsing and shared across module_run instances
  const std::map<std::string, patcher_config>* m_patcher_configs = nullptr;

  // Runtime patchers - each symbol_patcher references shared config + owns state
  // Created lazily on first patch for each argument
  std::map<std::string, symbol_patcher> m_patchers;

  // Arguments patched in the buffer object
  std::set<std::string> m_patched_args;

  // Dirty bit to indicate patching was done prior to last buffer sync
  bool m_dirty{ false };

  // First patch flag - buffers are synced fully on first run
  bool m_first_patch = true;
  // NOLINTEND

private:
  union debug_flag_union {
    struct debug_mode_struct {
      uint32_t dump_control_codes     : 1;
      uint32_t dump_control_packet    : 1;
      uint32_t dump_preemption_codes  : 1;
      uint32_t reserved : 29;
    } debug_flags;
    uint32_t all;
  } m_debug_mode = {};
  uint32_t m_id {0};

protected:
  bool
  is_dump_control_codes() const {
    return m_debug_mode.debug_flags.dump_control_codes != 0;
  }

  bool
  is_dump_control_packet() const {
    return m_debug_mode.debug_flags.dump_control_packet != 0;
  }

  bool
  is_dump_preemption_codes() const {
    return m_debug_mode.debug_flags.dump_preemption_codes != 0;
  }

  uint32_t get_id() const {
    return m_id;
  }

  // Fill buffer object with data from buf structure
  static void
  fill_bo_with_data(xrt::bo& bo, const xrt::buf& buf, bool sync = true)
  {
    auto ptr = bo.map<char*>();
    buf.copy_to(ptr);
    if (sync)
      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  }

  // Helper function for patching buffer with argument name or index
  bool
  patch_helper(xrt::bo& bo, const std::string& argnm, size_t index, uint64_t patch,
               xrt_core::elf_patcher::buf_type type)
  {
    // Check if patcher configs exist
    if (!m_patcher_configs || m_patcher_configs->empty())
        return false;

    const auto key_string = xrt_core::elf_patcher::generate_key_string(argnm, type);

    auto config_it = m_patcher_configs->find(key_string);
    auto not_found_use_argument_name = (config_it == m_patcher_configs->end());
    std::string used_key = key_string;

    if (not_found_use_argument_name) {
      // Search using index
      auto index_string = std::to_string(index);
      const auto key_index_string = xrt_core::elf_patcher::generate_key_string(index_string, type);
      config_it = m_patcher_configs->find(key_index_string);
      if (config_it == m_patcher_configs->end())
        return false;
      used_key = key_index_string;
    }

    // Get or create symbol_patcher for this key
    auto patcher_it = m_patchers.find(used_key);
    if (patcher_it == m_patchers.end()) {
      // Create new symbol_patcher referencing the shared config
      patcher_it = m_patchers.emplace(used_key, symbol_patcher{&config_it->second}).first;
    }

    // Call patch - symbol_patcher owns its state internally
    patcher_it->second.patch_symbol(bo, patch, m_first_patch);

    if (xrt_core::config::get_xrt_debug()) {
      if (not_found_use_argument_name) {
        std::stringstream ss;
        ss << "Patched " << xrt_core::elf_patcher::get_section_name(type)
           << " using argument index " << index
           << " with value " << std::hex << patch;
        xrt_core::message::send( xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
      else {
        std::stringstream ss;
        ss << "Patched " << xrt_core::elf_patcher::get_section_name(type)
           << " using argument name " << argnm
           << " with value " << std::hex << patch;
        xrt_core::message::send( xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }

    m_dirty = true;
    return true;
  }

public:
  module_run(const xrt::elf& elf, xrt::hw_context hw_context, uint32_t id)
    : module_impl(elf)
    , m_hwctx(std::move(hw_context))
    , m_ctrl_code_id(id)
    , m_patcher_configs(m_elf_impl->get_patcher_configs(m_ctrl_code_id))
  {
    if (xrt_core::config::get_xrt_debug()) {
      m_debug_mode.debug_flags.dump_control_codes = xrt_core::config::get_feature_toggle("Debug.dump_control_codes");
      m_debug_mode.debug_flags.dump_control_packet = xrt_core::config::get_feature_toggle("Debug.dump_control_packet");
      m_debug_mode.debug_flags.dump_preemption_codes = xrt_core::config::get_feature_toggle("Debug.dump_preemption_codes");
      static std::atomic<uint32_t> s_id {0};
      m_id = s_id++;
    }
  }

  xrt::hw_context
  get_hw_context() const override
  {
    return m_hwctx;
  }
};

class module_run_aie_gen2 : public module_run
{
  // Platform-specific configuration from ELF
  xrt::module_config_aie_gen2 m_config;

  // Instruction and control packet buffers
  xrt::bo m_instr_bo;
  xrt::bo m_ctrlpkt_bo;

  // Preemption buffers
  xrt::bo m_preempt_save_bo;
  xrt::bo m_preempt_restore_bo;

  // Control scratch pad memory buffer
  xrt::bo m_ctrl_scratch_pad_mem;

  // Map of ctrlpkt preemption buffers
  // key : section name (.ctrlpkt.pm.*)
  // value : xrt::bo filled with corresponding section data
  std::map<std::string, xrt::bo> m_ctrlpkt_pm_bos;

  // map storing xrt::bo that stores pdi data corresponding to each pdi symbol
  std::map<std::string, xrt::bo> m_pdi_bo_map;

  // Symbol names for patching
  static constexpr const char* Scratch_Pad_Mem_Symbol = "scratch-pad-mem";
  static constexpr const char* Control_Packet_Symbol = "control-packet";
  static constexpr const char* Control_ScratchPad_Symbol = "scratch-pad-ctrl";

  ////////////////////////////////////////////////////////////////
  // Functions that create and fill different buffers
  ////////////////////////////////////////////////////////////////
  void
  create_ctrlpkt_buf(const xrt::bo& ctrlpkt_bo)
  {
    if (ctrlpkt_bo.size() == 0) {
      XRT_DEBUGF("ctrpkt buf is empty\n");
      return;
    }

    m_ctrlpkt_bo = ctrlpkt_bo; // assign pre created buffer

    if (is_dump_control_packet()) {
      std::string dump_file_name = "ctr_packet_pre_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_ctrlpkt_bo, dump_file_name);

      std::stringstream ss;
      ss << "dumped file " << dump_file_name;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }
  }

  void
  create_ctrlpkt_pm_bufs()
  {
    for (const auto& [key, buf] : m_config.ctrlpkt_pm_bufs) {
      m_ctrlpkt_pm_bos[key] = xbi::create_bo(m_hwctx, buf.size(), xbi::use_type::ctrlpkt);
      fill_bo_with_data(m_ctrlpkt_pm_bos.at(key), buf);
    }
  }

  void
  create_instruction_buf()
  {
    XRT_DEBUGF("-> module_run_aie_gen2::create_instruction_buf()\n");

    // Get instruction buffer from config
    size_t sz = m_config.instr_data.size();
    if (sz == 0)
      throw std::runtime_error("Invalid instruction buffer size");

    // Create and fill instruction buffer object
    m_instr_bo = xbi::create_bo(m_hwctx, sz, xbi::use_type::instruction);
    fill_bo_with_data(m_instr_bo, m_config.instr_data, false /*don't sync*/);

    if (is_dump_control_codes()) {
      std::string dump_file_name = "ctr_codes_pre_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_instr_bo, dump_file_name);

      std::stringstream ss;
      ss << "dumped file " << dump_file_name << " ctr_codes size: " << std::to_string(sz);
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }

    // Handle preemption save/restore buffers from config
    auto preempt_save_size = m_config.preempt_save_data.size();
    auto preempt_restore_size = m_config.preempt_restore_data.size();

    if ((preempt_save_size > 0) && (preempt_restore_size > 0)) {
      m_preempt_save_bo = xbi::create_bo(m_hwctx, preempt_save_size, xbi::use_type::preemption);
      fill_bo_with_data(m_preempt_save_bo, m_config.preempt_save_data, false);

      m_preempt_restore_bo = xbi::create_bo(m_hwctx, preempt_restore_size, xbi::use_type::preemption);
      fill_bo_with_data(m_preempt_restore_bo, m_config.preempt_restore_data, false);

      if (is_dump_preemption_codes()) {
        std::string dump_file_name = "preemption_save_pre_patch" + std::to_string(get_id()) + ".bin";
        dump_bo(m_preempt_save_bo, dump_file_name);

        std::stringstream ss;
        ss << "dumped file " << dump_file_name;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());

        dump_file_name = "preemption_restore_pre_patch" + std::to_string(get_id()) + ".bin";
        dump_bo(m_preempt_restore_bo, dump_file_name);

        ss.str("");
        ss << "dumped file " << dump_file_name;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }

      // Get scratchpad memory and patch preemption buffers
      const auto& scratchpad_mem =
          xrt_core::hw_context_int::get_scratchpad_mem_buf(m_hwctx, m_config.scratch_pad_mem_size);

      if (!scratchpad_mem)
        throw std::runtime_error("Failed to get scratchpad buffer from context\n");

      patch_helper(m_preempt_save_bo, Scratch_Pad_Mem_Symbol, 0, scratchpad_mem.address(),
                   xrt_core::elf_patcher::buf_type::preempt_save);
      patch_helper(m_preempt_restore_bo, Scratch_Pad_Mem_Symbol, 0, scratchpad_mem.address(),
                   xrt_core::elf_patcher::buf_type::preempt_restore);

      if (is_dump_preemption_codes()) {
        std::stringstream ss;
        ss << "patched preemption-codes using scratch_pad_mem at address "
           << std::hex << scratchpad_mem.address()
           << " size "
           << std::hex << scratchpad_mem.size();
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }

    // Create control scratchpad buffer and patch if symbol is present
    if (m_config.ctrl_scratch_pad_mem_size > 0) {
      m_ctrl_scratch_pad_mem = xbi::create_bo(m_hwctx, m_config.ctrl_scratch_pad_mem_size, xbi::use_type::ctrl_scratch_pad);
      patch_helper(m_instr_bo, Control_ScratchPad_Symbol, 0, m_ctrl_scratch_pad_mem.address(),
                   xrt_core::elf_patcher::buf_type::ctrltext);
    }

    // Patch all PDI addresses using config's pdi symbols
    for (const auto& symbol : m_config.patch_pdi_symbols) {
      const auto& pdi_data = m_config.elf_parent->get_pdi(symbol);
      auto pdi_bo = xbi::create_bo(m_hwctx, pdi_data.size(), xbi::use_type::pdi);
      fill_bo_with_data(pdi_bo, pdi_data);
      // Move bo into map and get reference for patching
      auto [it, inserted] = m_pdi_bo_map.emplace(symbol, std::move(pdi_bo));

      // Patch instr buffer with PDI address
      patch_helper(m_instr_bo, symbol, 0, it->second.address(), xrt_core::elf_patcher::buf_type::ctrltext);
    }

    // Patch control packet address if present
    if (m_ctrlpkt_bo) {
      patch_helper(m_instr_bo, Control_Packet_Symbol, 0, m_ctrlpkt_bo.address(),
                   xrt_core::elf_patcher::buf_type::ctrltext);
    }

    // Patch ctrlpkt preemption buffers using config's dynsyms
    for (const auto& dynsym : m_config.ctrlpkt_pm_dynsyms) {
      // Convert symbol name to section name: ctrlpkt-pm-0 -> .ctrlpkt.pm.0
      std::string sec_name = "." + dynsym;
      std::replace(sec_name.begin(), sec_name.end(), '-', '.');

      auto bo_itr = m_ctrlpkt_pm_bos.find(sec_name);
      if (bo_itr == m_ctrlpkt_pm_bos.end())
        throw std::runtime_error("Unable to find ctrlpkt pm buffer for symbol " + dynsym);

      patch_helper(m_instr_bo, dynsym, 0, bo_itr->second.address(),
                   xrt_core::elf_patcher::buf_type::ctrltext);
    }

    XRT_DEBUGF("<- module_run_aie_gen2::create_instruction_buf()\n");
  }

  ////////////////////////////////////////////////////////////////
  // ERT payload fill functions
  ////////////////////////////////////////////////////////////////

  uint32_t*
  fill_ert_aie_gen2_preempt_data(uint32_t* payload) const
  {
    // npu preemption in elf_flow
    auto npu = reinterpret_cast<ert_npu_preempt_data*>(payload);
    npu->instruction_buffer = m_instr_bo.address();
    npu->instruction_buffer_size = static_cast<uint32_t>(m_instr_bo.size());
    npu->instruction_prop_count = 0; // Reserved for future use
    if (m_preempt_save_bo && m_preempt_restore_bo) {
      npu->save_buffer = m_preempt_save_bo.address();
      npu->save_buffer_size = static_cast<uint32_t>(m_preempt_save_bo.size());
      npu->restore_buffer = m_preempt_restore_bo.address();
      npu->restore_buffer_size = static_cast<uint32_t>(m_preempt_restore_bo.size());
    }
    payload += sizeof(ert_npu_preempt_data) / sizeof(uint32_t);
    return payload;
  }

  uint32_t*
  fill_ert_aie_gen2_non_preempt_data(uint32_t* payload) const
  {
    auto npu = reinterpret_cast<ert_npu_data*>(payload);
    npu->instruction_buffer = m_instr_bo.address();
    npu->instruction_buffer_size = static_cast<uint32_t>(m_instr_bo.size());
    npu->instruction_prop_count = 0; // Reserved for future use
    payload += sizeof(ert_npu_data) / sizeof(uint32_t);
    return payload;
  }

public:
  module_run_aie_gen2(const xrt::elf& elf, const xrt::hw_context& hw_context,
                      uint32_t id, const xrt::bo& ctrlpkt_bo)
    : module_run(elf, hw_context, id)
    , m_config(std::get<xrt::module_config_aie_gen2>(m_elf_impl->get_module_config(id)))
  {
    create_ctrlpkt_buf(ctrlpkt_bo);
    create_ctrlpkt_pm_bufs();
    create_instruction_buf();
  }

  // Fill in ERT command payload for AIE2P platform
  uint32_t*
  fill_ert_dpu_data(uint32_t* payload) const override
  {
    // Use preempt data if preemption buffers exist or if it's a group ELF
    if ((m_preempt_save_bo && m_preempt_restore_bo) || m_elf_impl->is_group_elf())
      return fill_ert_aie_gen2_preempt_data(payload);
    else
      return fill_ert_aie_gen2_non_preempt_data(payload);
  }

  // Patch argument in control code
  void
  patch(const std::string& argnm, size_t index, uint64_t value) override
  {
    bool patched = false;

    // patch control-packet buffer
    if (m_ctrlpkt_bo) {
      if (patch_helper(m_ctrlpkt_bo, argnm, index, value, xrt_core::elf_patcher::buf_type::ctrldata))
        patched = true;
    }

    // patch instruction buffer
    if (m_instr_bo) {
      if (patch_helper(m_instr_bo, argnm, index, value, xrt_core::elf_patcher::buf_type::ctrltext))
        patched = true;
    }

    if (patched) // if patched, add to patched args set
      m_patched_args.insert(argnm);
  }

  // Sync buffers to device if patching was done
  void
  sync_if_dirty() override
  {
    if (!m_dirty) {
      if (!m_first_patch)
        return;

      // its first run sync entire buffers
      m_instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
      if (m_ctrlpkt_bo)
        m_ctrlpkt_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

      if (m_preempt_save_bo && m_preempt_restore_bo) {
        m_preempt_save_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        m_preempt_restore_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
      }

      m_first_patch = false;
      return;
    }

    // sync full buffer only if its first time
    // For subsequent runs only part of buffer that is patched is synced
    if (m_first_patch)
      m_instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    if (is_dump_control_codes()) {
      std::string dump_file_name = "ctr_codes_post_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_instr_bo, dump_file_name);

      std::stringstream ss;
      ss << "dumped file " << dump_file_name;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }

    if (m_ctrlpkt_bo) {
      if (m_first_patch)
        m_ctrlpkt_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

      if (is_dump_control_packet()) {
        std::string dump_file_name = "ctr_packet_post_patch" + std::to_string(get_id()) + ".bin";
        dump_bo(m_ctrlpkt_bo, dump_file_name);

        std::stringstream ss;
        ss << "dumped file " << dump_file_name;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }

    if (m_preempt_save_bo && m_preempt_restore_bo) {
      if (m_first_patch) {
        m_preempt_save_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        m_preempt_restore_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
      }

      if (is_dump_preemption_codes()) {
        std::string dump_file_name = "preemption_save_post_patch" + std::to_string(get_id()) + ".bin";
        dump_bo(m_preempt_save_bo, dump_file_name);

        std::stringstream ss;
        ss << "dumped file " << dump_file_name;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());

        dump_file_name = "preemption_restore_post_patch" + std::to_string(get_id()) + ".bin";
        dump_bo(m_preempt_restore_bo, dump_file_name);

        ss.clear();
        ss << "dumped file " << dump_file_name;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }

    m_dirty = false;
    m_first_patch = false;
  }

  // Get control scratchpad buffer object
  xrt::bo
  get_ctrl_scratchpad_bo() override
  {
    if (!m_ctrl_scratch_pad_mem)
      throw std::runtime_error("Control scratchpad memory is not present\n");

    // Sync bo data before returning
    m_ctrl_scratch_pad_mem.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    return m_ctrl_scratch_pad_mem;
  }
};

class module_run_aie_gen2_plus : public module_run
{
  // Platform-specific configuration from ELF
  xrt::module_config_aie_gen2_plus m_config;

  // Instruction buffer (combined ctrlcode for all columns)
  xrt::bo m_buffer;

  // Map of control packet buffers
  // key: section name (.ctrlpkt.*), value: xrt::bo
  std::map<std::string, xrt::bo> m_ctrlpkt_bos;

  // Tuple of uC index, address, size, dtrace_addr for each column
  // Used in ert_dpu_data payload to identify ctrlcode and dtrace buffer per column
  enum column_bo_index : size_t { col_ucidx = 0, col_base_addr, col_size, col_dtrace_addr };
  std::vector<std::tuple<uint16_t, uint64_t, uint64_t, uint64_t>> m_column_bo_address;

  // Symbol name for control code patching
  static constexpr const char* Control_Code_Symbol = "control-code";

  ////////////////////////////////////////////////////////////////
  // Dtrace Implementation
  ////////////////////////////////////////////////////////////////

  // Dynamic tracing utility structure
  struct dtrace_util
  {
    // Function pointer for dtrace destroy handle
    using dtrace_destroy_handle = void (*)(dtrace_handle_t);

    std::unique_ptr<void, dtrace_destroy_handle> dtrace_handle;
    xrt::bo ctrl_bo;
    std::map<uint32_t, size_t> buf_offset_map;

    dtrace_util() : dtrace_handle(nullptr, destroy_dtrace_handle) {}

    dtrace_util(const std::string& ctrl_file_path, const std::string& map_data,
                uint32_t log_level)
      : dtrace_handle(create_dtrace_handle(ctrl_file_path.c_str(), map_data.c_str(), log_level),
                      destroy_dtrace_handle) {}
  };
  dtrace_util m_dtrace;

  // Creates dtrace util object.
  // Sets path (run-level overrides config file).
  // Returns true on success.
  bool
  create_dtrace_util(const std::string& run_level_ct_file)
  {
    std::string path = run_level_ct_file.empty()
      ? xrt_core::config::get_dtrace_control_file_path()
      : run_level_ct_file;
    if (path.empty())
      return false;
    if (!std::filesystem::exists(path)) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "xrt_module",
                              "Dtrace control file is not accessible");
      return false;
    }

    // Get dump buffer data from config
    if (m_config.dump_buf.size() == 0) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              "Dump section is empty in ELF");
      return false;
    }

    auto log_level = static_cast<uint32_t>(xrt_core::config::get_dtrace_log_level());
    log_level = (log_level > 3) ? 3U : (log_level < 1) ? 1U : log_level;

    m_dtrace = dtrace_util(path, m_config.dump_buf.to_string(), log_level);
    if (!m_dtrace.dtrace_handle.get()) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
        "[dtrace] : Failed to get dtrace handle");
      return false;
    }

    return true;
  }

  // Create dtrace buffers (ctrl_bo, buf_offset_map).
  // Assumes m_dtrace.dtrace_handle is already set.
  void
  create_dtrace_buffers()
  {
    uint32_t buffers_length = 0;
    get_dtrace_col_numbers(m_dtrace.dtrace_handle.get(), &buffers_length);

    if (!buffers_length) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
        "[dtrace] : Control buffer size is zero, no dtrace o/p");
      return;
    }

    try {
      std::vector<uint64_t> buffers(buffers_length);
      get_dtrace_buffer_size(m_dtrace.dtrace_handle.get(), buffers.data());

      size_t total_size = 0;

      constexpr uint32_t mask32 = 0xffffffff;
      constexpr uint32_t shift32 = 32;
      for (const auto& entry : buffers) {
        m_dtrace.buf_offset_map[static_cast<uint32_t>(entry & mask32)] = total_size;
        total_size += static_cast<size_t>(entry >> shift32) * sizeof(uint32_t);
      }

      m_dtrace.ctrl_bo = xbi::create_bo(m_hwctx, total_size, xbi::use_type::dtrace);
      populate_dtrace_buffer(m_dtrace.dtrace_handle.get(), m_dtrace.ctrl_bo.map<uint32_t*>(), m_dtrace.ctrl_bo.address());
      m_dtrace.ctrl_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              "[dtrace] : dtrace buffers initialized successfully");
    }
    catch (const std::exception &e) {
      m_dtrace.ctrl_bo = {};
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              std::string{"[dtrace] : dtrace buffers initialization failed, "} + e.what());
    }
  }

  // Initialize dtrace buffer for debugging/tracing.
  // Uses config path if run_level_ct_file is empty.
  void
  initialize_dtrace_buf(const std::string& run_level_ct_file = "")
  {
    if (!create_dtrace_util(run_level_ct_file))
      return;  // create failure

    create_dtrace_buffers();
  }

  // Reinit dtrace with a new control file (e.g. after run finished, before next
  // start). Run-level path preferred over config.
  void
  set_dtrace_control_file(const std::string& path) override
  {
    initialize_dtrace_buf(path);
    // Only update dtrace addresses; instruction buffer layout is unchanged.
    update_column_bo_dtrace_addresses();
  }

  ////////////////////////////////////////////////////////////////
  // Buffer creation and initialization functions
  ////////////////////////////////////////////////////////////////

  // Create control packet buffers for all ctrlpkt sections
  void
  create_ctrlpkt_bufs()
  {
    if (m_config.ctrlpkt_bufs.empty())
      return; // older ELFs have ctrlpkt in pad section

    // Create ctrlpkt bo's for all ctrlpkt sections
    for (const auto& [name, buf] : m_config.ctrlpkt_bufs) {
      m_ctrlpkt_bos[name] = xbi::create_bo(m_hwctx, buf.size(), xbi::use_type::ctrlpkt);
      fill_bo_with_data(m_ctrlpkt_bos[name], buf);
    }

    if (is_dump_control_packet()) {
      for (auto& [name, bo] : m_ctrlpkt_bos) {
        std::string dump_file_name = name + "_pre_patch" + std::to_string(get_id()) + ".bin";
        dump_bo(bo, dump_file_name);

        std::stringstream ss;
        ss << "dumped file " << dump_file_name;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }
  }

  // Fill instruction buffer with column data, dump pre-patch, then patch
  void
  fill_instruction_buffer()
  {
    const auto& col_data = m_config.ctrlcodes;

    // Copy all column control codes to instruction buffer
    auto ptr = m_buffer.map<char*>();
    for (const auto& ctrlcode : col_data) {
      ctrlcode.copy_to(ptr);
      ptr += ctrlcode.size();
    }

    // Dump pre-patched instruction buffer
    if (is_dump_control_codes()) {
      std::string dump_file_name = "ctr_codes_pre_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_buffer, dump_file_name);

      std::stringstream ss;
      ss << "dumped file " << dump_file_name << " ctr_codes size: " << std::to_string(m_buffer.size());
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }

    // Patch control-code addresses in instruction buffer
    size_t offset = 0;
    for (size_t i = 0; i < col_data.size(); ++i) {
      // Find the control-code-* sym-name and patch it in instruction buffer
      auto sym_name = std::string(Control_Code_Symbol) + "-" + std::to_string(i);
      if (patch_helper(m_buffer, sym_name, std::numeric_limits<size_t>::max(),
                       m_buffer.address() + offset,
                       xrt_core::elf_patcher::buf_type::ctrltext))
        m_patched_args.insert(sym_name);
      offset += col_data[i].size();
    }

    // Patch control packet addresses in instruction buffer
    for (const auto& [name, ctrlpktbo] : m_ctrlpkt_bos) {
      // Symbol name is section name without the grp idx
      // if sec name is .ctrlpkt-57.grp_idx then sym name is .ctrlpkt-57
      auto dot_pos = name.rfind('.');
      auto sym_name = (dot_pos != std::string::npos && dot_pos > 0)
                    ? name.substr(0, dot_pos)
                    : name;

      if (patch_helper(m_buffer, sym_name, std::numeric_limits<size_t>::max(),
                       ctrlpktbo.address(),
                       xrt_core::elf_patcher::buf_type::ctrltext))
        m_patched_args.insert(sym_name);
    }
  }

  // Create instruction buffer with all columns data along with pad section
  void
  create_instruction_buffer()
  {
    const auto& data = m_config.ctrlcodes;

    // Create bo with combined size of all ctrlcodes
    size_t sz = std::accumulate(data.begin(), data.end(), static_cast<size_t>(0),
      [](auto acc, const auto& ctrlcode) {
        return acc + ctrlcode.size();
      });

    if (sz == 0) {
      XRT_DEBUGF("ctrlcode buf is empty\n");
      return;
    }

    m_buffer = xbi::create_bo(m_hwctx, sz, xbi::use_type::instruction);

    fill_instruction_buffer();
  }

  // Fill column buffer addresses for command submission
  // Skips empty columns (holes in the ctrlcode array).
  void
  fill_column_bo_address()
  {
    const auto& ctrlcodes = m_config.ctrlcodes;

    m_column_bo_address.clear();
    uint16_t ucidx = 0;
    auto base_addr = m_buffer.address();

    for (const auto& ctrlcode : ctrlcodes) {
      if (auto size = ctrlcode.size()) {
        uint64_t dtrace_addr = 0;
        if (m_dtrace.ctrl_bo) {
          auto it = m_dtrace.buf_offset_map.find(ucidx);
          if (it != m_dtrace.buf_offset_map.end())
            dtrace_addr = m_dtrace.ctrl_bo.address() + it->second;
        }
        m_column_bo_address.emplace_back(ucidx, base_addr, size, dtrace_addr);
      }
      ++ucidx;
      base_addr += ctrlcode.size();
    }
  }

  // Update only dtrace buffer addresses in m_column_bo_address; instruction
  // addrs/sizes unchanged.
  void
  update_column_bo_dtrace_addresses()
  {
    for (auto& entry : m_column_bo_address) {
      auto ucidx = static_cast<uint16_t>(std::get<col_ucidx>(entry));
      uint64_t dtrace_addr = 0;
      if (m_dtrace.ctrl_bo) {
        auto it = m_dtrace.buf_offset_map.find(ucidx);
        if (it != m_dtrace.buf_offset_map.end())
          dtrace_addr = m_dtrace.ctrl_bo.address() + it->second;
      }
      std::get<col_dtrace_addr>(entry) = dtrace_addr;
    }
  }

  ////////////////////////////////////////////////////////////////
  // ERT payload fill functions
  ////////////////////////////////////////////////////////////////

  uint32_t*
  fill_ert_aie_gen2_plus(uint32_t* payload) const
  {
    auto ert_dpu_data_count = static_cast<uint16_t>(m_column_bo_address.size());
    // For multiple instruction buffers, the ert_dpu_data::chained has
    // the number of words remaining in the payload after the current
    // instruction buffer. The ert_dpu_data::chained of the last buffer
    // is zero.
    for (auto [ucidx, addr, size, dtrace_addr] : m_column_bo_address) {
      auto dpu = reinterpret_cast<ert_dpu_data*>(payload);
      dpu->dtrace_buffer = dtrace_addr;
      dpu->instruction_buffer = addr;
      dpu->instruction_buffer_size = static_cast<uint32_t>(size);
      dpu->uc_index = ucidx;
      dpu->chained = --ert_dpu_data_count;
      payload += sizeof(ert_dpu_data) / sizeof(uint32_t);
    }
    return payload;
  }

public:
  module_run_aie_gen2_plus(const xrt::elf& elf, const xrt::hw_context& hw_context, uint32_t id)
    : module_run(elf, hw_context, id)
    , m_config(std::get<xrt::module_config_aie_gen2_plus>(m_elf_impl->get_module_config(id)))
  {
    initialize_dtrace_buf("");  // use config path by default
    create_ctrlpkt_bufs();
    create_instruction_buffer();
    fill_column_bo_address();
  }

  // Fill in ERT command payload for AIE2PS platform
  uint32_t*
  fill_ert_dpu_data(uint32_t* payload) const override
  {
    return fill_ert_aie_gen2_plus(payload);
  }

  // Patch argument in control code
  void
  patch(const std::string& argnm, size_t index, uint64_t value) override
  {
    bool patched = false;

    // patch instruction buffer
    if (patch_helper(m_buffer, argnm, index, value, xrt_core::elf_patcher::buf_type::ctrltext))
      patched = true;

    // patch argument in pad section
    if (patch_helper(m_buffer, argnm, index, value, xrt_core::elf_patcher::buf_type::pad))
      patched = true;

    // New Elfs have multiple ctrlpkt sections
    // Iterate over all ctrlpkt buffers and patch them
    for (auto& ctrlpkt : m_ctrlpkt_bos) {
      if (patch_helper(ctrlpkt.second, argnm, index, value, xrt_core::elf_patcher::buf_type::ctrlpkt))
        patched = true;
    }

    if (patched) // if patched, add to patched args set
      m_patched_args.insert(argnm);
  }

  // Sync buffers to device if patching was done
  void
  sync_if_dirty() override
  {
    if (!m_dirty) {
      if (!m_first_patch)
        return;

      // its first run sync entire buffers
      m_buffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
      for (auto& [name, bo] : m_ctrlpkt_bos)
        bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

      m_first_patch = false;
      return;
    }

    if (m_patcher_configs && m_patched_args.size() != m_patcher_configs->size()) {
      auto fmt = boost::format("ctrlcode requires %d patched arguments, but only %d are patched")
          % m_patcher_configs->size() % m_patched_args.size();
      throw std::runtime_error{ fmt.str() };
    }

    // sync full buffer only if its first time
    // For subsequent runs only part of buffer that is patched is synced
    if (m_first_patch)
      m_buffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    if (is_dump_control_codes()) {
      std::string dump_file_name = "ctr_codes_post_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_buffer, dump_file_name);

      std::stringstream ss;
      ss << "dumped file " << dump_file_name;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }

    for (auto& [name, bo] : m_ctrlpkt_bos) {
      if (m_first_patch)
        bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

      if (is_dump_control_packet()) {
        std::string dump_file_name = name + "_pre_patch" + std::to_string(get_id()) + ".bin";
        dump_bo(bo, dump_file_name);

        std::stringstream ss;
        ss << "dumped file " << dump_file_name;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }

    m_dirty = false;
    m_first_patch = false;
  }

  // Dump dynamic trace buffer
  void
  dump_dtrace_buffer(uint32_t run_id) override
  {
    if (!m_dtrace.ctrl_bo || !m_dtrace.dtrace_handle.get())
      return;

    // sync dtrace buffers output from device
    m_dtrace.ctrl_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    try {
      // dtrace output is dumped into current working directory
      // output is a python file
      std::string result_file_path = std::filesystem::current_path().string()
                                   + "/dtrace_dump_"
                                   + xrt_core::get_timestamp_for_filename()
                                   + "_" + std::to_string(get_id())
                                   + "_run" + std::to_string(run_id)
                                   + ".py";

      get_dtrace_result_file(m_dtrace.dtrace_handle.get(), result_file_path.c_str());

      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              std::string{"[dtrace] : dtrace buffer dumped successfully to - "} + result_file_path);
    }
    catch (const std::exception &e) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              std::string{"[dtrace] : dtrace buffer dump failed, "} + e.what());
    }
  }
};

module::
module(const xrt::elf& elf)
: detail::pimpl<module_impl>(std::make_shared<module_impl>(elf))
{}

xrt::hw_context
module::
get_hw_context() const
{
  return get_handle()->get_hw_context();
}

} // namespace xrt

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal module APIs
////////////////////////////////////////////////////////////////
namespace xrt_core::module_int {

xrt::module
create_module_run(const xrt::elf& elf, const xrt::hw_context& hwctx,
                  uint32_t ctrl_code_id, const xrt::bo& ctrlpkt_bo)
{
  auto platform = elf.get_platform();
  switch (platform) {
  case xrt::elf::platform::aie2p:
    // pre created ctrlpkt bo is used only in aie2p platform
    return xrt::module{std::make_shared<xrt::module_run_aie_gen2>(elf, hwctx, ctrl_code_id, ctrlpkt_bo)};
  case xrt::elf::platform::aie2ps:
  case xrt::elf::platform::aie2ps_group:
  case xrt::elf::platform::aie4:
  case xrt::elf::platform::aie4a:
  case xrt::elf::platform::aie4z:
    return xrt::module{std::make_shared<xrt::module_run_aie_gen2_plus>(elf, hwctx, ctrl_code_id)};
  default:
    throw std::runtime_error("Unsupported platform");
  }
}

std::shared_ptr<xrt::elf_impl>
get_elf_handle(const xrt::module& module)
{
  return module.get_handle()->get_elf_handle();
}

uint32_t*
fill_ert_dpu_data(const xrt::module& module, uint32_t* payload)
{
  return module.get_handle()->fill_ert_dpu_data(payload);
}

void
patch(const xrt::module& module, const std::string& argnm, size_t index, const xrt::bo& bo)
{
  module.get_handle()->patch(argnm, index, bo.address());
}

void
patch(const xrt::module& module, const std::string& argnm, size_t index, const void* value, size_t size)
{
  if (size > 8) // NOLINT
  throw std::runtime_error{ "Patching scalar values only supports 64-bit values or less" };

  auto arg_value = *static_cast<const uint64_t*>(value);
  module.get_handle()->patch(argnm, index, arg_value);
}

void
sync(const xrt::module& module)
{
  module.get_handle()->sync_if_dirty();
}

void
dump_dtrace_buffer(const xrt::module& module, uint32_t run_id)
{
  module.get_handle()->dump_dtrace_buffer(run_id);
}

void
set_dtrace_control_file(const xrt::module& module, const std::string& path)
{
  module.get_handle()->set_dtrace_control_file(path);
}

xrt::bo
get_ctrl_scratchpad_bo(const xrt::module& module)
{
  return module.get_handle()->get_ctrl_scratchpad_bo();
}

size_t
get_patch_buf_size(const xrt::module& module, xrt_core::elf_patcher::buf_type type,
                   uint32_t ctrl_code_id)
{
  auto elf_hdl = module.get_handle()->get_elf_handle();
  auto platform = elf_hdl->get_platform();

  if (platform == xrt::elf::platform::aie2p) {
    auto module_config =
        std::get<xrt::module_config_aie_gen2>(elf_hdl->get_module_config(ctrl_code_id));

    switch (type) {
      case xrt_core::elf_patcher::buf_type::ctrltext:
        return module_config.instr_data.size();
      case xrt_core::elf_patcher::buf_type::ctrldata:
        return module_config.ctrl_packet_data.size();
      case xrt_core::elf_patcher::buf_type::preempt_save:
        return module_config.preempt_save_data.size();
      case xrt_core::elf_patcher::buf_type::preempt_restore:
        return module_config.preempt_restore_data.size();
      default:
        throw std::runtime_error("Unknown buffer type passed");
    }
  }
  else if (platform == xrt::elf::platform::aie2ps || platform == xrt::elf::platform::aie2ps_group ||
          platform == xrt::elf::platform::aie4 || platform == xrt::elf::platform::aie4a ||
          platform == xrt::elf::platform::aie4z) {
    auto module_config =
        std::get<xrt::module_config_aie_gen2_plus>(elf_hdl->get_module_config(ctrl_code_id));

        if (type != xrt_core::elf_patcher::buf_type::ctrltext)
      throw std::runtime_error("Info of given buffer type not available");

    const auto& col_data = module_config.ctrlcodes;
    if (col_data.empty())
      throw std::runtime_error{"No control code found for given id"};
    if (col_data.size() != 1)
      throw std::runtime_error{"Patch failed: only support patching single column"};
    return col_data[0].size();
  }
  else {
    throw std::runtime_error{"Patch failed: unsupported ELF ABI"};
  }
}

void
patch(const xrt::module& module, uint8_t* ibuf, size_t sz,
      const std::vector<std::pair<std::string, uint64_t>>* args,
      xrt_core::elf_patcher::buf_type type, uint32_t ctrl_code_id)
{
  auto elf_hdl = module.get_handle()->get_elf_handle();
  const xrt::buf* inst = nullptr;
  auto platform = elf_hdl->get_platform();

  if (platform == xrt::elf::platform::aie2p) {
    auto module_config =
        std::get<xrt::module_config_aie_gen2>(elf_hdl->get_module_config(ctrl_code_id));
    switch (type) {
      case xrt_core::elf_patcher::buf_type::ctrltext:
        inst = &module_config.instr_data;
        break;
      case xrt_core::elf_patcher::buf_type::ctrldata:
        inst = &module_config.ctrl_packet_data;
        break;
      case xrt_core::elf_patcher::buf_type::preempt_save:
        inst = &module_config.preempt_save_data;
        break;
      case xrt_core::elf_patcher::buf_type::preempt_restore:
        inst = &module_config.preempt_restore_data;
        break;
      default:
        throw std::runtime_error("Unknown buffer type passed");
    }
  }
  else if (platform == xrt::elf::platform::aie2ps || platform == xrt::elf::platform::aie2ps_group ||
           platform == xrt::elf::platform::aie4 || platform == xrt::elf::platform::aie4a ||
           platform == xrt::elf::platform::aie4z) {
    auto module_config =
        std::get<xrt::module_config_aie_gen2_plus>(elf_hdl->get_module_config(ctrl_code_id));
    const auto& col_data = module_config.ctrlcodes;

    if (col_data.empty())
      throw std::runtime_error{"No control code found for given id"};
    if (col_data.size() != 1)
      throw std::runtime_error{"Patch failed: only support patching single column"};

    inst = &col_data[0];
  }
  else {
    throw std::runtime_error{"Patch failed: unsupported ELF ABI"};
  }

  if (sz < inst->size())
    throw std::runtime_error{"Control code buffer passed in is too small"};
  inst->copy_to(ibuf);

  // If no args to patch, we're done
  if (!args || args->empty())
    return;

  // Get the patcher configs from module
  const auto* patcher_configs = elf_hdl->get_patcher_configs(ctrl_code_id);
  if (!patcher_configs)
    throw std::runtime_error{"No patcher configs found for given id"};

  size_t index = 0;
  for (const auto& [arg_name, arg_addr] : *args) {
    // Find the patcher config for this argument
    auto key_string = xrt_core::elf_patcher::generate_key_string(arg_name, type);
    auto it = patcher_configs->find(key_string);
    if (it == patcher_configs->end()) {
      // Try with index
      auto index_key = xrt_core::elf_patcher::generate_key_string(std::to_string(index), type);
      it = patcher_configs->find(index_key);
      if (it == patcher_configs->end())
        throw std::runtime_error{"Failed to patch " + arg_name};
    }

    // Use static patch method (no state needed for shim tests)
    xrt_core::elf_patcher::symbol_patcher::patch_symbol_raw(ibuf, arg_addr, it->second);
    index++;
  }
}

} // namespace xrt_core::module_int
