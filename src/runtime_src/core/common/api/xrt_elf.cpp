// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_elf.h
#define XRT_API_SOURCE         // exporting xrt_elf.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "xrt/experimental/xrt_aie.h"
#include "xrt/experimental/xrt_elf.h"
#include "xrt/xrt_uuid.h"

#include "elf_int.h"
#include "elf_patcher.h"
#include "core/common/config_reader.h"
#include "core/common/error.h"
#include "core/common/message.h"
#include "core/common/xclbin_parser.h"

#include <boost/interprocess/streams/bufferstream.hpp>
#include <elfio/elfio.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// Use the constant from elf_int.h
using xrt_core::elf_int::no_ctrl_code_id;

static constexpr size_t
operator"" _kb(unsigned long long v)  { return 1024u * v; } // NOLINT

///////////////////////////////////////////////////////////////
// Helper functions for kernel signature demangling and parsing
///////////////////////////////////////////////////////////////

// Mangled name prefix length for Itanium ABI
static constexpr size_t mangled_prefix_length = 2;
static constexpr size_t decimal_base = 10;

// Split string by delimiter
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

// Basic Itanium ABI type decoding
static std::string
get_demangle_type(char c)
{
  static const std::map<char, std::string> demangle_type_map = {
    {'v', "void"},
    {'c', "char"},
    {'i', "int"}
  };
  auto it = demangle_type_map.find(c);
  if (it == demangle_type_map.end())
    throw std::runtime_error("Unknown type character in mangled name: " + std::string(1, c));
  return it->second;
}

// Function to demangle kernel name
// Parse mangled name in Itanium ABI style: _Z<length><name><types>
// length : number of characters in the name string.
// name   : kernel name in string
// types  : kernel argument data type as below.
//  'c' represents the arg is a char.
//  'v' represents the arg is a void.
//  'i' represents the arg is an int.
//  'P' represents the arg is a pointer.
// Hence, "Pc" = char*, "Pv" = void*, "Pi" = int*, "PPc" = char**, etc.
static std::string
demangle(const std::string& mangled)
{
  // Check if mangled prefix "_Z" is present and length is greater than mangled_prefix_length
  if (mangled.size() <= mangled_prefix_length || mangled.substr(0, mangled_prefix_length) != "_Z")
    throw std::runtime_error("Doesn't have prefix _Z, not a mangled kernel name");

  size_t idx = 2;
  size_t len = 0;

  // Extract length of function name
  while (idx < mangled.size() && std::isdigit(mangled[idx]))
    len = len * decimal_base + (mangled[idx++] - '0');

  if (idx + len > mangled.size())
    throw std::runtime_error("Invalid mangled name, doesn't have expected kernel name length");

  std::string name = mangled.substr(idx, len);
  idx += len;
  std::vector<std::string> args;

  // Parse the argument types from the mangled name
  // Each argument can have multiple 'P' prefixes indicating pointer depth
  // followed by a single character representing the base type
  while (idx < mangled.size()) {
    int pointer_depth = 0;
    // Count pointer depth (number of 'P' characters indicating pointer levels)
    // For example: "P" = 1 level (char*), "PP" = 2 levels (char**), etc.
    while (idx < mangled.size() && mangled[idx] == 'P') {
      ++pointer_depth;
      ++idx;
    }
    if (idx >= mangled.size())
      throw std::runtime_error("demangle arg index out of bounds");

    std::string type = get_demangle_type(mangled[idx++]);
    for (int i = 0; i < pointer_depth; ++i)
      type += "*";
    args.push_back(type);
  }

  // Append arguments to the function name
  std::string result = name + "(";
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0)
      result += ", ";
    result += args[i];
  }
  result += ")";

  return result;
}

// Function that constructs kernel arguments from signature
// kernel signature has name followed by args
// kernel signature - name(argtype, argtype ...)
// eg : DPU(char*, char*, char*)
static std::vector<xrt::xarg>
construct_kernel_args(const std::string& signature)
{
  std::vector<xrt::xarg> args;

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
    xrt::xarg arg;
    arg.name = "argv" + std::to_string(count);
    arg.hosttype = "no-type";
    arg.port = "no-port";
    arg.index = count;
    arg.offset = offset;
    arg.dir = xrt::xarg::direction::input;
    // if arg has pointer(*) in its name (eg: char*, void*) it is of type global otherwise scalar
    arg.type = (str.find('*') != std::string::npos)
      ? xrt::xarg::argtype::global
      : xrt::xarg::argtype::scalar;

    // At present only global args are supported
    // TODO : Add support for scalar args in ELF flow
    if (arg.type == xrt::xarg::argtype::scalar)
      throw std::runtime_error("scalar args are not yet supported for this kind of kernel");
    else {
      // global arg
      static constexpr size_t global_arg_size = 0x8;
      arg.size = global_arg_size;

      offset += global_arg_size;
    }

    args.emplace_back(std::move(arg));
    count++;
  }
  return args;
}

///////////////////////////////////////////////////////////////
// ELFIO loading functions - used overloaded functions instead
// of template to display specific error messages
///////////////////////////////////////////////////////////////

// Load ELFIO from file path
static ELFIO::elfio
load_elfio(const std::string& fnm)
{
  ELFIO::elfio elfio;
  if (!elfio.load(fnm))
    throw std::runtime_error(fnm + " is not found or is not a valid ELF file");

  if (xrt_core::config::get_xrt_debug()) {
    std::string message = "Loaded elf file " + fnm;
    xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_elf", message);
  }
  return elfio;
}

// Load ELFIO from stream
static ELFIO::elfio
load_elfio(std::istream& stream)
{
  ELFIO::elfio elfio;
  if (!elfio.load(stream))
    throw std::runtime_error("not a valid ELF stream");
  return elfio;
}

