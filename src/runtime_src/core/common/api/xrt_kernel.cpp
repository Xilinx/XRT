// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT kernel APIs as declared in
// core/include/experimental/xrt_kernel.h
#define XRT_API_SOURCE  // exporting xrt_kernel.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_kernel.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/xrt/xrt_kernel.h"

#include "core/include/experimental/xrt_hw_context.h"
#include "core/include/experimental/xrt_mailbox.h"
#include "core/include/experimental/xrt_xclbin.h"
#include "core/include/ert.h"
#include "core/include/ert_fa.h"

#include "bo.h"
#include "command.h"
#include "context_mgr.h"
#include "device_int.h"
#include "handle.h"
#include "hw_context_int.h"
#include "hw_queue.h"
#include "kernel_int.h"
#include "native_profile.h"
#include "xclbin_int.h"

#include "core/common/bo_cache.h"
#include "core/common/config_reader.h"
#include "core/common/cuidx_type.h"
#include "core/common/device.h"
#include "core/common/debug.h"
#include "core/common/error.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/xclbin_parser.h"

#include <boost/format.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <condition_variable>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <fstream>
#include <type_traits>
#include <utility>
using namespace std::chrono_literals;

#ifdef _WIN32
# pragma warning( disable : 4244 4267 4996 4100 4201)
#endif

////////////////////////////////////////////////////////////////
// Exposed for Vitis aietools as extensions to xrt_kernel.h
// Revisit post 2020.1
////////////////////////////////////////////////////////////////
/**
 * xrtRunSetArgV() - Set a specific kernel argument for this run
 *
 * @runHandle:  Handle to the run object to modify
 * @index:      Index of kernel argument to set
 * @value:      The value to set for argument.
 * @size:       The size of value in bytes.
 * Return:      0 on success, -1 on error
 *
 * Use this API to explicitly set specific kernel arguments prior
 * to starting kernel execution.  After setting all arguments, the
 * kernel execution can be start with xrtRunStart()
 */
XCL_DRIVER_DLLESPEC
int
xrtRunSetArgV(xrtRunHandle runHandle, int index, const void* value, size_t bytes);

/**
 * xrtRunGetArgV() - Asynchronous get a specific kernel argument for this run
 *
 * @runHandle:  Handle to the run object to modify
 * @index:      Index of kernel argument to read
 * @value:      Destination data pointer where argument value is written
 * @size:       The size of value in bytes.
 * Return:      0 on success, -1 on error
 *
 * Use this API to asynchronously access a specific kernel argument while
 * kernel is running.  This function reads the register map for the compute
 * unit associated with this run.  It is an error to read from a run object
 * associated with multiple compute units.
 */
XCL_DRIVER_DLLESPEC
int
xrtRunGetArgV(xrtRunHandle runHandle, int index, void* value, size_t bytes);

// C++ run object variant
XCL_DRIVER_DLLESPEC
void
xrtRunGetArgVPP(xrt::run run, int index, void* value, size_t bytes);

/**
 * xrtRunUpdateArgV() - Asynchronous update of kernel argument
 *
 * @runHandle:  Handle to the run object to modify
 * @index:      Index of kernel argument to update
 * @value:      The value to set for argument.
 * @size:       The size of value in bytes.
 * Return:      0 on success, -1 on error
 *
 * Use this API to asynchronously update a specific kernel
 * argument of an existing run.
 *
 * This API is only supported on Edge.
 */
XCL_DRIVER_DLLESPEC
int
xrtRunUpdateArgV(xrtRunHandle rhdl, int index, const void* value, size_t bytes);
////////////////////////////////////////////////////////////////

namespace {

constexpr size_t mailbox_input_write = 1;
constexpr size_t mailbox_output_read = 1;
constexpr size_t mailbox_input_ack = (1 << 1);
constexpr size_t mailbox_output_ack = (1 << 1);
// TODO: find offsets from meta data
constexpr size_t mailbox_auto_restart_cntr = 0x10;
constexpr size_t mailbox_input_ctrl_reg = 0x14;
constexpr size_t mailbox_output_ctrl_reg = 0x18;

constexpr size_t max_cus = 128;
constexpr size_t cus_per_word = 32;

XRT_CORE_UNUSED // debug enabled function
std::string
debug_cmd_packet(const std::string& msg, const ert_packet* pkt)
{
  static auto fnm = std::getenv("MBS_PRINT_REGMAP");
  if (!fnm)
    return "";

  std::ofstream ostr(fnm,std::ios::app);
  //std::ostringstream ostr;
  constexpr auto indent3 = 3; // stupid lint warnings
  constexpr auto indent8 = 8; // stupid lint warnings
  ostr << msg << "\n";
  ostr << std::uppercase << std::setfill('0') << std::setw(3);
  ostr << "pkt->header    = 0x"
       << std::setw(indent8) << std::hex << pkt->header << std::dec << "\n";
  for (size_t i = 0; i < pkt->count; ++i)
    ostr << "pkt->data[" << std::setw(indent3) << i << "] = 0x"
         << std::setw(indent8) << std::hex << pkt->data[i] << std::dec << "\n";
  return fnm;
}

// Helper class for representing an in-memory kernel argument.  User
// calls kernel(arg1, arg2, ...).  This class stores the address of
// the kernel argument as provided by user and its size in number of
// words (sizeof(ValueType)).
//
// Previous incarnation used std::vector<uint32_t> to represent a
// kernel argument, but that incurs a heap operation constructing the
// vector data and that is too expensive.
//
// Templated header (xrt_kernel.h) passes &arg and sizeof(arg) to
// implementation (this file), where arg_range is constructed from the
// void* and size.
//
// The key here is that arg_range is zero-copy, it simply wraps caller
// storage used from the argument while provide an iterator interface.
//
// Note that in order to avoid ABR, host size (bytes) must be multiple
// sizeof(ValueType).  It is tempting to use ValueType matching the
// kernel register entry size (uint32_t), but host size can be byte
// aligned (e.g. single char) and if rounded up to sizeof(ValueType)
// it would result in reading junk data past the allocated bytes.
template <typename ValueType>
class arg_range
{
  const ValueType* uval;
  size_t words;

  // Number of bytes must multiple of sizeof(ValueType)
  size_t
  validate_bytes(size_t bytes)
  {
    if (bytes % sizeof(ValueType))
      throw std::runtime_error("arg_range unaligned bytes");
    return bytes;
  }

public:
  arg_range(const void* value, size_t bytes)
    : uval(reinterpret_cast<const ValueType*>(value))
    , words(validate_bytes(bytes) / sizeof(ValueType))
  {}

  const ValueType*
  begin() const
  {
    return uval;
  }

  const ValueType*
  end() const
  {
    return uval + words;
  }

  size_t
  size() const
  {
    return words;
  }

  size_t
  bytes() const
  {
    return words * sizeof(ValueType);
  }

  const ValueType*
  data() const
  {
    return uval;
  }
};

// Copy byte-by-byte from value to a uint64_t.
// At most sizeof(uint64_t) bytes are copied.
template <typename ValueType>
uint64_t
to_uint64_t(ValueType value)
{
  uint64_t ret = 0;
  auto data = reinterpret_cast<uint8_t*>(&ret);
  arg_range<uint8_t> range{&value, sizeof(ValueType)};
  std::copy_n(range.begin(), std::min<size_t>(sizeof(ret), range.size()), data);
  return ret;
}

inline bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? std::strcmp(xem,"sw_emu")==0 : false;
  return swem;
}

inline bool
has_reg_read_write()
{
#ifdef _WIN32
  return false;
#else
  return true;
#endif
}

inline std::vector<uint32_t>
value_to_uint32_vector(const void* value, size_t bytes)
{
  bytes = std::max(bytes, sizeof(uint32_t));
  auto uval = reinterpret_cast<const uint32_t*>(value);
  return { uval, uval + bytes / sizeof(uint32_t)};
}

template <typename ValueType>
inline std::vector<uint32_t>
value_to_uint32_vector(ValueType value)
{
  return value_to_uint32_vector(&value, sizeof(value));
}

static xrt::hw_context::access_mode
hwctx_access_mode(xrt::kernel::cu_access_mode mode)
{
  switch (mode) {
  case xrt::kernel::cu_access_mode::exclusive:
    return xrt::hw_context::access_mode::exclusive;
  case xrt::kernel::cu_access_mode::shared:
    return xrt::hw_context::access_mode::shared;
  default:
    throw std::runtime_error("unexpected access mode for kernel");
  }
}

// Transition only, to be removed
static xrt::kernel::cu_access_mode
cu_access_mode(xrt::hw_context::access_mode mode)
{
  switch (mode) {
  case xrt::hw_context::access_mode::exclusive:
    return xrt::kernel::cu_access_mode::exclusive;
  case xrt::hw_context::access_mode::shared:
    return xrt::kernel::cu_access_mode::shared;
  default:
    throw std::runtime_error("unexpected access mode for kernel");
  }
}

// struct device_type - Extends xrt_core::device
//
// This struct is not really needed.
// Data members should be moved to xrt_core::device, but
// some are maintained in shim and not accessible outside
// What's worse is that not all data members are in all shims
struct device_type
{
  std::shared_ptr<xrt_core::device> core_device;
  xrt_core::bo_cache exec_buffer_cache;
  uint32_t uid; // internal unique id for debug

  static constexpr unsigned int cache_size = 128;

  static uint32_t
  create_uid()
  {
    static std::atomic<uint32_t> count {0};
    return count++;
  }

  explicit
  device_type(xrtDeviceHandle dhdl)
    : core_device(xrt_core::device_int::get_core_device(dhdl))
    , exec_buffer_cache(core_device->get_device_handle(), cache_size)
    , uid(create_uid())
  {
    XRT_DEBUGF("device_type::device_type(%d)\n", uid);
  }

  explicit
  device_type(std::shared_ptr<xrt_core::device> cdev)
    : core_device(std::move(cdev))
    , exec_buffer_cache(core_device->get_device_handle(), cache_size)
    , uid(create_uid())
  {
    XRT_DEBUGF("device_type::device_type(%d)\n", uid);
  }

  // NOLINTNEXTLINE(modernize-use-equals-default)
  ~device_type()
  {
    XRT_DEBUGF("device_type::~device_type(%d)\n", uid);
  }

  device_type(const device_type&) = delete;
  device_type(device_type&&) = delete;
  device_type& operator=(device_type&) = delete;
  device_type& operator=(device_type&&) = delete;

  template <typename CommandType>
  xrt_core::bo_cache::cmd_bo<CommandType>
  create_exec_buf()
  {
    return exec_buffer_cache.alloc<CommandType>();
  }

  xrt_core::device*
  get_core_device() const
  {
    return core_device.get();
  }

  xrt::device
  get_xrt_device() const
  {
    return xrt::device{core_device};
  }
};

// struct encoded_bitset - Sparse bit set
//
// Used to represent compressed mem_topology indidices of an xclbin.
// Many entries are unused and can be ignored, yet section size
// (indices) can be arbitrary long.  The encoding is a mapping from
// original index to compressed index.
//
// Using this encoded bitset allows a smaller sized std::bitset
// to be used for representing memory connectivity, where as a
// uncompressed bitset would require 1000s of entries.
template <size_t size>
class encoded_bitset
{
public:
  encoded_bitset() = default;

  // Encoding is represented using a vector  that maps
  // the original index to the encoded (compressed) index.
  explicit
  encoded_bitset(const std::vector<size_t>* enc)
    : m_encoding(enc)
  {}

  void
  set(size_t idx)
  {
    m_bitset.set(m_encoding ? m_encoding->at(idx) : idx);
  }

  bool
  test(size_t idx) const
  {
    return m_bitset.test(m_encoding ? m_encoding->at(idx) : idx);
  }

private:
  const std::vector<size_t>* m_encoding = nullptr;
  std::bitset<size> m_bitset;
};

// struct ip_context - Manages process access to CUs
//
// Constructing a kernel object opens a context on the CUs associated
// with the kernel object.  The context is reference counted such that
// multiple kernel objects can open a context on the same CU provided
// the access type is shared.
//
// A CU context is released when the last kernel object referencing it
// is closed.  If the process closes without having released on kernel
// then behavior is undefined.
class ip_context
{
  // class connectivy - Represents argument connectiviy to memory banks
  //
  // The argument connectivity is represented using a compressed bitset
  // where unused mem_topology entries have been removed.  This allows
  // for a much smaller bitset to represented all possible connectivity
  //
  // @connections: connectivity for each ip argument
  // @default_connection: default connectivity for an argument
  class connectivity
  {
    static constexpr int32_t no_memidx = -1;
    static constexpr size_t max_connections = 64;
    std::vector<encoded_bitset<max_connections>> connections; // indexed by argidx
    std::vector<int32_t> default_connection;                  // indexed by argidx

