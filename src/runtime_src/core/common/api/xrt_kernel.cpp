/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
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

// This file implements XRT kernel APIs as declared in
// core/include/experimental/xrt_kernel.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_kernel.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_kernel.h"

#include "command.h"
#include "exec.h"
#include "bo.h"
#include "device_int.h"
#include "enqueue.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/xclbin_parser.h"
#include "core/common/config_reader.h"
#include "core/common/bo_cache.h"
#include "core/common/message.h"
#include "core/common/error.h"
#include "core/common/debug.h"
#include "core/include/xclbin.h"
#include "core/include/ert.h"
#include "core/include/ert_fa.h"
#include <memory>
#include <map>
#include <bitset>
#include <stdexcept>
#include <cstdarg>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
using namespace std::chrono_literals;

#include <boost/detail/endian.hpp>

#ifdef _WIN32
# pragma warning( disable : 4244 4267 4996 4100)
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

constexpr size_t operator"" _kb(unsigned long long v)  { return 1024u * v; }

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
  return !is_sw_emulation();
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

  device_type(xrtDeviceHandle dhdl)
    : core_device(xrt_core::device_int::get_core_device(dhdl))
    , exec_buffer_cache(core_device->get_device_handle(), 128)
  {}

  device_type(const std::shared_ptr<xrt_core::device>& cdev)
    : core_device(cdev)
    , exec_buffer_cache(core_device->get_device_handle(), 128)
  {}

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
public:
  using access_mode = xrt::kernel::cu_access_mode;
  constexpr static unsigned int virtual_cu_idx = std::numeric_limits<unsigned int>::max();

  static std::shared_ptr<ip_context>
  open(xrt_core::device* device, const xrt::uuid& xclbin_id, const ip_data* ip, unsigned int ipidx, access_mode am)
  {
    static std::vector<std::weak_ptr<ip_context>> ips(128);
    auto ipctx = ips[ipidx].lock();
    if (!ipctx) {
      ipctx = std::shared_ptr<ip_context>(new ip_context(device, xclbin_id.get(), ip, ipidx, am));
      ips[ipidx] = ipctx;
    }

    if (ipctx->access != am)
      throw std::runtime_error("Conflicting access mode for IP(" + std::to_string(ipidx) + ")");

    return ipctx;
  }

  static std::shared_ptr<ip_context>
  open_virtual_cu(xrt_core::device* device, const xrt::uuid& xclbin_id)
  {
    static std::weak_ptr<ip_context> vctx;
    auto ipctx = vctx.lock();
    if (!ipctx)
      vctx = ipctx = std::shared_ptr<ip_context>(new ip_context(device, xclbin_id));
    return ipctx;
  }

  // For symmetry
  void
  close()
  {}

  access_mode
  get_access_mode() const
  {
    return access;
  }

  size_t
  get_size() const
  {
    return size;
  }

  uint64_t
  get_address() const
  {
    return address;
  }

  unsigned int
  get_index() const
  {
    return idx;
  }

  ~ip_context()
  {
    device->close_context(xid.get(), idx);
  }

