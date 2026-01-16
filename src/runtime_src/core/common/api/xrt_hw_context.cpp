// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_queue.h
#define XRT_API_SOURCE         // exporting xrt_hwcontext.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_xclbin.h
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil

#include "core/common/buffer_dumper.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/time.h"
#include "core/common/utils.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/experimental/xrt_ext.h"
#include "core/include/xrt/experimental/xrt_module.h"
#include "bo_int.h"
#include "elf_int.h"
#include "hw_context_int.h"
#include "module_int.h"
#include "xclbin_int.h"

#include "core/common/device.h"
#include "core/common/trace.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/common/usage_metrics.h"
#include "core/common/xdp/profile.h"

#include <cstddef>
#include <ctime>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>

#ifdef _WIN32
# pragma warning ( disable : 4996 )
#endif

namespace {
static constexpr double hz_per_mhz = 1'000'000.0;

constexpr std::size_t
operator""_mb(unsigned long long v) { return 1024u * 1024u * v; }

// Dumps the content into a file with given size from given offset
static void
dump_bo(const char* buf_map, const std::string& filename, size_t size, size_t offset = 0)
{
  std::ofstream ofs(filename, std::ios::out | std::ios::binary);
  if (!ofs.is_open())
    throw std::runtime_error("Failure opening file " + filename + " for writing!");

  const char* buf = buf_map + offset;
  ofs.write(buf, static_cast<std::streamsize>(size));
}
}

namespace xrt {

// class hw_context_impl - insulated implemention of an xrt::hw_context
//
class hw_context_impl : public std::enable_shared_from_this<hw_context_impl>
{
  using cfg_param_type = xrt::hw_context::cfg_param_type;
  using qos_type = cfg_param_type;
  using access_mode = xrt::hw_context::access_mode;

  // Struct used for initializing and dumping firmware log buffer
  struct uc_log_buffer
  {
    size_t m_num_uc; // number of uc's
    uint32_t m_slot_idx; // index of slot in which these uc's are present
    std::unique_ptr<xrt_core::buffer_dumper> m_uc_log_bo_dumper;

    static std::unique_ptr<xrt_core::buffer_dumper>
    init_uc_log_bo_dumper(const std::shared_ptr<xrt_core::device>& device,
                          xrt_core::hwctx_handle* ctx_hdl,
                          size_t num_uc)
    {
      // parameters for uc log buffer dumper
      // tweak dump interval, size_per_uc based on experiments
      constexpr size_t size_per_uc = 2_mb;
      constexpr size_t dump_interval_ms = 50;
      constexpr size_t metadata_size = 32;
      constexpr size_t count_offset = 0;
      constexpr size_t count_size = 8;

      auto bo = xrt_core::bo_int::create_bo(
          device, (size_per_uc * num_uc), xrt_core::bo_int::use_type::log);

      // So make sure for each uC metadata bytes are initialized with
      // zero's before configuring
      char* buf_map = bo.map<char*>();
      if (!buf_map)
        throw std::runtime_error("Failed to map uc log buffer");

      // create map with uC index and log buffer size
      // and also initialize metadata bytes to zero for each uC
      std::map<uint32_t, size_t> uc_buf_map;
      for (uint32_t i = 0; i < num_uc; ++i) {
        std::memset(buf_map + (i * size_per_uc), 0, metadata_size);
        uc_buf_map[i] = size_per_uc;
      }

      // configure the log buffer
      xrt_core::bo_int::config_bo(bo, uc_buf_map, ctx_hdl);

      // create buffer dumper object to dump the log buffer contents
      xrt_core::buffer_dumper::config config;
      config.chunk_size = size_per_uc;
      config.metadata_size = metadata_size;
      config.count_offset = count_offset;
      config.count_size = count_size;
      config.num_chunks = num_uc;
      config.dump_interval_ms = dump_interval_ms;
      config.dump_file_prefix = "uc_log_" + std::to_string(ctx_hdl->get_slotidx());
      config.dump_buffer = std::move(bo);

      return std::make_unique<xrt_core::buffer_dumper>(std::move(config));
    }