    // Resize the vectors if neccessary
    void
    resize(size_t size, const std::vector<size_t>* encoding)
    {
      if (connections.size() >= size)
        return;

      connections.resize(size, encoded_bitset<max_connections>{encoding});
      default_connection.resize(size, no_memidx);
    }

  public:
    connectivity() = default;

    // @xclbin: meta data
    // @ip: ip to compute connectivity for
    connectivity(const xrt::xclbin& xclbin, const xrt::xclbin::ip& ip)
    {
      const auto& memidx_encoding = xrt_core::xclbin_int::get_membank_encoding(xclbin);

      // collect the memory connections for each IP argument
      for (const auto& arg : ip.get_args()) {
        auto argidx = arg.get_index();

        for (const auto& mem : arg.get_mems()) {
          auto memidx = mem.get_index();

          // disregard memory indices that do not map to a memory mapped bank
          // this could be streaming connections
          if (memidx_encoding.at(memidx) == std::numeric_limits<size_t>::max())
            continue;

          resize(argidx + 1, &memidx_encoding);
          connections[argidx].set(memidx);

          // default connections is largest memidx to account for groups
          default_connection[argidx] = std::max(default_connection[argidx], memidx);
        }
      }
    }

    // Get default memory index of an argument.  The default index is
    // the the largest memory index of a connection for specified argument.
    int32_t
    get_arg_memidx(size_t argidx) const
    {
      return default_connection.at(argidx);
    }

    // Validate that specified memory index is a valid connection for
    // argument identified by 'argidx'
    bool
    valid_arg_connection(size_t argidx, size_t memidx) const
    {
      return connections[argidx].test(memidx);
    }
  };


public:
  using access_mode = xrt::kernel::cu_access_mode;
  using slot_id = xrt_core::device::slot_id;

  // open() - open a context in a specific IP/CU
  //
  // @device:    Device on which context should opened
  // @xclbin:    xclbin containeing the IP definition
  // @ip:        The ip_data defintion for this IP from the xclbin
  // @cuidx:     Index of CU used when opening context and populating cmd pkt
  // @am:        Access mode, how this CU should be opened
  static std::shared_ptr<ip_context>
  open(const xrt::hw_context& hwctx, const xrt::xclbin::ip& ip)
  {
    // - IP index is unique per device
    // - IP index is unique per domain
    // - IP index is unique per slot (is this true for PS kernels in PL/PS xclbin?)
    // - IP index can be shared between hwctx, provided hwctx refer
    //   to same slot
    // - A slot can contain only one domain type of CUs (PL/PS xclbin ??)
    //
    // For cases (drivers) that support multi-xclbin and sharing it is
    // assumed that each hwctx is unique and has a unique ctx handle,
    // and that opening a CU context is required for each hwctx.
    //
    // This function therefore associates ipctx with each hwctx handle
    // so that if same hwctx is used repeatedly, CU contexts within
    // that hwctx are opened only once.
    //
    // The function also ensures that different devices can share same
    // hwctx handle, implying that even for same handle index, the CU
    // should be opened again if the device is different
    using ctx_ips = std::map<std::string, std::weak_ptr<ip_context>>;
    using ctx_to_ips = std::map<xcl_hwctx_handle, ctx_ips>;
    static std::mutex mutex;
    static std::map<xrt_core::device*, ctx_to_ips> dev2ips;
    auto device = xrt_core::hw_context_int::get_core_device_raw(hwctx);
    auto ctxhdl = static_cast<xcl_hwctx_handle>(hwctx);
    std::lock_guard<std::mutex> lk(mutex);
    auto& ctx2ips = dev2ips[device]; // hwctx handle -> [ip_context]*
    auto& ips = ctx2ips[ctxhdl];     // ipname -> ip_context
    auto ipctx = ips[ip.get_name()].lock();
    if (!ipctx)
      // NOLINTNEXTLINE(modernize-make-shared)  used in weak_ptr
      ips[ip.get_name()] = ipctx = std::shared_ptr<ip_context>(new ip_context(hwctx, ip));

    return ipctx;
  }

  access_mode
  get_access_mode() const
  {
    return cu_access_mode(m_hwctx.get_mode());
  }

  // For symmetry
  void
  close()
  {}

  size_t
  get_size() const
  {
    return m_size;
  }

  uint64_t
  get_address() const
  {
    return m_address;
  }

  xrt_core::cuidx_type
  get_index() const
  {
    return m_idx;
  }

  unsigned int
  get_cuidx() const
  {
    return m_idx.domain_index; // index used for execution cumask
  }

  slot_id
  get_slot() const
  {
    return static_cast<xcl_hwctx_handle>(m_hwctx);
  }

  // Check if arg is connected to specified memory bank
  bool
  valid_connection(size_t argidx, int32_t memidx)
  {
    return m_args.valid_arg_connection(argidx, memidx);
  }

  // Get default memory bank for argument at specified index The
  // default memory bank is the connection with the highest group
  // connectivity index
  int32_t
  arg_memidx(size_t argidx) const
  {
    return m_args.get_arg_memidx(argidx);
  }

  ~ip_context()
  {
    xrt_core::context_mgr::close_context(m_hwctx, m_idx);
  }

  ip_context(const ip_context&) = delete;
  ip_context(ip_context&&) = delete;
  ip_context& operator=(ip_context&) = delete;
  ip_context& operator=(ip_context&&) = delete;

  std::pair<uint32_t, uint32_t> m_readrange = {0,0};  // start address, size

private:
  // regular CU
  ip_context(xrt::hw_context xhwctx, xrt::xclbin::ip xip)
    : m_hwctx(std::move(xhwctx))
    , m_ip(std::move(xip))
    , m_args(m_hwctx.get_xclbin(), m_ip)
    , m_idx(xrt_core::context_mgr::open_context(m_hwctx, m_ip.get_name()))
    , m_address(m_ip.get_base_address())
    , m_size(m_ip.get_size())
  {}

  xrt::hw_context m_hwctx;    // hw context in which IP is opened
  xrt::xclbin::ip m_ip;       // the xclbin ip object
  connectivity m_args;        // argument memory connections
  xrt_core::cuidx_type m_idx; // cu domain and index
  uint64_t m_address;         // cache base address for programming
  size_t m_size;              // cache address space size
};

// Remove when c++17
constexpr int32_t ip_context::connectivity::no_memidx;

// class kernel_command - Immplements command API expected by schedulers
//
// The kernel command is
class kernel_command : public xrt_core::command
{
public:
  using execbuf_type = xrt_core::bo_cache::cmd_bo<ert_start_kernel_cmd>;
  using callback_function_type = std::function<void(ert_cmd_state)>;
  using callback_list = std::vector<callback_function_type>;

private:
  // Return state of underlying exec buffer packet This is an
  // asynchronous call, the command object may not be in the same
  // state as reflected by the return value.
  ert_cmd_state
  get_state_raw() const
  {
    auto pkt = get_ert_packet();
    return static_cast<ert_cmd_state>(pkt->state);
  }


public:
  explicit
  kernel_command(std::shared_ptr<device_type> dev, xrt_core::hw_queue hwqueue)
    : m_device(std::move(dev))
    , m_hwqueue(std::move(hwqueue))
    , m_execbuf(m_device->create_exec_buf<ert_start_kernel_cmd>())
    , m_done(true)
  {
    static unsigned int count = 0;
    m_uid = count++;
    XRT_DEBUGF("kernel_command::kernel_command(%d)\n", m_uid);
  }

  ~kernel_command() override
  {
    XRT_DEBUGF("kernel_command::~kernel_command(%d)\n", m_uid);
    // This is problematic, bo_cache should return managed BOs
    m_device->exec_buffer_cache.release(m_execbuf);
  }

  kernel_command(const kernel_command&) = delete;
  kernel_command(kernel_command&&) = delete;
  kernel_command& operator=(kernel_command&) = delete;
  kernel_command& operator=(kernel_command&&) = delete;

  void
  encode_compute_units(const std::bitset<max_cus>& cumask, size_t num_cumasks)
  {
    auto ecmd = get_ert_cmd<ert_packet*>();
    std::fill(ecmd->data, ecmd->data + num_cumasks, 0);

    for (size_t cu_idx = 0; cu_idx < max_cus; ++cu_idx) {
      if (!cumask.test(cu_idx))
        continue;
      auto mask_idx = cu_idx / cus_per_word;
      auto idx_in_mask = cu_idx - mask_idx * cus_per_word;
      ecmd->data[mask_idx] |= (1 << idx_in_mask);
    }
  }

  // Check if this kernel_command object is in done state
  bool
  is_done() const
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_done;
  }

  // Return state of command object.  The underlying packet
  // state is reflected in the command itself.  If function
  // returns completed, then the run object can be reused.
  ert_cmd_state
  get_state() const
  {
    auto state = get_state_raw();
    notify(state);  // update command state accordingly
    return state;
  }

  // Cast underlying exec buffer to its requested type
  template <typename ERT_COMMAND_TYPE>
  const ERT_COMMAND_TYPE
  get_ert_cmd() const
  {
    return reinterpret_cast<const ERT_COMMAND_TYPE>(get_ert_packet());
  }

  // Cast underlying exec buffer to its requested type
  template <typename ERT_COMMAND_TYPE>
  ERT_COMMAND_TYPE
  get_ert_cmd()
  {
    return reinterpret_cast<ERT_COMMAND_TYPE>(get_ert_packet());
  }

  // Add a callback, synchronize with concurrent state change
  // Call the callback if command is complete.
  void
  add_callback(callback_function_type&& fcn)
  {
    bool complete = false;
    ert_cmd_state state = ERT_CMD_STATE_MAX;
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      if (!m_managed && !m_done)
        throw xrt_core::error(ENOTSUP, "Cannot add callback to running unmanaged command");
      if (!m_callbacks)
        m_callbacks = std::make_unique<callback_list>();
      m_callbacks->emplace_back(std::move(fcn));
      auto pkt = get_ert_packet();
      state = static_cast<ert_cmd_state>(pkt->state);
      complete = m_done && state >= ERT_CMD_STATE_COMPLETED;
    }

    // lock must not be helt while calling callback function
    if (complete)
      m_callbacks.get()->back()(state);
  }

  // Remove last added callback
  void
  pop_callback()
  {
    if (m_callbacks && m_callbacks->size())
      m_callbacks->pop_back();
  }

  // Run registered callbacks.
  void
  run_callbacks(ert_cmd_state state) const
  {
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      if (!m_callbacks)
        return;
    }

    // cannot lock mutex while calling the callbacks
    // so copy address of callbacks while holding the lock
    // then execute callbacks without lock
    std::vector<callback_function_type*> copy;
    copy.reserve(m_callbacks->size());

    {
      std::lock_guard<std::mutex> lk(m_mutex);
      std::transform(m_callbacks->begin(),m_callbacks->end()
                     ,std::back_inserter(copy)
                     ,[](callback_function_type& cb) { return &cb; });
    }

    for (auto cb : copy)
      (*cb)(state);
  }

  // Submit the command for execution.
  void
  run()
  {
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      if (!m_done)
        throw std::runtime_error("bad command state, can't launch");
      m_managed = (m_callbacks && !m_callbacks->empty());
      m_done = false;
    }
    if (m_managed)
      m_hwqueue.managed_start(this);
    else
      m_hwqueue.unmanaged_start(this);
  }

  // Wait for command completion
  ert_cmd_state
  wait() const
  {
    if (m_managed) {
      std::unique_lock<std::mutex> lk(m_mutex);
      while (!m_done)
        m_exec_done.wait(lk);
    }
    else {
      m_hwqueue.wait(this);
    }

    return get_state_raw(); // state wont change after wait
  }

  ert_cmd_state
  wait(const std::chrono::milliseconds& timeout_ms) const
  {
    if (m_managed) {
      std::unique_lock<std::mutex> lk(m_mutex);
      while (!m_done)
        if (m_exec_done.wait_for(lk, timeout_ms) == std::cv_status::timeout)
          return ERT_CMD_STATE_TIMEOUT;
    }
    else {
      if (m_hwqueue.wait(this, timeout_ms) == std::cv_status::timeout)
        return ERT_CMD_STATE_TIMEOUT;
    }

    return get_state_raw(); // state wont change after wait
  }

  ////////////////////////////////////////////////////////////////
  // Implement xrt_core::command API
  ////////////////////////////////////////////////////////////////
  ert_packet*
  get_ert_packet() const override
  {
    return reinterpret_cast<ert_packet*>(m_execbuf.second);
  }

  xrt_core::device*
  get_device() const override
  {
    return m_device->get_core_device();
  }

  xclBufferHandle
  get_exec_bo() const override
  {
    return m_execbuf.first;
  }

  void
  notify(ert_cmd_state s) const override
  {
    bool complete = false;
    bool callbacks = false;
    if (s >= ERT_CMD_STATE_COMPLETED) {
      std::lock_guard<std::mutex> lk(m_mutex);

      // Handle potential race if multiple threads end up here. This
      // condition is by design because there are multiple paths into
      // this function and first conditional check should not be locked
      if (m_done)
        return;

      XRT_DEBUGF("kernel_command::notify() m_uid(%d) m_state(%d)\n", m_uid, s);
      complete = m_done = true;
      callbacks = (m_callbacks && !m_callbacks->empty());
    }

    if (complete) {
      m_exec_done.notify_all();
      if (callbacks)
        run_callbacks(s);
    }
  }

