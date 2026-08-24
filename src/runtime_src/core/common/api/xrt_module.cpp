// Copyright (C) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_module.h
#define XRT_API_SOURCE         // exporting xrt_module.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/trace.h"

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

#include <algorithm>
#include <cstddef>
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
#include <utility>

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
  // Name of the module (optional)
  std::string m_name; // NOLINT

public:
  explicit
  module_impl(const xrt::elf& elf)
    : m_elf_impl(elf.get_handle())
  {}

  module_impl(const xrt::elf& elf, std::string name)
    : m_elf_impl(elf.get_handle())
    , m_name(std::move(name))
  {}

  virtual ~module_impl() = default;

  // Base class managed through shared_ptr - no copy/move
  module_impl(const module_impl&) = delete;
  module_impl(module_impl&&) = delete;
  module_impl& operator=(const module_impl&) = delete;
  module_impl& operator=(module_impl&&) = delete;

  std::string
  get_name() const
  {
    return m_name;
  }

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

  // Check if dtrace is enabled
  virtual bool
  is_dtrace_enabled() const
  {
    return false;
  }

  // Dump dynamic trace buffer (optional)
  virtual void
  dump_dtrace_buffer(const std::string&)
  {
    throw std::runtime_error("Not supported");
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

  // scratchpad memory symbol name
  static constexpr const char* Scratch_Pad_Mem_Symbol = "scratch-pad-mem";

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

  // Helper function for patching buffer with argument name or index
  bool
  patch_helper(xrt::bo& bo, uint64_t patch, xrt_core::elf_patcher::buf_type type,
               const std::string& arg_string, const std::string& index_string,
               bool is_arg = true)
  {
    // Check if patcher configs exist
    if (!m_patcher_configs || m_patcher_configs->empty())
        return false;

    const auto key_string = xrt_core::elf_patcher::generate_key_string(arg_string, type);

    auto config_it = m_patcher_configs->find(key_string);
    auto not_found_use_argument_name = (config_it == m_patcher_configs->end());
    std::string used_key = key_string;

    if (not_found_use_argument_name) {
      // Search using index
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
    patcher_it->second.patch_symbol(bo, patch, m_first_patch, is_arg);

    if (xrt_core::config::get_xrt_debug()) {
      if (not_found_use_argument_name) {
        std::stringstream ss;
        ss << "Patched " << xrt_core::elf_patcher::get_section_name(type)
           << " using argument index " << index_string
           << " with value " << std::hex << patch;
        xrt_core::message::send( xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
      else {
        std::stringstream ss;
        ss << "Patched " << xrt_core::elf_patcher::get_section_name(type)
           << " using argument name " << arg_string
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
  // Instruction buffer — holds .ctrltext section data; patched in place
  // before each run and synced to device.
  xrt::bo m_instr_bo;

  // Optional control-packet buffer provided by the caller at construction.
  // Patched with its device address into the instruction buffer.
  xrt::bo m_ctrlpkt_bo;

  // Preemption save / restore buffers — present only when the ELF has
  // paired .preempt_save / .preempt_restore sections.
  xrt::bo m_preempt_save_bo;
  xrt::bo m_preempt_restore_bo;

  // Control scratch-pad memory — sized from the "scratch-pad-ctrl" dynsym;
  // its device address is patched into the instruction buffer.
  xrt::bo m_ctrl_scratch_pad_mem;

  // Preemption control-packet buffers, keyed by section name (.ctrlpkt.pm.*).
  // Each gets its own BO; addresses are patched into the instruction buffer.
  std::map<std::string, xrt::bo> m_ctrlpkt_pm_bos;

  // PDI image buffers, keyed by PDI symbol name.
  // Each PDI gets its own BO; addresses are patched into the instruction buffer.
  std::map<std::string, xrt::bo> m_pdi_bo_map;

  // ELF dynamic symbol names used to identify special patch targets
  static constexpr const char* Control_Packet_Symbol    = "control-packet";
  static constexpr const char* Control_ScratchPad_Symbol = "scratch-pad-ctrl";

  // Fixed preemption scratch-pad size for AIE2P — passed to
  // get_scratchpad_mem_buf() so the hw_context allocates enough space
  // for save/restore context.  Hard-coded for this platform; AIE2PS/AIE4
  // use a different (larger) value in module_run_aie_gen2_plus.
  static constexpr size_t scratch_pad_mem_size = 512 * 1024; // 512 KB // NOLINT

  void
  create_ctrlpkt_buf(const xrt::bo& ctrlpkt_bo)
  {
    if (ctrlpkt_bo.size() == 0) {
      XRT_DEBUGF("ctrpkt buf is empty\n");
      return;
    }
    m_ctrlpkt_bo = ctrlpkt_bo;
    if (is_dump_control_packet()) {
      std::string dump_file_name = "ctr_packet_pre_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_ctrlpkt_bo, dump_file_name);
      std::stringstream ss;
      ss << "dumped file " << dump_file_name;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }
  }

  // dynsym names use hyphens (e.g. "ctrlpkt-pm-0") while the corresponding
  // ELF section names use dots (e.g. ".ctrlpkt.pm.0").
  static std::string
  ctrlpkt_pm_dynsym_to_section(const std::string& dynsym)
  {
    std::string sec = "." + dynsym;
    std::replace(sec.begin(), sec.end(), '-', '.');
    return sec;
  }

  void
  create_ctrlpkt_pm_bufs()
  {
    for (const auto& dynsym : m_elf_impl->get_ctrlpkt_pm_dynsyms()) {
      std::string sec_name = ctrlpkt_pm_dynsym_to_section(dynsym);

      auto sz = m_elf_impl->get_ctrlpkt_pm_buf_size(sec_name);
      if (sz == 0)
        continue;

      auto& bo = m_ctrlpkt_pm_bos[sec_name] =
        xbi::create_bo(m_hwctx, sz, xbi::use_type::ctrlpkt);
      m_elf_impl->copy_ctrlpkt_pm_buf(sec_name, {bo.map<std::byte*>(), sz});
      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }
  }

  void
  create_instruction_buf()
  {
    XRT_DEBUGF("-> module_run_aie_gen2::create_instruction_buf()\n");

    // Step 1: allocate instruction BO and fill it with .ctrltext section data.
    // All subsequent steps patch addresses into this buffer before it is synced.
    auto sz = m_elf_impl->get_instr_buf_size(m_ctrl_code_id);
    if (sz == 0)
      throw std::runtime_error("Invalid instruction buffer size");

    m_instr_bo = xbi::create_bo(m_hwctx, sz, xbi::use_type::instruction);
    m_elf_impl->copy_instr_buf(m_ctrl_code_id, {m_instr_bo.map<std::byte*>(), sz});

    if (is_dump_control_codes()) {
      std::string dump_file_name = "ctr_codes_pre_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_instr_bo, dump_file_name);
      std::stringstream ss;
      ss << "dumped file " << dump_file_name << " ctr_codes size: " << sz;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }

    // Step 2: if the ELF has preemption sections, create save/restore BOs
    // and patch the shared scratchpad memory address into both of them.
    // The scratchpad is a hw_context-owned buffer used to spill AIE state.
    auto preempt_save_sz    = m_elf_impl->get_preempt_save_size(m_ctrl_code_id);
    auto preempt_restore_sz = m_elf_impl->get_preempt_restore_size(m_ctrl_code_id);

    if (preempt_save_sz > 0 && preempt_restore_sz > 0) {
      m_preempt_save_bo = xbi::create_bo(m_hwctx, preempt_save_sz, xbi::use_type::preemption);
      m_elf_impl->copy_preempt_save(m_ctrl_code_id, {m_preempt_save_bo.map<std::byte*>(), preempt_save_sz});

      m_preempt_restore_bo = xbi::create_bo(m_hwctx, preempt_restore_sz, xbi::use_type::preemption);
      m_elf_impl->copy_preempt_restore(m_ctrl_code_id, {m_preempt_restore_bo.map<std::byte*>(), preempt_restore_sz});

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

      // Patch scratchpad address into both preemption buffers so the firmware
      // knows where to save/restore AIE register state during preemption.
      const auto& scratchpad_mem =
        xrt_core::hw_context_int::get_scratchpad_mem_buf(m_hwctx, scratch_pad_mem_size);
      if (!scratchpad_mem)
        throw std::runtime_error("Failed to get scratchpad buffer from context\n");

      patch_helper(m_preempt_save_bo, scratchpad_mem.address(),
                   xrt_core::elf_patcher::buf_type::preempt_save, Scratch_Pad_Mem_Symbol, {}, false);
      patch_helper(m_preempt_restore_bo, scratchpad_mem.address(),
                   xrt_core::elf_patcher::buf_type::preempt_restore, Scratch_Pad_Mem_Symbol, {}, false);

      if (is_dump_preemption_codes()) {
        std::stringstream ss;
        ss << "patched preemption-codes using scratch_pad_mem at address "
           << std::hex << scratchpad_mem.address()
           << " size " << std::hex << scratchpad_mem.size();
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }

    // Step 3: if the ELF references a control scratch-pad symbol, allocate a
    // dedicated BO for it and patch its address into the instruction buffer.
    // This is distinct from the preemption scratchpad — it is used by the
    // firmware for internal control-code bookkeeping, not AIE register spill.
    auto ctrl_scratch_sz = m_elf_impl->get_ctrl_scratch_pad_mem_size();
    if (ctrl_scratch_sz > 0) {
      m_ctrl_scratch_pad_mem = xbi::create_bo(m_hwctx, ctrl_scratch_sz, xbi::use_type::ctrl_scratch_pad);
      patch_helper(m_instr_bo, m_ctrl_scratch_pad_mem.address(),
                   xrt_core::elf_patcher::buf_type::ctrltext, Control_ScratchPad_Symbol, {}, false);
    }

    // Step 4: allocate a PDI BO for each PDI symbol referenced by this
    // ctrl-code and patch its device address into the instruction buffer.
    // get_pdi_symbols() is an O(1) lookup into the pre-computed
    // m_ctrl_pdi_map — no iteration over all groups or all patch points.
    for (const auto& symbol : m_elf_impl->get_pdi_symbols(m_ctrl_code_id)) {
      // Multiple relocations can reference the same PDI symbol; only
      // create the BO once — patch_helper handles all patch sites.
      if (m_pdi_bo_map.count(symbol))
        continue;

      auto pdi_sz = m_elf_impl->get_pdi_size(symbol);
      auto pdi_bo = xbi::create_bo(m_hwctx, pdi_sz, xbi::use_type::pdi);
      m_elf_impl->copy_pdi(symbol, {pdi_bo.map<std::byte*>(), pdi_sz});
      pdi_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
      auto [it, inserted] = m_pdi_bo_map.emplace(symbol, std::move(pdi_bo));
      patch_helper(m_instr_bo, it->second.address(),
                   xrt_core::elf_patcher::buf_type::ctrltext, symbol, {}, false);
    }

    // Step 5: if a control-packet BO was provided by the caller, patch its
    // device address into the instruction buffer so the firmware can locate it.
    if (m_ctrlpkt_bo)
      patch_helper(m_instr_bo, m_ctrlpkt_bo.address(),
                   xrt_core::elf_patcher::buf_type::ctrltext, Control_Packet_Symbol, {}, false);

    // Step 6: patch the address of each preemption ctrl-pkt BO into the
    // instruction buffer.  These BOs were created in create_ctrlpkt_pm_bufs().
    for (const auto& dynsym : m_elf_impl->get_ctrlpkt_pm_dynsyms()) {
      std::string sec_name = ctrlpkt_pm_dynsym_to_section(dynsym);
      auto bo_itr = m_ctrlpkt_pm_bos.find(sec_name);
      if (bo_itr == m_ctrlpkt_pm_bos.end())
        throw std::runtime_error("Unable to find ctrlpkt pm buffer for symbol " + dynsym);

      patch_helper(m_instr_bo, bo_itr->second.address(),
                   xrt_core::elf_patcher::buf_type::ctrltext, dynsym, {}, false);
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
  {
    XRT_TRACE_POINT_SCOPE(xrt_module_run_aie_gen2);
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
    // patch control-packet buffer
    if (m_ctrlpkt_bo) {
      auto type = xrt_core::elf_patcher::buf_type::ctrldata;
      if (patch_helper(m_ctrlpkt_bo, value, type, argnm, std::to_string(index)))
        m_patched_args.insert(
            xrt_core::elf_patcher::generate_key_string(argnm, type));
    }

    // patch instruction buffer
    if (m_instr_bo) {
      auto type = xrt_core::elf_patcher::buf_type::ctrltext;
      if (patch_helper(m_instr_bo, value, type, argnm, std::to_string(index)))
        m_patched_args.insert(
            xrt_core::elf_patcher::generate_key_string(argnm, type));
    }
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
  // Combined instruction buffer — all column ctrlcode buffers concatenated
  // in column order, with page-aligned padding between them.  Patched in
  // place and synced to device before each run.
  xrt::bo m_buffer;

  // Control-packet buffers, keyed by section name (.ctrlpkt.*).
  // Each section gets its own BO; addresses are patched into m_buffer.
  std::map<std::string, xrt::bo> m_ctrlpkt_bos;

  // Per-column submission metadata packed into the ERT DPU payload.
  // Each entry holds: uC index, base address in m_buffer, size, dtrace address.
  enum column_bo_index : size_t { col_ucidx = 0, col_base_addr, col_size, col_dtrace_addr };
  std::vector<std::tuple<uint16_t, uint64_t, uint64_t, uint64_t>> m_column_bo_address;

  // Dynamic symbol name prefix used to patch per-column ctrlcode addresses
  // into the instruction buffer.
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
                uint32_t log_level, uint32_t output_fmt)
      : dtrace_handle(create_dtrace_handle(ctrl_file_path, map_data, log_level, output_fmt),
                      destroy_dtrace_handle) {}
  };
  dtrace_util m_dtrace;

  // Check dtrace coalesce buffer result is enabled based on both 
  // buffer results and json output format config
  bool
  is_dtrace_buffer_result() const
  {
    return xrt_core::config::get_dtrace_coalesce_result()
      && xrt_core::config::get_dtrace_output_json_format();
  }

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

    // Get dump buffer data from aiebu::elf.
    // Dump map is required only for jprobe; pass empty string when ELF has no .dump section.
    auto dump_sz = m_elf_impl->get_dump_buf_size(m_ctrl_code_id);
    std::string map_data;
    if (dump_sz > 0) {
      map_data.resize(dump_sz);
      m_elf_impl->copy_dump_buf(m_ctrl_code_id,
        {reinterpret_cast<std::byte*>(map_data.data()), dump_sz});
    }

    // log level 0: error, 1: warning, 2: info
    auto log_level = static_cast<uint32_t>(xrt_core::config::get_dtrace_log_level());
    log_level = (log_level > 2) ? 2U : log_level;

    // output format 0: python, 1: json
    uint32_t output_fmt = xrt_core::config::get_dtrace_output_json_format() ? 1U : 0U;

    m_dtrace = dtrace_util(path, map_data, log_level, output_fmt);
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
      m_dtrace = dtrace_util{};  // destroy handle; no usable dtrace without buffers
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
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              std::string{"[dtrace] : dtrace buffers initialization failed, "} + e.what());
      m_dtrace = dtrace_util{};  // destroy handle and clear partial state
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

  // Scratch pad size for gen2plus: 3MB for aie4 family, 0 otherwise
  size_t
  scratch_pad_mem_size() const
  {
    auto p = m_elf_impl->get_platform();
    return (p == xrt::elf::platform::aie4  ||
            p == xrt::elf::platform::aie4a ||
            p == xrt::elf::platform::aie4z)
      ? 3UL * 1024 * 1024  // NOLINT
      : 0;
  }

  // Allocate and fill one BO per .ctrlpkt section.  The addresses of these
  // BOs are later patched into the instruction buffer by fill_instruction_buffer().
  // for_each_ctrlpkt() performs a single outer map lookup and iterates the inner
  // map directly — no intermediate vector<string> is allocated.
  void
  create_ctrlpkt_bufs()
  {
    m_elf_impl->for_each_ctrlpkt(m_ctrl_code_id, [&](const std::string& name, size_t sz) {
      if (sz == 0)
        return;

      auto& bo = m_ctrlpkt_bos[name] = xbi::create_bo(m_hwctx, sz, xbi::use_type::ctrlpkt);
      m_elf_impl->copy_ctrlpkt(m_ctrl_code_id, name, {bo.map<std::byte*>(), sz});
      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    });

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

  // Fill m_buffer with column ctrl-code data and apply all static patches:
  //  1. Copy column ctrl-codes into m_buffer.
  //  2. Patch per-column device addresses ("control-code-<n>") into m_buffer.
  //  3. Obtain the shared scratchpad BO (aie4 family only).
  //  4. Patch scratchpad and ctrlpkt BO addresses into m_buffer and ctrlpkt BOs.
  void
  fill_instruction_buffer()
  {
    // Step 1: copy column ctrl-codes into m_buffer consecutively.
    auto ncols = m_elf_impl->get_column_count(m_ctrl_code_id);
    auto ptr   = m_buffer.map<std::byte*>();
    for (uint32_t col = 0; col < ncols; ++col) {
      auto sz = m_elf_impl->get_ctrlcode_col_size(m_ctrl_code_id, col);
      m_elf_impl->copy_ctrlcode_col(m_ctrl_code_id, col, {ptr, sz});
      ptr += sz;
    }

    if (is_dump_control_codes()) {
      std::string dump_file_name = "ctr_codes_pre_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_buffer, dump_file_name);
      std::stringstream ss;
      ss << "dumped file " << dump_file_name << " ctr_codes size: " << m_buffer.size();
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }

    // Step 2: patch per-column device addresses ("control-code-0", "control-code-1", ...)
    // into m_buffer so the firmware can locate each column's instruction stream.
    size_t offset = 0;
    for (uint32_t i = 0; i < ncols; ++i) {
      auto sym_name = std::string(Control_Code_Symbol) + "-" + std::to_string(i);
      auto type = xrt_core::elf_patcher::buf_type::ctrltext;
      if (patch_helper(m_buffer, m_buffer.address() + offset, type, sym_name, {}, false))
        m_patched_args.insert(xrt_core::elf_patcher::generate_key_string(sym_name, type));

      offset += m_elf_impl->get_ctrlcode_col_size(m_ctrl_code_id, i);
    }

    // Step 3: obtain the shared scratchpad BO (aie4 family only; null otherwise).
    xrt::bo scratchpad_mem;
    auto sp_sz = scratch_pad_mem_size();
    if (sp_sz > 0) {
      scratchpad_mem = xrt_core::hw_context_int::get_scratchpad_mem_buf(m_hwctx, sp_sz);
      if (!scratchpad_mem)
        throw std::runtime_error("Failed to get scratchpad buffer from context\n");
    }

    // Step 4: for each ctrlpkt BO, patch the scratchpad address into the
    // ctrlpkt buffer (if present), then patch the ctrlpkt BO's own device
    // address into m_buffer so the firmware can find the control packets.
    auto type = xrt_core::elf_patcher::buf_type::buf_type_count;
    for (auto& [name, ctrlpktbo] : m_ctrlpkt_bos) {
      auto sym_name = xrt_core::elf_patcher::get_symbol_name_from_section_name(name);

      if (scratchpad_mem) {
        type = xrt_core::elf_patcher::buf_type::ctrlpkt;
        auto symbol = std::string{Scratch_Pad_Mem_Symbol} + sym_name;
        if (patch_helper(ctrlpktbo, scratchpad_mem.address(), type, symbol, {}, false))
          m_patched_args.insert(xrt_core::elf_patcher::generate_key_string(symbol, type));
      }

      type = xrt_core::elf_patcher::buf_type::ctrltext;
      if (patch_helper(m_buffer, ctrlpktbo.address(), type, sym_name, {}, false))
        m_patched_args.insert(xrt_core::elf_patcher::generate_key_string(sym_name, type));
    }

    // Patch the scratchpad address into m_buffer itself (ctrltext side).
    if (scratchpad_mem) {
      type = xrt_core::elf_patcher::buf_type::ctrltext;
      if (patch_helper(m_buffer, scratchpad_mem.address(), type, Scratch_Pad_Mem_Symbol, {}, false))
        m_patched_args.insert(xrt_core::elf_patcher::generate_key_string(Scratch_Pad_Mem_Symbol, type));
    }
  }

  void
  create_instruction_buffer()
  {
    // Compute the combined size of all column ctrl-codes so that a single
    // contiguous BO can hold them back-to-back, with each column's data
    // at a known offset from the BO base address.
    auto ncols = m_elf_impl->get_column_count(m_ctrl_code_id);
    size_t sz = 0;
    for (uint32_t col = 0; col < ncols; ++col)
      sz += m_elf_impl->get_ctrlcode_col_size(m_ctrl_code_id, col);

    if (sz == 0) {
      XRT_DEBUGF("ctrlcode buf is empty\n");
      return;
    }

    m_buffer = xbi::create_bo(m_hwctx, sz, xbi::use_type::instruction);
    fill_instruction_buffer();
  }

  // Build m_column_bo_address — the per-column metadata written into the ERT
  // DPU payload at submission time.  Each entry records the uC (micro-controller)
  // index, the device address of that column's ctrl-code within m_buffer, the
  // column size, and the optional dtrace buffer address for dynamic tracing.
  //
  // Columns with zero size are sparse holes in the ctrl-code layout and are
  // skipped — they require no submission entry but still advance the base address.
  void
  fill_column_bo_address()
  {
    auto ncols = m_elf_impl->get_column_count(m_ctrl_code_id);
    m_column_bo_address.clear();
    uint16_t ucidx = 0;
    auto base_addr = m_buffer.address();

    for (uint32_t col = 0; col < ncols; ++col) {
      if (auto size = m_elf_impl->get_ctrlcode_col_size(m_ctrl_code_id, col)) {
        // Look up the dtrace buffer offset for this uC index, if dtrace is active.
        uint64_t dtrace_addr = 0;
        if (m_dtrace.ctrl_bo) {
          auto it = m_dtrace.buf_offset_map.find(ucidx);
          if (it != m_dtrace.buf_offset_map.end())
            dtrace_addr = m_dtrace.ctrl_bo.address() + it->second;
        }
        m_column_bo_address.emplace_back(ucidx, base_addr, size, dtrace_addr);
      }
      ++ucidx;
      base_addr += m_elf_impl->get_ctrlcode_col_size(m_ctrl_code_id, col);
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
  {
    XRT_TRACE_POINT_SCOPE(xrt_module_run_aie_gen2_plus);
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
    auto type = xrt_core::elf_patcher::buf_type::ctrltext;
    // patch instruction buffer
    if (patch_helper(m_buffer, value, type, argnm, std::to_string(index)))
      m_patched_args.insert(
          xrt_core::elf_patcher::generate_key_string(argnm, type));

    // patch argument in pad section
    type = xrt_core::elf_patcher::buf_type::pad;
    if (patch_helper(m_buffer, value, type, argnm, std::to_string(index)))
      m_patched_args.insert(
          xrt_core::elf_patcher::generate_key_string(argnm, type));

    // New Elfs have multiple ctrlpkt sections
    // Iterate over all ctrlpkt buffers and patch them
    type = xrt_core::elf_patcher::buf_type::ctrlpkt;
    for (auto& [name, ctrlpktbo] : m_ctrlpkt_bos) {
      auto sym_name =
          xrt_core::elf_patcher::get_symbol_name_from_section_name(name);

      if (patch_helper(ctrlpktbo, value, type, argnm + sym_name,
                       std::to_string(index) + sym_name))
        m_patched_args.insert(
            xrt_core::elf_patcher::generate_key_string(argnm, type));
    }
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

  bool
  is_dtrace_enabled() const override
  {
    return m_dtrace.dtrace_handle && m_dtrace.ctrl_bo;
  }

  // Dump dynamic trace buffer
  void
  dump_dtrace_buffer(const std::string& postfix) override
  {
    // sync dtrace buffers output from device
    m_dtrace.ctrl_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    try {
      if (is_dtrace_buffer_result()) {
        // dtrace output is serialized and buffered into result buffer
        std::string result_key = "dtrace_dump" + postfix;

        // Get result as JSON string from aiebu
        std::string result_json = get_dtrace_result_buffer(m_dtrace.dtrace_handle.get());
        if (result_json == "null") {
          xrt_core::message::send(xrt_core::message::severity_level::warning, "xrt_module",
                                  "[dtrace] : failed to get dtrace result buffer");
          return;
        }

        // Append JSON string to hwctx buffer
        xrt_core::hw_context_int::append_dtrace_result(m_hwctx, result_key, result_json);
      }
      else {
        // dtrace output is dumped into current working directory
        // output is a python/json file
        std::string result_file_name = "dtrace_dump"
                                    + postfix
                                    + (xrt_core::config::get_dtrace_output_json_format() ? ".json" : ".py");

        get_dtrace_result_file(m_dtrace.dtrace_handle.get(), result_file_name);

        // Check if file exists/created/probes fired and log the message
        const auto result_file_path = std::filesystem::current_path() / result_file_name;
        if (std::filesystem::exists(result_file_path))
          xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                                  std::string{"[dtrace] : dtrace buffer dumped successfully to - "}
                                  + result_file_path.string());
      }
    }
    catch (const std::exception& e) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              std::string{"[dtrace] : dtrace buffer dump failed, "} + e.what());
    }
  }
};

module::
module(const xrt::elf& elf)
: detail::pimpl<module_impl>(std::make_shared<module_impl>(elf))
{}

module::
module(const xrt::elf& elf, const std::string& name)
: detail::pimpl<module_impl>(std::make_shared<module_impl>(elf, name))
{}

xrt::hw_context
module::
get_hw_context() const
{
  // No null check at API level, application error if null
  return get_handle()->get_hw_context();
}

} // namespace xrt

namespace {

static void
valid_or_error(const xrt::module& module)
{
  if (!module)
    throw std::runtime_error("xrt::module object is not initialized");
}

} // namespace

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
  case xrt::elf::platform::aie2ps_legacy:
  case xrt::elf::platform::aie4:
  case xrt::elf::platform::aie4a:
  case xrt::elf::platform::aie4z:
    return xrt::module{std::make_shared<xrt::module_run_aie_gen2_plus>(elf, hwctx, ctrl_code_id)};
  default:
    throw std::runtime_error("Unsupported platform");
  }
}

std::string
get_name(const xrt::module& module)
{
  valid_or_error(module);
  return module.get_handle()->get_name();
}

std::shared_ptr<xrt::elf_impl>
get_elf_handle(const xrt::module& module)
{
  valid_or_error(module);
  return module.get_handle()->get_elf_handle();
}

uint32_t*
fill_ert_dpu_data(const xrt::module& module, uint32_t* payload)
{
  valid_or_error(module);
  return module.get_handle()->fill_ert_dpu_data(payload);
}

void
patch(const xrt::module& module, const std::string& argnm, size_t index, const xrt::bo& bo)
{
  valid_or_error(module);
  module.get_handle()->patch(argnm, index, bo.address());
}

void
patch(const xrt::module& module, const std::string& argnm, size_t index, const void* value, size_t size)
{
  if (size > 8) // NOLINT
  throw std::runtime_error{ "Patching scalar values only supports 64-bit values or less" };

  valid_or_error(module);
  auto arg_value = *static_cast<const uint64_t*>(value);
  module.get_handle()->patch(argnm, index, arg_value);
}

void
sync(const xrt::module& module)
{
  valid_or_error(module);
  module.get_handle()->sync_if_dirty();
}

bool
is_dtrace_enabled(const xrt::module& module)
{
  valid_or_error(module);
  return module.get_handle()->is_dtrace_enabled();
}

void
dump_dtrace_buffer(const xrt::module& module, const std::string& postfix)
{
  if (!is_dtrace_enabled(module))
    return;
  module.get_handle()->dump_dtrace_buffer(postfix);
}

void
set_dtrace_control_file(const xrt::module& module, const std::string& path)
{
  valid_or_error(module);
  module.get_handle()->set_dtrace_control_file(path);
}

xrt::bo
get_ctrl_scratchpad_bo(const xrt::module& module)
{
  valid_or_error(module);
  return module.get_handle()->get_ctrl_scratchpad_bo();
}

size_t
get_patch_buf_size(const xrt::module& module, xrt_core::elf_patcher::buf_type type,
                   uint32_t ctrl_code_id)
{
  valid_or_error(module);
  auto elf_hdl = module.get_handle()->get_elf_handle();
  auto platform = elf_hdl->get_platform();

  if (platform == xrt::elf::platform::aie2p) {
    switch (type) {
    case xrt_core::elf_patcher::buf_type::ctrltext:
      return elf_hdl->get_instr_buf_size(ctrl_code_id);
    case xrt_core::elf_patcher::buf_type::ctrldata:
      return elf_hdl->get_ctrl_packet_size(ctrl_code_id);
    case xrt_core::elf_patcher::buf_type::preempt_save:
      return elf_hdl->get_preempt_save_size(ctrl_code_id);
    case xrt_core::elf_patcher::buf_type::preempt_restore:
      return elf_hdl->get_preempt_restore_size(ctrl_code_id);
    default:
      throw std::runtime_error("Unknown buffer type passed");
    }
  }

  // gen2plus: only ctrltext (single column) supported
  if (type != xrt_core::elf_patcher::buf_type::ctrltext)
    throw std::runtime_error("Info of given buffer type not available");

  auto ncols = elf_hdl->get_column_count(ctrl_code_id);
  if (ncols == 0)
    throw std::runtime_error{"No control code found for given id"};
  if (ncols != 1)
    throw std::runtime_error{"Patch failed: only support patching single column"};

  return elf_hdl->get_ctrlcode_col_size(ctrl_code_id, 0);
}

// Copy the requested ELF buffer (identified by ctrl_code_id and buf_type)
// into ibuf, then apply any caller-supplied argument patches in place.
// Called from shim-level tests (xdna-driver/test/shim_test/exec_buf.h) that
// manage their own buffer lifecycle and device sync.
// The caller is responsible for buffer sizing (use get_patch_buf_size())
// and sync.  Normal execution paths use module_run::patch() instead.
void
patch(const xrt::module& module, uint8_t* ibuf, size_t sz,
      const std::vector<std::pair<std::string, uint64_t>>* args,
      xrt_core::elf_patcher::buf_type type, uint32_t ctrl_code_id)
{
  valid_or_error(module);
  auto elf_hdl = module.get_handle()->get_elf_handle();
  auto platform = elf_hdl->get_platform();

  // Copy the requested ELF section into ibuf, validating the buffer is large enough.
  size_t buf_sz = 0;
  if (platform == xrt::elf::platform::aie2p) {
    switch (type) {
    case xrt_core::elf_patcher::buf_type::ctrltext:
      buf_sz = elf_hdl->get_instr_buf_size(ctrl_code_id);
      if (sz < buf_sz)
        throw std::runtime_error{"Control code buffer passed in is too small"};

      elf_hdl->copy_instr_buf(ctrl_code_id, {reinterpret_cast<std::byte*>(ibuf), buf_sz});
      break;
    case xrt_core::elf_patcher::buf_type::ctrldata:
      buf_sz = elf_hdl->get_ctrl_packet_size(ctrl_code_id);
      if (sz < buf_sz)
        throw std::runtime_error{"Control code buffer passed in is too small"};

      elf_hdl->copy_ctrl_packet(ctrl_code_id, {reinterpret_cast<std::byte*>(ibuf), buf_sz});
      break;
    case xrt_core::elf_patcher::buf_type::preempt_save:
      buf_sz = elf_hdl->get_preempt_save_size(ctrl_code_id);
      if (sz < buf_sz)
        throw std::runtime_error{"Control code buffer passed in is too small"};

      elf_hdl->copy_preempt_save(ctrl_code_id, {reinterpret_cast<std::byte*>(ibuf), buf_sz});
      break;
    case xrt_core::elf_patcher::buf_type::preempt_restore:
      buf_sz = elf_hdl->get_preempt_restore_size(ctrl_code_id);
      if (sz < buf_sz)
        throw std::runtime_error{"Control code buffer passed in is too small"};

      elf_hdl->copy_preempt_restore(ctrl_code_id, {reinterpret_cast<std::byte*>(ibuf), buf_sz});
      break;
    default:
      throw std::runtime_error("Unknown buffer type passed");
    }
  }
  else {
    // gen2plus supports patching ctrltext only, and only for single-column ELFs.
    if (type != xrt_core::elf_patcher::buf_type::ctrltext)
      throw std::runtime_error{"Patch failed: unsupported buffer type for gen2plus"};

    auto ncols = elf_hdl->get_column_count(ctrl_code_id);
    if (ncols == 0)
      throw std::runtime_error{"No control code found for given id"};

    if (ncols != 1)
      throw std::runtime_error{"Patch failed: only support patching single column"};

    buf_sz = elf_hdl->get_ctrlcode_col_size(ctrl_code_id, 0);
    if (sz < buf_sz)
      throw std::runtime_error{"Control code buffer passed in is too small"};

    elf_hdl->copy_ctrlcode_col(ctrl_code_id, 0, {reinterpret_cast<std::byte*>(ibuf), buf_sz});
  }

  // If no args to patch, we're done — ibuf holds the raw ELF section data.
  if (!args || args->empty())
    return;

  // Get the patcher configs from the module — these encode the relocation
  // sites within the buffer for each argument.
  const auto* patcher_configs = elf_hdl->get_patcher_configs(ctrl_code_id);
  if (!patcher_configs)
    throw std::runtime_error{"No patcher configs found for given id"};

  size_t index = 0;
  for (const auto& [arg_name, arg_addr] : *args) {
    // Look up by argument name first; fall back to positional index for
    // callers that don't know the symbol names.
    auto key_string = xrt_core::elf_patcher::generate_key_string(arg_name, type);
    auto it = patcher_configs->find(key_string);
    if (it == patcher_configs->end()) {
      // Try with index
      auto index_key = xrt_core::elf_patcher::generate_key_string(std::to_string(index), type);
      it = patcher_configs->find(index_key);
      if (it == patcher_configs->end())
        throw std::runtime_error{"Failed to patch " + arg_name};

    }
    // Use the static patch method — no per-run state needed here since the
    // caller owns ibuf and handles sync themselves.
    xrt_core::elf_patcher::symbol_patcher::patch_symbol_raw(ibuf, sz, arg_addr, it->second);
    index++;
  }
}

} // namespace xrt_core::module_int