// Load ELFIO from buffer
static ELFIO::elfio
load_elfio(const void* data, size_t size)
{
  ELFIO::elfio elfio;
  boost::interprocess::ibufferstream istr(static_cast<const char*>(data), size);
  if (!elfio.load(istr))
    throw std::runtime_error("not valid ELF data");
  return elfio;
}

} // namespace

namespace xrt {

////////////////////////////////////////////////////////////////
// buf::append_section_data implementation
////////////////////////////////////////////////////////////////
void
buf::append_section_data(const ELFIO::section* sec)
{
  auto sz = sec->get_size();
  auto sdata = sec->get_data();
  m_data.insert(m_data.end(), sdata, sdata + sz);
}

class elf::kernel_impl
{
  std::string m_name;
  // Using kernel_argument, kernel_properties from xclbin codebase
  // as xrt::kernel_impl uses it most of the places, hard to decouple it
  // TODO : Remove this once ELF flow is stable
  std::vector<xrt::xarg> m_args;
  xrt_core::xclbin::kernel_properties m_properties;
  std::vector<elf::kernel::instance> m_instances;

  xrt_core::xclbin::kernel_properties
  construct_properties(const std::string& name) const
  {
    xrt_core::xclbin::kernel_properties properties;
    properties.name = name;
    properties.type = xrt_core::xclbin::kernel_properties::kernel_type::dpu;

    return properties;
  }

public:
  explicit
  kernel_impl(std::string name, std::vector<xrt::xarg> args,
              std::vector<elf::kernel::instance> instances)
    : m_name{std::move(name)}
    , m_args{std::move(args)}
    , m_properties{construct_properties(m_name)}
    , m_instances{std::move(instances)}
  {}

  std::string
  get_name() const
  {
    return m_name;
  }

  std::vector<elf::kernel::instance>
  get_instances() const
  {
    return m_instances;
  }

  std::vector<xrt::xarg>
  get_args() const
  {
    return m_args;
  }

  size_t
  get_num_args() const
  {
    return m_args.size();
  }

  elf::kernel::data_type
  get_arg_data_type(size_t index) const
  {
    return (m_args[index].type == xrt::xarg::argtype::global)
      ? elf::kernel::data_type::global
      : elf::kernel::data_type::scalar;
  }

  std::pair<xrt_core::xclbin::kernel_properties, std::vector<xrt::xarg>>
  get_properties_and_args() const
  {
    return {m_properties, m_args};
  }
};

class elf::kernel::instance_impl
{
  std::string m_name;

public:
  explicit
  instance_impl(std::string name)
    : m_name{std::move(name)}
  {}