private:
  std::shared_ptr<device_type> m_device;
  xrt_core::hw_queue m_hwqueue;  // hwqueue for command submission
  execbuf_type m_execbuf;        // underlying execution buffer
  unsigned int m_uid = 0;
  bool m_managed = false;
  mutable bool m_done = false;

  mutable std::mutex m_mutex;
  mutable std::condition_variable m_exec_done;

  std::unique_ptr<callback_list> m_callbacks;
};

// class argument - get argument value from va_arg
//
// This argument class employs type erasure trick to faciliate type
// specific argument value retrieval using va_arg.  Typed encasulated
// classes supports retrieval of scalar, global, and null arguments
// (essentially ignored arguments). The scalar values can be of any
// type and size even when the va_arg required type is different, for
// example double to retrieve float host type.
//
// The arguments are constructed from xclbin meta data, where the
// scalar type is used to construct argument typed enscapsulated
// scalar class.  Unfortunately the type of an argument is a free
// formed string in the xclbin (need schema to support all types).
class argument
{
public:
  // Base class for argument setters to allow setting
  // of kernel arguments inside same function that retrieves
  // the argument from va_list while controlling the lifetime
  // of the argument within the scope of setting the argument.
  struct setter
  {
    virtual void
    set_arg_value(const argument& arg, const arg_range<uint8_t>& value) = 0;

    virtual void
    set_arg_value(const argument& arg, const xrt::bo& bo) = 0;
  };

private:
  struct iarg
  {
    iarg() = default;

    virtual
    ~iarg() = default;

    iarg(const iarg&) = delete;
    iarg(iarg&&) = delete;
    iarg& operator=(iarg&) = delete;
    iarg& operator=(iarg&&) = delete;

    // somewhat expensive copy conversion of argument
    virtual std::vector<uint32_t>
    get_value(std::va_list*) const = 0;

    // direct setting of retrieved argument
    virtual void
    set(setter*, const argument&, std::va_list*) const = 0;
  };

  template <typename HostType, typename VaArgType>
  struct scalar_type : iarg
  {
    size_t size;  // size in bytes of argument per xclbin

    explicit
    scalar_type(size_t bytes)
      : size(bytes)
    {}

    std::vector<uint32_t>
    get_value(std::va_list* args) const override
    {
      HostType value = va_arg(*args, VaArgType); // NOLINT
      return value_to_uint32_vector(value);
    }

    void
    set(setter* setter, const argument& arg, std::va_list* args) const override
    {
      HostType value = va_arg(*args, VaArgType); // NOLINT
      setter->set_arg_value(arg, arg_range<uint8_t>{&value, sizeof(value)});
    }
  };

  template <typename HostType, typename VaArgType>
  struct scalar_type<HostType*, VaArgType*> : iarg
  {
    size_t size;  // size in bytes of argument per xclbin

    explicit
    scalar_type(size_t bytes)
      : size(bytes)
    {}

    std::vector<uint32_t>
    get_value(std::va_list* args) const override
    {
      HostType* value = va_arg(*args, VaArgType*); // NOLINT
      return value_to_uint32_vector(value, size);
    }

    void
    set(setter* setter, const argument& arg, std::va_list* args) const override
    {
      HostType* value = va_arg(*args, VaArgType*); // NOLINT
      setter->set_arg_value(arg, arg_range<uint8_t>{value, size});
    }
  };

  struct global_type : iarg
  {
    size_t size;   // size in bytes of argument per xclbin

    explicit
    global_type(size_t bytes)
      : size(bytes)
    {}

    std::vector<uint32_t>
    get_value(std::va_list* args) const override
    {
      if (!xrt_core::config::get_xrt_bo())
        throw std::runtime_error("xclBufferHandle not supported as kernel argument");

      auto bo = va_arg(*args, xrtBufferHandle); // NOLINT
      return value_to_uint32_vector(xrt_core::bo::address(bo));
    }

    void
    set(setter* setter, const argument& arg, std::va_list* args) const override
    {
      if (!xrt_core::config::get_xrt_bo())
        throw std::runtime_error("xclBufferHandle not supported as kernel argument");

      auto bo = va_arg(*args, xrtBufferHandle); // NOLINT
      auto addr = xrt_core::bo::address(bo);
      setter->set_arg_value(arg, arg_range<uint8_t>{&addr, sizeof(addr)});
    }
  };

  struct null_type : iarg
  {
    std::vector<uint32_t>
    get_value(std::va_list* args) const override
    {
      (void) va_arg(*args, void*);    // NOLINT swallow unsettable argument
      return std::vector<uint32_t>(); // empty
    }

    void
    set(setter*, const argument&, std::va_list* args) const override
    {
      (void) va_arg(*args, void*); // NOLINT swallow unsettable argument
    }
  };

  // Kernel argument meta data is copied from xrt::xclbin
  // but should consider using it directly from xrt::xclbin
  // as its lifetime exceed that of xrt::kernel (ensured by
  // shared xrt::xclbin ownership in kernel object).
  using xarg = xrt_core::xclbin::kernel_argument;
  xarg arg;    // argument meta data from xclbin

  std::unique_ptr<iarg> content;

public:
  static constexpr size_t no_index = xarg::no_index;
  using direction = xarg::direction;

  argument() = default;
  ~argument() = default;

  argument(argument&& rhs) noexcept
    : arg(std::move(rhs.arg)), content(std::move(rhs.content))
  {}

  explicit
  argument(const xarg& karg)
    : arg(karg)
  {
    // Determine type
    switch (arg.type) {
    case xarg::argtype::scalar : {
      if (arg.hosttype == "int")
        content = std::make_unique<scalar_type<int,int>>(arg.size);
      else if (arg.hosttype == "uint")
        content = std::make_unique<scalar_type<unsigned int,unsigned int>>(arg.size);
      else if (arg.hosttype == "float")
        // use of double here is intentional (per va_arg)
        content = std::make_unique<scalar_type<float,double>>(arg.size);
      else if (arg.hosttype == "double")
        content = std::make_unique<scalar_type<double,double>>(arg.size);
      else if (arg.hosttype == "int*")
        content = std::make_unique<scalar_type<int*,int*>>(arg.size);
      else if (arg.hosttype == "uint*")
        content = std::make_unique<scalar_type<unsigned int*,unsigned int*>>(arg.size);
      else if (arg.hosttype == "float*")
        throw std::runtime_error("float* kernel argument not supported");
      else if (arg.size == sizeof(uint32_t))
        content = std::make_unique<scalar_type<uint32_t,uint32_t>>(arg.size);
      else if (arg.size == sizeof(uint64_t))
        content = std::make_unique<scalar_type<uint64_t,uint64_t>>(arg.size);
      else
        // throw xrt_core::error(-EINVAL, "Unknown scalar argument type '" + arg.hosttype + "'");
        // arg.hosttype is free formed, default to size_t until clarified
        content = std::make_unique<scalar_type<size_t,size_t>>(arg.size);
      break;
    }
    case xarg::argtype::global :
    case xarg::argtype::constant :
      content = std::make_unique<global_type>(arg.size);
      break;
    case xarg::argtype::local :  // local memory
    case xarg::argtype::stream : // stream connection
      content = std::make_unique<null_type>();
      break;
    default:
      throw std::runtime_error("Unexpected error");
    }
  }

  argument(const argument&) = delete;
  argument& operator=(argument&) = delete;
  argument& operator=(argument&&) = delete;

  const xarg&
  get_xarg() const
  {
    return arg;
  }

  void
  valid_or_error() const
  {
    if (arg.index == argument::no_index)
      throw std::runtime_error("Bad argument index '" + std::to_string(arg.index) + "'");
  }

  void
  valid_or_error(size_t bytes) const
  {
    valid_or_error();
    if (bytes != arg.size)
      throw std::runtime_error("Bad argument size '" + std::to_string(bytes) + "'");
  }

  std::vector<uint32_t>
  get_value(std::va_list* args) const
  {
    return content->get_value(args);
  }

  void
  set(setter* setter, std::va_list* args) const
  {
    return content->set(setter, *this, args);
  }

  void
  set_fa_desc_offset(size_t offset)
  { arg.fa_desc_offset = offset; }

  size_t
  fa_desc_offset() const
  { return arg.fa_desc_offset; }

  size_t
  index() const
  { return arg.index; }

  size_t
  offset() const
  { return arg.offset; }

  size_t
  size() const
  { return arg.size; }

  const std::string&
  name() const
  { return arg.name; }

  direction
  dir() const
  { return arg.dir; }

  bool
  is_input() const
  { return arg.dir == direction::input; }

  bool
  is_output() const
  { return arg.dir == direction::output; }

  xarg::argtype
  type() const
  { return arg.type; }
};

} // namespace

namespace xrt {

// struct kernel_impl - The internals of an xrtKernelHandle
//
// An single object of kernel_type can be shared with multiple
// run handles.   The kernel object defines all kernel specific
// meta data used to create a launch a run object (command)
//
// The thread safe device compute unit context manager used by
// ip_context is constructed by kernel_impl if necessary.  It is
// shared ownership with other kernel impls, so while ctxmgr appears
// unused by kernel_impl, the construction and ownership is vital.
class kernel_impl
{
public:
  using property_type = xrt_core::xclbin::kernel_properties;
  using kernel_type = property_type::kernel_type;
  using control_type = xrt::xclbin::ip::control_type;
  using mailbox_type = property_type::mailbox_type;
  using ipctx = std::shared_ptr<ip_context>;
  using ctxmgr_type = xrt_core::context_mgr::device_context_mgr;
  using slot_id = xrt_core::device::slot_id;

private:
  std::string name;                    // kernel name
  std::shared_ptr<device_type> device; // shared ownership
  std::shared_ptr<ctxmgr_type> ctxmgr; // device context mgr ownership
  xrt::hw_context hwctx;               // context for hw resources if any (can be null)
  xrt_core::hw_queue hwqueue;          // hwqueue for command submission (shared by all runs)
  xrt::xclbin xclbin;                  // xclbin with this kernel
  xrt::xclbin::kernel xkernel;         // kernel xclbin metadata
  std::vector<argument> args;          // kernel args sorted by argument index
  std::vector<ipctx> ipctxs;           // CU context locks
  const property_type& properties;     // Kernel properties from XML meta
  std::bitset<max_cus> cumask;         // cumask for command execution
  size_t regmap_size = 0;              // CU register map size
  size_t fa_num_inputs = 0;            // Fast adapter number of inputs per meta data
  size_t fa_num_outputs = 0;           // Fast adapter number of outputs per meta data
  size_t fa_input_entry_bytes = 0;     // Fast adapter input desc bytes
  size_t fa_output_entry_bytes = 0;    // Fast adapter output desc bytes
  size_t num_cumasks = 1;              // Required number of command cu masks
  control_type protocol = control_type::none; // Default opcode
  uint32_t uid;                        // Internal unique id for debug

  // Open context of a specific compute unit.
  //
  // @cu:  compute unit to open
  // @am:  access mode for the CU
  // Return: shared ownership to the context in form of a shared_ptr
  //
  // This function opens the compute unit in the slot associated with
  // the hardware context from which the kernel was constructed.
  void
  open_cu_context(const xrt::xclbin::ip& cu)
  {
    // try open the cu context.  This may throw if cu in slot cannot be acquired.
    auto ctx = ip_context::open(hwctx, cu); // may throw

    // success, record cuidx in kernel cumask
    auto cuidx = ctx->get_cuidx();
    ipctxs.push_back(std::move(ctx));
    cumask.set(cuidx);
    num_cumasks = std::max<size_t>(num_cumasks, (cuidx / cus_per_word) + 1);
  }

  // Compute data for FAST_ADAPTER descriptor use (see ert_fa.h)
  //
  // Compute argument descriptor entry offset and compute total
  // descriptor bytes for inputs and outputs.
  //
  // This function amends the kernel arguments already captured such
  // that later kernel invocation can efficiently construct the fa
  // descriptor from pre computed data.
  //
  void
  amend_fa_args()
  {
    // remove last argument which is "nextDescriptorAddr" and
    // not set by user
    args.pop_back();

    size_t desc_offset = 0;

    // process inputs, compute descriptor entry offset
    for (auto& arg : args) {
      if (!arg.is_input())
        continue;

      ++fa_num_inputs;
      arg.set_fa_desc_offset(desc_offset);
      desc_offset += arg.size() + sizeof(ert_fa_desc_entry);
      fa_input_entry_bytes += arg.size();
    }

    // process outputs, compute descriptor entry offset
    for (auto& arg : args) {
      if (!arg.is_output())
        continue;

      ++fa_num_outputs;
      arg.set_fa_desc_offset(desc_offset);
      desc_offset += arg.size() + sizeof(ert_fa_desc_entry);
      fa_output_entry_bytes += arg.size();
    }

    // adjust regmap size to be size of descriptor and all entries
    regmap_size = (sizeof(ert_fa_descriptor) + desc_offset) / sizeof(uint32_t);
  }