    // may throw
    uc_log_buffer(const std::shared_ptr<xrt_core::device>& device,
                  xrt_core::hwctx_handle* ctx_hdl)
      : m_num_uc(ctx_hdl->get_num_uc())
      , m_slot_idx(ctx_hdl->get_slotidx())
      , m_uc_log_bo_dumper(init_uc_log_bo_dumper(device, ctx_hdl, m_num_uc))
    {}

    void
    dump()
    {
      m_uc_log_bo_dumper->flush();
    }
  };

  std::shared_ptr<xrt_core::device> m_core_device;
  xrt::xclbin m_xclbin;

  // map b/w kernel name and Elf
  // Stores the Elf corresponding to each kernel name it contains.
  // This map is used for lookup when creating xrt::kernel object
  // using kernel name.
  std::map<std::string, xrt::elf> m_elf_map;

  // map b/w kernel name and xrt::module
  // For xrt::kernel object created in this hw_context, they
  // should share the same xrt::module.
  std::map<std::string, xrt::module> m_kernel_mod_map;

  // No. of cols in the AIE partition managed by this hw ctx
  // Devices with no AIE will have partition size as 0
  uint32_t m_partition_size = 0;
  cfg_param_type m_cfg_param;
  access_mode m_mode;
  std::unique_ptr<xrt_core::hwctx_handle> m_hdl;
  std::unique_ptr<uc_log_buffer> m_uc_log_buf;
  // During preemption, when a hardware context is interrupted L2 memory contents
  // are saved to a scratchpad memory allocated specifically for that context.
  std::once_flag m_scratchpad_init_flag; // used for thread safe lazy init of scratchpad
  xrt::bo m_scratchpad_buf;
  std::shared_ptr<xrt_core::usage_metrics::base_logger> m_usage_logger =
      xrt_core::usage_metrics::get_usage_metrics_logger();
  bool m_elf_flow = false;

  void
  create_elf_map(const xrt::elf& elf)
  {
    // creating xrt::module object from elf to get kernels info
    // At present ELF is parsed when we create xrt::module object
    // TODO : remove module creation once we move all parsing logic
    // to xrt::elf or xrt::aie::program
    xrt::module module_obj{elf};

    // Store the ELF in the map against all available kernels in the it.
    // This will be useful for ELF lookup when creating xrt::kernel object
    // using kernel name
    const auto& kernels_info = xrt_core::module_int::get_kernels_info(module_obj);
    for (const auto& k_info : kernels_info) {
      auto kernel_name = k_info.props.name;
      if (m_elf_map.find(kernel_name) != m_elf_map.end())
        throw std::runtime_error("kernel already exists, cannot use this ELF with this hw ctx\n");

      m_elf_map.emplace(std::move(kernel_name), elf);
    }
  }

  // Initializes uc log buffer
  // Creates log buffer and starts a thread to dump the log buffer contents
  // periodically per column to a file
  static std::unique_ptr<uc_log_buffer>
  init_uc_log_buf(const std::shared_ptr<xrt_core::device>& device, xrt_core::hwctx_handle* ctx_hdl)
  {
    // Create uc log buffer only if ini option is enabled
    // If enabled, but not supported then this function returns nullptr
    static auto uc_log_enabled = xrt_core::config::get_uc_log();
    if (!uc_log_enabled || !ctx_hdl)
      return nullptr;

    try {
      return std::make_unique<uc_log_buffer>(device, ctx_hdl);
    }
    catch (const std::exception& e) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_hw_context",
                              std::string{"Failed to create UC log buffer : "} + e.what());
      return nullptr;
    }
  }

  // Gets partition size from xclbin if AIE partition section is present
  static uint32_t
  get_partition_size_from_xclbin(const xrt::xclbin& xclbin)
  {
    if (!xclbin)
      return 0;

    try {
      auto axlf = xclbin.get_axlf();
      if (!axlf)
        return 0;

      // try to get partition size from xclbin AIE_METADATA section
      auto aie_part = xrt_core::xclbin::get_aie_partition(axlf);
      return aie_part.ncol;
    }
    catch (...) {
      return 0;
    }
  }

