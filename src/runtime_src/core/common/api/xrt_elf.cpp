// Copyright (C) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_elf.h
#define XRT_API_SOURCE         // exporting xrt_elf.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "xrt/experimental/xrt_aie.h"
#include "xrt/experimental/xrt_elf.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_uuid.h"

#include "elf_int.h"
#include "elf_patcher.h"
#include "core/common/aiebu/src/cpp/elf/aie_elf_constants.h"
#include "core/common/aiebu/src/cpp/include/aiebu/aiebu_assembler.h"
#include "core/common/config_reader.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "core/common/trace.h"
#include "core/common/xclbin_parser.h"
#include "core/common/runner/capture.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
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

static constexpr size_t
operator"" _mb(unsigned long long v)  { return 1024u * 1024u * v; } // NOLINT

///////////////////////////////////////////////////////////////
// aiebu::elf construction helpers
///////////////////////////////////////////////////////////////

// Translate aiebu::elf::arg -> xrt::xarg
static xrt::xarg
make_xarg(const aiebu::elf::arg& a, size_t& offset)
{
  static constexpr size_t global_arg_size = 0x8;
  xrt::xarg arg;
  arg.name     = a.name;
  arg.hosttype = "no-type";
  arg.port     = "no-port";
  arg.index    = a.index;
  arg.offset   = offset;
  arg.dir      = xrt::xarg::direction::input;
  arg.type     = a.is_global
                   ? xrt::xarg::argtype::global
                   : xrt::xarg::argtype::scalar;
  if (arg.type == xrt::xarg::argtype::scalar)
    throw std::runtime_error("scalar args are not yet supported for this kind of kernel");

  arg.size = global_arg_size;
  offset  += global_arg_size;
  return arg;
}

// Translate aiebu::elf::kernel -> xrt::elf::kernel pimpl
static xrt::elf::kernel
make_kernel(const aiebu::elf::kernel& k)
{
  std::vector<xrt::xarg> xargs;
  size_t offset = 0;
  for (const auto& a : k.args)
    xargs.push_back(make_xarg(a, offset));

  std::vector<xrt::elf::kernel::instance> instances;
  for (const auto& inst : k.instances)
    instances.emplace_back(std::make_shared<xrt::elf::kernel::instance_impl>(inst));

  return xrt::elf::kernel{std::make_shared<xrt::elf::kernel_impl>(
    k.name, std::move(xargs), std::move(instances))};
}

// Translate aiebu::elf kernels into the XRT pimpl kernel objects.
static std::vector<xrt::elf::kernel>
create_kernels(const aiebu::elf& elf)
{
  std::vector<xrt::elf::kernel> kernels;
  for (const auto& k : elf.get_kernels())
    kernels.push_back(make_kernel(k));

  return kernels;
}

// Translate patch_points from aiebu::elf into an m_arg2patcher map.
// The key-string format and patcher_config/patch_config field layout
// are intentionally identical to the old ELFIO-based parsing, so the
// hot-path get_patcher_configs() / symbol_patcher::patch_symbol() are unchanged.
static std::map<uint32_t, std::map<std::string, xrt_core::elf_patcher::patcher_config>>
create_arg2patcher(const aiebu::elf& elf)
{
  using patcher_config   = xrt_core::elf_patcher::patcher_config;
  using patch_config     = xrt_core::elf_patcher::patch_config;
  using patcher_buf_type = xrt_core::elf_patcher::buf_type;
  using patcher_sym_type = xrt_core::elf_patcher::symbol_type;

  std::map<uint32_t, std::map<std::string, patcher_config>> out;
  for (const auto& [grp_idx, key_map] : elf.get_patch_points()) {
    for (const auto& [key, pts] : key_map) {
      for (const auto& pp : pts) {
        patch_config pc{pp.section_offset, pp.base_bo_offset, pp.mask};
        auto btype  = static_cast<patcher_buf_type>(pp.target_buf);
        auto schema = static_cast<patcher_sym_type>(pp.schema);

        auto& slot = out[grp_idx];
        auto  it   = slot.find(key);
        if (it != slot.end())
          it->second.add_patch(pc);
        else
          slot.emplace(key, patcher_config{schema, {pc}, btype});

      }
    }
  }
  return out;
}

} // namespace

