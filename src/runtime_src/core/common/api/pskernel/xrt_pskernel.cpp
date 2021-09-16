/*
 * Copyright (C) 2021, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// This file implements XRT PSkernel APIs as declared in
// core/include/experimental/xrt_pskernel.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_pskernel.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_pskernel.h"
#include "core/common/api/native_profile.h"
#include "core/common/api/kernel_int.h"

#include "core/common/api/command.h"
#include "core/common/api/exec.h"
#include "core/common/api/bo.h"
#include "core/common/api/device_int.h"
#include "core/common/api/enqueue.h"
#include "core/common/bo_cache.h"
#include "core/common/config_reader.h"
#include "core/common/device.h"
#include "core/common/debug.h"
#include "core/common/error.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/xclbin_parser.h"
#include "core/include/ert.h"
#include "core/include/ert_fa.h"
#include "core/include/xclbin.h"
#include "core/common/pskernel_parse.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <condition_variable>
#include <chrono>
#include <cstdarg>
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
# pragma warning( disable : 4244 4267 4996 4100)
#endif

namespace {

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
// Templated header (xrt_pskernel.h) passes &arg and sizeof(arg) to
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

inline bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? std::strcmp(xem,"sw_emu")==0 : false;
  return swem;
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

// struct psip_context - Manages process access to CUs
//
// Constructing a kernel object opens a context on the CUs associated
// with the kernel object.  The context is reference counted such that
// multiple kernel objects can open a context on the same CU provided
// the access type is shared.
//
// A CU context is released when the last kernel object referencing it
// is closed.  If the process closes without having released on kernel
// then behavior is undefined.
class psip_context
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

    // @device: core device
    // @conn: connectivity section of xclbin
    // @ipidx: index of the ip for which connectivity data is created
    connectivity(const xrt_core::device* device, const xrt::uuid& xclbin_id, int32_t ipidx)
    {
      const auto& memidx_encoding = device->get_memidx_encoding(xclbin_id);
      auto conn = device->get_axlf_section<const ::connectivity*>(ASK_GROUP_CONNECTIVITY, xclbin_id);
      if (!conn)
        return;
      // Compute the connections for IP with specified index
      for (int count = 0; count < conn->m_count; ++count) {
        auto& cxn  = conn->m_connection[count];
        if (cxn.m_ip_layout_index != ipidx)
          continue;

        auto argidx = cxn.arg_index;
        auto memidx = cxn.mem_data_index;

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
  using access_mode = xrt::pskernel::cu_access_mode;
  static constexpr unsigned int virtual_cu_idx = std::numeric_limits<unsigned int>::max();

  // open() - open a context in a specific IP/CU
  //
  // @device:    Device on which context should opened
  // @xclbin_id: UUID of xclbin containeing the IP definition
  // @ip:        The ip_data defintion for this IP from the xclbin
  // @ipidx:     Index of IP in the IP_LAYOUT section of xclbin
  // @cuidx:     Sorted index of CU used when populating cmd pkt
  // @am:        Access mode, how this CU should be opened
  static std::shared_ptr<psip_context>
  open(xrt_core::device* device, const xrt::uuid& xclbin_id,
       unsigned int ipidx, unsigned int cuidx, access_mode am)
  {
    static std::mutex mutex;
    static std::map<xrt_core::device*, std::array<std::weak_ptr<psip_context>, max_cus>> dev2ips;
    std::lock_guard<std::mutex> lk(mutex);
    auto& ips = dev2ips[device];
    auto ipctx = ips[cuidx].lock();
    if (!ipctx) {
      // NOLINTNEXTLINE(modernize-make-shared)  used in weak_ptr
      ipctx = std::shared_ptr<psip_context>(new psip_context(device, xclbin_id, ipidx, cuidx, am));
      ips[cuidx] = ipctx;
    }
    
    if (ipctx->access != am)
      throw std::runtime_error("Conflicting access mode for IP(" + std::to_string(cuidx) + ")");

    return ipctx;
  }

  // open() - open a context on the device virtual CU
  //
  // @device:    The device on which to open the virtual CU
  // xclbin_id:  The xclbin that is locked by this call
  //
  // This keeps a lock on the xclbin after it is loaded onto the device
  // without locking any specific CU.
  static std::shared_ptr<psip_context>
  open_virtual_cu(xrt_core::device* device, const xrt::uuid& xclbin_id)
  {
    static std::mutex mutex;
    static std::map<xrt_core::device*, std::weak_ptr<psip_context>> dev2vip;
    std::lock_guard<std::mutex> lk(mutex);
    auto& vip = dev2vip[device];
    auto ipctx = vip.lock();
    if (!ipctx)
      // NOLINTNEXTLINE(modernize-make-shared)  used in weak_ptr
      vip = ipctx = std::shared_ptr<psip_context>(new psip_context(device, xclbin_id));
    return ipctx;
  }

  // Access mode can be set only if it starts out as unspecifed (none).
  void
  set_access_mode(access_mode am)
  {
    if (access != access_mode::none)
      throw std::runtime_error("Cannot change current access mode");
    device->open_context(xid.get(), cuidx+max_cus, std::underlying_type<access_mode>::type(am));
    access = am;
  }

  access_mode
  get_access_mode() const
  {
    return access;
  }

  // For symmetry
  void
  close()
  {}

  unsigned int
  get_cuidx() const
  {
    return cuidx;
  }

  // Check if arg is connected to specified memory bank
  bool
  valid_connection(size_t argidx, int32_t memidx)
  {
    return args.valid_arg_connection(argidx, memidx);
  }

  // Get default memory bank for argument at specified index The
  // default memory bank is the connection with the highest group
  // connectivity index
  int32_t
  arg_memidx(size_t argidx) const
  {
    return args.get_arg_memidx(argidx);
  }

  ~psip_context()
  {
    if(cuidx==virtual_cu_idx) 
      device->close_context(xid.get(), cuidx);
    else
      device->close_context(xid.get(), cuidx+max_cus);
  }

  psip_context(const psip_context&) = delete;
  psip_context(psip_context&&) = delete;
  psip_context& operator=(psip_context&) = delete;
  psip_context& operator=(psip_context&&) = delete;

private:
  // regular CU
  psip_context(xrt_core::device* dev, const xrt::uuid& xclbin_id,
             unsigned int ipindex, unsigned int cuindex, access_mode am)
    : device(dev)
    , xid(xclbin_id)
    , args(dev, xclbin_id, ipindex)
    , cuidx(cuindex)
    , access(am)
  {
    if (access != access_mode::none)
      device->open_context(xid.get(), cuidx+max_cus, std::underlying_type<access_mode>::type(am));
  }

  // virtual CU
  psip_context(xrt_core::device* dev, xrt::uuid xclbin_id)
    : device(dev)
    , xid(std::move(xclbin_id))
    , cuidx(virtual_cu_idx)
    , access(access_mode::shared)
  {
    device->open_context(xid.get(), cuidx, std::underlying_type<access_mode>::type(access));
  }

  xrt_core::device* device; //
  xrt::uuid xid;            // xclbin uuid
  connectivity args;        // argument memory connections
  unsigned int cuidx;       // cu index for execution
  access_mode access;       // compute unit access mode
};

// Remove when c++17
constexpr int32_t psip_context::connectivity::no_memidx;

// class kernel_command - Immplements command API expected by schedulers
//
// The kernel command is
class kernel_command : public xrt_core::command
{
public:
  using execbuf_type = xrt_core::bo_cache::cmd_bo<ert_start_kernel_cmd>;
  using callback_function_type = std::function<void(ert_cmd_state)>;
  using callback_list = std::vector<callback_function_type>;

public:
  explicit
  kernel_command(std::shared_ptr<device_type> dev)
    : m_device(std::move(dev))
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

  // set_event() - enqueued notifcation of event
  //
  // @event:  Event to notify upon completion of cmd
  //
  // Event notification is used when a kernel/run is enqueued in an
  // event graph.  When cmd completes, the event must be notified.
  //
  // The event (stored in the event graph) participates in lifetime
  // of the object that holds on to cmd object.
  void
  set_event(const std::shared_ptr<xrt::event_impl>& event) const
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    XRT_DEBUGF("kernel_command::set_event() m_uid(%d)\n", m_uid);
    if (m_done) {
      xrt_core::enqueue::done(event.get());
      return;
    }
    m_event = event;
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

  // Submit the command for execution
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
      xrt_core::exec::managed_start(this);
    else
      xrt_core::exec::unmanaged_start(this);
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
      xrt_core::exec::unmanaged_wait(this);
    }

    auto pkt = get_ert_packet();
    return static_cast<ert_cmd_state>(pkt->state);
  }

  ert_cmd_state
  wait(const std::chrono::milliseconds& timeout_ms) const
  {
    if (m_managed) {
      std::unique_lock<std::mutex> lk(m_mutex);
      while (!m_done)
        m_exec_done.wait_for(lk, timeout_ms);
    }
    else {
      xrt_core::exec::unmanaged_wait(this);
    }

    auto pkt = get_ert_packet();
    return static_cast<ert_cmd_state>(pkt->state);
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
  notify(ert_cmd_state s) override
  {
    bool complete = false;
    bool callbacks = false;
    if (s>=ERT_CMD_STATE_COMPLETED) {
      std::lock_guard<std::mutex> lk(m_mutex);
      XRT_DEBUGF("kernel_command::notify() m_uid(%d) m_state(%d)\n", m_uid, s);
      complete = m_done = true;
      callbacks = (m_callbacks && !m_callbacks->empty());
      if (m_event)
        xrt_core::enqueue::done(m_event.get());
    }

    if (complete) {
      m_exec_done.notify_all();
      if (callbacks)
        run_callbacks(s);

      // Clear the event if any.  This must be last since if used, it
      // holds the lifeline to this command object which could end up
      // being deleted when the event is cleared.
      m_event = nullptr;
    }
  }

private:
  std::shared_ptr<device_type> m_device;
  mutable std::shared_ptr<xrt::event_impl> m_event;
  execbuf_type m_execbuf; // underlying execution buffer
  unsigned int m_uid = 0;
  bool m_managed = false;
  bool m_done = false;

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
      //      auto addr = xrt_core::bo::address(bo);
      //      setter->set_arg_value(arg, arg_range<uint8_t>{&addr, sizeof(addr)});
      setter->set_arg_value(arg, bo);
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

  using xarg = xrt_core::pskernel::kernel_argument;
  xarg arg;         // argument meta data from xclbin

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
  argument(xarg&& karg)
    : arg(std::move(karg))
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
      else if (arg.hosttype == "uint32_t")
        content = std::make_unique<scalar_type<uint32_t,uint32_t>>(arg.size);
      else if (arg.hosttype == "uint64_t")
        content = std::make_unique<scalar_type<uint64_t,uint64_t>>(arg.size);
      else if (arg.hosttype == "int32_t")
        content = std::make_unique<scalar_type<int32_t,int32_t>>(arg.size);
      else if (arg.hosttype == "int64_t")
        content = std::make_unique<scalar_type<int64_t,int64_t>>(arg.size);
      else
	// throw xrt_core::error(-EINVAL, "Unknown scalar argument type '" + arg.hosttype + "'");
        // arg.hosttype is free formed, default to size_t until clarified
        content = std::make_unique<scalar_type<size_t,size_t>>(arg.size);
      break;
    }
    case xarg::argtype::global :
      content = std::make_unique<global_type>(arg.size);
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

// struct kernel_type - The internals of an xrtPSKernelHandle
//
// An single object of kernel_type can be shared with multiple
// run handles.   The kernel object defines all kernel specific
// meta data used to create a launch a run object (command)
class pskernel_impl
{
  using ipctx = std::shared_ptr<psip_context>;
  using property_type = xrt_core::xclbin::kernel_properties;

  std::shared_ptr<device_type> device; // shared ownership
  std::string name;                    // kernel name
  std::vector<argument> args;          // kernel args sorted by argument index
  std::vector<ipctx> ipctxs;           // CU context locks
  ipctx vctx;                          // virtual CU context
  std::bitset<max_cus> cumask;         // cumask for command execution
  property_type properties;            // Kernel properties from XML meta
  size_t regmap_size = 0;              // CU register map size
  size_t num_cumasks = 1;              // Required number of command cu masks
  uint32_t protocol = 0;               // Default opcode
  uint32_t uid;                        // Internal unique id for debug

  void
  amend_args()
  {
    // adjust regmap size for kernels without arguments.
    // first 4 register map entries are control registers
    regmap_size = std::max<size_t>(regmap_size, 4);
  }

  unsigned int
  get_cuidx_or_error(size_t offset, bool force=false) const
  {
    if (ipctxs.size() != 1)
      throw std::runtime_error("Cannot read or write kernel with multiple compute units");
    auto& ipctx = ipctxs.back();
    auto mode = ipctx->get_access_mode();
    if (!force && mode != psip_context::access_mode::exclusive && !xrt_core::config::get_rw_shared())
      throw std::runtime_error("Cannot read or write kernel with shared access");

    return ipctx->get_cuidx();
  }

  void
  initialize_command_header(ert_start_kernel_cmd* kcmd)
  {
    kcmd->extra_cu_masks = num_cumasks - 1;  //  -1 for mandatory mask
    kcmd->count = num_cumasks + regmap_size;
    kcmd->opcode = ERT_SK_START;
    kcmd->type = ERT_CU;
    kcmd->state = ERT_CMD_STATE_NEW;
  }

  static uint32_t
  create_uid()
  {
    static std::atomic<uint32_t> count {0};
    return count++;
  }

public:
  // kernel_type - constructor
  //
  // @dev:     device associated with this kernel object
  // @uuid:    uuid of xclbin to mine for kernel meta data
  // @nm:      name identifying kernel and/or kernel and instances
  // @am:      access mode for underlying compute units
  pskernel_impl(std::shared_ptr<device_type> dev, const xrt::uuid& xclbin_id, const std::string& nm, psip_context::access_mode am)
    : device(std::move(dev))                                   // share ownership
    , name(nm.substr(0,nm.find(":")))                         
    , vctx(psip_context::open_virtual_cu(device->core_device.get(), xclbin_id))
    , uid(create_uid())
  {
    XRT_DEBUGF("pskernel_impl::pskernel_impl(%d)\n" , uid);

    xrt_core::xclbin::softkernel_object sko;
    bool sk_found = false;
    int cuidx_start = 0;
    
    // Extract soft kernel section for arguments
    auto sk_sections = device->core_device->get_axlf_sections_or_error(SOFT_KERNEL,xclbin_id);
    if (sk_sections.empty())
      throw std::runtime_error("No soft kernel metadata available to construct kernel, make sure xclbin is loaded");

    // Map soft kernel section
    for(auto sk : sk_sections) {
      auto soft = reinterpret_cast<const soft_kernel*>(sk.first);

      std::string soft_name = std::string(sk.first+soft->mpo_symbol_name);
      if(soft_name.compare(name) == 0) {
	sko.ninst = soft->m_num_instances;
	sko.symbol_name = std::string(sk.first+soft->mpo_symbol_name);
	sko.mpo_name = std::string(sk.first+soft->mpo_name);
	sko.mpo_version = std::string(sk.first+soft->mpo_version);
	sko.size = soft->m_image_size;
	sko.sk_buf = const_cast<char*>(sk.first+soft->m_image_offset);  // NOLINT
	sk_found = true;
	XRT_DEBUGF("pskernel_impl::sk_found!  sko_ninst = %d, sko.symbol_name = %s\n" , sko.ninst, sko.symbol_name.c_str());
	break;
      }
      else {
	cuidx_start += soft->m_num_instances;
      }
      
    }
    if(!sk_found)
      throw std::runtime_error("No soft kernel matching '" + name + "'");

    // Generate CU masks
    // Check for <kernel name>:{n,n+1,...}
    if(nm.find(":") == std::string::npos ) {
      // Use all instances of PS kernels
      for(int i=0;i<static_cast<int>(sko.ninst);i++) {
	int cuidx = cuidx_start + i;
	XRT_DEBUGF("PS kernel cuidx = %d\n",cuidx);
	ipctxs.emplace_back(psip_context::open(device->get_core_device(), xclbin_id, cuidx, cuidx, am));
	cumask.set(cuidx);
	num_cumasks = std::max<size_t>(num_cumasks, (cuidx / cus_per_word) + 1);
      }
    }
    else {
      // Use CUs from list
      std::vector<std::string> kernel_cus;
      auto culist = nm.substr(nm.find(":")+1);
      // Check for braces
      if(culist.find("{") != std::string::npos) {
	culist.erase(culist.find("{"),1);
      }
      if(culist.find("}") != std::string::npos) {
	culist.erase(culist.find("}"),1);
      }
      XRT_DEBUGF("CU list: %s\n",culist.c_str());
      std::stringstream ss(culist);
      while(ss.good()) {
	std::string substr;
	std::getline(ss,substr,',');
	kernel_cus.emplace_back(substr);
      }
      for (auto cu : kernel_cus) {
	XRT_DEBUGF("Picking CUs %s\n",cu.c_str());
	auto cuidx = cuidx_start + std::stoi(cu);
	ipctxs.emplace_back(psip_context::open(device->get_core_device(), xclbin_id, cuidx, cuidx, am));
	cumask.set(cuidx);
	num_cumasks = std::max<size_t>(num_cumasks, (cuidx / cus_per_word) + 1);
      }
    }

    // set kernel protocol
    protocol = AP_CTRL_HS;

    // get kernel arguments
    // compute regmap size, convert to typed argument
    for (auto& arg : xrt_core::pskernel::pskernel_parse(sko.sk_buf, sko.size, name.c_str())) {
      XRT_DEBUGF("arg index = %d, arg offset = %d, arg size = %d\n",arg.index, arg.offset, arg.size);
      regmap_size = std::max(regmap_size, (arg.offset + arg.size) / sizeof(uint32_t));
      args.emplace_back(std::move(arg));
    }

    // amend args with computed data based on kernel protocol
    amend_args();
  }

  ~pskernel_impl()
  {
    XRT_DEBUGF("pskernel_impl::~pskernel_impl(%d)\n" , uid);
  }

  pskernel_impl(const pskernel_impl&) = delete;
  pskernel_impl(pskernel_impl&&) = delete;
  pskernel_impl& operator=(pskernel_impl&) = delete;
  pskernel_impl& operator=(pskernel_impl&&) = delete;

  // Initialize kernel command and return pointer to payload
  // after mandatory static data.
  uint32_t*
  initialize_command(kernel_command* cmd)
  {
    auto kcmd = cmd->get_ert_cmd<ert_start_kernel_cmd*>();
    initialize_command_header(kcmd);
    cmd->encode_compute_units(cumask, num_cumasks);
    auto data = kcmd->data + kcmd->extra_cu_masks;

    return data;
  }

  std::string
  get_name() const
  {
    return name;
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
    // Last (for group id) connection of first ip in this kernel
    // The group id can change if cus are trimmed based on argument
    auto& ip = ipctxs.front();  // guaranteed to be non empty
    return ip->arg_memidx(argno);
  }

  int
  arg_offset(int argno)
  {
    return args.at(argno).offset();
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
};

// struct psrun_impl - The internals of an xrtPSRunHandle
//
// An run handle shares ownership of a kernel object.  The run object
// corresponds to an execution context for the given kernel object.
// Multiple run objects against the same kernel object can be created
// and submitted for execution concurrently.  Each run object manages
// its own execution buffer (ert command object)
class psrun_impl
{
  using ipctx = std::shared_ptr<psip_context>;

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
      hs_arg_setter::set_arg_value(arg, arg_range<uint8_t>{&value, sizeof(value)});
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
    return std::make_unique<ps_arg_setter>(data);
  }

  arg_setter*
  get_arg_setter()
  {
    if (!asetter)
      asetter = make_arg_setter();

    return asetter.get();
  }

  // Clone the commmand packet of another psrun_impl
  // Used when constructing a psrun_impl from another psrun_impl
  // for concurrent execution
  uint32_t*
  clone_command_data(const psrun_impl* rhs)
  {
    auto pkt = cmd->get_ert_packet();
    auto rhs_pkt = rhs->cmd->get_ert_packet();
    pkt->header = rhs_pkt->header;
    pkt->state = ERT_CMD_STATE_NEW;
    std::copy_n(rhs_pkt->data, rhs_pkt->count, pkt->data);
    return pkt->data + (rhs->data - rhs_pkt->data);
  }

  using callback_function_type = std::function<void(ert_cmd_state)>;
  std::shared_ptr<pskernel_impl> kernel;    // shared ownership
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

  // set_event() - enqueued notifcation of event
  //
  // @event:  Event to notify upon completion of run
  //
  // Event notification is used when a kernel/run is enqueued in an
  // event graph.  When run completes, the event must be notified.
  //
  // The event (stored in the event graph) participates in lifetime
  // of the run object.
  void
  set_event(const std::shared_ptr<event_impl>& event) const
  {
    cmd->set_event(event);
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
  psrun_impl(std::shared_ptr<pskernel_impl> k)
    : kernel(std::move(k))                        // share ownership
    , ips(kernel->get_ips())
    , cumask(kernel->get_cumask())
    , core_device(kernel->get_core_device())      // cache core device
    , cmd(std::make_shared<kernel_command>(kernel->get_device()))
    , data(kernel->initialize_command(cmd.get())) // default encodes CUs
    , uid(create_uid())
  {
    XRT_DEBUGF("psrun_impl::psrun_impl(%d)\n" , uid);
  }

  // Clones a psrun impl, so that the clone can be executed concurrently
  // with the clonee.
  explicit
  psrun_impl(const psrun_impl* rhs)
    : kernel(rhs->kernel)
    , ips(rhs->ips)
    , cumask(rhs->cumask)
    , core_device(rhs->core_device)
    , cmd(std::make_shared<kernel_command>(kernel->get_device()))
    , data(clone_command_data(rhs))
    , uid(create_uid())
  {
    XRT_DEBUGF("psrun_impl::psrun_impl(%d)\n" , uid);
  }

  virtual
  ~psrun_impl()
  {
    XRT_DEBUGF("psrun_impl::~psrun_impl(%d)\n" , uid);
  }

  psrun_impl(const psrun_impl&) = delete;
  psrun_impl(psrun_impl&&) = delete;
  psrun_impl& operator=(psrun_impl&) = delete;
  psrun_impl& operator=(psrun_impl&&) = delete;

  pskernel_impl*
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
  set_arg_at_index(size_t index, const xrt::bo& bo)
  {
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
  set_all_args(std::va_list* args)
  {
    for (auto& arg : kernel->get_args()) {
      if (arg.index() == argument::no_index)
        break;
      XRT_DEBUGF("arg name(%s) index(%d) offset(0x%x) size(%d)", arg.name().c_str(), arg.index(), arg.offset(), arg.size());
      set_arg(arg, args);
    }
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
    auto pkt = cmd->get_ert_packet();
    return static_cast<ert_cmd_state>(pkt->state);
  }

  ert_packet*
  get_ert_packet() const
  {
    return cmd->get_ert_packet();
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

// Active kernels per xrtPSKernelOpen/Close.  This is a mapping from
// xrtPSKernelHandle to the corresponding kernel object.  The
// xrtPSKernelHandle is the address of the kernel object.  This is
// shared ownership as application can close a kernel handle before
// closing an xrtPSRunHandle that references same kernel.
static std::map<void*, std::shared_ptr<xrt::pskernel_impl>> kernels;

// Active runs.  This is a mapping from xrtPSRunHandle to corresponding
// run object.  The xrtPSRunHandle is the address of the run object.
// This is unique ownership as only the host application holds on to a
// run object, e.g. the run object is desctructed immediately when it
// is closed.
static std::map<void*, std::unique_ptr<xrt::psrun_impl>> runs;

// Mutex to protect access to maps
static std::mutex map_mutex;

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
  std::lock_guard<std::mutex> lk(map_mutex);
  auto itr = devices.find(dhdl);
  std::shared_ptr<device_type> device = (itr != devices.end())
    ? (*itr).second.lock()
    : nullptr;
  if (!device) {
    // NOLINTNEXTLINE(modernize-make-shared)  used in weak_ptr
    device = std::shared_ptr<device_type>(new device_type(dhdl));
    xrt_core::exec::init(device->get_core_device());
    devices.emplace(std::make_pair(dhdl, device));
  }
  return device;
}

static std::shared_ptr<device_type>
get_device(const std::shared_ptr<xrt_core::device>& core_device)
{
  auto dhdl = core_device.get();

  std::lock_guard<std::mutex> lk(map_mutex);
  auto itr = devices.find(dhdl);
  std::shared_ptr<device_type> device = (itr != devices.end())
    ? (*itr).second.lock()
    : nullptr;
  if (!device) {
    // NOLINTNEXTLINE(modernize-make-shared)  used in weak_ptr
    device = std::shared_ptr<device_type>(new device_type(core_device));
    xrt_core::exec::init(device->get_core_device());
    devices.emplace(std::make_pair(dhdl, device));
  }
  return device;
}

static std::shared_ptr<device_type>
get_device(const xrt::device& xdev)
{
  return get_device(xdev.get_handle());
}

// get_kernel() - get a kernel object from an xrtPSKernelHandle
//
// The lifetime of a kernel object is shared ownerhip. The object
// is shared with host application and run objects.
static const std::shared_ptr<xrt::pskernel_impl>&
get_kernel(xrtPSKernelHandle khdl)
{
  auto itr = kernels.find(khdl);
  if (itr == kernels.end())
    throw xrt_core::error(-EINVAL, "Unknown kernel handle");
  return (*itr).second;
}

// get_run() - get a run object from an xrtPSRunHandle
//
// The lifetime of a run object is unique to the host application.
static xrt::psrun_impl*
get_run(xrtPSRunHandle rhdl)
{
  auto itr = runs.find(rhdl);
  if (itr == runs.end())
    throw xrt_core::error(-EINVAL, "Unknown run handle");
  return (*itr).second.get();
}

static std::unique_ptr<xrt::psrun_impl>
alloc_run(const std::shared_ptr<xrt::pskernel_impl>& khdl)
{
  return std::make_unique<xrt::psrun_impl>(khdl);
}

static std::shared_ptr<xrt::pskernel_impl>
alloc_kernel(const std::shared_ptr<device_type>& dev,
	     const xrt::uuid& xclbin_id,
	     const std::string& name,
	     xrt::pskernel::cu_access_mode mode)
{
  return std::make_shared<xrt::pskernel_impl>(dev, xclbin_id, name, mode) ;
}

////////////////////////////////////////////////////////////////
// Implementation helper for C API
////////////////////////////////////////////////////////////////
namespace api {

xrtPSKernelHandle
xrtPSKernelOpen(xrtDeviceHandle dhdl, const xuid_t xclbin_uuid, const char *name, psip_context::access_mode am)
{
  auto device = get_device(dhdl);
  auto kernel = std::make_shared<xrt::pskernel_impl>(device, xclbin_uuid, name, am);
  auto handle = kernel.get();
  kernels.emplace(std::make_pair(handle,std::move(kernel)));
  return handle;
}

void
xrtPSKernelClose(xrtPSKernelHandle khdl)
{
  auto itr = kernels.find(khdl);
  if (itr == kernels.end())
    throw xrt_core::error(-EINVAL, "Unknown kernel handle");
  kernels.erase(itr);
}

xrtPSRunHandle
xrtPSRunOpen(xrtPSKernelHandle khdl)
{
  const auto& kernel = get_kernel(khdl);
  auto run = alloc_run(kernel);
  auto handle = run.get();
  runs.emplace(std::make_pair(handle,std::move(run)));
  return handle;
}

void
xrtPSRunClose(xrtPSRunHandle rhdl)
{
  auto run = get_run(rhdl);
  runs.erase(run);
}

ert_cmd_state
xrtPSRunState(xrtPSRunHandle rhdl)
{
  auto run = get_run(rhdl);
  return run->state();
}

ert_cmd_state
xrtPSRunWait(xrtPSRunHandle rhdl, unsigned int timeout_ms)
{
  auto run = get_run(rhdl);
  return run->wait(timeout_ms * 1ms);
}

void
xrtPSRunSetCallback(xrtPSRunHandle rhdl, ert_cmd_state state,
                  void (* pfn_state_notify)(xrtPSRunHandle, ert_cmd_state, void*),
                  void* data)
{
  if (state != ERT_CMD_STATE_COMPLETED)
    throw xrt_core::error(-EINVAL, "xrtPSRunSetCallback state may only be ERT_CMD_STATE_COMPLETED");
  auto run = get_run(rhdl);
  run->add_callback([=](ert_cmd_state state) { pfn_state_notify(rhdl, state, data); });
}

void
xrtPSRunStart(xrtPSRunHandle rhdl)
{
  auto run = get_run(rhdl);
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
namespace xrt_core { namespace pskernel_int {

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
  auto cmd = std::make_shared<kernel_command>(dev);

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

xrt_core::pskernel::kernel_argument::argtype
arg_type_at_index(const xrt::pskernel& kernel, size_t argidx)
{
  auto& arg = kernel.get_handle()->get_arg(argidx);
  return arg.type();
}

void
set_arg_at_index(const xrt::psrun& run, size_t idx, const void* value, size_t bytes)
{
  const auto& rimpl = run.get_handle();
  auto& arg = rimpl->get_kernel()->get_arg(idx, true);
  rimpl->set_arg_value(arg, value, bytes);
}

xrt::psrun
clone(const xrt::psrun& psrun)
{
  return xrt::psrun{std::make_shared<xrt::psrun_impl>(psrun.get_handle().get())};
}

const std::bitset<max_cus>&
get_cumask(const xrt::psrun& psrun)
{
  return psrun.get_handle()->get_cumask();
}

void
pop_callback(const xrt::psrun& psrun)
{
  psrun.get_handle()->pop_callback();
}

std::vector<const xrt_core::pskernel::kernel_argument*>
get_args(const xrt::pskernel& kernel)
{
  const auto& args = kernel.get_handle()->get_args();
  std::vector<const xrt_core::pskernel::kernel_argument*> vec;
  for (const auto& arg : args)
    vec.push_back(&arg.get_xarg());
  return vec;
}

const xrt_core::pskernel::kernel_argument*
get_arg_info(const xrt::psrun& psrun, size_t argidx)
{
  auto& arg = psrun.get_handle()->get_kernel()->get_arg(argidx);
  return &arg.get_xarg();
}

std::vector<uint32_t>
get_arg_value(const xrt::psrun& psrun, size_t argidx)
{
  const auto& rimpl = psrun.get_handle();
  const auto kimpl = rimpl->get_kernel();

  // get argument info from pskernel and value from psrun
  const auto& arg = kimpl->get_arg(argidx);
  auto value = rimpl->get_arg_value(arg);
  std::vector<uint32_t> vec(value.size());
  std::copy_n(value.begin(), value.size(), vec.data());
  return vec;
}

}} // kernel_int, xrt_core


////////////////////////////////////////////////////////////////
// xrt_pskernel C++ API implmentations (xrt_pskernel.h)
////////////////////////////////////////////////////////////////
namespace xrt {

psrun::
psrun(const pskernel& krnl)
  : handle(xdp::native::profiling_wrapper
           ("xrt::psrun::psrun",alloc_run, krnl.get_handle()))
{}

void
psrun::
start()
{
  xdp::native::profiling_wrapper
    ("xrt::psrun::start", [this]{
    handle->start();
    });
}

ert_cmd_state
psrun::
wait(const std::chrono::milliseconds& timeout_ms) const
{
  return xdp::native::profiling_wrapper("xrt::psrun::wait",
    [this, &timeout_ms] {
      return handle->wait(timeout_ms);
    });
}

ert_cmd_state
psrun::
state() const
{
  return xdp::native::profiling_wrapper("xrt::psrun::state", [this]{
    return handle->state();
  });
}

void
psrun::
set_arg_at_index(int index, const void* value, size_t bytes)
{
  handle->set_arg_at_index(index, value, bytes);
}

void
psrun::
set_arg_at_index(int index, const xrt::bo& glb)
{
  handle->set_arg_at_index(index, glb);
}

void
psrun::
add_callback(ert_cmd_state state,
             std::function<void(const void*, ert_cmd_state, void*)> fcn,
             void* data)
{
  XRT_DEBUGF("psrun::add_callback psrun(%d)\n", handle->get_uid());
  if (state != ERT_CMD_STATE_COMPLETED)
    throw xrt_core::error(-EINVAL, "xrtPSRunSetCallback state may only be ERT_CMD_STATE_COMPLETED");
  // The function callback is passed a key that uniquely identifies
  // run objects referring to the same implmentation.  This allows
  // upstream to associate key with some run object that represents
  // the key. Note that the callback cannot pass *this (xrt::psrun) as
  // these objects are transient.
  auto key = handle.get();
  handle->add_callback([=](ert_cmd_state state) { fcn(key, state, data); });
}

void
psrun::
set_event(const std::shared_ptr<event_impl>& event) const
{
  xdp::native::profiling_wrapper("xrt::psrun::set_event", [this, &event]{
    handle->set_event(event);
  });
}

ert_packet*
psrun::
get_ert_packet() const
{
  return xdp::native::profiling_wrapper("xrt::psrun::get_ert_packet", [this]{
    return handle->get_ert_packet();
  });
}

pskernel::
pskernel(const xrt::device& xdev, const xrt::uuid& xclbin_id, const std::string& name, cu_access_mode mode)
  : handle(xdp::native::profiling_wrapper("xrt::pskernel::kernel",
	   alloc_kernel, get_device(xdev), xclbin_id, name, mode))
{}

pskernel::
pskernel(xclDeviceHandle dhdl, const xrt::uuid& xclbin_id, const std::string& name, cu_access_mode mode)
  : handle(xdp::native::profiling_wrapper("xrt::pskernel::kernel",
	   alloc_kernel, get_device(xrt_core::get_userpf_device(dhdl)), xclbin_id, name, mode))
{}

uint32_t
pskernel::
offset(int argno) const
{
  return xdp::native::profiling_wrapper("xrt::pskernel::offset", [this, argno]{
    return handle->arg_offset(argno);
  });
}

} // namespace xrt

////////////////////////////////////////////////////////////////
// xrt_pskernel API implmentations (xrt_pskernel.h)
////////////////////////////////////////////////////////////////
xrtPSKernelHandle
xrtPSKernelOpen(xrtDeviceHandle dhdl, const xuid_t xclbin_uuid, const char *name)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [dhdl, xclbin_uuid, name]{
      return api::xrtPSKernelOpen(dhdl, xclbin_uuid, name, psip_context::access_mode::shared);
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

xrtPSKernelHandle
xrtPSKernelOpenExclusive(xrtDeviceHandle dhdl, const xuid_t xclbin_uuid, const char *name)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [dhdl, xclbin_uuid, name]{
      return api::xrtPSKernelOpen(dhdl, xclbin_uuid, name, psip_context::access_mode::exclusive);
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
xrtPSKernelClose(xrtPSKernelHandle khdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [khdl]{
      api::xrtPSKernelClose(khdl);
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

xrtPSRunHandle
xrtPSRunOpen(xrtPSKernelHandle khdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [khdl]{
      return api::xrtPSRunOpen(khdl);
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
xrtPSKernelArgGroupId(xrtPSKernelHandle khdl, int argno)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [khdl, argno]{
      return get_kernel(khdl)->group_id(argno);
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
xrtPSKernelArgOffset(xrtPSKernelHandle khdl, int argno)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [khdl, argno]{
      return get_kernel(khdl)->arg_offset(argno);
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

xrtPSRunHandle
xrtPSKernelRun(xrtPSKernelHandle khdl, ...)
{
  try {
    std::va_list args;
    std::va_list* argptr = &args;
    va_start(args, khdl);  // NOLINT
    auto result = xdp::native::profiling_wrapper(__func__,
    [khdl, argptr]{
      auto handle = xrtPSRunOpen(khdl);
      auto psrun = get_run(handle);
      psrun->set_all_args(argptr);
      psrun->start();
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
xrtPSRunClose(xrtPSRunHandle rhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl]{
      api::xrtPSRunClose(rhdl);
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
xrtPSRunState(xrtPSRunHandle rhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl]{
      return api::xrtPSRunState(rhdl);
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
xrtPSRunWait(xrtPSRunHandle rhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl]{
      return api::xrtPSRunWait(rhdl, 0);
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
xrtPSRunWaitFor(xrtPSRunHandle rhdl, unsigned int timeout_ms)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl, timeout_ms]{
      return api::xrtPSRunWait(rhdl, timeout_ms);
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
xrtPSRunSetCallback(xrtPSRunHandle rhdl, ert_cmd_state state,
                  void (* pfn_state_notify)(xrtPSRunHandle, ert_cmd_state, void*),
                  void* data)
{
  try {
    return xdp::native::profiling_wrapper(__func__,
    [rhdl, state, pfn_state_notify, data]{
      api::xrtPSRunSetCallback(rhdl, state, pfn_state_notify, data);
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
xrtPSRunStart(xrtPSRunHandle rhdl)
{
  try {
    return xdp::native::profiling_wrapper(__func__, [rhdl]{
      api::xrtPSRunStart(rhdl);
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
