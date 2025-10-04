// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_queue.h
#define XRT_API_SOURCE         // exporting xrt_hwcontext.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_xclbin.h
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/time.h"
#include "core/common/utils.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"
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
    size_t m_size_per_uc;
    xrt::bo m_uc_log_bo; // log buffer used for uc logging

    static xrt::bo
    init_and_get_uc_log_bo(const std::shared_ptr<xrt_core::device>& device,
                           xrt_core::hwctx_handle* ctx_hdl,
                           size_t size_per_uc,
                           size_t num_uc)
    {
      auto bo = xrt_core::bo_int::
        create_bo(device, (size_per_uc * num_uc), xrt_core::bo_int::use_type::log);

      // Log buffers first 8 bytes are used for metadata
      // So make sure for each uC metadata bytes are initialized with
      // zero's before configuring
      constexpr size_t metadata_size = 8;
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

      xrt_core::bo_int::config_bo(bo, uc_buf_map, ctx_hdl); // configure the log buffer

      xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_hw_context",
                              "uC log buffer initialized successfully");

      return bo;
    }

    // may throw
    uc_log_buffer(const std::shared_ptr<xrt_core::device>& device,
                  xrt_core::hwctx_handle* ctx_hdl,
                  size_t size)
      : m_num_uc(ctx_hdl->get_num_uc())
      , m_slot_idx(ctx_hdl->get_slotidx())
      , m_size_per_uc(size)
      , m_uc_log_bo(init_and_get_uc_log_bo(device, ctx_hdl, size, m_num_uc))
    {}

    ~uc_log_buffer()
    {
      try {
        // dump uc log buffer if configured in constructor
        if (!m_uc_log_bo)
          return;

        // sync the log buffer
        m_uc_log_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

        // dump the log buffer for each uc in a separate file
        // Use timestamp, slot index in file name
        for (size_t i = 0; i < m_num_uc; i++) {
          auto file_name = "uc_log_" + std::to_string(xrt_core::utils::get_pid()) + "_"
              + xrt_core::get_timestamp_for_filename() + "_" + std::to_string(m_slot_idx) + "_" + std::to_string(i) + ".bin";
          dump_bo(m_uc_log_bo.map<char*>(), file_name, m_size_per_uc, (i * m_size_per_uc));
        }
      }
      catch (const std::exception& e) {
        xrt_core::message::send(xrt_core::message::severity_level::debug, "xrt_hw_context",
                                std::string{"Failed to dump UC log buffer : "} + e.what());
      }
    }
  };

  std::shared_ptr<xrt_core::device> m_core_device;
  xrt::xclbin m_xclbin;
  std::map<std::string, xrt::module> m_module_map; // map b/w kernel name and module
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
  // Vector of XDP runs to be added at beginning and end of runlist
  // These runs initializes/configures AI array and collects the profile/trace data
  std::vector<xrt::run> m_xdp_init_runs;
  std::vector<xrt::run> m_xdp_exit_runs;

  std::shared_ptr<xrt_core::usage_metrics::base_logger> m_usage_logger =
      xrt_core::usage_metrics::get_usage_metrics_logger();
  bool m_elf_flow = false;

  void
  create_module_map(const xrt::elf& elf)
  {
    xrt::module module_obj{elf};

    // Store the module in the map against all available kernels in the ELF
    // This will be useful for module lookup when creating xrt::kernel object
    // using kernel name
    const auto& kernels_info = xrt_core::module_int::get_kernels_info(module_obj);
    for (const auto& k_info : kernels_info) {
      auto kernel_name = k_info.props.name;
      if (m_module_map.find(kernel_name) != m_module_map.end())
        throw std::runtime_error("kernel already exists, cannot use this ELF with this hw ctx\n");

      m_module_map.emplace(std::move(kernel_name), module_obj);
    }
  }

  // Initializes uc log buffer, configures it by splitting the buffer
  // equally among available columns
  static std::unique_ptr<uc_log_buffer>
  init_uc_log_buf(const std::shared_ptr<xrt_core::device>& device, xrt_core::hwctx_handle* ctx_hdl)
  {
    // Create uc log buffer only if ini option is enabled
    // If enabled, but not supported then this function returns nullptr
    static auto uc_log_buf_size = xrt_core::config::get_log_buffer_size_per_uc();
    if (!uc_log_buf_size || !ctx_hdl)
      return nullptr;

    // We get size of single uc but we create one buffer for all uc's
    // and split it uc needs buffer that is 32 Byte aligned
    constexpr std::size_t alignment = 32;
    // round up size to be 32 Byte aligned
    size_t uc_aligned_size = (uc_log_buf_size + alignment - 1) & ~(alignment - 1);
    try {
      return std::make_unique<uc_log_buffer>(device, ctx_hdl, uc_aligned_size);
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
    create_module_map(elf);
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
      create_module_map(elf);
      m_elf_flow = true; // ELF flow
      m_uc_log_buf = init_uc_log_buf(m_core_device, m_hdl.get()); // create only for first config
      return;
    }

    // add module only if partition size matches existing configuration
    if (m_partition_size != part_size)
      throw std::runtime_error("can not add config to ctx with different configuration\n");

    // Add kernels available in ELF to module map
    // This function throws if kernel with same name is already present
    create_module_map(elf);
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
  get_module(const std::string& kname) const
  {
    if (auto itr = m_module_map.find(kname); itr != m_module_map.end())
      return itr->second;

    throw std::runtime_error("no module found with given kernel name in ctx");
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
        m_scratchpad_buf = xrt::ext::bo{xrt::hw_context(get_shared_ptr()),
                                        size_per_col * m_partition_size};
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

  // Callback Functions for XDP to register init and exit runs
  // that are added to xrt::runlist at start and end respectively
  void
  register_xdp_init_run(const xrt::run& run)
  {
    m_xdp_init_runs.push_back(run);
  }

  void
  register_xdp_exit_run(const xrt::run& run)
  {
    m_xdp_exit_runs.push_back(run);
  }

  // Functions to get XDP init and exit run vectors
  const std::vector<xrt::run>&
  get_xdp_init_runs() const
  {
    return m_xdp_init_runs;
  }

  const std::vector<xrt::run>&
  get_xdp_exit_runs() const
  {
    return m_xdp_exit_runs;
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
register_xdp_init_run(const xrt::hw_context& ctx, const xrt::run& run)
{
  ctx.get_handle()->register_xdp_init_run(run);
}

XRT_CORE_COMMON_EXPORT
void
register_xdp_exit_run(const xrt::hw_context& ctx, const xrt::run& run)
{
  ctx.get_handle()->register_xdp_exit_run(run);
}

const std::vector<xrt::run>&
get_xdp_init_runs(const xrt::hw_context& ctx)
{
  return ctx.get_handle()->get_xdp_init_runs();
}

const std::vector<xrt::run>&
get_xdp_exit_runs(const xrt::hw_context& ctx)
{
  return ctx.get_handle()->get_xdp_exit_runs();
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