namespace xrt {

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

elf_impl::
elf_impl(aiebu::elf&& elf, std::string path)
  : m_elf{std::move(elf)}
  , m_platform{static_cast<xrt::elf::platform>(m_elf.get_os_abi())}
  , m_path{std::move(path)}
  , m_kernels{create_kernels(m_elf)}
  , m_arg2patcher{create_arg2patcher(m_elf)}
{
  // m_patch_points has been translated into m_arg2patcher; release it to
  // avoid retaining a duplicate copy for the lifetime of the ELF object.
  m_elf.clear_patch_points();
}

// Get configuration UUID from ELF
// Returns an empty UUID if the section is absent (e.g. partial ELFs).
xrt::uuid
elf_impl::
get_cfg_uuid() const
{
  // Returns an empty UUID if the section is absent (e.g. partial ELFs).
  constexpr size_t uuid_size = 16;
  try {
    auto data = m_elf.get_cfg_uuid();
    xuid_t uuid_data;
    std::memcpy(uuid_data, data.data(), uuid_size);
    return {uuid_data};
  }
  catch (const std::exception&) {
    return {};
  }
}

// Get partition size from ELF notes
uint32_t
elf_impl::
get_partition_size() const
{
  return m_elf.get_partition_size();
}

////////////////////////////////////////////////////////////////
// elf_aie_gen2 — AIE2P (gen2) platform
//
// No buffer maps — all section data lives in aiebu::elf (elf_reader_aie2p).
// This class only provides the XRT-specific virtual interface.
////////////////////////////////////////////////////////////////
class elf_aie_gen2 : public elf_impl
{
public:
  explicit
  elf_aie_gen2(aiebu::elf&& elf, std::string path)
    : elf_impl{std::move(elf), std::move(path)}
  {
    XRT_TRACE_POINT_SCOPE(xrt_elf_aie_gen2);
  }

  bool
  is_group_elf() const override
  {
    auto [major, minor] = get_abi_version();
    return major >= 1;
  }

  ert_cmd_opcode
  get_ert_opcode() const override
  {
    if (m_elf.has_pdi())
      return ERT_START_NPU_PREEMPT_ELF;

    if (m_elf.has_preemption())
      return ERT_START_NPU_PREEMPT;

    return ERT_START_NPU;
  }
};

////////////////////////////////////////////////////////////////
// elf_aie_gen2_plus — AIE2PS / AIE4 family
//
// No buffer maps — all section data lives in aiebu::elf (elf_reader_gen2plus).
// This class only provides the XRT-specific virtual interface.
////////////////////////////////////////////////////////////////
class elf_aie_gen2_plus : public elf_impl
{
public:
  explicit
  elf_aie_gen2_plus(aiebu::elf&& elf, std::string path)
    : elf_impl{std::move(elf), std::move(path)}
  {
    XRT_TRACE_POINT_SCOPE(xrt_elf_aie_gen2_plus);
  }

  bool
  is_group_elf() const override
  {
    auto [major, minor] = get_abi_version();
    return (major > 0) || (major == 0 && minor >= 3);
  }

  ert_cmd_opcode
  get_ert_opcode() const override
  {
    return ERT_START_DPU;
  }
};

} // namespace xrt

namespace {

// Factory — dispatch on platform detected by aiebu::elf
static std::shared_ptr<xrt::elf_impl>
create_elf_impl(aiebu::elf&& elf, std::string path = {})
{
  switch (elf.get_platform()) {
  case aiebu::elf::platform::aie2p:
    return std::make_shared<xrt::elf_aie_gen2>(std::move(elf), std::move(path));
  case aiebu::elf::platform::aie2ps:
  case aiebu::elf::platform::aie2ps_legacy:
  case aiebu::elf::platform::aie4:
  case aiebu::elf::platform::aie4a:
  case aiebu::elf::platform::aie4z:
    return std::make_shared<xrt::elf_aie_gen2_plus>(std::move(elf), std::move(path));
  }
  // aiebu::elf::elf() already throws on unknown OS/ABI — unreachable
  throw std::runtime_error("Unsupported ELF platform");
}

} // namespace