public:
  hw_context_impl(std::shared_ptr<xrt_core::device> device, const xrt::uuid& xclbin_id, cfg_param_type cfg_param)
    : m_core_device(std::move(device))
    , m_xclbin(m_core_device->get_xclbin(xclbin_id))
    , m_partition_size(get_partition_size_from_xclbin(m_xclbin))
    , m_cfg_param(std::move(cfg_param))
    , m_mode(xrt::hw_context::access_mode::shared)
    , m_hdl{m_core_device->create_hw_context(xclbin_id, m_cfg_param, m_mode)}
    , m_uc_log_buf(init_uc_log_buf(m_core_device, m_hdl.get()))
  {}

  hw_context_impl(std::shared_ptr<xrt_core::device> device, const xrt::uuid& xclbin_id, access_mode mode)
    : m_core_device{std::move(device)}
    , m_xclbin{m_core_device->get_xclbin(xclbin_id)}
    , m_partition_size(get_partition_size_from_xclbin(m_xclbin))
    , m_mode{mode}
    , m_hdl{m_core_device->create_hw_context(xclbin_id, m_cfg_param, m_mode)}
    , m_uc_log_buf(init_uc_log_buf(m_core_device, m_hdl.get()))
  {}

  hw_context_impl(std::shared_ptr<xrt_core::device> device, cfg_param_type cfg_param, access_mode mode)
    : m_core_device{std::move(device)}
    , m_cfg_param{std::move(cfg_param)}
    , m_mode{mode}
  {}

  hw_context_impl(std::shared_ptr<xrt_core::device> device, const xrt::elf& elf,
                  cfg_param_type cfg_param, access_mode mode)
    : m_core_device{std::move(device)}
    , m_partition_size{xrt_core::elf_int::get_partition_size(elf)}
    , m_cfg_param{std::move(cfg_param)}
    , m_mode{mode}
    , m_hdl{m_core_device->create_hw_context(elf, m_cfg_param, m_mode)}
    , m_uc_log_buf(init_uc_log_buf(m_core_device, m_hdl.get()))
    , m_elf_flow{true}
  {
    create_elf_map(elf);
  }

  std::shared_ptr<hw_context_impl>
  get_shared_ptr()
  {
    return shared_from_this();
  }

  ~hw_context_impl()
  {
    // This trace point measures the time to tear down a hw context on the device
    XRT_TRACE_POINT_SCOPE(xrt_hw_context_dtor);

    // dump uC log buffer before shim hwctx handle is destroyed
    m_uc_log_buf.reset();

    try {
      // finish_flush_device should only be called when the underlying
      // hw_context_impl is destroyed. The xdp::update_device cannot exist
      // in the hw_context_impl constructor because an existing
      // shared pointer must already exist to call get_shared_ptr(),
      // which is not true at that time.
      xrt_core::xdp::finish_flush_device(this);

      // Reset within scope of dtor for trace point to measure time to reset
      m_hdl.reset();
    }
    catch (...) {
      // ignore, dtor cannot throw
    }
  }

  hw_context_impl() = delete;
  hw_context_impl(const hw_context_impl&) = delete;
  hw_context_impl(hw_context_impl&&) = delete;
  hw_context_impl& operator=(const hw_context_impl&) = delete;
  hw_context_impl& operator=(hw_context_impl&&) = delete;

  void
  add_config(const xrt::elf& elf)
  {
    auto part_size = xrt_core::elf_int::get_partition_size(elf);

    // create hw ctx handle if not already created
    if (!m_hdl) {
      m_hdl = m_core_device->create_hw_context(elf, m_cfg_param, m_mode);
      m_partition_size = part_size;
      create_elf_map(elf);
      m_elf_flow = true; // ELF flow
      m_uc_log_buf = init_uc_log_buf(m_core_device, m_hdl.get()); // create only for first config
      return;
    }

    // add ELF only if partition size matches existing configuration
    if (m_partition_size != part_size)
      throw std::runtime_error("can not add config to ctx with different configuration\n");

    // Add kernels available in ELF to elf map
    // This function throws if kernel with same name is already present
    create_elf_map(elf);
  }

  void
  update_qos(const qos_type& qos)
  {
    m_hdl->update_qos(qos);
  }

  void
  set_exclusive()
  {
    m_mode = xrt::hw_context::access_mode::exclusive;
    m_hdl->update_access_mode(m_mode);
  }

  const std::shared_ptr<xrt_core::device>&
  get_core_device() const
  {
    return m_core_device;
  }

  // Dump uC log buffer if configured
  // This function can be called from different object like run, runlist
  // to dump the log buffer on demand
  void
  dump_uc_log_buffer()
  {
    if (!m_uc_log_buf)
      return;

    m_uc_log_buf->dump();
  }

  xrt::uuid
  get_uuid() const
  {
    return m_xclbin.get_uuid();
  }

  xrt::xclbin
  get_xclbin() const
  {
    return m_xclbin;
  }

  access_mode
  get_mode() const
  {
    return m_mode;
  }

  size_t
  get_partition_size() const
  {
    return m_partition_size;
  }

  xrt_core::hwctx_handle*
  get_hwctx_handle()
  {
    return m_hdl.get();
  }

  xrt_core::usage_metrics::base_logger*
  get_usage_logger()
  {
    return m_usage_logger.get();
  }

  xrt::module
  get_module(const std::string& kname)
  {
     auto itr = m_elf_map.find(kname);
     if (itr == m_elf_map.end())
       throw std::runtime_error("no module found with given kernel name in ctx");

     auto [kmitr, inserted] = m_kernel_mod_map.try_emplace(kname, xrt::module{});
     if (inserted)
       kmitr->second = xrt::module{itr->second}; // create module from the ELF that has this kernel

     return kmitr->second;
  }

  bool
  get_elf_flow() const
  {
    return m_elf_flow;
  }

  double
  get_aie_freq() const
  {
    try {
      auto freq_hz = m_hdl->get_aie_freq();
      return static_cast<double>(freq_hz) / hz_per_mhz; // Convert Hz to MHz
    }
    catch (const xrt_core::error& e) {
      if (e.code() == std::errc::not_supported)
        throw std::runtime_error("get_aie_freq() API is not supported on this platform");

      throw std::runtime_error("Failed to read AIE frequency: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
      throw std::runtime_error("Failed to read AIE frequency: " + std::string(e.what()));
    }
  }

  void
  set_aie_freq(double freq_mhz)
  {
    try {
      auto freq_hz = static_cast<uint64_t>(freq_mhz * hz_per_mhz);
      m_hdl->set_aie_freq(freq_hz);
    }
    catch (const xrt_core::error& e) {
      if (e.code() == std::errc::not_supported)
        throw std::runtime_error("set_aie_freq() API is not supported on this platform");

      throw std::runtime_error("Failed to set AIE frequency: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
      throw std::runtime_error("Failed to set AIE frequency: " + std::string(e.what()));
    }
  }

  // Creates scratchpad memory buffer on demand
  // If buffer is already created, returns the existing one
  // std::call_once is used to ensure thread safe lazy initialization
  const xrt::bo&
  get_scratchpad_mem_buf(size_t size_per_col)
  {
    std::call_once(m_scratchpad_init_flag, [this, size_per_col] () {
      try {
        // create scratchpad memory buffer using this context
        auto buf_size = size_per_col * m_partition_size;
        m_scratchpad_buf = xrt_core::bo_int::create_bo(
            m_core_device, buf_size, xrt_core::bo_int::use_type::scratch_pad);
      }
      catch (...) { /*do nothing*/ }
    });

    return m_scratchpad_buf;
  }

  void
  dump_scratchpad_mem()
  {
    if (m_scratchpad_buf.size() == 0) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_hw_context",
                              "preemption scratchpad memory is not available");
      return;
    }

    // sync data from device before dumping into file
    m_scratchpad_buf.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    std::string dump_file_name =
        "preemption_scratchpad_mem_" + std::to_string(m_hdl->get_slotidx()) + "_" +
        xrt_core::get_timestamp_for_filename() + ".bin";
    dump_bo(m_scratchpad_buf.map<char*>(), dump_file_name, m_scratchpad_buf.size());

    std::string msg {"Dumped scratchpad buffer into file : "};
    msg.append(dump_file_name);
    xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_hw_context", msg);
  }

  std::vector<char>
  get_aie_coredump() const
  {
    try {
      xrt_core::query::aie_coredump::args args{};
      args.pid = static_cast<uint64_t>(xrt_core::utils::get_pid());
      args.context_id = static_cast<uint32_t>(m_hdl->get_slotidx());
      return xrt_core::device_query<xrt_core::query::aie_coredump>(m_core_device, args);
    }
    catch (const xrt_core::query::no_such_key&) {
      throw std::runtime_error("AIE coredump is not supported on this platform");
    }
    catch (const std::exception& e) {
      throw std::runtime_error("Failed to get AIE coredump: " + std::string(e.what()));
    }
  }

  // Returns map of kernel names to their corresponding elf files
  // registered with this hardware context
  const std::map<std::string, xrt::elf>&
  get_elf_map() const
  {
    return m_elf_map;
  }
};

} // xrt