  std::string
  get_name() const
  {
    return m_name;
  }
};

////////////////////////////////////////////////////////////////
// elf_impl method implementations
// (class declaration is in elf_int.h)
////////////////////////////////////////////////////////////////

// Protected constructor
elf_impl::
elf_impl(ELFIO::elfio&& elfio)
  : m_elfio{std::move(elfio)}
{}

// Get symbol information from .symtab at given index
elf_impl::symbol_info
elf_impl::
get_symbol_from_symtab(uint32_t sym_index) const
{
  ELFIO::section* symtab = m_elfio.sections[".symtab"];
  if (!symtab)
    throw std::runtime_error("No .symtab section found");

  const ELFIO::symbol_section_accessor symbols(m_elfio, symtab);

  symbol_info info;
  ELFIO::Elf64_Addr value{};
  ELFIO::Elf_Xword size{};
  unsigned char bind{};
  unsigned char other{};

  if (!symbols.get_symbol(sym_index, info.name, value, size, bind,
                          info.type, info.section_index, other))
    throw std::runtime_error("Unable to find symbol in .symtab section with index: " +
                             std::to_string(sym_index));

  return info;
}

// Extract kernel name from demangled signature
std::string
elf_impl::
extract_kernel_name(const std::string& signature)
{
  auto pos = signature.find('(');
  return (pos == std::string::npos) ? signature : signature.substr(0, pos);
}

// Check if kernel already exists in m_kernel_args_map
bool
elf_impl::
kernel_exists(const std::string& kernel_name) const
{
  return m_kernel_args_map.find(kernel_name) != m_kernel_args_map.end();
}

// Add kernel arguments to m_kernel_args_map during parsing
void
elf_impl::
add_kernel_info(const std::string& kernel_name, const std::string& signature)
{
  m_kernel_args_map[kernel_name] = construct_kernel_args(signature);
}

// Build elf::kernel objects from collected kernel data
void
elf_impl::
finalize_kernels()
{
  for (const auto& [kernel_name, args] : m_kernel_args_map) {
    // Build instance objects from subkernel names
    std::vector<elf::kernel::instance> instances;
    auto it = m_kernel_to_subkernels_map.find(kernel_name);
    if (it != m_kernel_to_subkernels_map.end()) {
      for (const auto& subkernel_name : it->second) {
        instances.emplace_back(
          std::make_shared<elf::kernel::instance_impl>(subkernel_name));
      }
    }

    // Create kernel with name, args, and instances
    m_kernels.emplace_back(
      std::make_shared<elf::kernel_impl>(kernel_name, args, std::move(instances)));
  }
}

// Parse .symtab section to extract kernel and subkernel information
std::pair<std::string, std::string>
elf_impl::
get_kernel_subkernel_from_symtab(uint32_t sym_index)
{
  // Get subkernel symbol - must be of type OBJECT
  auto subkernel_sym = get_symbol_from_symtab(sym_index);
  if (subkernel_sym.type != ELFIO::STT_OBJECT)
    throw std::runtime_error("Symbol doesn't point to subkernel entry (expected STT_OBJECT)");

  // Get parent kernel symbol - must be of type FUNC
  auto kernel_sym = get_symbol_from_symtab(subkernel_sym.section_index);
  if (kernel_sym.type != ELFIO::STT_FUNC)
    throw std::runtime_error("Subkernel doesn't point to kernel entry (expected STT_FUNC)");

  // Demangle kernel name and extract signature
  auto demangled_signature = demangle(kernel_sym.name);
  auto kernel_name = extract_kernel_name(demangled_signature);

  // Cache kernel info if not already present
  if (!kernel_exists(kernel_name))
    add_kernel_info(kernel_name, demangled_signature);

  return {kernel_name, subkernel_sym.name};
}

// Initialize maps for legacy ELF without .group sections
void
elf_impl::
init_legacy_section_maps()
{
  std::vector<uint32_t> all_section_ids;

  for (const auto& sec : m_elfio.sections) {
    auto sec_id = sec->get_index();
    all_section_ids.push_back(sec_id);
    m_section_to_group_map[sec_id] = no_ctrl_code_id;
  }

  // Use empty string as kernel name for legacy ELF
  m_kernel_name_to_id_map[""] = no_ctrl_code_id;
  m_group_to_sections_map.emplace(no_ctrl_code_id, std::move(all_section_ids));
}

// Parse a single .group section and update maps
void
elf_impl::
parse_single_group_section(const ELFIO::section* section)
{
  const auto* data = section->get_data();
  const auto size = section->get_size();
  auto group_id = section->get_index();

  // .group section data: flags followed by section indices
  if (data == nullptr || size < sizeof(ELFIO::Elf_Word))
    return;

  // Parse kernel/subkernel from symtab using .group's info field
  auto [kernel_name, subkernel_name] = get_kernel_subkernel_from_symtab(section->get_info());

  // Update kernel maps
  m_kernel_to_subkernels_map[kernel_name].push_back(subkernel_name);
  m_kernel_name_to_id_map[kernel_name + subkernel_name] = group_id;

  // Parse member section indices (skip flags at index 0)
  const auto* word_data = reinterpret_cast<const ELFIO::Elf_Word*>(data);
  const auto word_count = size / sizeof(ELFIO::Elf_Word);

  std::vector<uint32_t> member_sections;
  for (std::size_t i = 1; i < word_count; ++i) {
    auto member_id = word_data[i];
    member_sections.push_back(member_id);
    m_section_to_group_map[member_id] = group_id;
  }

  m_group_to_sections_map.emplace(group_id, std::move(member_sections));
}

// Parse .group sections in the ELF file and populate all maps
void
elf_impl::
parse_group_sections()
{
  if (!is_group_elf()) {
    init_legacy_section_maps();
    finalize_kernels();
    return;
  }

  for (const auto& section : m_elfio.sections) {
    if (section && section->get_type() == ELFIO::SHT_GROUP)
      parse_single_group_section(section.get());
  }

  // Build elf::kernel objects after all group sections are parsed
  finalize_kernels();
}

// Get configuration UUID from ELF
xrt::uuid
elf_impl::
get_cfg_uuid() const
{
  return {}; // tbd
}

// Extract section data by name
std::vector<uint8_t>
elf_impl::
get_section(const std::string& sname)
{
  auto sec = m_elfio.sections[sname];
  if (!sec)
    throw std::runtime_error("Failed to find section: " + sname);

  auto data = sec->get_data();
  std::vector<uint8_t> vec(data, data + sec->get_size());
  return vec;
}

// Get note data from ELF section
std::string
elf_impl::
get_note(const ELFIO::section* section, ELFIO::Elf_Word note_num) const
{
  ELFIO::note_section_accessor accessor(m_elfio, const_cast<ELFIO::section*>(section));
  ELFIO::Elf_Word type = 0;
  std::string name;
  char* desc = nullptr;
  ELFIO::Elf_Word desc_size = 0;
  if (!accessor.get_note(note_num, type, name, desc, desc_size))
    throw std::runtime_error("Failed to get note, note not found\n");
  return std::string{desc, desc_size};
}

// Get partition size from ELF notes
uint32_t
elf_impl::
get_partition_size() const
{
  auto section = m_elfio.sections[".note.xrt.configuration"];
  if (!section)
    throw std::runtime_error("ELF is missing xrt configuration info\n");

  uint32_t value = 0;
  auto data = get_note(section, 0);
  std::memcpy(&value, data.data(), std::min(data.size(), sizeof(uint32_t)));
  return value;
}

// Check if this is a full ELF
bool
elf_impl::
is_full_elf() const
{
  return m_elfio.sections[".note.xrt.configuration"] != nullptr;
}

// Get ABI version as (major, minor) pair
std::pair<uint8_t, uint8_t>
elf_impl::
get_abi_version() const
{
  constexpr uint8_t major_ver_mask = 0xF0;
  constexpr uint8_t minor_ver_mask = 0x0F;
  constexpr uint8_t shift = 4;

  auto abi_version = m_elfio.get_abi_version();
  auto major = static_cast<uint8_t>((abi_version & major_ver_mask) >> shift);
  auto minor = static_cast<uint8_t>(abi_version & minor_ver_mask);
  return {major, minor};
}

////////////////////////////////////////////////////////////////
// Derived class for AIE2P platform
////////////////////////////////////////////////////////////////
class elf_aie2p : public elf_impl
{
  // Elfs can have multiple kernels and its corresponding sub kernels
  // We use ctrl code id or grp idx to represent kernel + sub kernel combination
  // Below maps store data for each ctrl code id
  // key - ctrl code id, value - section buffer data
  std::map<uint32_t, instr_buf> m_instr_buf_map;
  std::map<uint32_t, control_packet> m_ctrl_packet_map;
  std::map<uint32_t, buf> m_save_buf_map;
  std::map<uint32_t, buf> m_restore_buf_map;

  // Elfs with preemption sections uses different opcode
  // Below variable is set to true if these sections exist
  bool m_preemption_exist = false;