////////////////////////////////////////////////////////////////
// xrt_elf C++ API implementation (xrt_elf.h)
////////////////////////////////////////////////////////////////
namespace xrt {

static void
valid_or_error(const std::shared_ptr<elf_impl>& handle)
{
  if (!handle)
    throw std::runtime_error("xrt::elf object is not initialized");
}

elf::
elf(const std::string& fnm)
  : detail::pimpl<elf_impl>{create_elf_impl(aiebu::elf{fnm}, fnm)}
{
  XRT_REPLAY_CAPTURE(elf_ctor, handle.get(), fnm);
}

elf::
elf(std::istream& stream)
  : detail::pimpl<elf_impl>{create_elf_impl(aiebu::elf{stream})}
{
  XRT_REPLAY_CAPTURE(elf_ctor, handle.get(), stream);
}

elf::
elf(const void* data, size_t size)
  : detail::pimpl<elf_impl>{create_elf_impl(aiebu::elf{data, size})}
{
  XRT_REPLAY_CAPTURE(elf_ctor, handle.get(), data, size);
}

elf::
elf(const std::string_view& sv)
  : detail::pimpl<elf_impl>{create_elf_impl(aiebu::elf{sv.data(), sv.size()})}
{
  XRT_REPLAY_CAPTURE(elf_ctor, handle.get(), sv.data(), sv.size());
}

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

detail::span<const char>
elf::
get_custom_section(const std::string& section_name) const
{
  valid_or_error(handle);
  auto sp = handle->get_aiebu_elf().get_section(section_name);
  if (sp.empty())
    throw std::runtime_error("Cannot get custom section " + section_name +
                             " data, section not found in ELF");

  return {reinterpret_cast<const char*>(sp.data()), sp.size()};
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

std::string
get_filename(const xrt::elf_impl* elf_impl)
{
  return elf_impl
    ? elf_impl->get_filename()
    : "";
}

static aiebu::aiebu_assembler::buffer_type
osabi_to_coredump_type(uint8_t os_abi)
{
  switch (os_abi) {
  case aiebu::osabi_aie2p:
    return aiebu::aiebu_assembler::buffer_type::coredump_aie2p;
  case aiebu::osabi_aie2ps:
    return aiebu::aiebu_assembler::buffer_type::coredump_aie2ps;
  case aiebu::osabi_aie4:
    return aiebu::aiebu_assembler::buffer_type::coredump_aie4;
  case aiebu::osabi_aie4a:
    return aiebu::aiebu_assembler::buffer_type::coredump_aie4a;
  case aiebu::osabi_aie4z:
    return aiebu::aiebu_assembler::buffer_type::coredump_aie4z;
  default:
    throw std::runtime_error("AIE coredump not supported for this ELF architecture");
  }
}

std::vector<char>
make_aie_coredump_elf(const xrt::elf& elf, const std::vector<char>& blob,
                      const xrt_core::device* device, uint32_t slot,
                      const std::string& uuid)
{
  // Fail fast: verify arch is supported before doing any device queries.
  auto buf_type = osabi_to_coredump_type(elf.get_handle()->get_os_abi());

  aiebu::aie_coredump_meta meta{};
  meta.timestamp_ns = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count());
  meta.uuid = uuid;

  try {
    boost::property_tree::ptree pt_xrt;
    xrt_core::get_driver_info(pt_xrt);
    std::string drv_str;
    if (const auto drivers = pt_xrt.get_child_optional("drivers")) {
      for (const auto& [dummy, drv] : *drivers) {
        auto name = drv.get<std::string>("name", "");
        auto ver  = drv.get<std::string>("version", "");
        if (!name.empty() && !ver.empty() && ver != "unknown") {
          if (!drv_str.empty())
            drv_str += "; ";

          drv_str += name + " " + ver;
        }
      }
    }
    meta.driver_version = drv_str;
  }
  catch (const std::exception&) {
    /* leave empty if not available */
  }

  try {
    auto data = xrt_core::device_query<xrt_core::query::aie_partition_info>(device);
    auto islot = static_cast<int>(slot);

    for (const auto& entry : data) {
      if (std::stoi(entry.metadata.id) != islot)
        continue;

      meta.context_status = entry.is_suspended
          ? aiebu::aie_context_status::idle
          : aiebu::aie_context_status::running;
      break;
    }
  }
  catch (const std::exception&) {
    /* leave as default idle if query not supported */
  }

  try {
    auto fw = xrt_core::device_query<xrt_core::query::firmware_version>(
        device,
        xrt_core::query::firmware_version::firmware_type::npu_firmware);
    meta.fw_version = std::to_string(fw.major) + "." + std::to_string(fw.minor)
                    + "." + std::to_string(fw.patch) + "." + std::to_string(fw.build);
  }
  catch (const std::exception&) {
    /* leave empty if not supported */
  }

  try {
    meta.device_info = xrt_core::device_query<xrt_core::query::rom_vbnv>(device);
  }
  catch (const std::exception&) {
    /* leave empty if not supported */
  }

  aiebu::aiebu_assembler a(buf_type, blob, meta);
  return a.get_elf();
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