  void
  amend_ap_args()
  {
    // adjust regmap size for kernels without arguments.
    // first 4 register map entries are control registers
    regmap_size = std::max<size_t>(regmap_size, 4);
  }

  void
  amend_args()
  {
    if (protocol == control_type::fa)
      amend_fa_args();
    else if (protocol == control_type::hs || protocol == control_type::chain)
      amend_ap_args();
  }

  unsigned int
  get_cuidx_or_error(size_t offset, bool force=false) const
  {
    if (ipctxs.size() != 1)
      throw std::runtime_error("Cannot read or write kernel with multiple compute units");
    auto& ipctx = ipctxs.back();
    auto mode = ipctx->get_access_mode();
    if (!force
        && mode != ip_context::access_mode::exclusive // shared cu cannot normally be read, except
        && !xrt_core::config::get_rw_shared()         //  - driver allows rw of shared cu
        && !std::get<1>(ipctx->m_readrange))          //  - special case no bounds check here
      throw std::runtime_error("Cannot read or write kernel with shared access");

    if ((offset + sizeof(uint32_t)) > ipctx->get_size())
        throw std::out_of_range("Cannot read or write outside kernel register space");

    return ipctx->get_cuidx();
  }

  control_type
  get_ip_control(const std::vector<xrt::xclbin::ip>& ips)
  {
    if (ips.empty())
      return control_type::none;

    auto ctrl = ips[0].get_control_type();
    for (size_t idx = 1; idx < ips.size(); ++idx) {
      auto ctrlatidx = ips[idx].get_control_type();
      if (ctrlatidx == ctrl)
        continue;
      if (ctrlatidx != control_type::chain && ctrlatidx != control_type::hs)
        throw std::runtime_error("CU control protocol mismatch");
      ctrl = control_type::hs; // mix of CHAIN and HS is recorded as AP_CTRL_HS
    }

    return ctrl;
  }

  void
  initialize_command_header(ert_start_kernel_cmd* kcmd)
  {
    kcmd->extra_cu_masks = num_cumasks - 1;  //  -1 for mandatory mask
    kcmd->count = num_cumasks + regmap_size;
    kcmd->type = ERT_CU;
    kcmd->state = ERT_CMD_STATE_NEW;

    switch (get_kernel_type()) {
    case kernel_type::ps :
      kcmd->opcode = ERT_SK_START;
      break;
    case kernel_type::pl :
      kcmd->opcode = (protocol == control_type::fa) ? ERT_START_FA : ERT_START_CU;
      break;
    case kernel_type::dpu :
      kcmd->opcode = ERT_START_CU;
      break;
    case kernel_type::none:
      throw std::runtime_error("Internal error: wrong kernel type can't set cmd opcode");
    }
  }

  void
  initialize_fadesc(uint32_t* data)
  {
    auto desc = reinterpret_cast<ert_fa_descriptor*>(data);
    desc->status = ERT_FA_ISSUED; // somewhat misleading
    desc->num_input_entries = fa_num_inputs;
    desc->input_entry_bytes = fa_input_entry_bytes;
    desc->num_output_entries = fa_num_outputs;
    desc->output_entry_bytes = fa_output_entry_bytes;
  }

  static uint32_t
  create_uid()
  {
    static std::atomic<uint32_t> count {0};
    return count++;
  }

  static xrt::xclbin::kernel
  get_kernel_or_error(const xrt::xclbin& xclbin, const std::string& nm)
  {
    if (auto krnl = xclbin.get_kernel(nm))
      return krnl;

    throw xrt_core::error("No such kernel '" + nm + "'");
  }

public:
  // kernel_type - constructor
  //
  // @dev:     device associated with this kernel object
  // @uuid:    uuid of xclbin to mine for kernel meta data
  // @nm:      name identifying kernel and/or kernel and instances
  // @am:      access mode for underlying compute units
  //
  // The ctxmgr is not directly used by kernel_impl, but its
  // construction and shared ownership must be tied to the kernel_impl
  kernel_impl(std::shared_ptr<device_type> dev, xrt::hw_context ctx, const std::string& nm)
    : name(nm.substr(0,nm.find(":")))                          // filter instance names
    , device(std::move(dev))                                   // share ownership
    , ctxmgr(xrt_core::context_mgr::create(device->core_device.get())) // owership tied to kernel_impl
    , hwctx(std::move(ctx))                                    // hw context
    , hwqueue(hwctx)                                           // hw queue
    , xclbin(hwctx.get_xclbin())                               // xclbin with kernel
    , xkernel(get_kernel_or_error(xclbin, name))               // kernel meta data managed by xclbin
    , properties(xrt_core::xclbin_int::get_properties(xkernel))// cache kernel properties
    , uid(create_uid())
  {
    XRT_DEBUGF("kernel_impl::kernel_impl(%d)\n" , uid);

    // mailbox kernels opens CU in exclusive mode for direct read/write access
    if (properties.mailbox != mailbox_type::none || properties.counted_auto_restart > 0) {
        XRT_DEBUGF("kernel_impl mailbox or counted auto restart detected, changing access mode to exclusive");
        xrt_core::hw_context_int::set_exclusive(hwctx);
    }

    // Compare the matching CUs against the CU sort order to create cumask
    const auto& kernel_cus = xkernel.get_cus(nm);  // xrt::xclbin::ip objects for matching nm
    if (kernel_cus.empty())
      throw std::runtime_error("No compute units matching '" + nm + "'");

    // Initialize / open compute unit contexts
    for (const auto& cu : kernel_cus) {
      if (cu.get_control_type() == xrt::xclbin::ip::control_type::none)
        throw xrt_core::error(ENOTSUP, "AP_CTRL_NONE is only supported by XRT native API xrt::ip");

      open_cu_context(cu);
    }

    // set kernel protocol
    protocol = get_ip_control(kernel_cus);

    // get kernel arguments from xclbin kernel meta data
    // compute regmap size, convert to typed argument
    for (auto& arg : xrt_core::xclbin_int::get_arginfo(xkernel)) {
      regmap_size = std::max(regmap_size, (arg.offset + arg.size) / sizeof(uint32_t));
      args.emplace_back(arg);
    }

    // amend args with computed data based on kernel protocol
    amend_args();
  }

  ~kernel_impl()
  {
    XRT_DEBUGF("kernel_impl::~kernel_impl(%d)\n" , uid);
  }

  kernel_impl(const kernel_impl&) = delete;
  kernel_impl(kernel_impl&&) = delete;
  kernel_impl& operator=(kernel_impl&) = delete;
  kernel_impl& operator=(kernel_impl&&) = delete;

  bool
  has_mailbox() const
  {
    return properties.mailbox != mailbox_type::none;
  }

  mailbox_type
  get_mailbox_type() const
  {
    return properties.mailbox;
  }

  size_t
  get_auto_restart_counters() const
  {
    return properties.counted_auto_restart;
  }

  kernel_type
  get_kernel_type() const
  {
    return properties.type;
  }

  // Initialize kernel command and return pointer to payload
  // after mandatory static data.
  uint32_t*
  initialize_command(kernel_command* cmd)
  {
    auto kcmd = cmd->get_ert_cmd<ert_start_kernel_cmd*>();
    initialize_command_header(kcmd);
    cmd->encode_compute_units(cumask, num_cumasks);
    auto data = kcmd->data + kcmd->extra_cu_masks;

    if (kcmd->opcode == ERT_START_FA)
      initialize_fadesc(data);

    return data;
  }

  std::string
  get_name() const
  {
    return name;
  }

  xrt::xclbin
  get_xclbin() const
  {
    return xclbin;
  }

  const std::bitset<max_cus>&
  get_cumask() const
  {
    return cumask;
  }

  size_t
  get_num_cumasks() const
  {
    return num_cumasks;
  }

  const std::vector<ipctx>&
  get_ips() const
  {
    return ipctxs;
  }

  control_type
  get_ip_control_protocol() const
  {
    return protocol;
  }

  // Group id is the memory bank index where a global buffer
  // can be allocated for use with this kernel.   If the kernel
  // contains imcompatible compute units, then these are
  // filtered out from a run object when the arguments are set.
  // This filtering implies that the group id returned by this
  // function may not necessarily be compatible with an existing
  // filtered run object, but it is guaranteed to be compatible
  // with a new 'fresh' run object.
  int
  group_id(int argno)
  {
    // This is a tight coupling with driver.  The group index is encoded
    // in a uint32_t that represents BO flags used when constructing the BO.
    // The flags are divided into 16 bits for memory bank index, 8 bits
    // for the xclbin slot, and 8 bits reserved for bo flags.  The latter
    // flags are populated when the xrt::bo object is constructed.

    // Last (for group id) connection of first ip in this kernel
    // The group id can change if cus are trimmed based on argument
    auto& ip = ipctxs.front();  // guaranteed to be non empty
    xcl_bo_flags grp = {0};     // xrt_mem.h
    grp.bank = ip->arg_memidx(argno);
    grp.slot = ip->get_slot();

    // This function should return uint32_t or some same symbolic type
    return static_cast<int>(grp.flags);
  }

  int
  arg_offset(int argno)
  {
    return args.at(argno).offset();
  }

  uint32_t
  read_register(uint32_t offset, bool force=false) const
  {
    auto idx = get_cuidx_or_error(offset, force);
    uint32_t value = 0;
    if (has_reg_read_write())
      device->core_device->reg_read(idx, offset, &value);
    else
      device->core_device->xread(XCL_ADDR_KERNEL_CTRL, ipctxs.back()->get_address() + offset, &value, 4);
    return value;
  }

  void
  write_register(uint32_t offset, uint32_t data)
  {
    auto idx = get_cuidx_or_error(offset);
    if (has_reg_read_write())
      device->core_device->reg_write(idx, offset, data);
    else
      device->core_device->xwrite(XCL_ADDR_KERNEL_CTRL, ipctxs.back()->get_address() + offset, &data, 4);
  }

  // Read 'count' 4 byte registers starting at offset
  // This API is internal and allows reading from shared IPs
  void
  read_register_n(uint32_t offset, size_t count, uint32_t* out)
  {
    for (size_t n = 0; n < count; ++n)
      out[n] = read_register(offset + n * 4, true);
  }

  // Write 'count' 4 byte registers starting at offset
  void
  write_register_n(uint32_t offset, size_t count, uint32_t* data)
  {
    for (size_t n = 0; n < count; ++n)
      write_register(offset + n * 4, *(data + n));
  }

  const std::shared_ptr<device_type>&
  get_device() const
  {
    return device;
  }

  xrt_core::device*
  get_core_device() const
  {
    return device->get_core_device();
  }

  xrt::hw_context
  get_hw_context() const
  {
    return hwctx;
  }

  xrt_core::hw_queue
  get_hw_queue() const
  {
    return hwqueue;
  }

  const std::vector<argument>&
  get_args() const
  {
    return args;
  }

  const argument&
  get_arg(size_t argidx, bool nocheck=false) const
  {
    auto& arg = args.at(argidx);
    if (!nocheck)
      arg.valid_or_error();
    return arg;
  }

  size_t
  get_regmap_size()
  {
      return regmap_size;
  }
};

// struct run_impl - The internals of an xrtRunHandle
//
// An run handle shares ownership of a kernel object.  The run object
// corresponds to an execution context for the given kernel object.
// Multiple run objects against the same kernel object can be created
// and submitted for execution concurrently.  Each run object manages
// its own execution buffer (ert command object)
class run_impl
{
  friend class mailbox_impl;
  using ipctx = std::shared_ptr<ip_context>;
  using control_type = kernel_impl::control_type;
  using kernel_type = kernel_impl::kernel_type;

  // Helper hierarchy to set argument value per control protocol type
  // The @data member is the payload to be populated with argument
  // value.  The interpretation of the payload depends on the control
  // protocol.
  struct arg_setter : argument::setter
  {
    uint8_t* data;

    explicit
    arg_setter(uint32_t* d)
      : data(reinterpret_cast<uint8_t*>(d))
    {}

    virtual
    ~arg_setter() = default;

    arg_setter(const arg_setter&) = delete;
    arg_setter(arg_setter&&) = delete;
    arg_setter& operator=(arg_setter&) = delete;
    arg_setter& operator=(arg_setter&&) = delete;