private:
  ip_context(xrt_core::device* dev, const xrt::uuid& xclbin_id, const ip_data* ip,
             unsigned int ipidx, access_mode am)
    : device(dev), xid(xclbin_id), idx(ipidx), address(ip->m_base_address), size(64_kb), access(am)
  {
    device->open_context(xid.get(), idx, std::underlying_type<access_mode>::type(am));
  }

  // virtual CU
  ip_context(xrt_core::device* dev, const xrt::uuid& xclbin_id)
    : device(dev), xid(xclbin_id), idx(virtual_cu_idx), address(0), size(0), access(access_mode::shared)
  {
    device->open_context(xid.get(), idx, std::underlying_type<access_mode>::type(access));
  }

  xrt_core::device* device;
  xrt::uuid xid;
  unsigned int idx;
  uint64_t address;
  size_t size;
  access_mode access;
};

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
  kernel_command(device_type* dev)
    : m_device(dev)
    , m_execbuf(m_device->create_exec_buf<ert_start_kernel_cmd>())
    , m_done(true)
  {
    static unsigned int count = 0;
    m_uid = count++;
    XRT_DEBUGF("kernel_command::kernel_command(%d)\n", m_uid);
  }

  ~kernel_command()
  {
    XRT_DEBUGF("kernel_command::~kernel_command(%d)\n", m_uid);
    // This is problematic, bo_cache should return managed BOs
    m_device->exec_buffer_cache.release(m_execbuf);
  }

  /**
   * Cast underlying exec buffer to its requested type
   */
  template <typename ERT_COMMAND_TYPE>
  const ERT_COMMAND_TYPE
  get_ert_cmd() const
  {
    return reinterpret_cast<const ERT_COMMAND_TYPE>(get_ert_packet());
  }

  /**
   * Cast underlying exec buffer to its requested type
   */
  template <typename ERT_COMMAND_TYPE>
  ERT_COMMAND_TYPE
  get_ert_cmd()
  {
    return reinterpret_cast<ERT_COMMAND_TYPE>(get_ert_packet());
  }

  /**
   * Add a callback, synchronize with concurrent state change
   * Call the callback if command is complete.
   */
  void
  add_callback(callback_function_type fcn)
  {
    bool complete = false;
    ert_cmd_state state;
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      if (!m_callbacks)
        m_callbacks = std::make_unique<callback_list>();
      m_callbacks->emplace_back(std::move(fcn));
      complete = m_done;
      auto pkt = get_ert_packet();
      state = static_cast<ert_cmd_state>(pkt->state);
      if (complete && state < ERT_CMD_STATE_COMPLETED)
        throw std::runtime_error("Unexpected state");
    }

    // lock must not be helt while calling callback function
    if (complete)
      m_callbacks.get()->back()(state);
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

  /**
   * Run registered callbacks.
   */
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

  /**
   * Submit the command for execution
   */
  void
  run()
  {
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      if (!m_done)
        throw std::runtime_error("bad command state, can't launch");
      m_done = false;
    }
    xrt_core::exec::schedule(this);
  }

  /**
   * Wait for command completion
   */
  ert_cmd_state
  wait() const
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (!m_done)
      m_exec_done.wait(lk);

    auto pkt = get_ert_packet();
    return static_cast<ert_cmd_state>(pkt->state);
  }

  ert_cmd_state
  wait(const std::chrono::milliseconds& timeout_ms) const
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (!m_done)
      m_exec_done.wait_for(lk, timeout_ms);

    auto pkt = get_ert_packet();
    return static_cast<ert_cmd_state>(pkt->state);
  }

  ////////////////////////////////////////////////////////////////
  // Implement xrt_core::command API
  ////////////////////////////////////////////////////////////////
  virtual ert_packet*
  get_ert_packet() const
  {
    return reinterpret_cast<ert_packet*>(m_execbuf.second);
  }

  virtual xrt_core::device*
  get_device() const
  {
    return m_device->get_core_device();
  }

  virtual xclBufferHandle
  get_exec_bo() const
  {
    return m_execbuf.first;
  }

  virtual void
  notify(ert_cmd_state s)
  {
    bool complete = false;
    if (s>=ERT_CMD_STATE_COMPLETED) {
      std::lock_guard<std::mutex> lk(m_mutex);
      XRT_DEBUGF("kernel_command::notify() m_uid(%d) m_state(%d)\n", m_uid, s);
      complete = m_done = true;
      if (m_event)
        xrt_core::enqueue::done(m_event.get());
      m_exec_done.notify_all();  // CAN THIS BE MOVED TO END AFTER CALLBACKS?
    }

    if (complete) {
      run_callbacks(s);

      // Clear the event if any.  This must be last since if used, it
      // holds the lifeline to this command object which could end up
      // being deleted when the event is cleared.
      m_event = nullptr;
    }
  }

private:
  device_type* m_device = nullptr;
  mutable std::shared_ptr<xrt::event_impl> m_event;
  execbuf_type m_execbuf; // underlying execution buffer
  unsigned int m_uid = 0;
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
  struct iarg
  {
    virtual ~iarg() {}
    virtual std::vector<uint32_t>
    get_value(std::va_list*) const = 0;
  };

  template <typename HostType, typename VaArgType>
  struct scalar_type : iarg
  {
    size_t size;  // size in bytes of argument per xclbin

    scalar_type(size_t bytes)
      : size(bytes)
    {
      // assert(bytes <= sizeof(VaArgType)
    }

    virtual std::vector<uint32_t>
    get_value(std::va_list* args) const
    {
      static_assert(BOOST_BYTE_ORDER==1234,"Big endian detected");

      HostType value = va_arg(*args, VaArgType);
      return value_to_uint32_vector(value);
    }
  };

  template <typename HostType, typename VaArgType>
  struct scalar_type<HostType*, VaArgType*> : iarg
  {
    size_t size;  // size in bytes of argument per xclbin

    scalar_type(size_t bytes)
      : size(bytes)
    {
      // assert(bytes <= sizeof(VaArgType)
    }

    virtual std::vector<uint32_t>
    get_value(std::va_list* args) const
    {
      static_assert(BOOST_BYTE_ORDER==1234,"Big endian detected");

      HostType* value = va_arg(*args, VaArgType*);
      return value_to_uint32_vector(value, size);
    }
  };

  struct global_type : iarg
  {
    xrt_core::device* core_device;
    size_t size;   // size in bytes of argument per xclbin

    global_type(xrt_core::device* dev, size_t bytes)
      : core_device(dev)
      , size(bytes)
    {
      // assert(bytes == 8)
    }

    virtual std::vector<uint32_t>
    get_value(std::va_list* args) const
    {
      static_assert(BOOST_BYTE_ORDER==1234,"Big endian detected");
      if (xrt_core::config::get_xrt_bo()) {
        auto bo = va_arg(*args, xrtBufferHandle);
        return value_to_uint32_vector(xrt_core::bo::address(bo));
      }
      else {
        // old style buffer handles
        auto bo = va_arg(*args, xclBufferHandle);
        xclBOProperties prop;
        core_device->get_bo_properties(bo, &prop);
        return value_to_uint32_vector(prop.paddr);
      }
    }
  };

  struct null_type : iarg
  {
    virtual std::vector<uint32_t>
    get_value(std::va_list* args) const
    {
      (void) va_arg(*args, void*); // swallow unsettable argument
      return std::vector<uint32_t>(); // empty
    }
  };

  using xarg = xrt_core::xclbin::kernel_argument;
  xarg arg;       // argument meta data from xclbin
  int32_t grpid;  // memory bank group id

  std::unique_ptr<iarg> content;