////////////////////////////////////////////////////////////////
// xrt_hw_context implementation of extension APIs not exposed to end-user
////////////////////////////////////////////////////////////////
namespace xrt_core::hw_context_int {

std::shared_ptr<xrt_core::device>
get_core_device(const xrt::hw_context& hwctx)
{
  return hwctx.get_handle()->get_core_device();
}

xrt_core::device*
get_core_device_raw(const xrt::hw_context& hwctx)
{
  return hwctx.get_handle()->get_core_device().get();
}

void
set_exclusive(xrt::hw_context& hwctx)
{
  hwctx.get_handle()->set_exclusive();
}

xrt::hw_context
create_hw_context_from_implementation(void* hwctx_impl)
{
  if (!hwctx_impl)
    throw std::runtime_error("Invalid hardware context implementation.");

  auto impl_ptr = static_cast<xrt::hw_context_impl*>(hwctx_impl);
  return xrt::hw_context(impl_ptr->get_shared_ptr());
}

xrt::module
get_module(const xrt::hw_context& ctx, const std::string& kname)
{
  return ctx.get_handle()->get_module(kname);
}

size_t
get_partition_size(const xrt::hw_context& ctx)
{
  return ctx.get_handle()->get_partition_size();
}

bool
get_elf_flow(const xrt::hw_context& ctx)
{
  return ctx.get_handle()->get_elf_flow();
}

const xrt::bo&
get_scratchpad_mem_buf(const xrt::hw_context& hwctx, size_t size_per_col)
{
  return hwctx.get_handle()->get_scratchpad_mem_buf(size_per_col);
}

void
dump_scratchpad_mem(const xrt::hw_context& hwctx)
{
  return hwctx.get_handle()->dump_scratchpad_mem();
}

void
dump_uc_log_buffer(const xrt::hw_context& hwctx)
{
  return hwctx.get_handle()->dump_uc_log_buffer();
}

const std::map<std::string, xrt::elf>&
get_elf_map(const xrt::hw_context& hwctx)
{
  return hwctx.get_handle()->get_elf_map();
}

} // xrt_core::hw_context_int