    void
    set_arg_value(const argument& arg, const arg_range<uint8_t>& value) override = 0;

    void
    set_arg_value(const argument& arg, const xrt::bo& bo) override
    {
      auto value = bo.address();
      set_arg_value(arg, arg_range<uint8_t>{&value, sizeof(value)});
    }

    virtual void
    set_offset_value(size_t offset, const arg_range<uint8_t>& value) = 0;

    virtual arg_range<uint8_t>
    get_arg_value(const argument& arg) = 0;
  };

  // AP_CTRL_HS, AP_CTRL_CHAIN
  struct hs_arg_setter : arg_setter
  {
    explicit
    hs_arg_setter(uint32_t* data)
      : arg_setter(data)
    {}

    void
    set_offset_value(size_t offset, const arg_range<uint8_t>& value) override
    {
      // max 4 bytes supported for direct register write
      auto count = std::min<size_t>(4, value.size());
      std::copy_n(value.begin(), count, data + offset);
    }

    void
    set_arg_value(const argument& arg, const arg_range<uint8_t>& value) override
    {
      auto count = std::min(arg.size(), value.size());
      std::copy_n(value.begin(), count, data + arg.offset());
    }

    arg_range<uint8_t>
    get_arg_value(const argument& arg) override
    {
      return { data + arg.offset(), arg.size() };
    }
  };

  // FAST_ADAPTER
  struct fa_arg_setter : arg_setter
  {
    explicit
    fa_arg_setter(uint32_t* data)
      : arg_setter(data)
    {}

    void
    set_offset_value(size_t offset, const arg_range<uint8_t>& value) override
    {
      throw xrt_core::error(std::errc::not_supported,"fast adapter set_offset_value");
    }

    void
    set_arg_value(const argument& arg, const arg_range<uint8_t>& value) override
    {
      auto desc = reinterpret_cast<ert_fa_descriptor*>(data);
      auto desc_entry = reinterpret_cast<ert_fa_desc_entry*>(desc->data + arg.fa_desc_offset() / sizeof(uint32_t));
      desc_entry->arg_offset = arg.offset();
      desc_entry->arg_size = arg.size();
      auto count = std::min(arg.size(), value.size());
      std::copy_n(value.begin(), count, reinterpret_cast<uint8_t*>(desc_entry->arg_value));
    }

    arg_range<uint8_t>
    get_arg_value(const argument& arg) override
    {
      auto desc = reinterpret_cast<ert_fa_descriptor*>(data);
      auto desc_entry = reinterpret_cast<ert_fa_desc_entry*>(desc->data + arg.fa_desc_offset() / sizeof(uint32_t));
      return { reinterpret_cast<uint8_t*>(desc_entry->arg_value), arg.size() };
    }
  };

  // PS_KERNEL
  struct ps_arg_setter : hs_arg_setter
  {
    explicit
    ps_arg_setter(uint32_t* data)
      : hs_arg_setter(data)
    {}

    void
    set_arg_value(const argument& arg, const xrt::bo& bo) override
    {
      uint64_t value[2] = {bo.address(), bo.size()};
      hs_arg_setter::set_arg_value(arg, arg_range<uint8_t>{value, sizeof(value)});
    }
  };

  static uint32_t
  create_uid()
  {
    static std::atomic<uint32_t> count {0};
    return count++;
  }

  virtual std::unique_ptr<arg_setter>
  make_arg_setter()
  {
    switch (kernel->get_kernel_type()) {
    case kernel_type::pl :
      if (kernel->get_ip_control_protocol() == control_type::fa)
        return std::make_unique<fa_arg_setter>(data);
      return std::make_unique<hs_arg_setter>(data);
    case kernel_type::ps :
      return std::make_unique<ps_arg_setter>(data);
    case kernel_type::dpu :
      return std::make_unique<hs_arg_setter>(data);
    case kernel_type::none :
      throw std::runtime_error("Internal error: unknown kernel type");
    }

    throw std::runtime_error("Internal error: xrt::kernel::make_arg_setter() not reachable");
  }

  arg_setter*
  get_arg_setter()
  {
    if (!asetter)
      asetter = make_arg_setter();

    return asetter.get();
  }

  bool
  validate_ip_arg_connectivity(size_t argidx, int32_t grpidx)
  {
    // remove ips that don't meet requested connectivity
    auto itr = std::remove_if(ips.begin(), ips.end(),
                   [argidx, grpidx] (const auto& ip) {
                     return !ip->valid_connection(argidx, grpidx);
                   });

    // if no ips are left then error
    if (itr == ips.begin())
      return false;

    // no ips were removed
    if (itr == ips.end())
      return true;

    // update the cumask to set remaining cus, note that removed
    // cus, while not erased, are no longer valid per move sematics
    cumask.reset();
    std::for_each(ips.begin(), itr, [this](const auto& ip) { cumask.set(ip->get_cuidx()); });

    // erase the removed ips and mark that CUs must be
    // encoded in command packet.
    ips.erase(itr,ips.end());
    encode_cumasks = true;
    return true;
  }

  xrt::bo
  validate_bo_at_index(size_t index, const xrt::bo& bo)
  {
    xcl_bo_flags grp {xrt_core::bo::group_id(bo)};
    if (validate_ip_arg_connectivity(index, grp.bank))
      return bo;

    auto fmt = boost::format
      ("Kernel %s has no compute units with connectivity required for global argument at index %d. "
       "The argument is allocated in bank %d, the compute unit is connected to bank %d. "
       "Allocating local copy of argument buffer in connected bank.")
      % kernel->get_name() % index % xrt_core::bo::group_id(bo) % kernel->group_id(index);
    xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", fmt.str());

    // try m2m with local buffer
    return xrt_core::bo::clone(bo, kernel->group_id(index));
  }

  // Clone the commmand packet of another run_impl
  // Used when constructing a run_impl from another run_impl
  // for concurrent execution
  uint32_t*
  clone_command_data(const run_impl* rhs)
  {
    auto pkt = cmd->get_ert_packet();
    auto rhs_pkt = rhs->cmd->get_ert_packet();
    pkt->header = rhs_pkt->header;
    pkt->state = ERT_CMD_STATE_NEW;
    std::copy_n(rhs_pkt->data, rhs_pkt->count, pkt->data);
    return pkt->data + (rhs->data - rhs_pkt->data);
  }

  using callback_function_type = std::function<void(ert_cmd_state)>;
  std::shared_ptr<kernel_impl> kernel;    // shared ownership
  std::vector<ipctx> ips;                 // ips controlled by this run object
  std::bitset<max_cus> cumask;            // cumask for command execution
  xrt_core::device* core_device;          // convenience, in scope of kernel
  std::shared_ptr<kernel_command> cmd;    // underlying command object
  uint32_t* data;                         // command argument data payload @0x0
  uint32_t uid;                           // internal unique id for debug
  std::unique_ptr<arg_setter> asetter;    // helper to populate payload data
  bool encode_cumasks = false;            // indicate if cmd cumasks must be re-encoded

public:
  uint32_t
  get_uid() const
  {
    return uid;
  }

  void
  add_callback(callback_function_type&& fcn)
  {
    cmd->add_callback(std::move(fcn));
  }

  void
  pop_callback()
  {
    cmd->pop_callback();
  }

  // run_type() - constructor
  //
  // @krnl:  kernel object to run
  //
  // Contructs and initializes a command packet.  The command packet
  // is further populated during setting of arguments.   By default
  // the command packet is initialized based in kernel meta data and
  // it encodes compute units based on the compute units associated
  // with the kernel object.  These compute units can be filtered
  // as a result of setting kernel arguments (global buffers) in
  // which case they must be re-encoded as indicated by encode_cumask
  // data member before starting the command.
  explicit
  run_impl(std::shared_ptr<kernel_impl> k)
    : kernel(std::move(k))                        // share ownership
    , ips(kernel->get_ips())
    , cumask(kernel->get_cumask())
    , core_device(kernel->get_core_device())      // cache core device
    , cmd(std::make_shared<kernel_command>(kernel->get_device(), kernel->get_hw_queue()))
    , data(kernel->initialize_command(cmd.get())) // default encodes CUs
    , uid(create_uid())
  {
    XRT_DEBUGF("run_impl::run_impl(%d)\n" , uid);
  }

  // Clones a run impl, so that the clone can be executed concurrently
  // with the clonee.
  explicit
  run_impl(const run_impl* rhs)
    : kernel(rhs->kernel)
    , ips(rhs->ips)
    , cumask(rhs->cumask)
    , core_device(rhs->core_device)
    , cmd(std::make_shared<kernel_command>(kernel->get_device(), kernel->get_hw_queue()))
    , data(clone_command_data(rhs))
    , uid(create_uid())
    , encode_cumasks(rhs->encode_cumasks)
  {
    XRT_DEBUGF("run_impl::run_impl(%d)\n" , uid);
  }

  virtual
  ~run_impl()
  {
    XRT_DEBUGF("run_impl::~run_impl(%d)\n" , uid);
  }

  run_impl(const run_impl&) = delete;
  run_impl(run_impl&&) = delete;
  run_impl& operator=(run_impl&) = delete;
  run_impl& operator=(run_impl&&) = delete;

  kernel_impl*
  get_kernel() const
  {
    return kernel.get();
  }

  template <typename ERT_COMMAND_TYPE>
  ERT_COMMAND_TYPE
  get_ert_cmd()
  {
    return cmd->get_ert_cmd<ERT_COMMAND_TYPE>();
  }

  // Use to explicitly restrict what CUs can be used
  // Specified CUs are ignored if they are not currently
  // managed by this run object
  void
  set_cus(const std::bitset<max_cus>& mask)
  {
    auto itr = std::remove_if(ips.begin(), ips.end(),
                              [&mask] (const auto& ip) {
                                return !mask.test(ip->get_cuidx());
                              });

    if (itr == ips.begin())
      throw std::runtime_error("Specified No compute units left");

    // update the cumask to set remaining cus, note that removed
    // cus, while not erased, are no longer valid per move sematics
    cumask.reset();
    std::for_each(ips.begin(), itr, [this](const auto& ip) { cumask.set(ip->get_cuidx()); });

    // erase the removed ips and mark that CUs must be
    // encoded in command packet.
    ips.erase(itr,ips.end());
    encode_cumasks = true;
  }

  const std::bitset<max_cus>&
  get_cumask() const
  {
    return cumask;
  }

  arg_range<uint8_t>
  get_arg_value(const argument& arg)
  {
    return get_arg_setter()->get_arg_value(arg);
  }

  void
  set_arg_value(const argument& arg, const arg_range<uint8_t>& value)
  {
    get_arg_setter()->set_arg_value(arg, value);
  }

  void
  set_arg_value(const argument& arg, const xrt::bo& bo)
  {
    get_arg_setter()->set_arg_value(arg, bo);
  }

  void
  set_arg_value(const argument& arg, const void* value, size_t bytes)
  {
    set_arg_value(arg, arg_range<uint8_t>{value, bytes});
  }

  void
  set_offset_value(uint32_t offset, const arg_range<uint8_t>& value)
  {
    get_arg_setter()->set_offset_value(offset, value);
  }

  void
  set_offset_value(uint32_t offset, const void* value, size_t bytes)
  {
    set_offset_value(offset, arg_range<uint8_t>{value, bytes});
  }

  void
  set_arg(const argument& arg, std::va_list* args)
  {
    arg.set(get_arg_setter(), args);
  }

  void
  set_arg_at_index(size_t index, const xrt::bo& argbo)
  {
    auto bo = validate_bo_at_index(index, argbo);
    auto& arg = kernel->get_arg(index);
    set_arg_value(arg, bo);
  }

  void
  set_arg_at_index(size_t index, std::va_list* args)
  {
    auto& arg = kernel->get_arg(index);
    set_arg(arg, args);
  }

  void
  set_arg_at_index(size_t index, const void* value, size_t bytes)
  {
    auto& arg = kernel->get_arg(index);
    set_arg_value(arg, value, bytes);
  }

  void
  get_arg_at_index(size_t index, uint32_t* out, size_t bytes)
  {
    auto& arg = kernel->get_arg(index);
    arg.valid_or_error(bytes);
    kernel->read_register_n(arg.offset(), bytes / sizeof(uint32_t), out);
  }

  void
  set_all_args(std::va_list* args)
  {
    for (auto& arg : kernel->get_args()) {
      if (arg.index() == argument::no_index)
        break;
      XRT_DEBUGF("arg name(%s) index(%d) offset(0x%x) size(%d)", arg.name().c_str(), arg.index(), arg.offset(), arg.size());
      set_arg(arg, args);
    }
  }

  int
  get_arg_index(const std::string& argnm) const
  {
    for (const auto& arg : kernel->get_args())
      if (arg.name() == argnm)
        return arg.index();

    throw xrt_core::error(EINVAL, "No such kernel argument '" + argnm + "'");
  }

