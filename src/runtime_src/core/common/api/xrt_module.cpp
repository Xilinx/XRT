// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_module.h
#define XRT_API_SOURCE         // exporting xrt_module.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "xrt/experimental/xrt_module.h"
#include "xrt/experimental/xrt_aie.h"
#include "xrt/experimental/xrt_elf.h"
#include "xrt/experimental/xrt_ext.h"

#include "xrt/xrt_bo.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_uuid.h"

#include "xrt/detail/ert.h"

#include "bo_int.h"
#include "elf_int.h"
#include "module_int.h"
#include "core/common/debug.h"
#include "core/common/dlfcn.h"

#include <boost/format.hpp>
#include <elfio/elfio.hpp>

#include <algorithm>
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

#ifndef AIE_COLUMN_PAGE_SIZE
# define AIE_COLUMN_PAGE_SIZE 8192  // NOLINT
#endif

namespace
{

// Control code is padded to page size, where page size is
// 0 if no padding is required.   The page size should be
// embedded as ELF metadata in the future.
static constexpr size_t elf_page_size = AIE_COLUMN_PAGE_SIZE;
static constexpr uint8_t Elf_Amd_Aie2p        = 69;
static constexpr uint8_t Elf_Amd_Aie2ps       = 64;
static constexpr uint8_t Elf_Amd_Aie2ps_group = 70;

// In aie2p max bd data words is 8 and in aie4/aie2ps its 9
// using max bd words as 9 to cover all cases
static constexpr size_t max_bd_words = 9;

static const char* const Scratch_Pad_Mem_Symbol = "scratch-pad-mem";
static const char* const Control_ScratchPad_Symbol = "scratch-pad-ctrl";
static const char* const Control_Packet_Symbol = "control-packet";
static const char* const Control_Code_Symbol = "control-code";

struct buf
{
  std::vector<uint8_t> m_data;

  void
  append_section_data(const ELFIO::section* sec)
  {
    auto sz = sec->get_size();
    auto sdata = sec->get_data();
    m_data.insert(m_data.end(), sdata, sdata + sz);
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

  void
  append_section_data(const uint8_t* userptr, size_t sz)
  {
    m_data.insert(m_data.end(), userptr, userptr + sz);
  }

  void
  pad_to_page(uint32_t page)
  {
    if (!elf_page_size)
      return;

    auto pad = (page + 1) * elf_page_size;

    if (m_data.size() > pad)
      throw std::runtime_error("Invalid ELF section size");

    m_data.resize(pad);
  }

  static const buf&
  get_empty_buf()
  {
    static const buf b = {};
    return b;
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
    shim_dma_base_addr_symbol_kind = 2,      // patching scheme needed by AIE2PS firmware
    scalar_32bit_kind = 3,
    control_packet_48 = 4,                   // patching scheme needed by firmware to patch control packet
    shim_dma_48 = 5,                         // patching scheme needed by firmware to patch instruction buffer
    shim_dma_aie4_base_addr_symbol_kind = 6, // patching scheme needed by AIE4 firmware
    control_packet_57 = 7,                   // patching scheme needed by firmware to patch control packet for aie2ps
    address_64 = 8,                          // patching scheme needed to patch pdi address
    unknown_symbol_kind = 9
  };

  xrt_core::patcher::buf_type m_buf_type = xrt_core::patcher::buf_type::ctrltext;
  symbol_type m_symbol_type = symbol_type::shim_dma_48;

  struct patch_info {
    uint64_t offset_to_patch_buffer;
    uint32_t offset_to_base_bo_addr;
    uint32_t mask; // This field is valid only when patching scheme is scalar_32bit_kind
    bool dirty = false; // Tells whether this entry is already patched or not
    uint32_t bd_data_ptrs[max_bd_words]; // array to store bd ptrs original values
  };

  std::vector<patch_info> m_ctrlcode_patchinfo;

  inline static const std::string_view
  to_string(xrt_core::patcher::buf_type bt)
  {
    static constexpr std::array<std::string_view, static_cast<int>(xrt_core::patcher::buf_type::buf_type_count)> section_name_array = {
      ".ctrltext",
      ".ctrldata",
      ".preempt_save",
      ".preempt_restore",
      ".pdi",
      ".ctrlpkt.pm",
      ".pad",
      ".dump"
    };

    return section_name_array[static_cast<int>(bt)];
  }

  patcher(symbol_type type, std::vector<patch_info> ctrlcode_offset, xrt_core::patcher::buf_type t)
    : m_buf_type(t)
    , m_symbol_type(type)
    , m_ctrlcode_patchinfo(std::move(ctrlcode_offset))
  {}

private:
  void
  patch64(uint32_t* data_to_patch, uint64_t addr) const
  {
    *data_to_patch = static_cast<uint32_t>(addr & 0xffffffff);
    *(data_to_patch + 1) = static_cast<uint32_t>((addr >> 32) & 0xffffffff);
  }
  
  // Replace certain bits of *data_to_patch with register_value. Which bits to be replaced is specified by mask
  // For     *data_to_patch be 0xbb11aaaa and mask be 0x00ff0000
  // To make *data_to_patch be 0xbb55aaaa, register_value must be 0x00550000
  void
  patch32(uint32_t* data_to_patch, uint64_t register_value, uint32_t mask) const
  {
    if ((reinterpret_cast<uintptr_t>(data_to_patch) & 0x3) != 0)
      throw std::runtime_error("address is not 4 byte aligned for patch32");

    auto new_value = *data_to_patch;
    new_value = (new_value & ~mask) | (register_value & mask);
    *data_to_patch = new_value;
  }

  void
  patch57(uint32_t* bd_data_ptr, uint64_t patch) const
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
  patch57_aie4(uint32_t* bd_data_ptr, uint64_t patch) const
  {
    constexpr uint64_t ddr_aie_addr_offset = 0x80000000;

    uint64_t base_address =
      ((static_cast<uint64_t>(bd_data_ptr[0]) & 0x1FFFFFF) << 32) |                   // NOLINT
      bd_data_ptr[1];

    base_address += patch + ddr_aie_addr_offset;  //2G offset
    bd_data_ptr[1] = (uint32_t)(base_address & 0xFFFFFFFF);                           // NOLINT
    bd_data_ptr[0] = (bd_data_ptr[0] & 0xFE000000) | ((base_address >> 32) & 0x1FFFFFF);// NOLINT
  }

  void
  patch_ctrl57(uint32_t* bd_data_ptr, uint64_t patch) const
  {
    //TODO need to change below logic to patch 57 bits
    uint64_t base_address =
      ((static_cast<uint64_t>(bd_data_ptr[3]) & 0xFFF) << 32) |                       // NOLINT
      ((static_cast<uint64_t>(bd_data_ptr[2])));

    base_address = base_address + patch;
    bd_data_ptr[2] = (uint32_t)(base_address & 0xFFFFFFFC);                           // NOLINT
    bd_data_ptr[3] = (bd_data_ptr[3] & 0xFFFF0000) | (base_address >> 32);            // NOLINT
  }

  void
  patch_ctrl48(uint32_t* bd_data_ptr, uint64_t patch) const
  {
    // This patching scheme is originated from NPU firmware
    constexpr uint64_t ddr_aie_addr_offset = 0x80000000;

    uint64_t base_address =
      ((static_cast<uint64_t>(bd_data_ptr[3]) & 0xFFF) << 32) |                       // NOLINT
      ((static_cast<uint64_t>(bd_data_ptr[2])));

    base_address = base_address + patch + ddr_aie_addr_offset;
    bd_data_ptr[2] = (uint32_t)(base_address & 0xFFFFFFFC);                           // NOLINT
    bd_data_ptr[3] = (bd_data_ptr[3] & 0xFFFF0000) | (base_address >> 32);            // NOLINT
  }

  void
  patch_shim48(uint32_t* bd_data_ptr, uint64_t patch) const
  {
    // This patching scheme is originated from NPU firmware
    constexpr uint64_t ddr_aie_addr_offset = 0x80000000;

    uint64_t base_address =
      ((static_cast<uint64_t>(bd_data_ptr[2]) & 0xFFFF) << 32) |                      // NOLINT
      ((static_cast<uint64_t>(bd_data_ptr[1])));

    base_address = base_address + patch + ddr_aie_addr_offset;
    bd_data_ptr[1] = (uint32_t)(base_address & 0xFFFFFFFC);                           // NOLINT
    bd_data_ptr[2] = (bd_data_ptr[2] & 0xFFFF0000) | (base_address >> 32);            // NOLINT
  }

  template<typename T>
  void
  patch_it_impl(T base_or_bo, uint64_t new_value, bool first)
  {
    // base_or_bo is either a pointer to base address of buffer to be patched
    // or xrt::bo object itself
    // shim tests call this function with address directly and call sync themselves
    // but when xrt::bo is passed, we need to call sync explictly only if its not the
    // first time patching, as in first time patching entire bo is synced
    uint8_t* base = nullptr;

    if constexpr (std::is_same_v<T, xrt::bo>) {
      base = reinterpret_cast<uint8_t*>(base_or_bo.map());
    }
    else
      base = base_or_bo;

    for (auto& item : m_ctrlcode_patchinfo) {
      auto offset = item.offset_to_patch_buffer;
      auto bd_data_ptr = reinterpret_cast<uint32_t*>(base + offset);

      if (!item.dirty) {
        // first time patching cache bd ptr values using bd ptrs array in patch info
        std::copy(bd_data_ptr, bd_data_ptr + max_bd_words, item.bd_data_ptrs);
        item.dirty = true;
      }
      else {
        // not the first time patching, restore bd ptr values from patch info bd ptrs array
        std::copy(item.bd_data_ptrs, item.bd_data_ptrs + max_bd_words, bd_data_ptr);
      }

      // lambda for calling sync bo if template arg is of type xrt::bo
      // We only sync the words that are patched not the entire bo
      auto sync = [&](size_t size) {
        if constexpr (std::is_same_v<T, xrt::bo>) {
          base_or_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, size, offset);
        }
      };

      switch (m_symbol_type) {
      case symbol_type::address_64:
        // new_value is a 64bit address
        patch64(bd_data_ptr, new_value);
        if (!first) {
          // sync 64 bits patched
          sync(sizeof(uint64_t));
        }
        break;
      case symbol_type::scalar_32bit_kind:
        // new_value is a register value
        if (item.mask) {
          patch32(bd_data_ptr, new_value, item.mask);
          if (!first) {
            // sync 32 bits patched
            sync(sizeof(uint32_t));
          }
        }
        break;
      case symbol_type::shim_dma_base_addr_symbol_kind:
        // new_value is a bo address
        patch57(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // Data in this case is written to 8th offset of bd_data_ptr
          // so sync all the words (max_bd_words)
          sync(sizeof(uint32_t) * max_bd_words);
        }
        break;
      case symbol_type::shim_dma_aie4_base_addr_symbol_kind:
        // new_value is a bo address
        patch57_aie4(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // sync 64 bits or 2 words
          sync(sizeof(uint64_t));
        }
        break;
      case symbol_type::control_packet_57:
        // new_value is a bo address
        patch_ctrl57(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // Data in this case is written till 3rd offset of bd_data_ptr
          // so syncing 4 words
          sync(4 * sizeof(uint32_t));    // NOLINT
        }
        break;
      case symbol_type::control_packet_48:
        // new_value is a bo address
        patch_ctrl48(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // Data in this case is written till 3rd offset of bd_data_ptr
          // so syncing 4 words
          sync(4 * sizeof(uint32_t));    // NOLINT
        }
        break;
      case symbol_type::shim_dma_48:
        // new_value is a bo address
        patch_shim48(bd_data_ptr, new_value + item.offset_to_base_bo_addr);
        if (!first) {
          // Data in this case is written till 2nd offset of bd_data_ptr
          // so syncing 3 words
          sync(3 * sizeof(uint32_t));    // NOLINT
        }
        break;
      default:
        throw std::runtime_error("Unsupported symbol type");
      }
    }
  }

public:
  void
  patch_it(uint8_t* base, uint64_t value, bool)
  {
    // this function is used by internal shim level tests
    // which does explicit sync of buffers
    // so needn't do a partial sync
    patch_it_impl(base, value, true /*no partial sync*/);
  }