////////////////////////////////////////////////////////////////
// xrt_hwcontext C++ API implmentations (xrt_hw_context.h)
////////////////////////////////////////////////////////////////
namespace xrt {
// common function called with hw ctx created from different ways
static std::shared_ptr<hw_context_impl>
post_alloc_hwctx(const std::shared_ptr<hw_context_impl>& handle)
{
  // Update device is called with a raw pointer to dyanamically
  // link to callbacks that exist in XDP via a C-style interface
  // The create_hw_context_from_implementation function is then
  // called in XDP create a hw_context to the underlying implementation
  xrt_core::xdp::update_device(handle.get(), true);
  handle->get_usage_logger()->log_hw_ctx_info(handle.get());
  return handle;
}

static std::shared_ptr<hw_context_impl>
alloc_hwctx_from_cfg(const xrt::device& device, const xrt::uuid& xclbin_id, const xrt::hw_context::cfg_param_type& cfg_param)
{
  XRT_TRACE_POINT_SCOPE(xrt_hw_context);
  return post_alloc_hwctx(std::make_shared<hw_context_impl>(device.get_handle(), xclbin_id, cfg_param));
}

static std::shared_ptr<hw_context_impl>
alloc_hwctx_from_mode(const xrt::device& device, const xrt::uuid& xclbin_id, xrt::hw_context::access_mode mode)
{
  XRT_TRACE_POINT_SCOPE(xrt_hw_context);
  return post_alloc_hwctx(std::make_shared<hw_context_impl>(device.get_handle(), xclbin_id, mode));
}

static std::shared_ptr<hw_context_impl>
alloc_empty_hwctx(const xrt::device& device, const xrt::hw_context::cfg_param_type& cfg_param, xrt::hw_context::access_mode mode)
{
  XRT_TRACE_POINT_SCOPE(xrt_hw_context);
  return post_alloc_hwctx(std::make_shared<hw_context_impl>(device.get_handle(), cfg_param, mode));
}

static std::shared_ptr<hw_context_impl>
alloc_hwctx_from_elf(const xrt::device& device, const xrt::elf& elf, const xrt::hw_context::cfg_param_type& cfg_param,
                     xrt::hw_context::access_mode mode)
{
  XRT_TRACE_POINT_SCOPE(xrt_hw_context);
  return post_alloc_hwctx(std::make_shared<hw_context_impl>(device.get_handle(), elf, cfg_param, mode));
}

hw_context::
hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, const xrt::hw_context::cfg_param_type& cfg_param)
  : detail::pimpl<hw_context_impl>(alloc_hwctx_from_cfg(device, xclbin_id, cfg_param))
{}