  // If this run object's cus were filtered compared to kernel cus
  // then update the command packet encoded cus.
  void
  encode_compute_units()
  {
    if (!encode_cumasks)
      return;

    cmd->encode_compute_units(cumask, kernel->get_num_cumasks());
    encode_cumasks = false;
  }

  // start() - start the run object (execbuf)
  virtual void
  start()
  {
    encode_compute_units();

    auto pkt = cmd->get_ert_packet();
    pkt->state = ERT_CMD_STATE_NEW;

    XRT_DEBUG_CALL(debug_cmd_packet(kernel->get_name(), pkt));

    cmd->run();
  }

  void
  start(const autostart& iterations)
  {
    if (cumask.count() > 1)
      throw xrt_core::error(std::errc::value_too_large, "Only one compute unit allowed with auto restart");

    if (!kernel->get_auto_restart_counters())
      throw xrt_core::error(ENOSYS, "No auto-restart counters found for kernel");

    uint32_t value = iterations.iterations;
    if (!value)
      value = std::numeric_limits<uint32_t>::max();
    set_offset_value(mailbox_auto_restart_cntr, &value, sizeof(value));
    start();
  }

  void
  stop()
  {
    if (cumask.count() > 1)
      throw xrt_core::error(std::errc::value_too_large, "Only one compute unit allowed with auto restart");

    if (!kernel->get_auto_restart_counters())
      throw xrt_core::error(ENOSYS, "Support for auto restart counters have not been implemented");

    // Clear AUTO_RESTART bit if set, then wait() for completion
    // TODO: find offset once in meta data
    uint32_t value = 0;
    set_offset_value(mailbox_auto_restart_cntr, &value, sizeof(value));
    wait(std::chrono::milliseconds{0});
  }

  ert_cmd_state
  abort() const
  {
    // don't bother if command is done by the time abort is called
    if (cmd->is_done()) {
      if (cmd->get_state() == ERT_CMD_STATE_NEW)
        throw xrt_core::error("Cannot abort command that wasn't started");
      return cmd->get_state();
    }

    // create and populate abort command
    auto abort_cmd = std::make_shared<kernel_command>(kernel->get_device(), kernel->get_hw_queue());
    auto abort_pkt = abort_cmd->get_ert_cmd<ert_abort_cmd*>();
    abort_pkt->state = ERT_CMD_STATE_NEW;
    abort_pkt->count = sizeof(abort_pkt->exec_bo_handle) / sizeof(uint32_t);
    abort_pkt->opcode = ERT_ABORT;
    abort_pkt->type = ERT_CTRL;
    abort_pkt->exec_bo_handle = to_uint64_t(cmd->get_exec_bo());

    // schedule abort command and wait for it to complete
    abort_cmd->run();
    abort_cmd->wait();

    // wait for current run command to be aborted, return cmd status
    return cmd->wait();
  }

  // wait() - wait for execution to complete
  ert_cmd_state
  wait(const std::chrono::milliseconds& timeout_ms) const
  {
    return timeout_ms.count() ? cmd->wait(timeout_ms) : cmd->wait();
  }

  // state() - get current execution state
  ert_cmd_state
  state() const
  {
    return cmd->get_state();
  }

  ert_packet*
  get_ert_packet() const
  {
    return cmd->get_ert_packet();
  }
};

// class mailbox_impl - Extension of run_impl for mailbox support
//
// Implements an argument setter override that writes kernel arguments
// to mailbox using register_write.
//
// Overrides start() function to sync mailbox to HW compute unit
// register map.
class mailbox_impl : public run_impl
{
  using mailbox_type = xrt_core::xclbin::kernel_properties::mailbox_type;

  // enum class for mailbox operations
  enum class mailbox_operation {
    write,
    read
  };


  // struct hs_arg_setter - AP_CTRL_* argument setter for mailbox
  //
  // This argument setter amends base argument setter by writing and
  // reading arguments to/from mailbox.  After setting or before
  // reading arguments, the mailbox must have been synced with HW
  struct hs_arg_setter : run_impl::hs_arg_setter
  {
    uint32_t* data32;    // note that 'data' in base is uint8_t*
    mailbox_impl* mbox;
    static constexpr size_t wsize = sizeof(uint32_t);  // register word size

    hs_arg_setter(uint32_t* data, mailbox_impl* mimpl)
      : run_impl::hs_arg_setter(data), data32(data), mbox(mimpl)
    {}

    void
    set_offset_value(size_t offset, const arg_range<uint8_t>& value) override
    {
      // single 4 byte register write
      run_impl::hs_arg_setter::set_offset_value(offset, value);

      // write single 4 byte value to mailbox
      mbox->mailbox_wait(mailbox_operation::write);
      mbox->kernel->write_register(offset, *(data32 + offset / wsize));
    }

    void
    set_arg_value(const argument& arg, const arg_range<uint8_t>& value) override
    {
      run_impl::hs_arg_setter::set_arg_value(arg, value);

      // write argument value to mailbox
      // arg size is always a multiple of 4 bytes
      mbox->mailbox_wait(mailbox_operation::write);
      mbox->kernel->write_register_n(arg.offset(), arg.size() / wsize, data32 + arg.offset() / wsize);
    }

    arg_range<uint8_t>
    get_arg_value(const argument& arg) override
    {
      // read arg size bytes from mailbox at arg offset
      // arg size is alwaus a multiple of 4 bytes
      mbox->mailbox_wait(mailbox_operation::read);
      mbox->kernel->read_register_n(arg.offset(), arg.size() / wsize, data32 + arg.offset() / wsize);
      return run_impl::hs_arg_setter::get_arg_value(arg);
    }
  };

  void
  poll(const mailbox_operation& mbop)
  {
    if (mbop == mailbox_operation::write) {
        uint32_t ctrlreg_write = kernel->read_register(mailbox_input_ctrl_reg);
        m_busy_write = ctrlreg_write & mailbox_input_ack; //Low - free, High - Busy
    }

    if(mbop == mailbox_operation::read) {
        uint32_t ctrlreg_read = kernel->read_register(mailbox_output_ctrl_reg);
        m_busy_read = ctrlreg_read & mailbox_output_ack;//Low - free, High - Busy
    }
  }

  void
  mailbox_idle_or_error(const mailbox_operation& mbop)
  {
    poll(mbop);
    if (mbop == mailbox_operation::write) {
       if (m_busy_write)
           throw xrt_core::system_error(EBUSY, "Mailbox is busy, Unable to do mailbox write");
    }

    if(mbop == mailbox_operation::read) {
       if (m_busy_read)
           throw xrt_core::system_error(EBUSY, "Mailbox is busy, Unable to do mailbox read");
    }
  }

  void
  mailbox_wait(const mailbox_operation& mbop)
  {
    poll(mbop);//Get initial status of read/write ack bit before polling in while loop.
    if (mbop == mailbox_operation::write) {
	    while (m_busy_write) //poll write done bit
            poll(mailbox_operation::write);
        if (m_aquire_write) //Return if already aquired.
            return;
       uint32_t ctrlreg_write = kernel->read_register(mailbox_input_ctrl_reg);
       kernel->write_register(mailbox_input_ctrl_reg, ctrlreg_write & ~mailbox_input_write);//0, Mailbox Aquire/Lock sync HOST -> SW
       m_aquire_write = true;
    }

    if (mbop == mailbox_operation::read) {
        while (m_busy_read) //poll read done bit
            poll(mailbox_operation::read);
        if (m_aquire_read) //Return if already aquired.
            return;
        uint32_t ctrlreg_read = kernel->read_register(mailbox_output_ctrl_reg);
        kernel->write_register(mailbox_output_ctrl_reg, ctrlreg_read & ~mailbox_output_read);//0, Mailbox Aquire/Lock sync HOST -> SW
        m_aquire_read = true;
    }
  }

  // All mailboxes should be writeable otherwise nothing, not even
  // starting the kernel will work.
  void
  mailbox_writeable_or_error()
  {
    if (m_readonly)
      throw xrt_core::system_error(EPERM, "Mailbox is read-only");
  }

  // It is possible for the mailbox to not supporting reading
  // kernel HW outputs, so this exception can trigger if the
  // mailbox is used incorrectly.
  void
  mailbox_readable_or_error()
  {
    if (m_writeonly)
      throw xrt_core::system_error(EPERM, "Mailbox is write-only");
  }

  bool m_busy_read = false; // Read register acknowledgement -> Low - free , High - busy
  bool m_busy_write = false; // Write register acknowledgement -> Low - free , High - busy
  bool m_aquire_write = false;
  bool m_aquire_read = false;
  bool m_readonly = false;    //
  bool m_writeonly = false;   //

public:
  explicit
  mailbox_impl(const std::shared_ptr<kernel_impl>& k)
    : run_impl(k)
  {
    if (cumask.count() > 1)
      throw xrt_core::error(std::errc::value_too_large, "Only one compute unit allowed with mailbox");
    auto mtype = k->get_mailbox_type();
    m_readonly = (mtype == mailbox_type::out);
    m_writeonly = (mtype == mailbox_type::in);
  }

  //Aquring mailbox read and write if not acquired already.
  ~mailbox_impl() {
      if (!m_aquire_write) {
          uint32_t ctrlreg_write = kernel->read_register(mailbox_input_ctrl_reg);
          kernel->write_register(mailbox_input_ctrl_reg, ctrlreg_write & ~mailbox_input_write);//0, Mailbox Aquire/Lock sync HOST -> SW
      }
      if (!m_aquire_read) {
          uint32_t ctrlreg_read = kernel->read_register(mailbox_output_ctrl_reg);
          kernel->write_register(mailbox_output_ctrl_reg, ctrlreg_read & ~mailbox_output_read);//0, Mailbox Aquire/Lock sync HOST -> SW
      }
  }
  // write mailbox to hw
  void
  write()
  {
    mailbox_writeable_or_error();
    mailbox_idle_or_error(mailbox_operation::write);
    // release the write mailbox, so that cu can read from mailbox
    uint32_t ctrlreg_write = kernel->read_register(mailbox_input_ctrl_reg);
    kernel->write_register(mailbox_input_ctrl_reg, ctrlreg_write | mailbox_input_write);//1, Mailbox Release/Unlock sync SW -> HW
    m_aquire_write = false;
  }

  // read hw to mailbox
  void
  read()
  {
    mailbox_readable_or_error();
    mailbox_idle_or_error(mailbox_operation::read);
    // release the read mailbox, so that cu can write to mailbox
    uint32_t ctrlreg_read = kernel->read_register(mailbox_output_ctrl_reg);
    kernel->write_register(mailbox_output_ctrl_reg, ctrlreg_read | mailbox_output_read);//1, Mailbox Release/Unlock sync SW -> HW
    m_aquire_read = false;
  }

  // blocking read directly from mailbox
  // assumes prior read()
  std::pair<const void*, size_t>
  get_arg(int index)
  {
    mailbox_wait(mailbox_operation::read);
    auto& arg = kernel->get_arg(index);
    auto val = get_arg_value(arg);
    return {val.data(), val.bytes()};
  }

  ////////////////////////////////////////////////////////////////
  // xrt::run_impl overrides
  ////////////////////////////////////////////////////////////////
  std::unique_ptr<arg_setter>
  make_arg_setter() override
  {
    auto ktype = kernel->get_kernel_type();
    if (ktype == kernel_type::pl) {
      if (kernel->get_ip_control_protocol() == control_type::fa)
        throw xrt_core::error("Mailbox not supported with FAST_ADAPTER");

      return std::make_unique<hs_arg_setter>(data, this); // data is run_impl::data
    }

    throw xrt_core::error("Mailbox not supported for non pl kernel types");
  }

  void
  start() override
  {
    // sync command payload to mailbox if necessary
    write();

    // adjust payload count to avoid scheduler writing to mailbox
    constexpr size_t ap_ctrl_reserved = 4;
    auto pkt = cmd->get_ert_packet();
    pkt->count = kernel->get_num_cumasks() + ap_ctrl_reserved;

    // Regular start
    run_impl::start();
  }
};

// struct run_update_type - RTP update
//
// Asynchronous runtime update of kernel arguments.  Each argument is
// updated in one execution, e.g.  batching up of multiple arguments
// changes before physically updating the kernel command is not
// supported.
//
// Once created, the run_update object is alive until the corresponding
// run handle is closed.
class run_update_type
{
  run_impl* run;                       // active run object to update
  kernel_impl* kernel;                 // kernel associated with run object
  std::shared_ptr<kernel_command> cmd; // command to use for updating

  // ert_init_kernel_cmd data offset per ert.h
  static constexpr size_t data_offset = 9;