  // maps related to PDI sections
  std::map<std::string, buf> m_pdi_buf_map; // pdi_section_name (.pdi.*) -> section buffer data
  // map that stores pdi symbols that needs patching in corresponding ctrl codes
  // key - ctrl code id, value - set of pdi symbols that needs patching
  std::map<uint32_t, std::unordered_set<std::string>> m_ctrl_pdi_map;

  // size of control scratch pad memory
  size_t m_ctrl_scratch_pad_mem_size = 0;

  // set of preemption dynsyms in elf
  std::set<std::string> m_ctrlpkt_pm_dynsyms;
  // map storing preemption section buffers
  // key - preemption section name (.ctrlpkt.pm.*)
  // value - section buffer data
  std::map<std::string, buf> m_ctrlpkt_pm_bufs;

  ////////////////////////////////////////////////////////////////
  // Buffer initialization helper functions
  ////////////////////////////////////////////////////////////////

  // Initialize buffer map for given section type
  void
  initialize_section_buf_map(patcher_buf_type type, std::map<uint32_t, buf>& buf_map)
  {
    auto section_pattern = xrt_core::elf_patcher::get_section_name(type);
    for (const auto& sec : m_elfio.sections) {
      auto name = sec->get_name();
      if (name.find(section_pattern) == std::string::npos)
        continue;

      auto grp_idx = m_section_to_group_map[sec->get_index()];
      buf_map[grp_idx].append_section_data(sec.get());
    }
  }

  // Initialize preempt save/restore buffers from ELF sections
  void
  initialize_save_restore_buf_map()
  {
    auto save_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::preempt_save);
    auto restore_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::preempt_restore);

    for (const auto& [grp_id, sec_ids] : m_group_to_sections_map) {
      bool has_save = false;
      bool has_restore = false;

      for (auto sec_idx : sec_ids) {
        const auto& sec = m_elfio.sections[sec_idx];
        auto name = sec->get_name();

        if (name.find(save_pattern) != std::string::npos) {
          m_save_buf_map[grp_id].append_section_data(sec);
          has_save = true;
        }
        else if (name.find(restore_pattern) != std::string::npos) {
          m_restore_buf_map[grp_id].append_section_data(sec);
          has_restore = true;
        }
      }

      if (has_save != has_restore)
        throw std::runtime_error("Invalid ELF: preempt save and restore sections are not paired");

      // If both save and restore sections are found, we set preemption exist flag
      // At present this flag is set at Elf level assuming if one kernel + sub kernel has these
      // sections, all others will have these sections
      // TODO : Move this logic to group(kernel + sub kernel) level
      if (has_save && has_restore)
        m_preemption_exist = true;
    }
  }

  // Initialize PDI buffers from ELF sections
  void
  initialize_pdi_buf_map()
  {
    auto pdi_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::pdi);
    for (const auto& sec : m_elfio.sections) {
      auto name = sec->get_name();
      if (name.find(pdi_pattern) == std::string::npos)
        continue;

      m_pdi_buf_map[name].append_section_data(sec.get());
    }
  }

  // Initialize control packet preemption buffers from ELF sections
  void
  initialize_ctrlpkt_pm_buf_map()
  {
    auto pm_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::ctrlpkt_pm);
    for (const auto& sec : m_elfio.sections) {
      auto name = sec->get_name();
      if (name.find(pm_pattern) == std::string::npos)
        continue;

      m_ctrlpkt_pm_bufs[name].append_section_data(sec.get());
    }
  }

  // Initialize section buffers
  void
  initialize_section_buffer_maps()
  {
    initialize_section_buf_map(patcher_buf_type::ctrltext, m_instr_buf_map);
    initialize_section_buf_map(patcher_buf_type::ctrldata, m_ctrl_packet_map);
    initialize_save_restore_buf_map();
    initialize_pdi_buf_map();
    initialize_ctrlpkt_pm_buf_map();
  }

  ////////////////////////////////////////////////////////////////
  // Argument patcher initialization
  ////////////////////////////////////////////////////////////////

  // Determine section type and size based on section name
  std::pair<size_t, patcher_buf_type>
  determine_section_type(const std::string& section_name, uint32_t id)
  {
    // Use static constexpr to compute section names at compile time
    static constexpr auto ctrltext_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::ctrltext);
    static constexpr auto ctrldata_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::ctrldata);
    static constexpr auto save_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::preempt_save);
    static constexpr auto restore_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::preempt_restore);
    static constexpr auto pdi_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::pdi);

    if (section_name.find(ctrltext_pattern) != std::string::npos) {
      if (m_instr_buf_map.find(id) == m_instr_buf_map.end())
        throw std::runtime_error("Invalid section passed, section info is not cached\n");
      return { m_instr_buf_map[id].size(), patcher_buf_type::ctrltext };
    }
    else if (!m_ctrl_packet_map.empty() &&
             section_name.find(ctrldata_pattern) != std::string::npos) {
      if (m_ctrl_packet_map.find(id) == m_ctrl_packet_map.end())
        throw std::runtime_error("Invalid section passed, section info is not cached\n");
      return { m_ctrl_packet_map[id].size(), patcher_buf_type::ctrldata };
    }
    else if (section_name.find(save_pattern) != std::string::npos) {
      if (m_save_buf_map.find(id) == m_save_buf_map.end())
        throw std::runtime_error("Invalid section passed, section info is not cached\n");
      return { m_save_buf_map[id].size(), patcher_buf_type::preempt_save };
    }
    else if (section_name.find(restore_pattern) != std::string::npos) {
      if (m_restore_buf_map.find(id) == m_restore_buf_map.end())
        throw std::runtime_error("Invalid section passed, section info is not cached\n");
      return { m_restore_buf_map[id].size(), patcher_buf_type::preempt_restore };
    }
    else if (!m_pdi_buf_map.empty() &&
             section_name.find(pdi_pattern) != std::string::npos) {
      if (m_pdi_buf_map.find(section_name) == m_pdi_buf_map.end())
        throw std::runtime_error("Invalid section passed, section info is not cached\n");
      return { m_pdi_buf_map[section_name].size(), patcher_buf_type::pdi };
    }

    throw std::runtime_error("Invalid section passed\n");
  }

  // Initialize argument patchers from ELF relocation sections
  void
  initialize_arg_patchers()
  {
    static constexpr const char* Control_ScratchPad_Symbol = "scratch-pad-ctrl";
    static constexpr const char* ctrlpkt_pm_dynsym = "ctrlpkt-pm";

    auto dynsym = m_elfio.sections[".dynsym"];
    auto dynstr = m_elfio.sections[".dynstr"];
    auto dynsec = m_elfio.sections[".rela.dyn"];

    if (!dynsym || !dynstr || !dynsec)
      return;

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
      if (!m_ctrl_scratch_pad_mem_size && (strcmp(symname, Control_ScratchPad_Symbol) == 0)) {
        m_ctrl_scratch_pad_mem_size = static_cast<size_t>(sym->st_size);
      }

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
      auto grp_idx = m_section_to_group_map[sec_index];
      auto [sec_size, buf_type] = determine_section_type(section->get_name(), grp_idx);

      if (offset >= sec_size)
        throw std::runtime_error("Invalid offset " + std::to_string(offset));

      if (std::string(symname).find("pdi") != std::string::npos) {
        // pdi symbol, add to map of which ctrl code needs it
        m_ctrl_pdi_map[grp_idx].insert(symname);
      }

      patcher_symbol_type patch_scheme;
      uint32_t add_end_addr;
      auto abi_version = static_cast<uint16_t>(m_elfio.get_abi_version());
      if (abi_version != 1) {
        add_end_addr = rela->r_addend;
        patch_scheme = static_cast<patcher_symbol_type>(type);
      }
      else {
        // rela addend have offset to base_bo_addr info along with schema
        add_end_addr = (rela->r_addend & addend_mask) >> addend_shift;
        patch_scheme = static_cast<patcher_symbol_type>(rela->r_addend & schema_mask);
      }

      std::string argnm{ symname, symname + std::min(strlen(symname), dynstr->get_size()) };
      patch_config pc = patch_scheme == patcher_symbol_type::scalar_32bit_kind
        // st_size is is encoded using register value mask for scaler_32
        // for other pacthing scheme it is encoded using size of dma
        ? patch_config{ offset, add_end_addr, static_cast<uint32_t>(sym->st_size) }
        : patch_config{ offset, add_end_addr, 0 };

      auto key_string = xrt_core::elf_patcher::generate_key_string(argnm, buf_type);

      auto search = m_arg2patcher[grp_idx].find(key_string);
      if (search != m_arg2patcher[grp_idx].end()) {
        search->second.add_patch(pc);
      }
      else
        m_arg2patcher[grp_idx].emplace(std::move(key_string), patcher_config{patch_scheme, {pc}, buf_type});
    }
  }