public:
  static constexpr size_t no_index = xarg::no_index;
  using direction = xarg::direction;

  argument()
    : grpid(std::numeric_limits<int32_t>::max()), content(nullptr)
  {}

  argument(argument&& rhs)
    : arg(std::move(rhs.arg)), grpid(rhs.grpid), content(std::move(rhs.content))
  {}

  argument(xrt_core::device* dev, xarg&& karg, int32_t grp)
    : arg(std::move(karg)), grpid(grp)
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
      else if (arg.size == 4)
        content = std::make_unique<scalar_type<uint32_t,uint32_t>>(arg.size);
      else if (arg.size == 8)
        content = std::make_unique<scalar_type<uint64_t,uint64_t>>(arg.size);
      else
        // throw xrt_core::error(-EINVAL, "Unknown scalar argument type '" + arg.hosttype + "'");
        // arg.hosttype is free formed, default to size_t until clarified
        content = std::make_unique<scalar_type<size_t,size_t>>(arg.size);
      break;
    }
    case xarg::argtype::global :
      content = std::make_unique<global_type>(dev, arg.size);
      break;
    case xarg::argtype::stream :
      content = std::make_unique<null_type>();
      break;
    default:
      throw std::runtime_error("Unexpected error");
    }
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

  int32_t
  group_id() const
  { return grpid; }

  direction
  dir() const
  { return arg.dir; }

  bool
  is_input() const
  { return arg.dir == direction::input; }

  bool
  is_output() const
  { return arg.dir == direction::output; }
};

} // namespace

namespace xrt {

// struct kernel_type - The internals of an xrtKernelHandle
//
// An single object of kernel_type can be shared with multiple
// run handles.   The kernel object defines all kernel specific
// meta data used to create a launch a run object (command)
class kernel_impl
{
  using ipctx = std::shared_ptr<ip_context>;

  std::shared_ptr<device_type> device; // shared ownership
  std::string name;                    // kernel name
  std::vector<int32_t> arg2grp;        // argidx to memory group index
  std::vector<argument> args;          // kernel args sorted by argument index
  std::vector<ipctx> ipctxs;           // CU context locks
  ipctx vctx;                          // virtual CU context
  std::bitset<128> cumask;             // cumask for command execution
  size_t regmap_size = 0;              // CU register map size
  size_t fa_num_inputs = 0;            // Fast adapter number of inputs per meta data
  size_t fa_num_outputs = 0;           // Fast adapter number of outputs per meta data
  size_t fa_input_entry_bytes = 0;     // Fast adapter input desc bytes
  size_t fa_output_entry_bytes = 0;    // Fast adapter output desc bytes
  size_t num_cumasks = 1;              // Required number of command cu masks
  uint32_t protocol = 0;               // Default opcode

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
  amend_args()
  {
    if (protocol == FAST_ADAPTER)
      amend_fa_args();
  }

  // Traverse xclbin connectivity section and find
  int32_t
  get_arg_grpid(const connectivity* cons, int32_t argidx, int32_t ipidx)
  {
    for (int32_t count=0; cons && count <cons->m_count; ++count) {
      auto& con = cons->m_connection[count];
      if (con.m_ip_layout_index != ipidx)
        continue;
      if (con.arg_index != argidx)
        continue;
      return con.mem_data_index;
    }
    return std::numeric_limits<int32_t>::max();
  }

  int32_t
  get_arg_grpid(const connectivity* cons, int32_t argidx, const std::vector<int32_t>& ips)
  {
    auto grpidx = std::numeric_limits<int32_t>::max();
    for (auto ipidx : ips) {
      auto gidx = get_arg_grpid(cons, argidx, ipidx);
      if (gidx != grpidx && grpidx != std::numeric_limits<int32_t>::max())
        throw std::runtime_error("Ambigious kernel connectivity for argument " + std::to_string(argidx));
      grpidx = gidx;
    }
    return grpidx;
  }