  void
  patch_it(xrt::bo bo, uint64_t value, bool first)
  {
    patch_it_impl(bo, value, first);
  }
};

XRT_CORE_UNUSED void
dump_bo(xrt::bo& bo, const std::string& filename)
{
  std::ofstream ofs(filename, std::ios::out | std::ios::binary);
  if (!ofs.is_open())
    throw std::runtime_error("Failure opening file " + filename + " for writing!");

  auto buf = bo.map<char*>();
  ofs.write(buf, static_cast<std::streamsize>(bo.size()));
}

XRT_CORE_UNUSED std::string
generate_key_string(const std::string& argument_name, xrt_core::patcher::buf_type type)
{
  std::string buf_string = std::to_string(static_cast<int>(type));
  return argument_name + buf_string;
}

static std::string
demangle(const std::string& mangled_name)
{
#ifdef _WIN32
  char demangled_name[1024];
  if (UnDecorateSymbolName(mangled_name.c_str(), demangled_name, sizeof(demangled_name), UNDNAME_COMPLETE))
    return std::string(demangled_name);
  else
    throw std::runtime_error("Error demangling kernel signature");
#else
  int status = 0;
  std::unique_ptr<char, decltype(&std::free)> demangled_name
    (abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status), std::free);

  if (status)
    throw std::runtime_error("Error demangling kernel signature");

  return demangled_name.get();
#endif
}