public:
  explicit
  elf_aie2p(ELFIO::elfio&& elfio)
    : elf_impl{std::move(elfio)}
  {
    m_platform = xrt::elf::platform::aie2p;
    // Parse group sections to populate kernel and section maps
    parse_group_sections();
    // Initialize all section buffer maps
    initialize_section_buffer_maps();
    // Initialize argument patchers from relocation sections
    initialize_arg_patchers();
  }

  // AIE2P: uses group sections if version >= 1.0
  bool
  is_group_elf() const override
  {
    constexpr uint8_t group_elf_major_version = 1;
    auto [major, minor] = get_abi_version();
    return major >= group_elf_major_version;
  }

  ////////////////////////////////////////////////////////////////
  // Virtual accessor overrides
  ////////////////////////////////////////////////////////////////

  // Get module configuration for module_run_aie2p
  module_config
  get_module_config(uint32_t ctrl_code_id) override
  {
    // Get instruction buffer (required)
    auto instr_it = m_instr_buf_map.find(ctrl_code_id);
    if (instr_it == m_instr_buf_map.end())
      throw std::runtime_error("Instruction buffer not found for ctrl_code_id: " + std::to_string(ctrl_code_id));

    // Get optional buffers with fallback to empty
    auto ctrl_pkt_it = m_ctrl_packet_map.find(ctrl_code_id);
    auto save_it = m_save_buf_map.find(ctrl_code_id);
    auto restore_it = m_restore_buf_map.find(ctrl_code_id);
    auto pdi_it = m_ctrl_pdi_map.find(ctrl_code_id);

    static const std::unordered_set<std::string> empty_pdi_set;

    return module_config_aie2p{
      instr_it->second,                                                          // instr_data
      ctrl_pkt_it != m_ctrl_packet_map.end() ? ctrl_pkt_it->second : buf::get_empty_buf(), // ctrl_packet_data
      save_it != m_save_buf_map.end() ? save_it->second : buf::get_empty_buf(),  // preempt_save_data
      restore_it != m_restore_buf_map.end() ? restore_it->second : buf::get_empty_buf(), // preempt_restore_data
      512_kb,                                                                     // scratch_pad_mem_size
      m_ctrl_scratch_pad_mem_size,                                               // ctrl_scratch_pad_mem_size
      pdi_it != m_ctrl_pdi_map.end() ? pdi_it->second : empty_pdi_set,           // patch_pdi_symbols
      m_ctrlpkt_pm_dynsyms,                                                      // ctrlpkt_pm_dynsyms
      m_ctrlpkt_pm_bufs,                                                         // ctrlpkt_pm_bufs
      m_preemption_exist,                                                        // has_preemption
      this                                                                       // elf_parent
    };
  }

  // PDI buffer accessors (needed for mutable/lazy operations)
  const buf&
  get_pdi(const std::string& symbol) const override
  {
    auto it = m_pdi_buf_map.find(symbol);
    if (it == m_pdi_buf_map.end())
      throw std::runtime_error("PDI buffer not found for symbol: " + symbol);
    return it->second;
  }

  uint32_t
  get_ctrlcode_id(const std::string& name) const override
  {
    if (auto pos = name.find(":"); pos != std::string::npos) {
      // name passed is kernel name + sub kernel name
      auto key = name.substr(0, pos) + name.substr(pos + 1);
      auto it = m_kernel_name_to_id_map.find(key);
      if (it == m_kernel_name_to_id_map.end())
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
    if (auto entry = m_kernel_to_subkernels_map.find(name); entry != m_kernel_to_subkernels_map.end()) {
      if (entry->second.size() == 1) {
        auto key = name + *(entry->second.begin());
        auto it = m_kernel_name_to_id_map.find(key);
        if (it == m_kernel_name_to_id_map.end())
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

  ert_cmd_opcode
  get_ert_opcode() const override
  {
    if (!m_pdi_buf_map.empty())
      return ERT_START_NPU_PREEMPT_ELF;

    if (m_preemption_exist)
      return ERT_START_NPU_PREEMPT;

    return ERT_START_NPU;
  }
};

////////////////////////////////////////////////////////////////
// Derived class for AIE2PS platform
////////////////////////////////////////////////////////////////
class elf_aie2ps : public elf_impl
{
  // Control code is padded to page size
  static constexpr size_t elf_page_size = 8192;  // AIE_COLUMN_PAGE_SIZE

  // map for holding control code data for each sub kernel
  // key : control code id (grp sec idx), value : vector of column ctrlcodes
  std::map<uint32_t, std::vector<ctrlcode>> m_ctrlcodes_map;

  // map for holding multiple ctrl pkt sections data for each sub kernel
  // key : control code id (grp sec idx), value : map of ctrl pkt section name and data
  std::map<uint32_t, std::map<std::string, buf>> m_ctrlpkt_buf_map;

  // map to hold .dump section of different sub kernels used for debug/trace
  std::map<uint32_t, buf> m_dump_buf_map;

  ////////////////////////////////////////////////////////////////
  // Helper functions
  ////////////////////////////////////////////////////////////////

  // Extract the column and page information from the section name
  // section name can be .ctrltext.<col>.<page> or .ctrldata.<col>.<page>
  // or .ctrltext.<col>.<page>.<id> or .ctrldata.<col>.<page>.<id> - newer Elfs
  static std::pair<uint32_t, uint32_t>
  get_column_and_page(const std::string& name)
  {
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

  ////////////////////////////////////////////////////////////////
  // Buffer initialization functions
  ////////////////////////////////////////////////////////////////

  // Extract control code from ELF sections organized by columns and pages
  void
  initialize_column_ctrlcode(std::map<uint32_t, std::vector<size_t>>& pad_offsets)
  {
    static constexpr auto ctrltext_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::ctrltext);
    static constexpr auto ctrldata_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::ctrldata);
    static constexpr auto pad_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::pad);

    // Elf sections for a single page
    struct elf_page
    {
      ELFIO::section* ctrltext = nullptr;
      ELFIO::section* ctrldata = nullptr;
    };

    // Elf ctrl code for a partition spanning multiple uC
    // ucidx -> [page -> [ctrltext, ctrldata]]
    using page_index = uint32_t;
    using uc_index = uint32_t;
    using uc_sections = std::map<uc_index, std::map<page_index, elf_page>>;

    // Elf can have multiple kernel instances
    // key - instance id(grp idx) : value - uc sections
    using ctrlcode_map = std::map<uint32_t, uc_sections>;
    ctrlcode_map ctrl_map;

    // Iterate sections for each sub kernel
    // collect ctrltext and ctrldata per column and page
    for (const auto& [id, sec_ids] : m_group_to_sections_map) {
      for (auto sec_idx : sec_ids) {
        const auto& sec = m_elfio.sections[sec_idx];
        auto name = sec->get_name();

        if (name.find(ctrltext_pattern) != std::string::npos) {
          auto [col, page] = get_column_and_page(name);
          ctrl_map[id][col][page].ctrltext = sec;
        }
        else if (name.find(ctrldata_pattern) != std::string::npos) {
          auto [col, page] = get_column_and_page(name);
          ctrl_map[id][col][page].ctrldata = sec;
        }
      }
    }

    // Create uC control code from the collected data
    // Pad to page size for each page of a column
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

          // Pad to page boundary
          auto current_size = m_ctrlcodes_map[id][ucidx].size();
          auto target_size = (page + 1) * elf_page_size;
          if (current_size < target_size) {
            m_ctrlcodes_map[id][ucidx].m_data.resize(target_size, 0);
          }
        }
        pad_offsets[id][ucidx] = m_ctrlcodes_map[id][ucidx].size();
      }
    }

    // Append pad section to the control code
    for (const auto& [id, sec_ids] : m_group_to_sections_map) {
      for (auto sec_idx : sec_ids) {
        const auto& sec = m_elfio.sections[sec_idx];
        const auto& name = sec->get_name();
        if (name.find(pad_pattern) == std::string::npos)
          continue;
        auto [col, page] = get_column_and_page(name);
        m_ctrlcodes_map[id][col].append_section_data(sec);
      }
    }
  }

  // Parse the ELF and extract all ctrlpkt sections data
  void
  initialize_ctrlpkt_bufs()
  {
    static constexpr auto ctrlpkt_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::ctrlpkt);

    for (const auto& sec : m_elfio.sections) {
      auto name = sec->get_name();
      if (name.find(ctrlpkt_pattern) == std::string::npos)
        continue;

      buf ctrlpkt_buf;
      ctrlpkt_buf.append_section_data(sec.get());
      auto grp_idx = m_section_to_group_map[sec->get_index()];
      m_ctrlpkt_buf_map[grp_idx][name] = std::move(ctrlpkt_buf);
    }
  }

  // Extract .dump section from ELF sections
  void
  initialize_dump_buf()
  {
    static constexpr auto dump_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::dump);

    for (const auto& sec : m_elfio.sections) {
      auto name = sec->get_name();
      if (name.find(dump_pattern) == std::string::npos)
        continue;

      auto ctrl_id = m_section_to_group_map[sec->get_index()];
      m_dump_buf_map[ctrl_id].append_section_data(sec.get());
    }
  }

  // Initialize argument patchers from ELF relocation sections
  void
  initialize_arg_patchers(const std::map<uint32_t, std::vector<size_t>>& pad_offsets)
  {
    static constexpr auto pad_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::pad);
    static constexpr auto ctrlpkt_pattern = xrt_core::elf_patcher::get_section_name(patcher_buf_type::ctrlpkt);

    auto dynsym = m_elfio.sections[".dynsym"];
    auto dynstr = m_elfio.sections[".dynstr"];
    auto dynsec = m_elfio.sections[".rela.dyn"];

    if (!dynsym || !dynstr || !dynsec)
      return;

    // Iterate over all relocations and construct a patcher for each
    auto begin = reinterpret_cast<const ELFIO::Elf32_Rela*>(dynsec->get_data());
    auto end = begin + dynsec->get_size() / sizeof(const ELFIO::Elf32_Rela);

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
      auto grp_idx = m_section_to_group_map[sec_idx];
      if (m_ctrlcodes_map.find(grp_idx) == m_ctrlcodes_map.end())
        throw std::runtime_error(std::string{"Unable to fetch ctrlcode to patch for given symbol: "} + argnm);

      const auto& ctrlcodes = m_ctrlcodes_map[grp_idx];
      size_t abs_offset = 0;
      patcher_buf_type buf_type = patcher_buf_type::buf_type_count;

      if (patch_sec_name.find(pad_pattern) != std::string::npos) {
        const auto& pad_off_vec = pad_offsets.at(grp_idx);
        for (uint32_t i = 0; i < col; ++i)
          abs_offset += ctrlcodes[i].size();
        abs_offset += pad_off_vec[col];
        abs_offset += rela->r_offset;
        buf_type = patcher_buf_type::pad;
      }
      else if (patch_sec_name.find(ctrlpkt_pattern) != std::string::npos) {
        // section to patch is ctrlpkt
        abs_offset += rela->r_offset;
        buf_type = patcher_buf_type::ctrlpkt;
      }
      else {
        // section to patch is ctrlcode
        auto column_ctrlcode_size = ctrlcodes.at(col).size();
        auto sec_offset = page * elf_page_size + rela->r_offset + 16; // NOLINT magic number 16
        if (sec_offset >= column_ctrlcode_size)
          throw std::runtime_error("Invalid ctrlcode offset " + std::to_string(sec_offset));
        // Compute absolute offset
        for (uint32_t i = 0; i < col; ++i)
          abs_offset += ctrlcodes.at(i).size();
        abs_offset += sec_offset;
        buf_type = patcher_buf_type::ctrltext;
      }

      // Construct the patcher config for the argument
      patcher_symbol_type patch_scheme = patcher_symbol_type::unknown_symbol_kind;
      uint32_t add_end_addr = 0;
      auto abi_version = static_cast<uint16_t>(m_elfio.get_abi_version());
      if (abi_version != 1) {
        add_end_addr = rela->r_addend;
        patch_scheme = static_cast<patcher_symbol_type>(type);
      }
      else {
        // rela addend have offset to base_bo_addr info along with schema
        add_end_addr = (rela->r_addend & addend_mask) >> addend_shift;
        patch_scheme = static_cast<patcher_symbol_type>(rela->r_addend & schema_mask);
      }

      auto key_string = xrt_core::elf_patcher::generate_key_string(argnm, buf_type);

      auto search = m_arg2patcher[grp_idx].find(key_string);
      if (search != m_arg2patcher[grp_idx].end()) {
        search->second.add_patch(patch_config{abs_offset, add_end_addr, 0});
      }
      else
        m_arg2patcher[grp_idx].emplace(std::move(key_string), patcher_config{patch_scheme, {patch_config{abs_offset, add_end_addr, 0}}, buf_type});
    }
  }

  // Initialize all section buffers
  void
  initialize_section_buffer_maps()
  {
    std::map<uint32_t, std::vector<size_t>> pad_offsets;
    initialize_column_ctrlcode(pad_offsets);
    initialize_ctrlpkt_bufs();
    initialize_dump_buf();
    initialize_arg_patchers(pad_offsets);
  }