hw_context::
hw_context(const xrt::device& device, const xrt::uuid& xclbin_id, access_mode mode)
  : detail::pimpl<hw_context_impl>(alloc_hwctx_from_mode(device, xclbin_id, mode))
{}

hw_context::
hw_context(const xrt::device& device, const xrt::elf& elf, const cfg_param_type& cfg_param, access_mode mode)
  : detail::pimpl<hw_context_impl>(alloc_hwctx_from_elf(device, elf, cfg_param, mode))
{}

hw_context::
hw_context(const xrt::device& device, const xrt::elf& elf)
  : hw_context(device, elf, {}, access_mode::shared)
{}

hw_context::
hw_context(const xrt::device& device, const cfg_param_type& cfg_param, access_mode mode)
  : detail::pimpl<hw_context_impl>(alloc_empty_hwctx(device, cfg_param, mode))
{}

void
hw_context::
add_config(const xrt::elf& elf)
{
  get_handle()->add_config(elf);
}

void
hw_context::
update_qos(const qos_type& qos)
{
  XRT_TRACE_POINT_SCOPE(xrt_hw_context_update_qos);
  get_handle()->update_qos(qos);
}

xrt::device
hw_context::
get_device() const
{
  return xrt::device{get_handle()->get_core_device()};
}

xrt::uuid
hw_context::
get_xclbin_uuid() const
{
  return get_handle()->get_uuid();
}

xrt::xclbin
hw_context::
get_xclbin() const
{
  return get_handle()->get_xclbin();
}

hw_context::access_mode
hw_context::
get_mode() const
{
  return get_handle()->get_mode();
}

hw_context::
operator xrt_core::hwctx_handle* () const
{
  return get_handle()->get_hwctx_handle();
}

hw_context::
~hw_context()
{}

std::vector<char>
hw_context::
get_aie_coredump() const
{
  return get_handle()->get_aie_coredump();
}

} // xrt

////////////////////////////////////////////////////////////////
// xrt_aie_hw_context C++ API implmentations (xrt_aie.h)
////////////////////////////////////////////////////////////////
namespace xrt::aie {

double
hw_context::
get_aie_freq() const
{
  return get_handle()->get_aie_freq();
}

void
hw_context::
set_aie_freq(double freq_mhz)
{
  get_handle()->set_aie_freq(freq_mhz);
}

void
hw_context::
reset_array()
{
  auto core_handle = get_handle()->get_hwctx_handle();
  core_handle->reset_array();
}
} //xrt::aie