// checks if ELF has .group sections
static bool
is_group_elf(const ELFIO::elfio& elfio)
{
  constexpr uint8_t major_ver_mask = 0xF0;
  constexpr uint8_t minor_ver_mask = 0x0F;
  constexpr uint8_t shift = 4;

  auto abi_version = elfio.get_abi_version();
  auto os_abi = elfio.get_os_abi();

  if (os_abi == Elf_Amd_Aie2p) {
    // ELF of aie2p with version >= 1.0 uses group sections
    constexpr uint8_t group_elf_major_version = 1;
    if (((abi_version & major_ver_mask) >> shift) >= group_elf_major_version)
      return true;

    return false;
  }
  else {
    // ELF of aie2ps/aie4 with version >= 0.3 uses group sections
    constexpr uint8_t group_elf_minor_version = 3;
    if ((abi_version & minor_ver_mask) >= group_elf_minor_version)
      return true;
      
    return false;
  }
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

  xrt::uuid
  get_cfg_uuid() const
  {
    return m_cfg_uuid;
  }

  // Get raw instruction buffer data for all columns or for
  // single partition.  The returned vector has the control
  // code as extracted from ELF or userptr.
  virtual const std::vector<ctrlcode>&
  get_data(uint32_t /*id*/) const
  {
    throw std::runtime_error("Not supported");
  }

  virtual const instr_buf&
  get_instr(uint32_t /*id*/) const
  {
    throw std::runtime_error("Not supported");
  }

  virtual const buf&
  get_preempt_save(uint32_t /*id*/) const
  {
    throw std::runtime_error("Not supported");
  }

  virtual const buf&
  get_preempt_restore(uint32_t /*id*/) const
  {
    throw std::runtime_error("Not supported");
  }

  virtual const buf&
  get_pdi(const std::string& /*pdi_name*/) const
  {
    throw std::runtime_error("Not supported");
  }

  virtual xrt::bo&
  get_pdi_bo(const std::string& /*pdi_name*/)
  {
    throw std::runtime_error("Not supported");
  }

  virtual const std::unordered_set<std::string>&
  get_patch_pdis(uint32_t /*id*/) const
  {
    throw std::runtime_error("Not supported");
  }

  virtual size_t
  get_scratch_pad_mem_size() const
  {
    throw std::runtime_error("Not supported");
  }

  virtual size_t
  get_ctrl_scratch_pad_mem_size() const
  {
    throw std::runtime_error("Not supported");
  }

  virtual const control_packet&
  get_ctrlpkt(uint32_t /*id*/) const
  {
    throw std::runtime_error("Not supported");
  }

  virtual const std::set<std::string>&
  get_ctrlpkt_pm_dynsyms() const
  {
    throw std::runtime_error("Not supported");
  }

  virtual const std::map<std::string, buf>&
  get_ctrlpkt_pm_bufs() const
  {
    throw std::runtime_error("Not supported");
  }

  virtual xrt::bo&
  get_scratch_pad_mem()
  {
    throw std::runtime_error("Not supported");
  }

  virtual const buf&
  get_dump_buf(uint32_t /*id*/) const
  {
    throw std::runtime_error("Not supported");
  }

  virtual xrt::hw_context
  get_hw_context() const
  {
    return {};
  }

  // Fill in ERT command payload in ELF flow. The payload is after
  // extra_cu_mask and before CU arguments. Return the current point of
  // the ERT command payload
  virtual uint32_t*
  fill_ert_dpu_data(uint32_t *) const
  {
    throw std::runtime_error("Not supported");
  }

  virtual uint8_t
  get_os_abi() const
  {
    throw std::runtime_error("Not supported");
  }

  // Returns underlying elfio object held by this module
  virtual const ELFIO::elfio&
  get_elfio_object() const
  {
    throw std::runtime_error("Not supported");
  }

  // Patch ctrlcode buffer object for global argument
  //
  // @param argname - argument name
  // @param index - argument index
  // @param bo - global argument to patch into ctrlcode
  virtual void
  patch(const std::string&, size_t, const xrt::bo&)
  {
    throw std::runtime_error("Not supported");
  }

  // Patch ctrlcode buffer object for scalar argument
  //
  // @param symbol - symbol name
  // @param value - patch value
  // @param size - size of patch value
  virtual void
  patch(const std::string&, size_t, const void*, size_t)
  {
    throw std::runtime_error("Not supported");
  }

  // Patch symbol in control code with patch value
  //
  // @param base - base address of control code buffer object
  // @param symbol - symbol name
  // @param index - argument index
  // @param patch - patch value
  // @param buf_type - whether it is control-code, control-packet, preempt-save or preempt-restore
  // @param grp_index - grp index to identify the ctrlcode that is being patched
  // @param first - boolean indicating if its first time patching
  // @Return true if symbol was patched, false otherwise
  virtual bool
  patch_it(uint8_t*, const std::string&, size_t, uint64_t, xrt_core::patcher::buf_type,
           uint32_t, [[maybe_unused]] bool first = true)
  {
    throw std::runtime_error("Not supported");
  }

  // Patch symbol in control code with patch value
  //
  // @param bo - buffer to be patched
  // @param symbol - symbol name
  // @param index - argument index
  // @param patch - patch value
  // @param buf_type - whether it is control-code, control-packet, preempt-save or preempt-restore
  // @param grp_index - grp index to identify the ctrlcode that is being patched
  // @param first - boolean indicating if its first time patching
  // @Return true if symbol was patched, false otherwise
  virtual bool
  patch_it(xrt::bo, const std::string&, size_t, uint64_t, xrt_core::patcher::buf_type,
           uint32_t, bool)
  {
    throw std::runtime_error("Not supported");
  }

  // Get the number of patchers for arguments.  The returned
  // value is the number of arguments that must be patched before
  // the control code can be executed.
  virtual size_t
  number_of_arg_patchers(uint32_t /*id*/) const
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

  // Get the ERT command opcode in ELF flow.
  virtual ert_cmd_opcode
  get_ert_opcode() const
  {
    throw std::runtime_error("Not supported");
  }

  // get kernel info (name, properties and args) if elf has the info
  virtual const std::vector<xrt_core::module_int::kernel_info>&
  get_kernels_info() const
  {
    throw std::runtime_error("Not supported");
  }

  virtual uint32_t
  get_ctrlcode_id(const std::string& /*kname*/) const
  {
    throw std::runtime_error("Not supported");
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

  const std::vector<ctrlcode>&
  get_data(uint32_t /*id*/) const override
  {
    return m_ctrlcode;
  }

  const instr_buf&
  get_instr(uint32_t /*id*/) const override
  {
    return m_instr_buf;
  }

  const control_packet&
  get_ctrlpkt(uint32_t /*id*/) const override
  {
    return m_ctrl_pkt;
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
  static std::vector<std::string>
  split(const std::string& s, char delimiter)
  {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delimiter))
      tokens.push_back(item);

    return tokens;
  }

  // kernel signature has name followed by args
  // kernel signature - name(argtype, argtype ...)
  // eg : DPU(char*, char*, char*)
  static std::vector<xrt_core::xclbin::kernel_argument>
  construct_kernel_args(const std::string& signature)
  {
    std::vector<xrt_core::xclbin::kernel_argument> args;

    size_t start_pos = signature.find('(');
    size_t end_pos = signature.find(')', start_pos);

    if (start_pos == std::string::npos)
      return args; // kernel with no args

    if ((start_pos != std::string::npos) && (end_pos == std::string::npos || start_pos > end_pos))
      throw std::runtime_error("Failed to construct kernel args");

    std::string argstring = signature.substr(start_pos + 1, end_pos - start_pos - 1);
    std::vector<std::string> argstrings = split(argstring, ',');

    size_t count = 0;
    size_t offset = 0;
    for (const std::string& str : argstrings) {
      xrt_core::xclbin::kernel_argument arg;
      arg.name = "argv" + std::to_string(count);
      arg.hosttype = "no-type";
      arg.port = "no-port";
      arg.index = count;
      arg.offset = offset;
      arg.dir = xrt_core::xclbin::kernel_argument::direction::input;
      // if arg has pointer(*) in its name (eg: char*, void*) it is of type global otherwise scalar
      arg.type = (str.find('*') != std::string::npos)
        ? xrt_core::xclbin::kernel_argument::argtype::global
        : xrt_core::xclbin::kernel_argument::argtype::scalar;

      // At present only global args are supported
      // TODO : Add support for scalar args in ELF flow
      if (arg.type == xrt_core::xclbin::kernel_argument::argtype::scalar)
        throw std::runtime_error("scalar args are not yet supported for this kind of kernel");
      else {
        // global arg
        static constexpr size_t global_arg_size = 0x8;
        arg.size = global_arg_size;

        offset += global_arg_size;
      }

      args.emplace_back(arg);
      count++;
    }
    return args;
  }

  // Function that parses .symtab section
  // checks sub kernel entry at given symbol index and
  // gets corresponding kernel, parses it to create kernel info
  // Each group corresponds to a kernel, sub kernel combination
  // This function returns kernel, sub kernel name pair
  std::pair<std::string, std::string>
  get_kernel_subkernel_from_symtab(uint32_t sym_index)
  {
    ELFIO::section* symtab = m_elfio.sections[".symtab"];
    if (!symtab)
      throw std::runtime_error("No .symtab section found\n");

    // Get the symbol table
    const ELFIO::symbol_section_accessor symbols(m_elfio, symtab);
    // Get the symbol at sym_index
    std::string name;
    ELFIO::Elf64_Addr value{};
    ELFIO::Elf_Xword size{};
    unsigned char bind{};
    unsigned char type{};
    ELFIO::Elf_Half index = UINT16_MAX; // as 0 is a valid index
    unsigned char other{};

    // Read symbol data
    if (symbols.get_symbol(sym_index, name, value, size, bind, type, index, other)) {
      // sub kernel entries will be of type OBJECT
      // sanity check for corrupt Elf
      if (type != ELFIO::STT_OBJECT)
        throw std::runtime_error("symbol index doesn't point to sub kernel entry type in .symtab\n");

      // Here name will be sub kernel name and index will point to symtab index which
      // contains entry of kernel it belongs
      // Fetch kernel entry, parse it to fill kernel info
      auto subkernel_name = name;
      std::string kname;
      ELFIO::Elf_Half idx;

      // sanity checks
      if (!symbols.get_symbol(index, kname, value, size, bind, type, idx, other))
        throw std::runtime_error("Unable to get symbol pointed by sub kernel entry in .symtab\n");

      if (type != ELFIO::STT_FUNC)
        throw std::runtime_error("index pointed by sub kernel doesn't have kernel entry\n");

      // demangle the string and extract kernel name, args from it
      kname = demangle(kname);

      std::string kernel_name;
      size_t pos = kname.find('(');
      if (pos == std::string::npos)
        kernel_name = kname; // function with no args
      else
        kernel_name = kname.substr(0, pos);

      // Each kernel can have multiple sub kernels
      // So kernel info might have already been fetched and inserted from previous
      // sub kernel entry
      for (const auto& kinfo : m_kernels_info) {
        if (kinfo.props.name == kernel_name) {
          // kernel already exists
          return {kernel_name, subkernel_name};
        }
      }

      // kernel entry is not found
      // construct kernel args and properties and cache them
      // this info is used at the time of xrt::kernel object creation
      xrt_core::module_int::kernel_info k_info;
      k_info.args = construct_kernel_args(kname);

      // fill kernel properties
      k_info.props.name = kernel_name;
      k_info.props.type = xrt_core::xclbin::kernel_properties::kernel_type::dpu;

      m_kernels_info.push_back(std::move(k_info));

      return {kernel_name, subkernel_name};
    }

    throw std::runtime_error(std::string{"Unable to find symbol in .symtab section with index : "}
        + std::to_string(sym_index));
  }

  template <typename T>
  bool
  patch_it_impl(T base, const std::string& argnm, size_t index, uint64_t patch,
                xrt_core::patcher::buf_type type, uint32_t grp_index, bool first)
  {
    const auto key_string = generate_key_string(argnm, type);
    // check if arg patcher exists for this ctrl code 
    if (m_arg2patcher.find(grp_index) == m_arg2patcher.end())
      return false; // no patch entries for given grp idx

    auto it = m_arg2patcher[grp_index].find(key_string);
    auto not_found_use_argument_name = (it == m_arg2patcher[grp_index].end());
    if (not_found_use_argument_name) {// Search using index
      auto index_string = std::to_string(index);
      const auto key_index_string = generate_key_string(index_string, type);
      it = m_arg2patcher[grp_index].find(key_index_string);
      if (it == m_arg2patcher[grp_index].end())
        return false;
    }

    it->second.patch_it(base, patch, first);
    if (xrt_core::config::get_xrt_debug()) {
      if (not_found_use_argument_name) {
        std::stringstream ss;
        ss << "Patched " << patcher::to_string(type)
           << " using argument index " << index
           << " with value " << std::hex << patch;
        xrt_core::message::send( xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
      else {
        std::stringstream ss;
        ss << "Patched " << patcher::to_string(type)
           << " using argument name " << argnm
           << " with value " << std::hex << patch;
        xrt_core::message::send( xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }
    return true;
  }

protected:
  // Below members are made protected to allow direct access in derived
  // classes for simplicity and avoids unnecessary boilerplate setters,
  // getters code
  // NOLINTBEGIN
  xrt::elf m_elf;
  const ELFIO::elfio& m_elfio; // we should not modify underlying elf
  uint8_t m_os_abi = Elf_Amd_Aie2p;
  // store arg and its corresponding patcher information for each
  // kernel + subkernel combination
  // key - ctrlcode id(grp idx), value - map of key string and patcher info
  std::map<uint32_t, std::map<std::string, patcher>> m_arg2patcher;

  // Elf can have multiple group sections
  // sections belongs to a particular kernel, sub kernel combination
  // are grouped under a group section
  // lookup map for section index to group index
  std::map<uint32_t, uint32_t> m_sec_to_grp_map;
  // lookup map for kernel + sub kernel to grp idx(ctrl code id)
  std::map<std::string, uint32_t> m_kname_to_id_map;

  // Elf can have multiple kernels
  // Each kernel will have a kernel signature entry in .symtab section
  // kernel_info contains kernel name, args constructed from signature
  std::vector<xrt_core::module_int::kernel_info> m_kernels_info;
  // map that stores available subkernels of a kernel
  // key - kernel name, value - vector of sub kernel names
  std::map<std::string, std::vector<std::string>> m_kernels_map;

  // rela->addend have offset to base-bo-addr info along with schema
  // [0:3] bit are used for patching schema, [4:31] used for base-bo-addr
  constexpr static uint32_t addend_shift = 4;
  constexpr static uint32_t addend_mask = ~((uint32_t)0) << addend_shift;
  constexpr static uint32_t schema_mask = ~addend_mask;
  // NOLINTEND

  explicit module_elf(xrt::elf elf)
    : module_impl{ elf.get_cfg_uuid() }
    , m_elf{std::move(elf)}
    , m_elfio(xrt_core::elf_int::get_elfio(m_elf))
    , m_os_abi(m_elfio.get_os_abi())
  {}

public:
  const ELFIO::elfio&
  get_elfio_object() const override
  {
    return m_elfio;
  }

  // Function that parses the .group sections in the ELF file
  // and returns a map with ctrl code id which is grp idx and
  // vector of section ids that belong to this group as value
  std::map<uint32_t, std::vector<uint32_t>>
  parse_group_sections(bool is_grp_elf)
  {
    std::map<uint32_t, std::vector<uint32_t>> id_sections_map;
    if (!is_grp_elf) {
      // Older version ELf, so doesn't have .group sections
      // This Elf doesnt have multiple ctrlcodes
      // Using empty string as kernel + subkernel combination
      // and using no_ctrl_code_id(UINT32_MAX) as grp idx
      constexpr const char* kname = "";
      std::vector<uint32_t> sec_ids;
      for (const auto& sec : m_elfio.sections) {
        // insert all the sections into a common group
        sec_ids.push_back(sec->get_index());
        // fill the sec_to_grp_map for lookup later
        // for this kind of ELF we fill UINT32_MAX as there is no group section
        m_sec_to_grp_map[sec->get_index()] = xrt_core::module_int::no_ctrl_code_id;
      }

      // fill kenerl name to id map with id as grp idx
      m_kname_to_id_map[kname] = xrt_core::module_int::no_ctrl_code_id;
      id_sections_map.emplace(xrt_core::module_int::no_ctrl_code_id, std::move(sec_ids));
    }
    else {
      for (const auto& section : m_elfio.sections) {
        if (section == nullptr || section->get_type() != ELFIO::SHT_GROUP)
          continue; // not a .group section

        const auto* data = section->get_data();
        const auto size = section->get_size();
        auto sec_idx = section->get_index();

        // .group section's data will be flags followed by section indices
        // that belong to this group
        if (data == nullptr || size < sizeof(ELFIO::Elf_Word))
          continue;

        // .group section's info field data is index of sub kernel entry
        // in .symtab section
        auto sec_info = section->get_info();
        auto [kname, sub_kname] = get_kernel_subkernel_from_symtab(sec_info);
        m_kernels_map[kname].push_back(sub_kname);
        // store ctrl code id against kernel + subkernel name
        m_kname_to_id_map[kname + sub_kname] = sec_idx;

        const auto* word_data = reinterpret_cast<const ELFIO::Elf_Word*>(data);
        const auto word_count = size / sizeof(ELFIO::Elf_Word);

        std::vector<uint32_t> members;
        // skip flags at index 0
        for (std::size_t i = 1; i < word_count; ++i) {
          members.push_back(word_data[i]);
          // fill section index to group index map
          m_sec_to_grp_map[word_data[i]] = sec_idx;
        }

        id_sections_map.emplace(sec_idx, std::move(members));
      }
    }

    return id_sections_map;
  }

  const std::vector<xrt_core::module_int::kernel_info>&
  get_kernels_info() const override
  {
    return m_kernels_info;
  }

  bool
  patch_it(uint8_t* base, const std::string& argnm, size_t index, uint64_t patch,
           xrt_core::patcher::buf_type type, uint32_t grp_index, bool first) override
  {
    return patch_it_impl(base, argnm, index, patch, type, grp_index, first);
  }

  bool
  patch_it(xrt::bo bo, const std::string& argnm, size_t index, uint64_t patch,
           xrt_core::patcher::buf_type type, uint32_t grp_index, bool first) override
  {
    return patch_it_impl(bo, argnm, index, patch, type, grp_index, first);
  }

  uint8_t
  get_os_abi() const override
  {
    return m_os_abi;
  }

  size_t
  number_of_arg_patchers(uint32_t id) const override
  {
    if (auto it = m_arg2patcher.find(id); it != m_arg2patcher.end())
      return it->second.size();

    throw std::runtime_error(
        std::string{"Unable to get arg patcher for index: " + std::to_string(id)});
  }
};

// module class for ELFs with os_abi - Elf_Amd_Aie2p
class module_elf_aie2p : public module_elf
{
  xrt::aie::program m_program;
  // New Elfs of Aie2p contain multiple kernels and corresponding sub kernels
  // We use ctrl code id or grp idx to represent kernel + sub kernel combination
  // Below maps store data for each ctrl code id
  std::map<uint32_t, instr_buf> m_instr_buf_map;
  std::map<uint32_t, control_packet> m_ctrl_packet_map;
  std::map<uint32_t, buf> m_save_buf_map;
  std::map<uint32_t, buf> m_restore_buf_map;

  // Elf with save/restore sections uses different opcode
  // Below variable is set to true if these sections exist
  bool m_preemption_exist = false;

  // New Elfs have multiple PDI sections of format .pdi.*
  // Below map has pdi section symbol name as key and section data as value
  std::map<std::string, buf> m_pdi_buf_map;
  // map storing pdi symbols that needs patching in ctrl codes
  std::map<uint32_t, std::unordered_set<std::string>> m_ctrl_pdi_map;
  // map storing xrt::bo that stores pdi data corresponding to each pdi symbol
  std::map<std::string, xrt::bo> m_pdi_bo_map;
  
  size_t m_scratch_pad_mem_size = 0;
  size_t m_ctrl_scratch_pad_mem_size = 0;
  uint32_t m_partition_size = UINT32_MAX;

  std::set<std::string> m_ctrlpkt_pm_dynsyms; // preemption dynsyms in elf
  std::map<std::string, buf> m_ctrlpkt_pm_bufs; // preemption buffers map

  // Extract buffer from ELF sections without assuming anything
  // about order of sections in the ELF file.
  template<typename buf_type>
  void
  initialize_buf(xrt_core::patcher::buf_type type, std::map<uint32_t, buf_type>& map)
  {
    for (const auto& sec : m_elfio.sections) {
      auto name = sec->get_name();
      auto sec_index = sec->get_index();
      buf_type buf;
      // Instruction, control pkt buffers are in section of type .ctrltext.* .ctrldata.*.
      if (name.find(patcher::to_string(type)) == std::string::npos)
        continue;

      auto grp_idx = m_sec_to_grp_map[sec_index];
      buf.append_section_data(sec.get());
      map.emplace(grp_idx, buf);
    }
  }

  void
  initialize_pdi_buf()
  {
    for (const auto& sec : m_elfio.sections) {
      auto name = sec->get_name();
      if (name.find(patcher::to_string(xrt_core::patcher::buf_type::pdi)) == std::string::npos)
        continue;
      
      buf pdi_buf;
      pdi_buf.append_section_data(sec.get());
      m_pdi_buf_map.emplace(name, pdi_buf);
    }
  }

  // Extract preempt_save/preempt_restore buffer from ELF sections
  void
  initialize_save_restore_buf(const std::map<uint32_t, std::vector<uint32_t>>& id_secs_map)
  {
    // iterate over all secs in each grp/id and collect save/restore section
    for (const auto& [id, sec_ids] : id_secs_map) {
      bool save = false;
      bool restore = false;
      for (auto sec_idx : sec_ids) {
        const auto& sec = m_elfio.sections[sec_idx];
        auto name = sec->get_name();
        if (name.find(patcher::to_string(xrt_core::patcher::buf_type::preempt_save)) != std::string::npos) {
          buf save_buf;
          save_buf.append_section_data(sec);
          m_save_buf_map.emplace(id, save_buf);
          save = true;
        }
        else if (name.find(patcher::to_string(xrt_core::patcher::buf_type::preempt_restore)) != std::string::npos) {
          buf restore_buf;
          restore_buf.append_section_data(sec);
          m_restore_buf_map.emplace(id, restore_buf);
          restore = true;
        }
      }

      if (save != restore)
        throw std::runtime_error("Invalid elf because preempt save and restore is not paired");

      // If both save and restore buffers are found, set preemption exist flag
      // At present this flag is set at Elf level assuming if one kernel + sub kernel has these
      // sections, all others will have these sections
      // TODO : Move this logic to group or kernel + sub kernel level
      if (save && restore)
        m_preemption_exist = true;
    }
  }

  // Extract ctrlpkt preemption buffers from ELF sections
  // and store it in map with section name as key
  void
  initialize_ctrlpkt_pm_bufs()
  {
    for (const auto& sec : m_elfio.sections) {
      auto name = sec->get_name();
      if (name.find(patcher::to_string(xrt_core::patcher::buf_type::ctrlpkt_pm)) == std::string::npos)
        continue;

      m_ctrlpkt_pm_bufs[name].append_section_data(sec.get());
    }
  }

  std::pair<size_t, xrt_core::patcher::buf_type>
  determine_section_type(const std::string& section_name, uint32_t id)
  {
    if (section_name.find(patcher::to_string(xrt_core::patcher::buf_type::ctrltext)) != std::string::npos) {
      if (m_instr_buf_map.find(id) == m_instr_buf_map.end())
        throw std::runtime_error("Invalid section passed, section info is not cached\n");
      return { m_instr_buf_map[id].size(), xrt_core::patcher::buf_type::ctrltext};
    }
    else if (!m_ctrl_packet_map.empty() &&
             section_name.find(patcher::to_string(xrt_core::patcher::buf_type::ctrldata)) != std::string::npos) {
      if (m_ctrl_packet_map.find(id) == m_ctrl_packet_map.end())
        throw std::runtime_error("Invalid section passed, section info is not cached\n");
      return { m_ctrl_packet_map[id].size(), xrt_core::patcher::buf_type::ctrldata};
    }
    else if (section_name.find(patcher::to_string(xrt_core::patcher::buf_type::preempt_save)) != std::string::npos) {
      if (m_save_buf_map.find(id) == m_save_buf_map.end())
        throw std::runtime_error("Invalid section passed, section info is not cached\n");
      return { m_save_buf_map[id].size(), xrt_core::patcher::buf_type::preempt_save };
    }
    else if (section_name.find(patcher::to_string(xrt_core::patcher::buf_type::preempt_restore)) != std::string::npos) {
      if (m_restore_buf_map.find(id) == m_restore_buf_map.end())
        throw std::runtime_error("Invalid section passed, section info is not cached\n");
      return { m_restore_buf_map[id].size(), xrt_core::patcher::buf_type::preempt_restore };
    }
    else if (!m_pdi_buf_map.empty() &&
             section_name.find(patcher::to_string(xrt_core::patcher::buf_type::pdi)) != std::string::npos) {
      if (m_pdi_buf_map.find(section_name) == m_pdi_buf_map.end())
        throw std::runtime_error("Invalid pdi section passed, section info is not cached\n");
      return { m_pdi_buf_map[section_name].size(), xrt_core::patcher::buf_type::pdi };
    }
    else
      throw std::runtime_error("Invalid section name " + section_name);
  }

  void
  initialize_arg_patchers()
  {
    auto dynsym = m_elfio.sections[".dynsym"];
    auto dynstr = m_elfio.sections[".dynstr"];
    auto dynsec = m_elfio.sections[".rela.dyn"];

    if (!dynsym || !dynstr || !dynsec)
      return;

    auto name = dynsec->get_name();

    // Iterate over all relocations and construct a patcher for each
    // relocation that refers to a symbol in the .dynsym section.
    auto begin = reinterpret_cast<const ELFIO::Elf32_Rela*>(dynsec->get_data());
    auto end = begin + dynsec->get_size() / sizeof(const ELFIO::Elf32_Rela);
    for (auto rela = begin; rela != end; ++rela) {
      auto symidx = ELFIO::get_sym_and_type<ELFIO::Elf32_Rela>::get_r_sym(rela->r_info);
      auto type = ELFIO::get_sym_and_type<ELFIO::Elf32_Rela>::get_r_type(rela->r_info);

      auto dynsym_offset = symidx * sizeof(ELFIO::Elf32_Sym);
      if (dynsym_offset >= dynsym->get_size())
        throw std::runtime_error("Invalid symbol index " + std::to_string(symidx));
      auto sym = reinterpret_cast<const ELFIO::Elf32_Sym*>(dynsym->get_data() + dynsym_offset);

      auto dynstr_offset = sym->st_name;
      if (dynstr_offset >= dynstr->get_size())
        throw std::runtime_error("Invalid symbol name offset " + std::to_string(dynstr_offset));
      auto symname = dynstr->get_data() + dynstr_offset;

      if (!m_scratch_pad_mem_size && (strcmp(symname, Scratch_Pad_Mem_Symbol) == 0)) {
        m_scratch_pad_mem_size = static_cast<size_t>(sym->st_size);
      }
      else if (!m_ctrl_scratch_pad_mem_size && (strcmp(symname, Control_ScratchPad_Symbol) == 0)) {
        m_ctrl_scratch_pad_mem_size = static_cast<size_t>(sym->st_size);
      }

      static constexpr const char* ctrlpkt_pm_dynsym = "ctrlpkt-pm";
      if (std::string(symname).find(ctrlpkt_pm_dynsym) != std::string::npos) {
        // store ctrlpkt preemption symbols which is later used for patching instr buf
        m_ctrlpkt_pm_dynsyms.emplace(symname);
      }

      // Get control code section referenced by the symbol, col, and page
      auto section = m_elfio.sections[sym->st_shndx];
      if (!section)
        throw std::runtime_error("Invalid section index " + std::to_string(sym->st_shndx));

      auto offset = rela->r_offset;
      auto sec_index = section->get_index();
      auto grp_idx = m_sec_to_grp_map[sec_index];
      auto [sec_size, buf_type] = determine_section_type(section->get_name(), grp_idx);

      if (offset >= sec_size)
        throw std::runtime_error("Invalid offset " + std::to_string(offset));

      if (std::string(symname).find("pdi") != std::string::npos) {
        // pdi symbol, add to map of which ctrl code needs it
        m_ctrl_pdi_map[grp_idx].insert(symname);
      }

      patcher::symbol_type patch_scheme;
      uint32_t add_end_addr;
      auto abi_version = static_cast<uint16_t>(m_elfio.get_abi_version());
      if (abi_version != 1) {
        add_end_addr = rela->r_addend;
        patch_scheme = static_cast<patcher::symbol_type>(type);
      }
      else {
        // rela addend have offset to base_bo_addr info along with schema
        add_end_addr = (rela->r_addend & addend_mask) >> addend_shift;
        patch_scheme = static_cast<patcher::symbol_type>(rela->r_addend & schema_mask);
      }

      std::string argnm{ symname, symname + std::min(strlen(symname), dynstr->get_size()) };
      patcher::patch_info pi = patch_scheme == patcher::symbol_type::scalar_32bit_kind
        // st_size is is encoded using register value mask for scaler_32
        // for other pacthing scheme it is encoded using size of dma
        ? patcher::patch_info{ offset, add_end_addr, static_cast<uint32_t>(sym->st_size) }
        : patcher::patch_info{ offset, add_end_addr, 0 };

      auto key_string = generate_key_string(argnm, buf_type);

      auto search = m_arg2patcher[grp_idx].find(key_string);
      if (search != m_arg2patcher[grp_idx].end()) {
        auto& patcher = search->second;
        patcher.m_ctrlcode_patchinfo.emplace_back(pi);
      }
      else
        m_arg2patcher[grp_idx].emplace(std::move(key_string), patcher{patch_scheme, {pi}, buf_type});
    }
  }

public:
  explicit module_elf_aie2p(const xrt::elf& elf)
    : module_elf{elf}
    , m_program{elf}
  {
    auto id_secs_map = parse_group_sections(::is_group_elf(xrt_core::elf_int::get_elfio(elf)));
    initialize_buf(xrt_core::patcher::buf_type::ctrltext, m_instr_buf_map);
    initialize_buf(xrt_core::patcher::buf_type::ctrldata, m_ctrl_packet_map);
    initialize_save_restore_buf(id_secs_map);
    initialize_pdi_buf();
    initialize_ctrlpkt_pm_bufs();
    initialize_arg_patchers();
  }

  ert_cmd_opcode
  get_ert_opcode() const override
  {
    if (!m_pdi_buf_map.empty())
      return ERT_START_NPU_PREEMPT_ELF;

    if (m_preemption_exist)
      return ERT_START_NPU_PREEMPT;

    return ERT_START_NPU;
  }

  const std::unordered_set<std::string>&
  get_patch_pdis(uint32_t id) const override
  {
    static const std::unordered_set<std::string> empty_set = {};
    auto it = m_ctrl_pdi_map.find(id);
    if (it != m_ctrl_pdi_map.end())
      return it->second;

    return empty_set;
  }

  const buf&
  get_pdi(const std::string& pdi_name) const override
  {
    auto it = m_pdi_buf_map.find(pdi_name);
    if (it != m_pdi_buf_map.end())
      return it->second;

    return buf::get_empty_buf();
  }

  xrt::bo&
  get_pdi_bo(const std::string& pdi_name) override
  {
    return m_pdi_bo_map[pdi_name];
  }

  const instr_buf&
  get_instr(uint32_t id) const override
  {
    auto it = m_instr_buf_map.find(id);
    if (it != m_instr_buf_map.end())
      return it->second;
    return instr_buf::get_empty_buf();
  }

  const control_packet&
  get_ctrlpkt(uint32_t id) const override
  {
    auto it = m_ctrl_packet_map.find(id);
    if (it != m_ctrl_packet_map.end())
      return it->second;
    return control_packet::get_empty_buf();
  }

  const std::set<std::string>&
  get_ctrlpkt_pm_dynsyms() const override
  {
    return m_ctrlpkt_pm_dynsyms;
  }

  const std::map<std::string, buf>&
  get_ctrlpkt_pm_bufs() const override
  {
    return m_ctrlpkt_pm_bufs;
  }

  size_t
  get_scratch_pad_mem_size() const override
  {
    return m_scratch_pad_mem_size;
  }

  size_t
  get_ctrl_scratch_pad_mem_size() const override
  {
    return m_ctrl_scratch_pad_mem_size;
  }

  const buf&
  get_preempt_save(uint32_t id) const override
  {
    auto it = m_save_buf_map.find(id);
    if (it != m_save_buf_map.end())
      return it->second;
    return buf::get_empty_buf();
  }

  const buf&
  get_preempt_restore(uint32_t id) const override
  {
    auto it = m_restore_buf_map.find(id);
    if (it != m_restore_buf_map.end())
      return it->second;
    return buf::get_empty_buf();
  }

  uint32_t
  get_ctrlcode_id(const std::string& name) const override
  {
    if (auto pos = name.find(":"); pos != std::string::npos) {
      // name passed is kernel name + sub kernel name
      auto key = name.substr(0, pos) + name.substr(pos + 1);
      auto it = m_kname_to_id_map.find(key);
      if (it == m_kname_to_id_map.end())
        throw std::runtime_error(std::string{"Unable to find group idx for given kernel: "} + name);

      auto id = it->second;
      if ((m_instr_buf_map.find(id) == m_instr_buf_map.end()) &&
          (m_ctrl_packet_map.find(id) == m_ctrl_packet_map.end()))
        throw std::runtime_error(std::string{"Unable to find ctrlcode entry for given kernel: "} + name);

      return id;
    }

    // If user doesn't provide sub kernel we default to first entry
    // if its the only availble sub kernel of given kernel
    // otherwise an exception is thrown
    // check if given kernel is present
    if (auto entry = m_kernels_map.find(name); entry != m_kernels_map.end()) {
      if (entry->second.size() == 1) {
        auto key = name + *(entry->second.begin());
        auto it = m_kname_to_id_map.find(key);
        if (it == m_kname_to_id_map.end())
          throw std::runtime_error(std::string{"Unable to find group idx for given kernel: "} + key);

        auto id = it->second;
        if ((m_instr_buf_map.find(id) == m_instr_buf_map.end()) &&
            (m_ctrl_packet_map.find(id) == m_ctrl_packet_map.end()))
          throw std::runtime_error(std::string{"Unable to find ctrlcode entry for given kernel: "} + name);
    
        return id;
      }
      else
        throw std::runtime_error("Multiple sub kernels present for given kernel, cannot choose sub kernel\n");
    }

    throw std::runtime_error(std::string{"cannot get ctrlcode id from given kernel name: "} + name);
  }
};

// module class for ELFs with os_abi - Elf_Amd_Aie2ps, Elf_Amd_Aie2ps_group
class module_elf_aie2ps : public module_elf
{
  xrt::aie::program m_program;

  // map for holding control code data for each sub kernel
  // key : control code id (grp sec idx), value : vector of column ctrlcodes
  std::map<uint32_t, std::vector<ctrlcode>> m_ctrlcodes_map;

  // map to hold .dump section of different sub kernels used for debug/trace
  std::map<uint32_t, buf> m_dump_buf_map;

  // The ELF sections embed column and page information in their
  // names.  Extract the column and page information from the
  // section name, default to single column and page when nothing
  // is specified.  Note that in some usecases the extracted column
  // is actually the index of column microblase controller; the term
  // column and uC index is used interchangably in such cases.
  static std::pair<uint32_t, uint32_t>
  get_column_and_page(const std::string& name)
  {
    // section name can be
    // .ctrltext.<col>.<page> or .ctrldata.<col>.<page>
    // .ctrltext.<col>.<page>.<id> or .ctrldata.<col>.<page>.<id> - newer Elfs

    // Max expected tokens: prefix, col, page, id
    constexpr size_t col_token_id  = 1;
    constexpr size_t page_token_id = 2;

    std::vector<std::string> tokens;
    std::stringstream ss(name);
    std::string token;
    while (std::getline(ss, token, '.')) {
      if (!token.empty())
        tokens.emplace_back(std::move(token));
    }

    try {
      if (tokens.size() <= col_token_id)
        return {0, 0}; // Only prefix present

      if (tokens.size() == (col_token_id + 1))
        return {std::stoul(tokens[col_token_id]), 0}; // Only col present

      return {std::stoul(tokens[col_token_id]), std::stoul(tokens[page_token_id])};
    }
    catch (const std::exception&) {
      throw std::runtime_error("Invalid section name passed to parse col or page index\n");
    }
  }

  // Extract control code from ELF sections without assuming anything
  // about order of sections in the ELF file.  Build helper data
  // structures that manages the control code data per page for each
  // microblaze controller (uC), then create ctrlcode objects from the
  // data.
  void
  initialize_column_ctrlcode(std::map<uint32_t, std::vector<size_t>>& pad_offsets, bool is_grp_elf)
  {
    // Elf sections for a single page
    struct elf_page
    {
      ELFIO::section* ctrltext = nullptr;
      ELFIO::section* ctrldata = nullptr;
    };

    // Elf ctrl code for a partition spanning multiple uC, where each
    // uC has its own control code.  For architectures where a
    // partition is not divided into multiple controllers, there will
    // be just one entry in the associative map.
    // ucidx -> [page -> [ctrltext, ctrldata]]
    using page_index = uint32_t;
    using uc_index = uint32_t;
    using uc_sections = std::map<uc_index, std::map<page_index, elf_page>>;

    // Elf can have multiple kernel instances
    // Each instance has its own uc_sections map
    // key - instance id(grp idx) : value - uc sections
    using ctrlcode_map = std::map<uint32_t, uc_sections>;
    ctrlcode_map ctrl_map;

    auto id_secs_map = parse_group_sections(is_grp_elf);
    // Iterate sections for each sub kernel
    // collect ctrltext and ctrldata per column and page
    for (const auto& [id, sec_ids] : id_secs_map) {
      for (auto sec_idx : sec_ids) {
        const auto& sec = m_elfio.sections[sec_idx];
        auto name = sec->get_name();

        if (name.find(patcher::to_string(xrt_core::patcher::buf_type::ctrltext)) != std::string::npos) {
          auto [col, page] = get_column_and_page(name);
          ctrl_map[id][col][page].ctrltext = sec;
        }
        else if (name.find(patcher::to_string(xrt_core::patcher::buf_type::ctrldata)) != std::string::npos) {
          auto [col, page] = get_column_and_page(name);
          ctrl_map[id][col][page].ctrldata = sec;
        }
      }
    }

    // Create uC control code from the collected data.  If page
    // requirement, then pad to page size for page of a column so that
    // embedded processor can load a page at a time.  Note, that not
    // all column uC need be used, so account for holes in
    // uc_sections.  Leverage that uc_sections is a std::map and that
    // std::map stores its elements in ascending order of keys
    for (const auto& [id, uc_sec] : ctrl_map) {
      auto size = uc_sec.empty() ? 0 : uc_sec.rbegin()->first + 1;
      m_ctrlcodes_map[id].resize(size);
      pad_offsets[id].resize(size);
      for (auto& [ucidx, elf_sects] : uc_sec) {
        for (auto& [page, page_sec] : elf_sects) {
          if (page_sec.ctrltext)
            m_ctrlcodes_map[id][ucidx].append_section_data(page_sec.ctrltext);

          if (page_sec.ctrldata)
            m_ctrlcodes_map[id][ucidx].append_section_data(page_sec.ctrldata);
    
          m_ctrlcodes_map[id][ucidx].pad_to_page(page);
        }
        pad_offsets[id][ucidx] = m_ctrlcodes_map[id][ucidx].size();
      }
    }

    // Append pad section to the control code.
    // This section may contain scratchpad/control-packet etc
    for (const auto& [id, sec_ids] : id_secs_map) {
      for (auto sec_idx : sec_ids) {
        const auto& sec = m_elfio.sections[sec_idx];
        const auto& name = sec->get_name();
        if (name.find(patcher::to_string(xrt_core::patcher::buf_type::pad)) == std::string::npos)
          continue;
        auto [col, page] = get_column_and_page(name);
        m_ctrlcodes_map[id][col].append_section_data(sec);
      }
    }
  }

  void
  initialize_arg_patchers(const std::map<uint32_t, std::vector<size_t>>& pad_offsets)
  {
    auto dynsym = m_elfio.sections[".dynsym"];
    auto dynstr = m_elfio.sections[".dynstr"];

    for (const auto& sec : m_elfio.sections) {
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
        auto type = ELFIO::get_sym_and_type<ELFIO::Elf32_Rela>::get_r_type(rela->r_info);

        auto dynstr_offset = sym->st_name;
        if (dynstr_offset >= dynstr->get_size())
          throw std::runtime_error("Invalid symbol name offset " + std::to_string(dynstr_offset));
        auto symname = dynstr->get_data() + dynstr_offset;
        std::string argnm{ symname, symname + std::min(strlen(symname), dynstr->get_size()) };

        // patching can be done to ctrlcode or ctrlpkt section
        auto patch_sec = m_elfio.sections[sym->st_shndx];
        if (!patch_sec)
          throw std::runtime_error("Invalid section index " + std::to_string(sym->st_shndx));

        auto patch_sec_name = patch_sec->get_name();
        auto [col, page] = get_column_and_page(patch_sec_name);
        auto sec_idx = patch_sec->get_index();
        auto grp_idx = m_sec_to_grp_map[sec_idx];
        if (m_ctrlcodes_map.find(grp_idx) == m_ctrlcodes_map.end())
          throw std::runtime_error(std::string{"Unable to fetch ctrlcode to patch for given symbol: "} + argnm);
        auto ctrlcodes = m_ctrlcodes_map[grp_idx];

        size_t abs_offset = 0;
        xrt_core::patcher::buf_type buf_type = xrt_core::patcher::buf_type::buf_type_count;
        if (patch_sec_name.find(patcher::to_string(xrt_core::patcher::buf_type::pad)) != std::string::npos) {
          auto pad_off_vec = pad_offsets.at(grp_idx);
          for (uint32_t i = 0; i < col; ++i)
            abs_offset += ctrlcodes[i].size();
          abs_offset += pad_off_vec[col];
          abs_offset += rela->r_offset;
          buf_type = xrt_core::patcher::buf_type::pad;
        }
        else {
          // section to patch is ctrlcode
          // Get control code section referenced by the symbol, col, and page
          auto column_ctrlcode_size = ctrlcodes.at(col).size();
          auto sec_offset = page * elf_page_size + rela->r_offset + 16; // NOLINT magic number 16??
          if (sec_offset >= column_ctrlcode_size)
            throw std::runtime_error("Invalid ctrlcode offset " + std::to_string(sec_offset));
          // The control code for all columns will be represented as one
          // contiguous buffer object.  The patcher will need to know
          // the offset into the buffer object for the particular column
          // and page being patched.  Past first [0, col) columns plus
          // page offset within column and the relocation offset within
          // the page.
          for (uint32_t i = 0; i < col; ++i)
            abs_offset += ctrlcodes.at(i).size();
          abs_offset += sec_offset;
          buf_type = xrt_core::patcher::buf_type::ctrltext;
        }

        // Construct the patcher for the argument with the symbol name
        patcher::symbol_type patch_scheme;
        uint32_t add_end_addr;
        auto abi_version = static_cast<uint16_t>(m_elfio.get_abi_version());
        if (abi_version != 1) {
          add_end_addr = rela->r_addend;
          patch_scheme = static_cast<patcher::symbol_type>(type);
        }
        else {
          // rela addend have offset to base_bo_addr info along with schema
          add_end_addr = (rela->r_addend & addend_mask) >> addend_shift;
          patch_scheme = static_cast<patcher::symbol_type>(rela->r_addend & schema_mask);
        }

        // using grp_idx in identifying key string for patching as there can be
        // multiple sub kernels and each group can hold one sub kernel
        auto key_string = generate_key_string(argnm, buf_type);
        // One arg may need to be patched at multiple offsets of control code
        // arg2patcher map contains a key & value pair of arg & patcher object
        // patcher object uses m_ctrlcode_patchinfo vector to store multiple offsets
        // this vector size would be equal to number of places which needs patching
        // On first occurrence of arg, Create a new patcher object and
        // Initialize the m_ctrlcode_patchinfo vector of the single patch_info structure
        // On all further occurences of arg, add patch_info structure to existing vector
        auto search = m_arg2patcher[grp_idx].find(key_string);
        if (search != m_arg2patcher[grp_idx].end()) {
          auto& patcher = search->second;
          patcher.m_ctrlcode_patchinfo.emplace_back(patcher::patch_info{abs_offset, add_end_addr, 0});
        }
        else
          m_arg2patcher[grp_idx].emplace(std::move(key_string), patcher{patch_scheme, {{abs_offset, add_end_addr}}, buf_type});
      }
    }
  }

  // Extract .dump section from ELF sections
  void
  initialize_dump_buf()
  {
    for (const auto& sec : m_elfio.sections) {
      auto name = sec->get_name();
      if (name.find(patcher::to_string(xrt_core::patcher::buf_type::dump)) == std::string::npos)
        continue;

      // get ctrl code id from group index
      auto ctrl_id = m_sec_to_grp_map[sec->get_index()];
      m_dump_buf_map[ctrl_id].append_section_data(sec.get());
    }
  }

public:
  explicit module_elf_aie2ps(const xrt::elf& elf)
    : module_elf{elf}
    , m_program{elf}
  {
    std::map<uint32_t, std::vector<uint64_t>> pad_offsets;
    initialize_column_ctrlcode(pad_offsets, ::is_group_elf(xrt_core::elf_int::get_elfio(elf)));
    initialize_arg_patchers(pad_offsets);
    initialize_dump_buf();
  }

  ert_cmd_opcode
  get_ert_opcode() const override
  {
    return ERT_START_DPU;
  }

  const std::vector<ctrlcode>&
  get_data(uint32_t id) const override
  {
    if (auto it = m_ctrlcodes_map.find(id); it != m_ctrlcodes_map.end())
      return it->second;

    static const std::vector<ctrlcode> empty_vec;
    return empty_vec;
  }

  const buf&
  get_dump_buf(uint32_t id) const override
  {
    if (auto it = m_dump_buf_map.find(id); it != m_dump_buf_map.end())
      return it->second;

    return buf::get_empty_buf();
  }

  uint32_t
  get_ctrlcode_id(const std::string& name) const override
  {
    if (auto pos = name.find(":"); pos != std::string::npos) {
      // name passed is kernel name + sub kernel name
      auto key = name.substr(0, pos) + name.substr(pos + 1);
      auto it = m_kname_to_id_map.find(key);
      if (it == m_kname_to_id_map.end())
        throw std::runtime_error(std::string{"Unable to find group idx for given kernel: "} + name);

      auto id = it->second;
      if (m_ctrlcodes_map.find(id) == m_ctrlcodes_map.end())
        throw std::runtime_error(std::string{"Unable to find ctrlcode entry for given kernel: "} + name);

      return id;
    }

    // If user doesn't provide sub kernel we default to first entry
    // if its the only availble sub kernel of given kernel
    // otherwise an exception is thrown
    // check if given kernel is present
    if (auto entry = m_kernels_map.find(name); entry != m_kernels_map.end()) {
      if (entry->second.size() == 1) {
        auto key = name + *(entry->second.begin());
        auto it = m_kname_to_id_map.find(key);
        if (it == m_kname_to_id_map.end())
          throw std::runtime_error(std::string{"Unable to find group idx for given kernel: "} + key);

        auto id = it->second;
        if (m_ctrlcodes_map.find(id) == m_ctrlcodes_map.end())
          throw std::runtime_error(std::string{"Unable to find ctrlcode entry for given kernel: "} + name);
    
        return id;
      }
      else
        throw std::runtime_error("Multiple sub kernels present for given kernel, cannot choose sub kernel\n");
    }

    throw std::runtime_error(std::string{"cannot get ctrlcode id from given kernel name: "} + name);
  }
};

// class module_sram - Create an hwctx specific (sram) module from parent
//
// Allocate a buffer object to hold the ctrlcodes for each column created
// by parent module.  The ctrlcodes are concatenated into a single buffer
// where buffer object address of offset for each column.
class module_sram : public module_impl
{
  std::shared_ptr<module_impl> m_parent;
  xrt::hw_context m_hwctx;
  // New ELFs can have multiple kernels and each kernel can have multiple
  // sub kernels. We need id to identify which ctrlcode corresponds to
  // kernel + sub kernel combination.
  uint32_t m_ctrl_code_id;

  // The instruction buffer object contains the ctrlcodes for each
  // column.  The ctrlcodes are concatenated into a single buffer
  // padded at page size specific to hardware.
  xrt::bo m_buffer;
  xrt::bo m_instr_bo;
  xrt::bo m_ctrlpkt_bo;
  xrt::bo m_scratch_pad_mem;
  xrt::bo m_ctrl_scratch_pad_mem;
  xrt::bo m_preempt_save_bo;
  xrt::bo m_preempt_restore_bo;

  // map of ctrlpkt preemption buffers
  // key : dynamic symbol patch name of ctrlpkt-pm
  // value : xrt::bo filled with corresponding section data
  std::map<std::string, xrt::bo> m_ctrlpkt_pm_bos;

  // Tuple of uC index, address, size, where address is the address of
  // the ctrlcode for indexed uC and size is the size of the ctrlcode.
  // The first ctrlcode is at the base address (m_buffer.address()) of
  // the buffer object.  The addresses are used in ert_dpu_data
  // payload to identify the ctrlcode for each column processor.
  std::vector<std::tuple<uint16_t, uint64_t, uint64_t>> m_column_bo_address;

  // Arguments patched in the ctrlcode buffer object
  // Must match number of argument patchers in parent module
  std::set<std::string> m_patched_args;

  // Dirty bit to indicate that patching was done prior to last
  // buffer sync to device.
  bool m_dirty{ false };

  union debug_flag_union {
    struct debug_mode_struct {
      uint32_t dump_control_codes     : 1;
      uint32_t dump_control_packet    : 1;
      uint32_t dump_preemption_codes  : 1;
      uint32_t reserved : 29;
    } debug_flags;
    uint32_t all;
  } m_debug_mode = {};
  uint32_t m_id {0}; //TODO: it needs come from the elf file

  // instruction, ctrlpkt, save, restore buffers are synced only once
  // and in subsequent runs only parts of buffer(where patching happened)
  // are synced for improved perfomance
  // this variable tells if its first time patching
  bool m_first_patch = true;

  // In platforms that support Dynamic tracing xrt bo's are
  // created and passed to driver/firmware to hold tracing output
  // written by it.
  xrt::bo m_dtrace_ctrl_bo;

  bool
  inline is_dump_control_codes() const {
    return m_debug_mode.debug_flags.dump_control_codes != 0;
  }

  bool
  inline is_dump_control_packet() const {
    return m_debug_mode.debug_flags.dump_control_packet != 0;
  }

  bool
  inline is_dump_preemption_codes() {
    return m_debug_mode.debug_flags.dump_preemption_codes != 0;
  }

  uint32_t get_id() const {
    return m_id;
  }

  // For separated multi-column control code, compute the ctrlcode
  // buffer object address of each column (used in ert_dpu_data).
  // Note, that ctrlcodes is indexed by microblaze controller index
  // and may have holes. A hole is skipped prior to populating
  // m_column_bo_address.
  void
  fill_column_bo_address(const std::vector<ctrlcode>& ctrlcodes)
  {
    m_column_bo_address.clear();
    uint16_t ucidx = 0;
    auto base_addr = m_buffer.address();
    for (const auto& ctrlcode : ctrlcodes) {
      if (auto size = ctrlcode.size())
        m_column_bo_address.push_back({ ucidx, base_addr, size }); // NOLINT

      ++ucidx;
      base_addr += ctrlcode.size();
    }
  }

  void
  fill_bo_addresses()
  {
    m_column_bo_address.clear();
    m_column_bo_address.push_back({ static_cast<uint16_t>(0), m_instr_bo.address(), m_instr_bo.size() }); // NOLINT
  }

  // Fill the instruction buffer object with the data for each column
  void
  fill_instruction_buffer(const std::vector<ctrlcode>& ctrlcodes)
  {
    auto ptr = m_buffer.map<char*>();
    for (const auto& ctrlcode : ctrlcodes) {
      std::memcpy(ptr, ctrlcode.data(), ctrlcode.size());
      ptr += ctrlcode.size();
    }

    if (is_dump_control_codes()) {
      // dump pre patched instruction buffer
      std::string dump_file_name = "ctr_codes_pre_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_buffer, dump_file_name);

      std::stringstream ss;
      ss << "dumped file " << dump_file_name << " ctr_codes size: " << std::to_string(m_buffer.size());
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }

    // Iterate over control packets of all columns & patch it in instruction
    // buffer
    const auto& col_data = m_parent->get_data(m_ctrl_code_id);
    size_t offset = 0;
    for (size_t i = 0; i < col_data.size(); ++i) {
      // find the control-code-* sym-name and patch it in instruction buffer
      // This name is an agreement between aiebu and XRT
      auto sym_name = std::string(Control_Code_Symbol) + "-" + std::to_string(i);
      if (patch_instr_value(m_buffer,
                            sym_name,
                            std::numeric_limits<size_t>::max(),
                            m_buffer.address() + offset,
                            xrt_core::patcher::buf_type::ctrltext,
                            m_ctrl_code_id))
        m_patched_args.insert(sym_name);
      offset += col_data[i].size();
    }
  }

  void
  fill_bo_with_data(xrt::bo& bo, const buf& buf, bool sync = true)
  {
    auto ptr = bo.map<char*>();
    std::memcpy(ptr, buf.data(), buf.size());
    if (sync)
      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  }

  void
  fill_ctrlpkt_buf(xrt::bo& bo, const control_packet& ctrlpktbuf, bool sync = true)
  {
    auto ptr = bo.map<char*>();
    std::memcpy(ptr, ctrlpktbuf.data(), ctrlpktbuf.size());
    if (sync)
      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  }

  void
  create_instr_buf(const module_impl* parent)
  {
    XRT_DEBUGF("-> module_sram::create_instr_buf()\n");
    const auto& instr_buf = parent->get_instr(m_ctrl_code_id);
    size_t sz = instr_buf.size();
    if (sz == 0)
      throw std::runtime_error("Invalid instruction buffer size");

    // create bo combined size of all ctrlcodes
    m_instr_bo = xrt::bo{ m_hwctx, sz, xrt::bo::flags::cacheable, 1 /* fix me */ };

    // copy instruction into bo
    fill_bo_with_data(m_instr_bo, instr_buf, false /*don't sync*/);

    if (is_dump_control_codes()) {
      std::string dump_file_name = "ctr_codes_pre_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_instr_bo, dump_file_name);

      std::stringstream ss;
      ss << "dumped file " << dump_file_name << " ctr_codes size: " << std::to_string(sz);
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }

    auto preempt_save_data = parent->get_preempt_save(m_ctrl_code_id);
    auto preempt_save_data_size = preempt_save_data.size();

    auto preempt_restore_data = parent->get_preempt_restore(m_ctrl_code_id);
    auto preempt_restore_data_size = preempt_restore_data.size();

    if ((preempt_save_data_size > 0) && (preempt_restore_data_size > 0)) {
      m_preempt_save_bo = xrt::bo{ m_hwctx, preempt_save_data_size, xrt::bo::flags::cacheable, 1 /* fix me */ };
      fill_bo_with_data(m_preempt_save_bo, preempt_save_data, false);

      m_preempt_restore_bo = xrt::bo{ m_hwctx, preempt_restore_data_size, xrt::bo::flags::cacheable, 1 /* fix me */ };
      fill_bo_with_data(m_preempt_restore_bo, preempt_restore_data, false);

      if (is_dump_preemption_codes()) {
        std::string dump_file_name = "preemption_save_pre_patch" + std::to_string(get_id()) + ".bin";
        dump_bo(m_preempt_save_bo, dump_file_name);

        std::stringstream ss;
        ss << "dumped file " << dump_file_name;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());

        dump_file_name = "preemption_restore_pre_patch" + std::to_string(get_id()) + ".bin";
        dump_bo(m_preempt_restore_bo, dump_file_name);

        ss.clear();
        ss << "dumped file " << dump_file_name;
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }

    if ((preempt_save_data_size > 0) && (preempt_restore_data_size > 0)) {
      m_scratch_pad_mem = xrt::ext::bo{ m_hwctx, m_parent->get_scratch_pad_mem_size() };
      patch_instr(m_preempt_save_bo, Scratch_Pad_Mem_Symbol, 0, m_scratch_pad_mem,
                  xrt_core::patcher::buf_type::preempt_save, m_ctrl_code_id);
      patch_instr(m_preempt_restore_bo, Scratch_Pad_Mem_Symbol, 0, m_scratch_pad_mem,
                  xrt_core::patcher::buf_type::preempt_restore, m_ctrl_code_id);

      if (is_dump_preemption_codes()) {
        std::stringstream ss;
        ss << "patched preemption-codes using scratch_pad_mem at address " << std::hex << m_scratch_pad_mem.address() << " size " << std::hex << m_parent->get_scratch_pad_mem_size();
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
      }
    }

    // create ctrl scratchpad buffer and patch it if symbol is present
    try {
      auto size = m_parent->get_ctrl_scratch_pad_mem_size();
      if (size > 0) {
        m_ctrl_scratch_pad_mem = xrt::bo{ m_hwctx, size, xrt::bo::flags::cacheable, 1 /* fix me */ };
        patch_instr(m_instr_bo, Control_ScratchPad_Symbol, 0, m_ctrl_scratch_pad_mem,
                    xrt_core::patcher::buf_type::ctrltext, m_ctrl_code_id);
      }
    }
    catch (...) { /*Do Nothing*/ };

    // patch all pdi addresses
    auto pdi_symbols = parent->get_patch_pdis(m_ctrl_code_id);
    for (const auto& symbol : pdi_symbols) {
      auto& pdi_bo = m_parent->get_pdi_bo(symbol);
      if (!pdi_bo) {
        // pdi_bo doesn't exist create it
        const auto& pdi_data = parent->get_pdi(symbol);
        pdi_bo = xrt::bo{ m_hwctx, pdi_data.size(), xrt::bo::flags::cacheable, 1 /* fix me */ };
        fill_bo_with_data(pdi_bo, pdi_data);
      }
      // patch instr buffer with pdi address
      patch_instr(m_instr_bo, symbol, 0, pdi_bo, xrt_core::patcher::buf_type::ctrltext, m_ctrl_code_id);
    }

    if (m_ctrlpkt_bo) {
      patch_instr(m_instr_bo, Control_Packet_Symbol, 0, m_ctrlpkt_bo, xrt_core::patcher::buf_type::ctrltext, m_ctrl_code_id);
    }

    // patch ctrlpkt pm buffers
    for (const auto& dynsym : parent->get_ctrlpkt_pm_dynsyms()) {
      // get xrt::bo corresponding to this dynsym to patch its address in instr bo
      // convert sym name to sec name eg: ctrlpkt-pm-0 to .ctrlpkt.pm.0
      std::string sec_name = "." + dynsym;
      std::replace(sec_name.begin(), sec_name.end(), '-', '.');
      auto bo_itr = m_ctrlpkt_pm_bos.find(sec_name);
      if (bo_itr == m_ctrlpkt_pm_bos.end())
        throw std::runtime_error("Unable to find ctrlpkt pm buffer for symbol " + dynsym);
      patch_instr(m_instr_bo, dynsym, 0, bo_itr->second, xrt_core::patcher::buf_type::ctrltext, m_ctrl_code_id);
    }

    XRT_DEBUGF("<- module_sram::create_instr_buf()\n");
  }

  void
  create_ctrlpkt_buf(const module_impl* parent)
  {
    const auto& ctrl_pkt_buf = parent->get_ctrlpkt(m_ctrl_code_id);
    size_t sz = ctrl_pkt_buf.size();
    if (sz == 0) {
      XRT_DEBUGF("ctrpkt buf is empty\n");
      return;
    }

    m_ctrlpkt_bo = xrt::ext::bo{ m_hwctx, sz };
    fill_ctrlpkt_buf(m_ctrlpkt_bo, ctrl_pkt_buf, false /*don't sync*/);

    if (is_dump_control_packet()) {
      std::string dump_file_name = "ctr_packet_pre_patch" + std::to_string(get_id()) + ".bin";
      dump_bo(m_ctrlpkt_bo, dump_file_name);

      std::stringstream ss;
      ss << "dumped file " << dump_file_name;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", ss.str());
    }
  }

  void
  create_ctrlpkt_pm_bufs(const module_impl* parent)
  {
    const auto& ctrlpkt_pm_info = parent->get_ctrlpkt_pm_bufs();

    for (const auto& [key, buf] : ctrlpkt_pm_info) {
      m_ctrlpkt_pm_bos[key] = xrt::ext::bo{ m_hwctx, buf.size() };
      fill_bo_with_data(m_ctrlpkt_pm_bos[key], buf);
    }
  }

  // Create the instruction buffer with all columns data along with pad section
  void
  create_instruction_buffer(const module_impl* parent)
  {
    const auto& data = parent->get_data(m_ctrl_code_id);

    // create bo combined size of all ctrlcodes
    size_t sz = std::accumulate(data.begin(), data.end(), static_cast<size_t>(0), [](auto acc, const auto& ctrlcode) {
      return acc + ctrlcode.size();
      });
    if (sz == 0) {
      XRT_DEBUGF("ctrcode buf is empty\n");
      return;
    }

    m_buffer = xrt::bo{ m_hwctx, sz, xrt::bo::flags::cacheable, 1 /* fix me */ };

    fill_instruction_buffer(data);
  }

  // Patch the instruction buffer with global argument(xrt::bo)
  // The symbol to be patched is identified using argnm/index
  void
  patch_instr(xrt::bo& bo_ctrlcode, const std::string& argnm, size_t index, const xrt::bo& bo,
              xrt_core::patcher::buf_type type, uint32_t grp_idx)
  {
    patch_instr_value(bo_ctrlcode, argnm, index, bo.address(), type, grp_idx);
  }

  void
  patch_value(const std::string& argnm, size_t index, uint64_t value)
  {
    bool patched = false;
    if (m_parent->get_os_abi() == Elf_Amd_Aie2p) {
      // patch control-packet buffer
      if (m_ctrlpkt_bo) {
        if (m_parent->patch_it(m_ctrlpkt_bo, argnm, index, value, xrt_core::patcher::buf_type::ctrldata,
                               m_ctrl_code_id, m_first_patch))
          patched = true;
      }
      // patch instruction buffer
      if (m_parent->patch_it(m_instr_bo, argnm, index, value, xrt_core::patcher::buf_type::ctrltext,
                             m_ctrl_code_id, m_first_patch))
          patched = true;
    }
    else {
      if (m_parent->patch_it(m_buffer, argnm, index, value, xrt_core::patcher::buf_type::ctrltext,
                             m_ctrl_code_id, m_first_patch))
        patched = true;

      if (m_parent->patch_it(m_buffer, argnm, index, value, xrt_core::patcher::buf_type::pad,
                             m_ctrl_code_id, m_first_patch))
          patched = true;
    }

    if (patched) {
      m_patched_args.insert(argnm);
      m_dirty = true;
    }
  }

  bool
  patch_instr_value(xrt::bo& bo, const std::string& argnm, size_t index, uint64_t value,
                    xrt_core::patcher::buf_type type, uint32_t grp_idx)
  {
    if (!m_parent->patch_it(bo, argnm, index, value, type, grp_idx, m_first_patch))
      return false;

    m_dirty = true;
    return true;
  }

  // Check that all arguments have been patched and sync the buffer
  // to device if it is dirty.
  void
  sync_if_dirty() override
  {
    auto os_abi = m_parent.get()->get_os_abi();

    if (!m_dirty) {
      if (!m_first_patch)
        return;

      // its first run sync entire buffers
      if (os_abi == Elf_Amd_Aie2ps || os_abi == Elf_Amd_Aie2ps_group)
        m_buffer.sync(XCL_BO_SYNC_BO_TO_DEVICE);
      else if (os_abi == Elf_Amd_Aie2p) {
        m_instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        if (m_ctrlpkt_bo)
          m_ctrlpkt_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        if (m_preempt_save_bo && m_preempt_restore_bo) {
          m_preempt_save_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
          m_preempt_restore_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
      }
      return;
    }

    if (os_abi == Elf_Amd_Aie2ps || os_abi == Elf_Amd_Aie2ps_group) {
      if (m_patched_args.size() != m_parent->number_of_arg_patchers(m_ctrl_code_id)) {
        auto fmt = boost::format("ctrlcode requires %d patched arguments, but only %d are patched")
            % m_parent->number_of_arg_patchers(m_ctrl_code_id) % m_patched_args.size();
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
    }
    else if (os_abi == Elf_Amd_Aie2p) {
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
    }

    m_dirty = false;
    m_first_patch = false;
  }

  uint32_t*
  fill_ert_aie2p_preempt_data(uint32_t *payload) const
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
  fill_ert_aie2p_non_preempt_data(uint32_t *payload) const
  {
    auto npu = reinterpret_cast<ert_npu_data*>(payload);
    npu->instruction_buffer = m_instr_bo.address();
    npu->instruction_buffer_size = static_cast<uint32_t>(m_instr_bo.size());
    npu->instruction_prop_count = 0; // Reserved for future use
    payload += sizeof(ert_npu_data) / sizeof(uint32_t);

    return payload;
  }

  uint32_t*
  fill_ert_aie2ps(uint32_t *payload) const
  {
    auto ert_dpu_data_count = static_cast<uint16_t>(m_column_bo_address.size());
    // For multiple instruction buffers, the ert_dpu_data::chained has
    // the number of words remaining in the payload after the current
    // instruction buffer. The ert_dpu_data::chained of the last buffer
    // is zero.
    for (auto [ucidx, addr, size] : m_column_bo_address) {
      auto dpu = reinterpret_cast<ert_dpu_data*>(payload);
      dpu->instruction_buffer = addr;
      dpu->instruction_buffer_size = static_cast<uint32_t>(size);
      dpu->uc_index = ucidx;
      dpu->chained = --ert_dpu_data_count;
      payload += sizeof(ert_dpu_data) / sizeof(uint32_t);
    }

    return payload;
  }

  struct dtrace_util
  {
    using dlhandle = std::unique_ptr<void, decltype(&xrt_core::dlclose)>;
    dlhandle lib_hdl{nullptr, {}};
    std::string ctrl_file_path;
    std::string map_data;
  };

  // Function that returns true on successful initialization and false otherwise
  // Ideally exceptions should have been used but return status is used as ths
  // exception alternative is not good because in cases of failure dtrace
  // functionality is just a no-op.
  bool
  init_dtrace_helper(const module_impl* parent, dtrace_util& dtrace_obj)
  {
    // The APIs used for dynamic tracing in future will be checked into
    // AIEBU repo after Telluride related code is made public.
    // Till then we are using local library whose path is specified using
    // ini option. Instead of having the library as a dependency to XRT we
    // are using dlopen/dlsym methods.
    // TODO : remove ini options and dlopen/dlsym logic after the dtrace
    //        API's are integrated into AIEBU repo
    static auto dtrace_lib_path = xrt_core::config::get_dtrace_lib_path();
    if (dtrace_lib_path.empty())
      return false; // dtrace lib path is not set, dtrace is not enabled

    dtrace_obj.lib_hdl =
        dtrace_util::dlhandle{xrt_core::dlopen(dtrace_lib_path.c_str(), RTLD_LAZY),
                              &xrt_core::dlclose};
    if (!dtrace_obj.lib_hdl) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              "Failed to load dtrace library");
      return false;
    }

    static auto path = xrt_core::config::get_dtrace_control_file_path();
    if (!std::filesystem::exists(path)) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              "Dtrace control file is not accessible");
      return false;
    }
    dtrace_obj.ctrl_file_path = path;

    const auto& data = parent->get_dump_buf(m_ctrl_code_id);
    if (data.size() == 0) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              "Dump section is empty in ELF");
      return false;
    }

    dtrace_obj.map_data = std::string{reinterpret_cast<const char*>(data.data()), data.size()};
    return true;
  }

  void
  initialize_dtrace_buf(const module_impl* parent)
  {
    dtrace_util dtrace;
    if (!init_dtrace_helper(parent, dtrace))
      return; // init failure

    // dtrace is enabled and library is opened successfully
    // Below code gets function pointers for functions
    // get_dtrace_col_number, get_dtrace_buffer_size and populate_dtrace_buffer
    using get_dtrace_col_numbers_fun = uint32_t (*)(const char*, const char*, uint32_t*);
    auto get_dtrace_col_numbers = reinterpret_cast<get_dtrace_col_numbers_fun>
        (xrt_core::dlsym(dtrace.lib_hdl.get(), "get_dtrace_col_numbers"));
    if (!get_dtrace_col_numbers) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", xrt_core::dlerror());
      return;
    }

    using get_dtrace_buffer_size_fun = void (*)(uint64_t*);
    auto get_dtrace_buffer_size = reinterpret_cast<get_dtrace_buffer_size_fun>
        (xrt_core::dlsym(dtrace.lib_hdl.get(), "get_dtrace_buffer_size"));
    if (!get_dtrace_buffer_size) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", xrt_core::dlerror());
      return;
    }

    using populate_dtrace_buffer_fun = void (*)(uint32_t*, uint64_t);
    auto populate_dtrace_buffer = reinterpret_cast<populate_dtrace_buffer_fun>
       (xrt_core::dlsym(dtrace.lib_hdl.get(), "populate_dtrace_buffer"));
    if (!populate_dtrace_buffer) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", xrt_core::dlerror());
      return;
    }

    // using function ptr get dtrace control buffer size
    uint32_t buffers_length = 0;
    if (get_dtrace_col_numbers(dtrace.ctrl_file_path.c_str(), dtrace.map_data.c_str(),
                               &buffers_length) != 0) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              "[dtrace] : Failed to get col numbers");
      return;
    }

    if (!buffers_length) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              "[dtrace] : Control buffer size is zero, no dtrace o/p");
      return;
    }

    // control buf size is empty, control script does nothing.
    // Create xrt::bo's with size returned and call the API to fill the buffers with
    // required data, buffers are synced after data is filled.
    try {
      std::vector<uint64_t> buffers(buffers_length);
      get_dtrace_buffer_size(buffers.data());

      std::map<uint32_t, size_t> buf_sizes;
      size_t total_size = 0;

      constexpr uint32_t mask32 = 0xffffffff;
      constexpr uint32_t shift32 = 32;
      for (const auto& entry : buffers) {
        //for each entry, lower 32 is the uc index, and upper 32 is the length in word for that uc
        buf_sizes[static_cast<uint32_t>(entry & mask32)] = static_cast<size_t>(entry >> shift32);
	total_size += static_cast<size_t>(entry >> shift32);
      }
      // below call creates dtrace xrt control buffer and informs driver / firmware with the buffer address
      m_dtrace_ctrl_bo = xrt_core::bo_int::create_bo(m_hwctx,
                                                     total_size * sizeof(uint32_t),
                                                     xrt_core::bo_int::use_type::dtrace);
      // fill data by calling dtrace library API
      populate_dtrace_buffer(m_dtrace_ctrl_bo.map<uint32_t*>(), m_dtrace_ctrl_bo.address());

      // sync dtrace control buffer
      m_dtrace_ctrl_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

      xrt_core::bo_int::config_bo(m_dtrace_ctrl_bo, buf_sizes);

      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              "[dtrace] : dtrace buffers initialized successfully");
    }
    catch (const std::exception &e) {
      m_dtrace_ctrl_bo = {};
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              std::string{"[dtrace] : dtrace buffers initialization failed, "} + e.what());
    }
  }