  unsigned int
  get_ipidx_or_error(size_t offset, bool force=false) const
  {
    if (ipctxs.size() != 1)
      throw std::runtime_error("Cannot read or write kernel with multiple compute units");
    auto& ipctx = ipctxs.back();
    auto mode = ipctx->get_access_mode();
    if (!force && mode != ip_context::access_mode::exclusive)
      throw std::runtime_error("Cannot read or write kernel with shared access");

    if ((offset + sizeof(uint32_t)) > ipctx->get_size())
        throw std::out_of_range("Cannot read or write outside kernel register space");

    return ipctx->get_index();
  }

  IP_CONTROL
  get_ip_control(const std::vector<const ip_data*>& ips)
  {
    // assert ( ips.size() >= 1);
    auto ctrl = IP_CONTROL((ips[0]->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT);
    for (size_t idx = 1; idx < ips.size(); ++idx)
      if (IP_CONTROL((ips[0]->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT) != ctrl)
        throw std::runtime_error("CU control protocol mismatch");

    return ctrl;
  }

  void
  encode_compute_units(kernel_command* cmd)
  {
    auto ecmd = cmd->get_ert_cmd<ert_packet*>();
    std::fill(ecmd->data, ecmd->data + num_cumasks, 0);

    for (size_t cu_idx = 0; cu_idx < 128; ++cu_idx) {
      if (!cumask.test(cu_idx))
        continue;
      auto mask_idx = cu_idx / 32;
      auto idx_in_mask = cu_idx - mask_idx * 32;
      ecmd->data[mask_idx] |= (1 << idx_in_mask);
    }
  }

  void
  initialize_command_header(ert_start_kernel_cmd* kcmd)
  {
    kcmd->extra_cu_masks = num_cumasks - 1;  //  -1 for mandatory mask
    kcmd->count = num_cumasks + regmap_size;
    kcmd->opcode = (protocol == FAST_ADAPTER) ? ERT_START_FA : ERT_START_CU;
    kcmd->type = ERT_CU;
  }

  void
  initialize_fadesc(uint32_t* data)
  {
    auto desc = reinterpret_cast<ert_fa_descriptor*>(data);
    desc->status = ERT_FA_UNDEFINED;
    desc->num_input_entries = fa_num_inputs;
    desc->input_entry_bytes = fa_input_entry_bytes;
    desc->num_output_entries = fa_num_outputs;
    desc->output_entry_bytes = fa_output_entry_bytes;
  }


public:
  // kernel_type - constructor
  //
  // @dev:     device associated with this kernel object
  // @uuid:    uuid of xclbin to mine for kernel meta data
  // @nm:      name identifying kernel and/or kernel and instances
  // @am:      access mode for underlying compute units
  kernel_impl(std::shared_ptr<device_type> dev, const xrt::uuid& xclbin_id, const std::string& nm, ip_context::access_mode am)
    : device(std::move(dev))                                   // share ownership
    , name(nm.substr(0,nm.find(":")))                          // filter instance names
    , vctx(ip_context::open_virtual_cu(device->core_device.get(), xclbin_id))
  {
    // ip_layout section for collecting CUs
    auto ip_section = device->core_device->get_axlf_section(IP_LAYOUT, xclbin_id);
    if (!ip_section.first)
      throw std::runtime_error("No ip layout available to construct kernel, make sure xclbin is loaded");
    auto ip_layout = reinterpret_cast<const ::ip_layout*>(ip_section.first);

    // connectivity section for CU memory connectivity, permissible for section to not exist
    auto connectivity_section = device->core_device->get_axlf_section(ASK_GROUP_CONNECTIVITY, xclbin_id);
    auto connectivity = reinterpret_cast<const ::connectivity*>(connectivity_section.first);

    // xml section for kernel arguments
    auto xml_section = device->core_device->get_axlf_section(EMBEDDED_METADATA, xclbin_id);
    if (!xml_section.first)
      throw std::runtime_error("No xml metadata available to construct kernel, make sure xclbin is loaded");

    // Compare the matching CUs against the CU sort order to create cumask
    auto ips = xrt_core::xclbin::get_cus(ip_layout, nm);
    if (ips.empty())
      throw std::runtime_error("No compute units matching '" + nm + "'");

    auto cus = xrt_core::xclbin::get_cus(ip_layout);  // sort order
    for (const ip_data* cu : ips) {
      auto itr = std::find(cus.begin(), cus.end(), cu->m_base_address);
      if (itr == cus.end())
        throw std::runtime_error("unexpected error");
      auto idx = std::distance(cus.begin(), itr);
      ipctxs.emplace_back(ip_context::open(device->get_core_device(), xclbin_id, cu, idx, am));
      cumask.set(idx);
      num_cumasks = std::max<size_t>(num_cumasks, (idx / 32) + 1);
    }

    // set kernel protocol
    protocol = get_ip_control(ips);

    // Collect ip_layout index of the selected CUs so that xclbin
    // connectivity section can be used to gather memory group index
    // for each kernel argument.
    std::vector<int32_t> ip2idx(ips.size());
    std::transform(ips.begin(), ips.end(), ip2idx.begin(),
        [ip_layout](auto& ip) { return std::distance(ip_layout->m_ip_data, ip); });

    // get kernel arguments from xml parser
    // compute regmap size, convert to typed argument
    for (auto& arg : xrt_core::xclbin::get_kernel_arguments(xml_section.first, xml_section.second, name)) {
      regmap_size = std::max(regmap_size, (arg.offset + arg.size) / 4);
      args.emplace_back(device->get_core_device(), std::move(arg), get_arg_grpid(connectivity, arg.index, ip2idx));
    }

    // amend args with computed data based on kernel protocol
    amend_args();
  }

  // Initialize kernel command and return pointer to payload
  // after mandatory static data.
  uint32_t*
  initialize_command(kernel_command* cmd)
  {
    auto kcmd = cmd->get_ert_cmd<ert_start_kernel_cmd*>();
    initialize_command_header(kcmd);
    encode_compute_units(cmd);
    auto data = kcmd->data + kcmd->extra_cu_masks;

    if (kcmd->opcode == ERT_START_FA)
      initialize_fadesc(data);

    return data;
  }

  IP_CONTROL
  get_ip_control_protocol() const
  {
    return IP_CONTROL(protocol);
  }

  int
  group_id(int argno)
  {
    return args.at(argno).group_id();
  }

  uint32_t
  read_register(uint32_t offset, bool force=false) const
  {
    auto idx = get_ipidx_or_error(offset, force);
    uint32_t value = 0;
    if (has_reg_read_write())
      device->core_device->reg_read(idx, offset, &value);
    else
      device->core_device->xread(ipctxs.back()->get_address() + offset, &value, 4);
    return value;
  }

  void
  write_register(uint32_t offset, uint32_t data)
  {
    auto idx = get_ipidx_or_error(offset);
    if (has_reg_read_write())
      device->core_device->reg_write(idx, offset, data);
    else
      device->core_device->xwrite(ipctxs.back()->get_address() + offset, &data, 4);
  }

  // Read 'count' 4 byte registers starting at offset
  // This API is internal and allows reading from shared IPs
  void
  read_register_n(uint32_t offset, size_t count, uint32_t* out)
  {
    for (size_t n = 0; n < count; ++n)
      out[n] = read_register(offset + n * 4, true);
  }

  device_type*
  get_device() const
  {
    return device.get();
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
  get_arg(size_t argidx) const
  {
    return args.at(argidx);
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
  // Helper hierarchy to set argument value per control protocol type
  // The @data member is the payload to be populated with argument
  // value.  The interpretation of the payload depends on the control
  // protocol.
  struct arg_setter
  {
    uint32_t* data;

    arg_setter(uint32_t* d)
      : data(d)
    {}

    virtual void
    set_arg_value(const argument& arg, const std::vector<uint32_t>& value) = 0;
  };

  // AP_CTRL_HS, AP_CTRL_CHAIN
  struct hs_arg_setter : arg_setter
  {
    hs_arg_setter(uint32_t* data)
      : arg_setter(data)
    {}

    virtual void
    set_arg_value(const argument& arg, const std::vector<uint32_t>& value)
    {
      auto cmdidx = arg.offset() / 4;
      auto count = std::min<size_t>(arg.size() / sizeof(uint32_t), value.size());
      std::copy_n(value.begin(), count, data + cmdidx);
    }
  };

  // FAST_ADAPTER
  struct fa_arg_setter : arg_setter
  {
    fa_arg_setter(uint32_t* data)
      : arg_setter(data)
    {}

    virtual void
    set_arg_value(const argument& arg, const std::vector<uint32_t>& value)
    {
      auto desc = reinterpret_cast<ert_fa_descriptor*>(data);
      auto desc_entry = reinterpret_cast<ert_fa_desc_entry*>(desc->data + arg.fa_desc_offset() / sizeof(uint32_t));
      desc_entry->arg_offset = arg.offset();
      desc_entry->arg_size = arg.size();
      auto count = std::min<size_t>(arg.size() / sizeof(uint32_t), value.size());
      std::copy_n(value.begin(), count, desc_entry->arg_value);
    }
  };

  std::unique_ptr<arg_setter>
  make_arg_setter()
  {
    if (kernel->get_ip_control_protocol() == FAST_ADAPTER)
      return std::make_unique<fa_arg_setter>(data);
    else
      return std::make_unique<hs_arg_setter>(data);
  }
  
  using callback_function_type = std::function<void(ert_cmd_state)>;
  std::shared_ptr<kernel_impl> kernel;    // shared ownership
  xrt_core::device* core_device;          // convenience, in scope of kernel
  std::shared_ptr<kernel_command> cmd;    // underlying command object
  uint32_t* data;                         // command argument data payload @0x0
  std::unique_ptr<arg_setter> arg_setter; // helper to populate payload data

public:
  void
  add_callback(callback_function_type fcn)
  {
    cmd->add_callback(fcn);
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
  run_impl(std::shared_ptr<kernel_impl> k)
    : kernel(std::move(k))                   // share ownership
    , core_device(kernel->get_core_device()) // cache core device
    , cmd(std::make_shared<kernel_command>(kernel->get_device()))
    , data(kernel->initialize_command(cmd.get()))
    , arg_setter(make_arg_setter())
  {}

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

  void
  set_arg_value(const argument& arg, const std::vector<uint32_t>& value)
  {
    arg_setter->set_arg_value(arg, value);
  }

  void
  set_arg(const argument& arg, std::va_list* args)
  {
    auto value = arg.get_value(args);
    set_arg_value(arg, value);
  }

  void
  set_arg_at_index(size_t index, const std::vector<uint32_t>& value)
  {
    auto& arg = kernel->get_arg(index);
    set_arg_value(arg, value);
  }

  void
  set_arg_at_index(size_t index, const xrt::bo& bo)
  {
    auto value = xrt_core::bo::address(bo);
    set_arg_at_index(index, value_to_uint32_vector(value));
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
    arg.valid_or_error(bytes);
    set_arg_value(arg, value_to_uint32_vector(value, bytes));
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

  // start() - start the run object (execbuf)
  void
  start()
  {
    auto pkt = cmd->get_ert_packet();
    pkt->state = ERT_CMD_STATE_NEW;
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
  run_update_type(run_impl* r)
    : run(r)
    , kernel(run->get_kernel())
    , cmd(std::make_shared<kernel_command>(kernel->get_device()))
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
  update_arg_value(const argument& arg, const std::vector<uint32_t>& value)
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
    cmd->run();
    cmd->wait();
  }

  void
  update_arg_at_index(size_t index, std::va_list* args)
  {
    auto& arg = kernel->get_arg(index);
    arg.valid_or_error();
    update_arg_value(arg, arg.get_value(args));
  }

  void
  update_arg_at_index(size_t index, const std::vector<uint32_t>& value)
  {
    auto& arg = kernel->get_arg(index);
    arg.valid_or_error();
    update_arg_value(arg, value);
  }

  void
  update_arg_at_index(size_t index, const xrt::bo& glb)
  {
    auto& arg = kernel->get_arg(index);
    auto value = xrt_core::bo::address(glb);
    arg.valid_or_error(sizeof(value));
    update_arg_value(arg, value_to_uint32_vector(value));
  }

  void
  update_arg_at_index(size_t index, const void* value, size_t bytes)
  {
    auto& arg = kernel->get_arg(index);
    if (arg.index() == argument::no_index)
      throw std::runtime_error("Bad argument index '" + std::to_string(index) + "'");
    if (bytes != arg.size())
      throw std::runtime_error("Bad argument size '" + std::to_string(bytes) + "'");
    update_arg_value(arg, value_to_uint32_vector(value, bytes));
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

// Active kernels per xrtKernelOpen/Close.  This is a mapping from
// xrtKernelHandle to the corresponding kernel object.  The
// xrtKernelHandle is the address of the kernel object.  This is
// shared ownership as application can close a kernel handle before
// closing an xrtRunHandle that references same kernel.
static std::map<void*, std::shared_ptr<xrt::kernel_impl>> kernels;

// Active runs.  This is a mapping from xrtRunHandle to corresponding
// run object.  The xrtRunHandle is the address of the run object.
// This is unique ownership as only the host application holds on to a
// run object, e.g. the run object is desctructed immediately when it
// is closed.
static std::map<void*, std::unique_ptr<xrt::run_impl>> runs;

// Run updates, if used are tied to existing runs and removed
// when run is closed.
static std::map<const xrt::run_impl*, std::unique_ptr<xrt::run_update_type>> run_updates;

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

// get_kernel() - get a kernel object from an xrtKernelHandle
//
// The lifetime of a kernel object is shared ownerhip. The object
// is shared with host application and run objects.
static std::shared_ptr<xrt::kernel_impl>
get_kernel(xrtKernelHandle khdl)
{
  auto itr = kernels.find(khdl);
  if (itr == kernels.end())
    throw xrt_core::error(-EINVAL, "Unknown kernel handle");
  return (*itr).second;
}

// get_run() - get a run object from an xrtRunHandle
//
// The lifetime of a run object is unique to the host application.
static xrt::run_impl*
get_run(xrtRunHandle rhdl)
{
  auto itr = runs.find(rhdl);
  if (itr == runs.end())
    throw xrt_core::error(-EINVAL, "Unknown run handle");
  return (*itr).second.get();
}

static xrt::run_update_type*
get_run_update(xrt::run_impl* run)
{
  auto itr = run_updates.find(run);
  if (itr == run_updates.end()) {
    auto ret = run_updates.emplace(std::make_pair(run,std::make_unique<xrt::run_update_type>(run)));
    itr = ret.first;
  }
  return (*itr).second.get();
}

static xrt::run_update_type*
get_run_update(xrtRunHandle rhdl)
{
  auto run = get_run(rhdl);
  return get_run_update(run);
}

////////////////////////////////////////////////////////////////
// Implementation helper for C API
////////////////////////////////////////////////////////////////
namespace api {

xrtKernelHandle
xrtKernelOpen(xrtDeviceHandle dhdl, const xuid_t xclbin_uuid, const char *name, ip_context::access_mode am)
{
  auto device = get_device(dhdl);
  auto kernel = std::make_shared<xrt::kernel_impl>(device, xclbin_uuid, name, am);
  auto handle = kernel.get();
  kernels.emplace(std::make_pair(handle,std::move(kernel)));
  return handle;
}

void
xrtKernelClose(xrtKernelHandle khdl)
{
  auto itr = kernels.find(khdl);
  if (itr == kernels.end())
    throw xrt_core::error(-EINVAL, "Unknown kernel handle");
  kernels.erase(itr);
}

xrtRunHandle
xrtRunOpen(xrtKernelHandle khdl)
{
  auto kernel = get_kernel(khdl);
  auto run = std::make_unique<xrt::run_impl>(kernel);
  auto handle = run.get();
  runs.emplace(std::make_pair(handle,std::move(run)));
  return handle;
}

void
xrtRunClose(xrtRunHandle rhdl)
{
  auto run = get_run(rhdl);
  {
    auto itr = run_updates.find(run);
    if (itr != run_updates.end())
      run_updates.erase(itr);
  }
  runs.erase(run);
}

ert_cmd_state
xrtRunState(xrtRunHandle rhdl)
{
  auto run = get_run(rhdl);
  return run->state();
}

ert_cmd_state
xrtRunWait(xrtRunHandle rhdl, unsigned int timeout_ms)
{
  auto run = get_run(rhdl);
  return run->wait(timeout_ms * 1ms);
}

void
xrtRunSetCallback(xrtRunHandle rhdl, ert_cmd_state state,
                  void (* pfn_state_notify)(xrtRunHandle, ert_cmd_state, void*),
                  void* data)
{
  if (state != ERT_CMD_STATE_COMPLETED)
    throw xrt_core::error(-EINVAL, "xrtRunSetCallback state may only be ERT_CMD_STATE_COMPLETED");
  auto run = get_run(rhdl);
  run->add_callback([=](ert_cmd_state state) { pfn_state_notify(rhdl, state, data); });
}

void
xrtRunStart(xrtRunHandle rhdl)
{
  auto run = get_run(rhdl);
  run->start();
}

} // api

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", msg);
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
  // Construct a kernel command to copy bo.  Kernel commands
  // must be shared ptrs
  auto dev = get_device(core_device);
  auto cmd = std::make_shared<kernel_command>(dev.get());

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

}} // kernel_int, xrt_core


////////////////////////////////////////////////////////////////
// xrt_kernel C++ API implmentations (xrt_kernel.h)
////////////////////////////////////////////////////////////////
namespace xrt {

run::
run(const kernel& krnl)
  : handle(std::make_shared<run_impl>(krnl.get_handle()))
{}

void
run::
start()
{
  handle->start();
}

ert_cmd_state
run::
wait(const std::chrono::milliseconds& timeout_ms) const
{
  return handle->wait(timeout_ms);
}

ert_cmd_state
run::
state() const
{
  return handle->state();
}

void
run::
set_arg_at_index(int index, const std::vector<uint32_t>& value)
{
  handle->set_arg_at_index(index, value);
}

void
run::
set_arg_at_index(int index, const xrt::bo& glb)
{
  handle->set_arg_at_index(index, glb);
}

void
run::
update_arg_at_index(int index, const std::vector<uint32_t>& value)
{
  auto upd = get_run_update(handle.get());
  upd->update_arg_at_index(index, value);
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
             std::function<void(const run&, ert_cmd_state, void*)> fcn,
             void* data)
{
  if (state != ERT_CMD_STATE_COMPLETED)
    throw xrt_core::error(-EINVAL, "xrtRunSetCallback state may only be ERT_CMD_STATE_COMPLETED");
  handle->add_callback([=](ert_cmd_state state) { fcn(*this, state, data); });
}

void
run::
set_event(const std::shared_ptr<event_impl>& event) const
{
  handle->set_event(event);
}

kernel::
kernel(const xrt::device& xdev, const xrt::uuid& xclbin_id, const std::string& name, cu_access_mode mode)
  : handle(std::make_shared<kernel_impl>
      (get_device(xdev), xclbin_id, name, mode))
{}

kernel::
kernel(xclDeviceHandle dhdl, const xrt::uuid& xclbin_id, const std::string& name, cu_access_mode mode)
  : handle(std::make_shared<kernel_impl>
      (get_device(xrt_core::get_userpf_device(dhdl)), xclbin_id, name, mode))
{}

uint32_t
kernel::
read_register(uint32_t offset) const
{
  return handle->read_register(offset);
}

void
kernel::
write_register(uint32_t offset, uint32_t data)
{
  handle->write_register(offset, data);
}


int
kernel::
group_id(int argno) const
{
  return handle->group_id(argno);
}

} // namespace xrt

////////////////////////////////////////////////////////////////
// xrt_kernel API implmentations (xrt_kernel.h)
////////////////////////////////////////////////////////////////
xrtKernelHandle
xrtPLKernelOpen(xrtDeviceHandle dhdl, const xuid_t xclbin_uuid, const char *name)
{
  try {
    return api::xrtKernelOpen(dhdl, xclbin_uuid, name, ip_context::access_mode::shared);
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

xrtKernelHandle
xrtPLKernelOpenExclusive(xrtDeviceHandle dhdl, const xuid_t xclbin_uuid, const char *name)
{
  try {
    return api::xrtKernelOpen(dhdl, xclbin_uuid, name, ip_context::access_mode::exclusive);
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

int
xrtKernelClose(xrtKernelHandle khdl)
{
  try {
    api::xrtKernelClose(khdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

xrtRunHandle
xrtRunOpen(xrtKernelHandle khdl)
{
  try {
    return api::xrtRunOpen(khdl);
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

int
xrtKernelArgGroupId(xrtKernelHandle khdl, int argno)
{
  try {
    auto kernel = get_kernel(khdl);
    return kernel->group_id(argno);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtKernelReadRegister(xrtKernelHandle khdl, uint32_t offset, uint32_t* datap)
{
  try {
    auto kernel = get_kernel(khdl);
    *datap = kernel->read_register(offset);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }

}

int
xrtKernelWriteRegister(xrtKernelHandle khdl, uint32_t offset, uint32_t data)
{
  try {
    auto kernel = get_kernel(khdl);
    kernel->write_register(offset, data);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

xrtRunHandle
xrtKernelRun(xrtKernelHandle khdl, ...)
{
  try {
    auto handle = xrtRunOpen(khdl);
    auto run = get_run(handle);

    std::va_list args;
    va_start(args, khdl);
    run->set_all_args(&args);
    va_end(args);

    run->start();

    return handle;
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

int
xrtRunClose(xrtRunHandle rhdl)
{
  try {
    api::xrtRunClose(rhdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

ert_cmd_state
xrtRunState(xrtRunHandle rhdl)
{
  try {
    return api::xrtRunState(rhdl);
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
    return api::xrtRunWait(rhdl, 0);
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return ERT_CMD_STATE_ABORT;
  }
}

ert_cmd_state
xrtRunWaitFor(xrtRunHandle rhdl, unsigned int timeout_ms)
{
  try {
    return api::xrtRunWait(rhdl, timeout_ms);
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return ERT_CMD_STATE_ABORT;
  }
}

int
xrtRunSetCallback(xrtRunHandle rhdl, ert_cmd_state state,
                  void (* pfn_state_notify)(xrtRunHandle, ert_cmd_state, void*),
                  void* data)
{
  try {
    api::xrtRunSetCallback(rhdl, state, pfn_state_notify, data);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtRunStart(xrtRunHandle rhdl)
{
  try {
    api::xrtRunStart(rhdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtRunUpdateArg(xrtRunHandle rhdl, int index, ...)
{
  try {
    auto upd = get_run_update(rhdl);

    std::va_list args;
    va_start(args, index);
    upd->update_arg_at_index(index, &args);
    va_end(args);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtRunUpdateArgV(xrtRunHandle rhdl, int index, const void* value, size_t bytes)
{
  try {
    auto upd = get_run_update(rhdl);
    upd->update_arg_at_index(index, value, bytes);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtRunSetArg(xrtRunHandle rhdl, int index, ...)
{
  try {
    auto run = get_run(rhdl);

    std::va_list args;
    va_start(args, index);
    run->set_arg_at_index(index, &args);
    va_end(args);

    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtRunSetArgV(xrtRunHandle rhdl, int index, const void* value, size_t bytes)
{
  try {
    auto run = get_run(rhdl);
    run->set_arg_at_index(index, value, bytes);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtRunGetArgV(xrtRunHandle rhdl, int index, void* value, size_t bytes)
{
  try {
    auto run = get_run(rhdl);
    run->get_arg_at_index(index, static_cast<uint32_t*>(value), bytes);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }

}

void
xrtRunGetArgVPP(xrt::run run, int index, void* value, size_t bytes)
{
  auto rimpl = run.get_handle();
  rimpl->get_arg_at_index(index, static_cast<uint32_t*>(value), bytes);
}