  void
  reset_cmd()
  {
    auto kcmd = cmd->get_ert_cmd<ert_init_kernel_cmd*>();
    kcmd->count = data_offset + kcmd->extra_cu_masks;  // reset payload size
  }

public:
  explicit
  run_update_type(run_impl* r)
    : run(r)
    , kernel(run->get_kernel())
    , cmd(std::make_shared<kernel_command>(kernel->get_device(), kernel->get_hw_queue()))
  {
    auto kcmd = cmd->get_ert_cmd<ert_init_kernel_cmd*>();
    auto rcmd = run->get_ert_cmd<ert_start_kernel_cmd*>();
    kcmd->opcode = ERT_INIT_CU;
    kcmd->type = ERT_CU;
    kcmd->update_rtp = 1;
    kcmd->extra_cu_masks = rcmd->extra_cu_masks;
    kcmd->cu_mask = rcmd->cu_mask;
    std::copy(rcmd->data, rcmd->data + rcmd->extra_cu_masks, kcmd->data);
    reset_cmd();
  }

  void
  update_arg_value(const argument& arg, const arg_range<uint8_t>& value)
  {
    reset_cmd();

    auto kcmd = cmd->get_ert_cmd<ert_init_kernel_cmd*>();
    auto idx = kcmd->count - data_offset;
    auto offset = arg.offset();
    for (auto v : value) {
      kcmd->data[idx++] = offset;
      kcmd->data[idx++] = v;
      offset += 4;
    }
    kcmd->count += value.size() * 2;

    // make the updated arg sticky in current run
    run->set_arg_value(arg, value);

    auto pkt = cmd->get_ert_packet();
    pkt->state = ERT_CMD_STATE_NEW;

    // There is a problem here if the run object from which
    // this update was constructed has been CU filtered.  If
    // that is the case then the update cmd cumask should be
    // re-encoded.  This condition is not currently checked.
    cmd->run();
    cmd->wait();
  }

  void
  update_arg_value(const argument& arg, const void* value, size_t bytes)
  {
    update_arg_value(arg, arg_range<uint8_t>{value, std::min(arg.size(), bytes)});
  }

  void
  update_arg_at_index(size_t index, std::va_list* args)
  {
    auto& arg = kernel->get_arg(index);
    auto value = arg.get_value(args);  // vector<uint32_t>
    auto bytes = value.size() * sizeof(uint32_t);
    update_arg_value(arg, value.data(), bytes);
  }

  void
  update_arg_at_index(size_t index, const void* value, size_t bytes)
  {
    auto& arg = kernel->get_arg(index);
    update_arg_value(arg, value, bytes);
  }

  void
  update_arg_at_index(size_t index, const xrt::bo& glb)
  {
    auto& arg = kernel->get_arg(index);
    auto value = xrt_core::bo::address(glb);
    update_arg_value(arg, &value, sizeof(value));
  }
};

} // namespace xrt

namespace {

// Device wrapper.  Lifetime is tied to kernel object.  Using
// std::weak_ptr to treat as cache rather sharing ownership.
// Ownership of device is shared by kernel objects, when last kernel
// object is destructed, the correponding device object is deleted and
// cache will miss lookup for subsequent kernel creation.  Without
// weak_ptr, the cache would hold on to the device until static global
// destruction and long after application calls xclClose on the
// xrtDeviceHandle.
static std::map<xrtDeviceHandle, std::weak_ptr<device_type>> devices;
static std::mutex devices_mutex;

// get_device() - get a device object from an xrtDeviceHandle
//
// The lifetime of the device object is shared ownership. The object
// is cached so that subsequent look-ups from same xrtDeviceHandle
// result in same device object if it exists already.
//
// Refactor to share, or better get rid of device_type and fold
// extension into xrt_core::device
static std::shared_ptr<device_type>
get_device(xrtDeviceHandle dhdl)
{
  std::lock_guard<std::mutex> lk(devices_mutex);
  auto itr = devices.find(dhdl);
  std::shared_ptr<device_type> device = (itr != devices.end())
    ? (*itr).second.lock()
    : nullptr;
  if (!device) {
    // NOLINTNEXTLINE(modernize-make-shared)  used in weak_ptr
    device = std::shared_ptr<device_type>(new device_type(dhdl));
    devices.emplace(std::make_pair(dhdl, device));
  }
  return device;
}

static std::shared_ptr<device_type>
get_device(const std::shared_ptr<xrt_core::device>& core_device)
{
  auto dhdl = core_device.get();
  std::lock_guard<std::mutex> lk(devices_mutex);
  auto itr = devices.find(dhdl);
  std::shared_ptr<device_type> device = (itr != devices.end())
    ? (*itr).second.lock()
    : nullptr;
  if (!device) {
    // NOLINTNEXTLINE(modernize-make-shared)  used in weak_ptr
    device = std::shared_ptr<device_type>(new device_type(core_device));
    devices.emplace(std::make_pair(dhdl, device));
  }
  return device;
}

static std::shared_ptr<device_type>
get_device(const xrt::device& xdev)
{
  return get_device(xdev.get_handle());
}

// Active kernels per xrtKernelOpen/Close.  This is a mapping from
// xrtKernelHandle to the corresponding kernel object.  The
// xrtKernelHandle is the address of the kernel object.  This is
// shared ownership as application can close a kernel handle before
// closing an xrtRunHandle that references same kernel.
static xrt_core::handle_map<xrtKernelHandle, std::shared_ptr<xrt::kernel_impl>> kernels;

// Active runs.  This is a mapping from xrtRunHandle to corresponding
// run object.  The xrtRunHandle is the address of the run object.
// This is unique ownership as only the host application holds on to a
// run object, e.g. the run object is desctructed immediately when it
// is closed.
static xrt_core::handle_map<xrtRunHandle, std::unique_ptr<xrt::run_impl>> runs;

// Run updates, if used are tied to existing runs and removed
// when run is closed.
static xrt_core::handle_map<const xrt::run_impl*, std::unique_ptr<xrt::run_update_type>> run_updates;

static xrt::run_update_type*
get_run_update(xrt::run_impl* run)
{
  auto update = run_updates.get(run); // raw ptr
  if (!update) {
    auto val = std::make_unique<xrt::run_update_type>(run);
    update = val.get();
    run_updates.add(run, std::move(val));
  }
  return update;
}

static xrt::run_update_type*
get_run_update(xrtRunHandle rhdl)
{
  auto run = runs.get_or_error(rhdl); // raw ptr
  return get_run_update(run);
}

static std::unique_ptr<xrt::run_impl>
alloc_run(const std::shared_ptr<xrt::kernel_impl>& khdl)
{
  return khdl->has_mailbox()
    ? std::make_unique<xrt::mailbox_impl>(khdl)
    : std::make_unique<xrt::run_impl>(khdl);
}

static std::shared_ptr<xrt::kernel_impl>
alloc_kernel(const std::shared_ptr<device_type>& dev,
	     const xrt::uuid& xclbin_id,
	     const std::string& name,
	     xrt::kernel::cu_access_mode mode)
{
  auto amode = hwctx_access_mode(mode);  // legacy access mode to hwctx qos
  return std::make_shared<xrt::kernel_impl>(dev, xrt::hw_context{dev->get_xrt_device(), xclbin_id, amode}, name);
}

static std::shared_ptr<xrt::kernel_impl>
alloc_kernel_from_ctx(const std::shared_ptr<device_type>& dev,
                      const xrt::hw_context& hwctx,
                      const std::string& name)
{
  return std::make_shared<xrt::kernel_impl>(dev, hwctx, name);
}

static std::shared_ptr<xrt::mailbox_impl>
get_mailbox_impl(const xrt::run& run)
{
  auto rimpl = run.get_handle();
  auto mimpl = std::dynamic_pointer_cast<xrt::mailbox_impl>(rimpl);
  if (!mimpl)
    throw xrt_core::error("Mailbox not supported by this run object");
  return mimpl;
}

////////////////////////////////////////////////////////////////
// Implementation helper for C API
////////////////////////////////////////////////////////////////
namespace api {

xrtKernelHandle
xrtKernelOpen(xrtDeviceHandle dhdl, const xuid_t xclbin_uuid, const char *name, ip_context::access_mode am)
{
  auto device = get_device(dhdl);
  auto mode = hwctx_access_mode(am);  // legacy access mode to hwctx qos
  auto kernel = std::make_shared<xrt::kernel_impl>(device, xrt::hw_context{device->get_xrt_device(), xclbin_uuid, mode}, name);
  auto handle = kernel.get();
  kernels.add(handle, std::move(kernel));
  return handle;
}

void
xrtKernelClose(xrtKernelHandle khdl)
{
  kernels.remove_or_error(khdl);
}

xrtRunHandle
xrtRunOpen(xrtKernelHandle khdl)
{
  const auto& kernel = kernels.get_or_error(khdl);
  auto run = alloc_run(kernel);
  auto handle = run.get();
  runs.add(handle, std::move(run));
  return handle;
}

void
xrtRunClose(xrtRunHandle rhdl)
{
  auto run = runs.get_or_error(rhdl);
  run_updates.remove(run);
  runs.remove_or_error(rhdl);
}

ert_cmd_state
xrtRunState(xrtRunHandle rhdl)
{
  auto run = runs.get_or_error(rhdl);
  return run->state();
}

ert_cmd_state
xrtRunWait(xrtRunHandle rhdl, unsigned int timeout_ms)
{
  auto run = runs.get_or_error(rhdl);
  return run->wait(timeout_ms * 1ms);
}

void
xrtRunSetCallback(xrtRunHandle rhdl, ert_cmd_state state,
                  void (* pfn_state_notify)(xrtRunHandle, ert_cmd_state, void*),
                  void* data)
{
  if (state != ERT_CMD_STATE_COMPLETED)
    throw xrt_core::error(-EINVAL, "xrtRunSetCallback state may only be ERT_CMD_STATE_COMPLETED");
  auto run = runs.get_or_error(rhdl);
  run->add_callback([=](ert_cmd_state state) { pfn_state_notify(rhdl, state, data); });
}

void
xrtRunStart(xrtRunHandle rhdl)
{
  auto run = runs.get_or_error(rhdl);
  run->start();
}

} // api

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", msg);
}

} // namespace

////////////////////////////////////////////////////////////////
// XRT implmentation access to internal kernel APIs
////////////////////////////////////////////////////////////////
namespace xrt_core { namespace kernel_int {

void
copy_bo_with_kdma(const std::shared_ptr<xrt_core::device>& core_device,
                  size_t sz,
                  xclBufferHandle dst_bo, size_t dst_offset,
                  xclBufferHandle src_bo, size_t src_offset)
{
#ifndef _WIN32
  if (is_sw_emulation())
    throw std::runtime_error("KDMA not support in software emulation");

  // Construct a kernel command to copy bo.  Kernel commands
  // must be shared ptrs
  auto dev = get_device(core_device);
  auto cmd = std::make_shared<kernel_command>(dev, xrt_core::hw_queue{core_device.get()});

  // Get and fill the underlying packet
  auto pkt = cmd->get_ert_cmd<ert_start_copybo_cmd*>();
  ert_fill_copybo_cmd(pkt, src_bo, dst_bo, src_offset, dst_offset, sz);

  // Run the command and wait for completion
  cmd->run();
  cmd->wait();
#else
  throw std::runtime_error("KDMA not supported on windows");
#endif
}

xrt_core::xclbin::kernel_argument::argtype
arg_type_at_index(const xrt::kernel& kernel, size_t argidx)
{
  auto& arg = kernel.get_handle()->get_arg(argidx);
  return arg.type();
}

void
set_arg_at_index(const xrt::run& run, size_t idx, const void* value, size_t bytes)
{
  const auto& rimpl = run.get_handle();
  auto& arg = rimpl->get_kernel()->get_arg(idx, true);
  rimpl->set_arg_value(arg, value, bytes);
}

xrt::run
clone(const xrt::run& run)
{
  return xrt::run{std::make_shared<xrt::run_impl>(run.get_handle().get())};
}

const std::bitset<max_cus>&
get_cumask(const xrt::run& run)
{
  return run.get_handle()->get_cumask();
}

void
set_cus(xrt::run& run, const std::bitset<max_cus>& mask)
{
  return run.get_handle()->set_cus(mask);
}

void
pop_callback(const xrt::run& run)
{
  run.get_handle()->pop_callback();
}

xrt::xclbin::ip::control_type
get_control_protocol(const xrt::run& run)
{
  return run.get_handle()->get_kernel()->get_ip_control_protocol();
}

std::vector<const xclbin::kernel_argument*>
get_args(const xrt::kernel& kernel)
{
  const auto& args = kernel.get_handle()->get_args();
  std::vector<const xclbin::kernel_argument*> vec;
  for (const auto& arg : args)
    vec.push_back(&arg.get_xarg());
  return vec;
}

const xclbin::kernel_argument*
get_arg_info(const xrt::run& run, size_t argidx)
{
  auto& arg = run.get_handle()->get_kernel()->get_arg(argidx);
  return &arg.get_xarg();
}

std::vector<uint32_t>
get_arg_value(const xrt::run& run, size_t argidx)
{
  const auto& rimpl = run.get_handle();
  const auto kimpl = rimpl->get_kernel();

  // get argument info from kernel and value from run
  const auto& arg = kimpl->get_arg(argidx);
  auto value = rimpl->get_arg_value(arg);
  std::vector<uint32_t> vec(value.size());
  std::copy_n(value.begin(), value.size(), vec.data());
  return vec;
}

size_t
get_regmap_size(const xrt::kernel& kernel)
{
    return kernel.get_handle()->get_regmap_size();
}

}} // kernel_int, xrt_core