public:
  module_sram(std::shared_ptr<module_impl> parent, xrt::hw_context hwctx, uint32_t id = xrt_core::module_int::no_ctrl_code_id)
    : module_impl{ parent->get_cfg_uuid() }
    , m_parent{ std::move(parent) }
    , m_hwctx{ std::move(hwctx) }
    , m_ctrl_code_id{ std::move(id) }
  {
    if (xrt_core::config::get_xrt_debug()) {
      m_debug_mode.debug_flags.dump_control_codes = xrt_core::config::get_feature_toggle("Debug.dump_control_codes");
      m_debug_mode.debug_flags.dump_control_packet = xrt_core::config::get_feature_toggle("Debug.dump_control_packet");
      m_debug_mode.debug_flags.dump_preemption_codes = xrt_core::config::get_feature_toggle("Debug.dump_preemption_codes");
      static std::atomic<uint32_t> s_id {0};
      m_id = s_id++;
    }

    auto os_abi = m_parent.get()->get_os_abi();

    if (os_abi == Elf_Amd_Aie2p) {
      // make sure to create control-packet buffer first because we may
      // need to patch control-packet address to instruction buffer
      create_ctrlpkt_buf(m_parent.get());
      create_ctrlpkt_pm_bufs(m_parent.get());
      create_instr_buf(m_parent.get());
      fill_bo_addresses();
    }
    else if (os_abi == Elf_Amd_Aie2ps || os_abi == Elf_Amd_Aie2ps_group) {
      initialize_dtrace_buf(m_parent.get());
      create_instruction_buffer(m_parent.get());
      fill_column_bo_address(m_parent->get_data(m_ctrl_code_id));
    }
  }

  uint32_t*
  fill_ert_dpu_data(uint32_t *payload) const override
  {
    auto os_abi = m_parent.get()->get_os_abi();

    switch (os_abi) {
    case Elf_Amd_Aie2p :
      if ((m_preempt_save_bo && m_preempt_restore_bo) || ::is_group_elf(m_parent->get_elfio_object()))
        return fill_ert_aie2p_preempt_data(payload);
      else
        return fill_ert_aie2p_non_preempt_data(payload);
    case Elf_Amd_Aie2ps :
    case Elf_Amd_Aie2ps_group :
      return fill_ert_aie2ps(payload);
    default :
      throw std::runtime_error("unknown ELF type passed\n");
    }
  }

  xrt::bo&
  get_scratch_pad_mem() override
  {
    return m_scratch_pad_mem;
  }

  void
  dump_scratchpad_mem()
  {
    if (m_scratch_pad_mem.size() == 0) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              "preemption scratchpad memory is not available");
      return;
    }

    // sync data from device before dumping into file
    m_scratch_pad_mem.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    std::string dump_file_name = "preemption_scratchpad_mem" + std::to_string(get_id()) + ".bin";
    dump_bo(m_scratch_pad_mem, dump_file_name);

    std::string msg {"dumped file "};
    msg.append(dump_file_name);
    xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", msg);
  }

  void
  patch(const std::string& argnm, size_t index, const xrt::bo& bo) override
  {
    patch_value(argnm, index, bo.address());
  }

  void
  patch(const std::string& argnm, size_t index, const void* value, size_t size) override
  {
    if (size > 8) // NOLINT
      throw std::runtime_error{ "patch_value() only supports 64-bit values or less" };
    
    auto arg_value = *static_cast<const uint64_t*>(value);
    patch_value(argnm, index, arg_value);
  }

  void
  dump_dtrace_buffer()
  {
    dtrace_util dtrace;
    if (!m_dtrace_ctrl_bo || !init_dtrace_helper(m_parent.get(), dtrace))
      return;

    // sync dtrace buffers output from device
    m_dtrace_ctrl_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    // Get function pointer to create result file
    using get_dtrace_result_file_fun = void (*)(const char*);
    auto get_dtrace_result_file = reinterpret_cast<get_dtrace_result_file_fun>
      (xrt_core::dlsym(dtrace.lib_hdl.get(), "get_dtrace_result_file"));
    if (!get_dtrace_result_file) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module", xrt_core::dlerror());
      return;
    }

    try {
      // dtrace output is dumped into current working directory
      // output is a python file
      auto result_file_path = std::filesystem::current_path().string() + "/dtrace_dump_" + std::to_string(get_id()) + ".py";
      get_dtrace_result_file(result_file_path.c_str());

      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              std::string{"[dtrace] : dtrace buffer dumped successfully to - "} + result_file_path);
    }
    catch (const std::exception &e) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_module",
                              std::string{"[dtrace] : dtrace buffer dump failed, "} + e.what());
    }
  }

  xrt::bo
  get_ctrl_scratchpad_bo()
  {
    if (!m_ctrl_scratch_pad_mem)
      throw std::runtime_error("Control scratchpad memory is not present\n");
  
    // sync bo data before returning
    m_ctrl_scratch_pad_mem.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    return m_ctrl_scratch_pad_mem;
  }
};

} // namespace xrt

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal module APIs
////////////////////////////////////////////////////////////////
namespace xrt_core::module_int {

xrt::module
create_run_module(const xrt::module& parent, const xrt::hw_context& hwctx, uint32_t ctrl_code_id)
{
  return xrt::module{std::make_shared<xrt::module_sram>(parent.get_handle(), hwctx, ctrl_code_id)};
}

uint32_t
get_ctrlcode_id(const xrt::module& module, const std::string& kname)
{
  return module.get_handle()->get_ctrlcode_id(kname);
}

uint32_t*
fill_ert_dpu_data(const xrt::module& module, uint32_t* payload)
{
  return module.get_handle()->fill_ert_dpu_data(payload);
}

void
patch(const xrt::module& module, const std::string& argnm, size_t index, const xrt::bo& bo)
{
  auto module_sram = std::dynamic_pointer_cast<xrt::module_sram>(module.get_handle());
  if (!module_sram)
    throw std::runtime_error("Getting module_sram failed, wrong module object passed\n");
  module_sram->patch(argnm, index, bo);
}

// This function is used to get patch buffer size based on buf type passed.
// It is used with internal test cases that verifies shim functionality.
size_t
get_patch_buf_size(const xrt::module& module, xrt_core::patcher::buf_type type, uint32_t id)
{
  auto handle = module.get_handle();
  auto os_abi = handle->get_os_abi();

  if (os_abi == Elf_Amd_Aie2p) {
    switch (type) {
      case xrt_core::patcher::buf_type::ctrltext :
        return handle->get_instr(id).size();
      case xrt_core::patcher::buf_type::ctrldata :
        return handle->get_ctrlpkt(id).size();
      case xrt_core::patcher::buf_type::preempt_save :
        return handle->get_preempt_save(id).size();
      case xrt_core::patcher::buf_type::preempt_restore :
        return handle->get_preempt_restore(id).size();
      default :
        throw std::runtime_error("Unknown buffer type passed");
    }
  }
  else if(os_abi == Elf_Amd_Aie2ps || os_abi == Elf_Amd_Aie2ps_group) {
    if (type != xrt_core::patcher::buf_type::ctrltext)
      throw std::runtime_error("Info of given buffer type not available");

    const auto& instr_buf = handle->get_data(id);
    if (instr_buf.empty())
      throw std::runtime_error{"No control code found for given id"};

    if (instr_buf.size() != 1)
      throw std::runtime_error{"multiple column ctrlcode is not supported in this flow"};

    return instr_buf[0].size();
  }

  throw std::runtime_error{"Unsupported ELF ABI"};
}

// This function is used for patching buffers at shim level
// It is used with internal test cases that verifies shim functionality.
void
patch(const xrt::module& module, uint8_t* ibuf, size_t sz, const std::vector<std::pair<std::string, uint64_t>>* args,
      xrt_core::patcher::buf_type type, uint32_t id)
{
  auto hdl = module.get_handle();
  const buf* inst = nullptr;
  uint32_t patch_index = id;
  auto os_abi = hdl->get_os_abi();

  if (os_abi == Elf_Amd_Aie2p) {
    switch (type) {
    case xrt_core::patcher::buf_type::ctrltext : {
      inst = &(hdl->get_instr(id));
      break;
    }
    case xrt_core::patcher::buf_type::ctrldata : {
      inst = &(hdl->get_ctrlpkt(id));
      break;
    }
    case xrt_core::patcher::buf_type::preempt_save : {
      inst = &(hdl->get_preempt_save(id));
      break;
    }
    case xrt_core::patcher::buf_type::preempt_restore : {
      inst = &(hdl->get_preempt_restore(id));
      break;
    }
    default :
      throw std::runtime_error("Unknown buffer type passed");
      break;
    }
  }
  else if(os_abi == Elf_Amd_Aie2ps || os_abi == Elf_Amd_Aie2ps_group) {
    const auto& instr_buf = hdl->get_data(id);

    if (instr_buf.empty())
      throw std::runtime_error{"No control code found for given id"};
    if (instr_buf.size() != 1)
      throw std::runtime_error{"Patch failed: only support patching single column"};

    inst = &instr_buf[0];
  }
  else {
    throw std::runtime_error{"Patch failed: unsupported ELF ABI"};
  }

  if (sz < inst->size())
    throw std::runtime_error{"Control code buffer passed in is too small"}; // Need a bigger buffer.
  std::memcpy(ibuf, inst->data(), sz);

  size_t index = 0;
  for (auto& [arg_name, arg_addr] : *args) {
    if (!hdl->patch_it(ibuf, arg_name, index, arg_addr, type, patch_index))
      throw std::runtime_error{"Failed to patch " + arg_name};
    index++;
  }
}

void
patch(const xrt::module& module, const std::string& argnm, size_t index, const void* value, size_t size)
{
  auto module_sram = std::dynamic_pointer_cast<xrt::module_sram>(module.get_handle());
  if (!module_sram)
    throw std::runtime_error("Getting module_sram failed, wrong module object passed\n");
  module_sram->patch(argnm, index, value, size);
}

void
sync(const xrt::module& module)
{
  module.get_handle()->sync_if_dirty();
}

enum ert_cmd_opcode
get_ert_opcode(const xrt::module& module)
{
  return module.get_handle()->get_ert_opcode();
}

void
dump_scratchpad_mem(const xrt::module& module)
{
  auto module_sram = std::dynamic_pointer_cast<xrt::module_sram>(module.get_handle());
  if (!module_sram)
    throw std::runtime_error("Getting module_sram failed, wrong module object passed\n");

  module_sram->dump_scratchpad_mem();
}

const std::vector<xrt_core::module_int::kernel_info>&
get_kernels_info(const xrt::module& module)
{
  return module.get_handle()->get_kernels_info();
}

void
dump_dtrace_buffer(const xrt::module& module)
{
  auto module_sram = std::dynamic_pointer_cast<xrt::module_sram>(module.get_handle());
  if (!module_sram)
    throw std::runtime_error("Getting module_sram failed, wrong module object passed\n");

  module_sram->dump_dtrace_buffer();
}

xrt::bo
get_ctrl_scratchpad_bo(const xrt::module& module)
{
  auto module_sram = std::dynamic_pointer_cast<xrt::module_sram>(module.get_handle());
  if (!module_sram)
    throw std::runtime_error("Getting module_sram failed, wrong module object passed\n");

  return module_sram->get_ctrl_scratchpad_bo();
}
} // xrt_core::module_int

namespace {

static std::shared_ptr<xrt::module_elf>
construct_module_elf(const xrt::elf& elf)
{
  auto os_abi = xrt_core::elf_int::get_elfio(elf).get_os_abi();
  switch (os_abi) {
  case Elf_Amd_Aie2p :
    return std::make_shared<xrt::module_elf_aie2p>(elf);
  case Elf_Amd_Aie2ps :
  case Elf_Amd_Aie2ps_group :
    return std::make_shared<xrt::module_elf_aie2ps>(elf);
  default :
    throw std::runtime_error("unknown ELF type passed\n");
  }
}

} // namespace

////////////////////////////////////////////////////////////////
// xrt_module C++ API implementation (xrt_module.h)
////////////////////////////////////////////////////////////////
namespace xrt {

module::
module(const xrt::elf& elf)
: detail::pimpl<module_impl>(construct_module_elf(elf))
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