public:
  explicit
  elf_aie2ps(ELFIO::elfio&& elfio)
    : elf_impl{std::move(elfio)}
  {
    m_platform = xrt::elf::platform::aie2ps;
    // Parse group sections to populate kernel and section maps
    parse_group_sections();
    // Initialize all section buffer maps
    initialize_section_buffer_maps();
  }

  // AIE2PS/AIE4: uses group sections if version >= 0.3
  bool
  is_group_elf() const override
  {
    constexpr uint8_t group_elf_major_version = 0;
    constexpr uint8_t group_elf_minor_version = 3;

    auto [major, minor] = get_abi_version();

    // Version >= 0.3: major > 0 OR (major == 0 AND minor >= 3)
    return (major > group_elf_major_version) ||
           (major == group_elf_major_version && minor >= group_elf_minor_version);
  }

  ////////////////////////////////////////////////////////////////
  // Virtual accessor overrides
  ////////////////////////////////////////////////////////////////

  // Get module configuration for module_run_aie2ps
  module_config
  get_module_config(uint32_t ctrl_code_id) override
  {
    // Get control codes (required)
    auto ctrlcode_it = m_ctrlcodes_map.find(ctrl_code_id);
    if (ctrlcode_it == m_ctrlcodes_map.end())
      throw std::runtime_error("Control codes not found for ctrl_code_id: " + std::to_string(ctrl_code_id));

    // Get optional buffers with fallback to empty
    auto ctrlpkt_it = m_ctrlpkt_buf_map.find(ctrl_code_id);
    auto dump_it = m_dump_buf_map.find(ctrl_code_id);

    static const std::map<std::string, buf> empty_ctrlpkt_map;

    return module_config_aie2ps{
      ctrlcode_it->second,                                                       // ctrlcodes
      ctrlpkt_it != m_ctrlpkt_buf_map.end() ? ctrlpkt_it->second : empty_ctrlpkt_map, // ctrlpkt_bufs
      dump_it != m_dump_buf_map.end() ? dump_it->second : buf::get_empty_buf(),  // dump_buf
      this                                                                       // elf_parent
    };
  }

  uint32_t
  get_ctrlcode_id(const std::string& name) const override
  {
    if (auto pos = name.find(":"); pos != std::string::npos) {
      // name passed is kernel name + sub kernel name
      auto key = name.substr(0, pos) + name.substr(pos + 1);
      auto it = m_kernel_name_to_id_map.find(key);
      if (it == m_kernel_name_to_id_map.end())
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
    if (auto entry = m_kernel_to_subkernels_map.find(name); entry != m_kernel_to_subkernels_map.end()) {
      if (entry->second.size() == 1) {
        auto key = name + *(entry->second.begin());
        auto it = m_kernel_name_to_id_map.find(key);
        if (it == m_kernel_name_to_id_map.end())
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

  ert_cmd_opcode
  get_ert_opcode() const override
  {
    return ERT_START_DPU;
  }
};

} // namespace xrt

namespace {

// Factory function - creates correct derived type based on platform
static std::shared_ptr<xrt::elf_impl>
create_elf_impl(ELFIO::elfio&& elfio)
{
  auto os_abi = elfio.get_os_abi();

  switch (os_abi) {
  case static_cast<uint8_t>(xrt::elf::platform::aie2p):
    return std::make_shared<xrt::elf_aie2p>(std::move(elfio));

  case static_cast<uint8_t>(xrt::elf::platform::aie2ps):
  case static_cast<uint8_t>(xrt::elf::platform::aie2ps_group):
    return std::make_shared<xrt::elf_aie2ps>(std::move(elfio));

  default:
    throw std::runtime_error("ELF contains unsupported platform OS/ABI: " +
                             std::to_string(static_cast<int>(os_abi)));
  }
}

} // namespace

////////////////////////////////////////////////////////////////
// xrt_elf C++ API implementation (xrt_elf.h)
////////////////////////////////////////////////////////////////
namespace xrt {

// Helper to validate elf object is properly initialized.
// Default constructed elf objects have null handle.
// Throws if handle is null.
static void
valid_or_error(const std::shared_ptr<elf_impl>& handle)
{
  if (!handle)
    throw std::runtime_error("xrt::elf object is not initialized");
}

elf::
elf(const std::string& fnm)
  : detail::pimpl<elf_impl>{create_elf_impl(load_elfio(fnm))}
{}

elf::
elf(std::istream& stream)
  : detail::pimpl<elf_impl>{create_elf_impl(load_elfio(stream))}
{}

elf::
elf(const void* data, size_t size)
  : detail::pimpl<elf_impl>{create_elf_impl(load_elfio(data, size))}
{}

elf::
elf(const std::string_view& sv)
  : detail::pimpl<elf_impl>{create_elf_impl(load_elfio(sv.data(), sv.size()))}
{}

xrt::uuid
elf::
get_cfg_uuid() const
{
  valid_or_error(handle);
  return handle->get_cfg_uuid();
}

bool
elf::
is_full_elf() const
{
  valid_or_error(handle);
  return handle->is_full_elf();
}

uint32_t
elf::
get_partition_size() const
{
  valid_or_error(handle);
  return handle->get_partition_size();
}

elf::platform
elf::
get_platform() const
{
  valid_or_error(handle);
  return handle->get_platform();
}

std::vector<elf::kernel>
elf::
get_kernels() const
{
  valid_or_error(handle);
  return handle->get_kernels();
}

////////////////////////////////////////////////////////////////
// elf::kernel API implementation
////////////////////////////////////////////////////////////////

std::string
elf::kernel::
get_name() const
{
  return handle->get_name();
}

size_t
elf::kernel::
get_num_args() const
{
  return handle->get_num_args();
}

elf::kernel::data_type
elf::kernel::
get_arg_data_type(size_t index) const
{
  return handle->get_arg_data_type(index);
}

std::vector<elf::kernel::instance>
elf::kernel::
get_instances() const
{
  return handle->get_instances();
}

////////////////////////////////////////////////////////////////
// elf::kernel::instance API implementation
////////////////////////////////////////////////////////////////

std::string
elf::kernel::instance::
get_name() const
{
  return handle->get_name();
}

} // namespace xrt

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal elf APIs
////////////////////////////////////////////////////////////////
namespace xrt_core::elf_int {

std::pair<xrt_core::xclbin::kernel_properties, std::vector<xrt::xarg>>
get_kernel_properties_and_args(std::shared_ptr<xrt::elf_impl> elf_impl,
                               const std::string& kernel_name)
{
  auto kernels = elf_impl->get_kernels();
  for (const auto& kernel : kernels) {
    if (kernel.get_name() == kernel_name) {
      return kernel.get_handle()->get_properties_and_args();
    }
  }
  throw std::runtime_error("Kernel not found: " + kernel_name);
}

} // xrt_core::elf_int

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

} // namespace xrt::aie