////////////////////////////////////////////////////////////////
// xrt_kernel C++ API implmentations (xrt_kernel.h)
////////////////////////////////////////////////////////////////
namespace xrt {

run::
run(const kernel& krnl)
  : handle(xdp::native::profiling_wrapper
           ("xrt::run::run",alloc_run, krnl.get_handle()))
{}

void
run::
start()
{
  xdp::native::profiling_wrapper
    ("xrt::run::start", [this]{
    handle->start();
    });
}

void
run::
start(const autostart& iterations)
{
  handle->start(iterations);
}

void
run::
stop()
{
  handle->stop();
}

ert_cmd_state
run::
abort()
{
  return handle->abort();
}

ert_cmd_state
run::
wait(const std::chrono::milliseconds& timeout_ms) const
{
  return xdp::native::profiling_wrapper("xrt::run::wait",
    [this, &timeout_ms] {
      return handle->wait(timeout_ms);
    });
}

ert_cmd_state
run::
state() const
{
  return xdp::native::profiling_wrapper("xrt::run::state", [this]{
    return handle->state();
  });
}

int
run::
get_arg_index(const std::string& argnm) const
{
  return handle->get_arg_index(argnm);
}

void
run::
set_arg_at_index(int index, const void* value, size_t bytes)
{
  handle->set_arg_at_index(index, value, bytes);
}

void
run::
set_arg_at_index(int index, const xrt::bo& glb)
{
  handle->set_arg_at_index(index, glb);
}

void
run::
update_arg_at_index(int index, const void* value, size_t bytes)
{
  auto upd = get_run_update(handle.get());
  upd->update_arg_at_index(index, value, bytes);
}

void
run::
update_arg_at_index(int index, const xrt::bo& glb)
{
  auto upd = get_run_update(handle.get());
  upd->update_arg_at_index(index, glb);
}

void
run::
add_callback(ert_cmd_state state,
             std::function<void(const void*, ert_cmd_state, void*)> fcn,
             void* data)
{
  XRT_DEBUGF("run::add_callback run(%d)\n", handle->get_uid());
  if (state != ERT_CMD_STATE_COMPLETED)
    throw xrt_core::error(-EINVAL, "xrtRunSetCallback state may only be ERT_CMD_STATE_COMPLETED");
  // The function callback is passed a key that uniquely identifies
  // run objects referring to the same implmentation.  This allows
  // upstream to associate key with some run object that represents
  // the key. Note that the callback cannot pass *this (xrt::run) as
  // these objects are transient.
  auto key = handle.get();
  handle->add_callback([=](ert_cmd_state state) { fcn(key, state, data); });
}

ert_packet*
run::
get_ert_packet() const
{
  return xdp::native::profiling_wrapper("xrt::run::get_ert_packet", [this]{
    return handle->get_ert_packet();
  });
}

kernel::
kernel(const xrt::device& xdev, const xrt::uuid& xclbin_id, const std::string& name, cu_access_mode mode)
  : handle(xdp::native::profiling_wrapper("xrt::kernel::kernel",
      alloc_kernel, get_device(xdev), xclbin_id, name, mode))
{}

kernel::
kernel(xclDeviceHandle dhdl, const xrt::uuid& xclbin_id, const std::string& name, cu_access_mode mode)
  : handle(xdp::native::profiling_wrapper("xrt::kernel::kernel",
      alloc_kernel, get_device(xrt_core::get_userpf_device(dhdl)), xclbin_id, name, mode))
{}

kernel::
kernel(const xrt::hw_context& ctx, const std::string& name)
  : handle(alloc_kernel_from_ctx(get_device(ctx.get_device()), ctx, name))
{}

uint32_t
kernel::
read_register(uint32_t offset) const
{
  return xdp::native::profiling_wrapper("xrt::kernel::read_register", [this, offset]{
    return handle->read_register(offset);
  });
}

void
kernel::
write_register(uint32_t offset, uint32_t data)
{
  xdp::native::profiling_wrapper("xrt::kernel::write_register", [this, offset, data]{
    handle->write_register(offset, data);
  });
}


int
kernel::
group_id(int argno) const
{
  return xdp::native::profiling_wrapper("xrt::kernel::group_id", [this, argno]{
    return handle->group_id(argno);
  });
}

uint32_t
kernel::
offset(int argno) const
{
  return xdp::native::profiling_wrapper("xrt::kernel::offset", [this, argno]{
    return handle->arg_offset(argno);
  });
}

std::string
kernel::
get_name() const
{
  return handle->get_name();
}

xrt::xclbin
kernel::
get_xclbin() const
{
  return handle->get_xclbin();
}

// Experimental API
// This function defines the read-only register range on a compute unit
// associated with a kernel if and only if (1) the kernel has exactly
// one compute unit and (2) the compute unit is opened in shared mode.
// The read range allows xrt::kernel::read_register to read directly
// from compute unit registers even if the compute unit is shared between
// this process and another or between multiple kernel objects.
//
// @start: the start offset of the read-only register range
// @size: the size of the read-only register range.
//
// Throws on error.
void
set_read_range(const xrt::kernel& kernel, uint32_t start, uint32_t size)
{
  auto handle = kernel.get_handle();
  const auto& ips = handle->get_ips();
  if (ips.size() != 1)
    throw xrt_core::error("read range only supported for kernels with one compute unit");

  auto ip = ips.front();
  if (ip->get_access_mode() != xrt::kernel::cu_access_mode::shared)
    throw xrt_core::error("read range only supported for kernels with shared compute unit");

  auto core_device = handle->get_core_device();
  core_device->set_cu_read_range(ip->get_index(), start, size);

  ip->m_readrange = {start, size};
}


} // namespace xrt

////////////////////////////////////////////////////////////////
// xrt_mailbox C++ experimental API implmentations
// see experimental/xrt_mailbox.h
////////////////////////////////////////////////////////////////
namespace xrt {

mailbox::
mailbox(const xrt::run& run)
  : detail::pimpl<mailbox_impl>(get_mailbox_impl(run))
{
}

void
mailbox::
read()
{
  handle->read();
}

void
mailbox::
write()
{
  handle->write();
}

std::pair<const void*, size_t>
mailbox::
get_arg(int index) const
{
  return handle->get_arg(index);
}

int
mailbox::
get_arg_index(const std::string& argnm) const
{
  return handle->get_arg_index(argnm);
}

void
mailbox::
set_arg_at_index(int index, const void* value, size_t bytes)
{
  handle->set_arg_at_index(index, value, bytes);
}

void
mailbox::
set_arg_at_index(int index, const xrt::bo& glb)
{
  handle->set_arg_at_index(index, glb);
}

}
////////////////////////////////////////////////////////////////
// xrt_kernel API implmentations (xrt_kernel.h)
////////////////////////////////////////////////////////////////
xrtKernelHandle
xrtPLKernelOpen(xrtDeviceHandle dhdl, const xuid_t xclbin_uuid, const char *name)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [dhdl, xclbin_uuid, name]{
      return api::xrtKernelOpen(dhdl, xclbin_uuid, name, ip_context::access_mode::shared);
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return XRT_NULL_HANDLE;
}

xrtKernelHandle
xrtPLKernelOpenExclusive(xrtDeviceHandle dhdl, const xuid_t xclbin_uuid, const char *name)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [dhdl, xclbin_uuid, name]{
      return api::xrtKernelOpen(dhdl, xclbin_uuid, name, ip_context::access_mode::exclusive);
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return XRT_NULL_HANDLE;
}

int
xrtKernelClose(xrtKernelHandle khdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [khdl]{
      api::xrtKernelClose(khdl);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

xrtRunHandle
xrtRunOpen(xrtKernelHandle khdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [khdl]{
      return api::xrtRunOpen(khdl);
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return XRT_NULL_HANDLE;
}

int
xrtKernelArgGroupId(xrtKernelHandle khdl, int argno)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [khdl, argno]{
      return kernels.get_or_error(khdl)->group_id(argno);
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

uint32_t
xrtKernelArgOffset(xrtKernelHandle khdl, int argno)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [khdl, argno]{
      return kernels.get_or_error(khdl)->arg_offset(argno);
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return std::numeric_limits<uint32_t>::max();
}

int
xrtKernelReadRegister(xrtKernelHandle khdl, uint32_t offset, uint32_t* datap)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [khdl, offset, datap]{
      *datap = kernels.get_or_error(khdl)->read_register(offset);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtKernelWriteRegister(xrtKernelHandle khdl, uint32_t offset, uint32_t data)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [khdl, offset, data]{
      kernels.get_or_error(khdl)->write_register(offset, data);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

xrtRunHandle
xrtKernelRun(xrtKernelHandle khdl, ...)
{
  try {
    std::va_list args;
    std::va_list* argptr = &args;
    va_start(args, khdl);  // NOLINT
    auto result = xdp::native::profiling_wrapper(__func__,
    [khdl, argptr]{
      auto handle = xrtRunOpen(khdl);
      auto run = runs.get_or_error(handle);
      run->set_all_args(argptr);
      run->start();
      return handle;
    });
    va_end(args);
    return result;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return XRT_NULL_HANDLE;
}

int
xrtRunClose(xrtRunHandle rhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl]{
      api::xrtRunClose(rhdl);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

ert_cmd_state
xrtRunState(xrtRunHandle rhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl]{
      return api::xrtRunState(rhdl);
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return ERT_CMD_STATE_ABORT;
}

ert_cmd_state
xrtRunWait(xrtRunHandle rhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl]{
      return api::xrtRunWait(rhdl, 0);
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return ERT_CMD_STATE_ABORT;
}

ert_cmd_state
xrtRunWaitFor(xrtRunHandle rhdl, unsigned int timeout_ms)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl, timeout_ms]{
      return api::xrtRunWait(rhdl, timeout_ms);
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return ERT_CMD_STATE_ABORT;
}

int
xrtRunSetCallback(xrtRunHandle rhdl, ert_cmd_state state,
                  void (* pfn_state_notify)(xrtRunHandle, ert_cmd_state, void*),
                  void* data)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [rhdl, state, pfn_state_notify, data]{
      api::xrtRunSetCallback(rhdl, state, pfn_state_notify, data);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtRunStart(xrtRunHandle rhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl]{
      api::xrtRunStart(rhdl);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtRunUpdateArg(xrtRunHandle rhdl, int index, ...)
{
  try {
    std::va_list args;
    std::va_list* argptr = &args;
    va_start(args, index); // NOLINT
    auto result = xdp::native::profiling_wrapper(__func__,
    [rhdl, index, argptr]{
      auto upd = get_run_update(rhdl);
      upd->update_arg_at_index(index, argptr);
      return 0;
    });
    va_end(args);
    return result;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtRunUpdateArgV(xrtRunHandle rhdl, int index, const void* value, size_t bytes)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [rhdl, index, value, bytes]{
      auto upd = get_run_update(rhdl);
      upd->update_arg_at_index(index, value, bytes);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtRunSetArg(xrtRunHandle rhdl, int index, ...)
{
  try {
    std::va_list args;
    std::va_list* argptr = &args;
    va_start(args, index);  // NOLINT
    auto result = xdp::native::profiling_wrapper(__func__,
    [rhdl, index, argptr]{
      auto run = runs.get_or_error(rhdl);
      run->set_arg_at_index(index, argptr);
      return 0;
    });
    va_end(args);
    return result ;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtRunSetArgV(xrtRunHandle rhdl, int index, const void* value, size_t bytes)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [rhdl, index, value, bytes]{
      auto run = runs.get_or_error(rhdl);
      run->set_arg_at_index(index, value, bytes);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

int
xrtRunGetArgV(xrtRunHandle rhdl, int index, void* value, size_t bytes)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [rhdl, index, value, bytes]{
      auto run = runs.get_or_error(rhdl);
      run->get_arg_at_index(index, static_cast<uint32_t*>(value), bytes);
      return 0;
    });
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get_code();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
  return -1;
}

void
xrtRunGetArgVPP(xrt::run run, int index, void* value, size_t bytes)
{
  xdp::native::profiling_wrapper(__func__, [run, index, value, bytes]{
    const auto& rimpl = run.get_handle();
    rimpl->get_arg_at_index(index, static_cast<uint32_t*>(value), bytes);
  });
}
