// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

// This file implements a dummy (no-op) shim level driver that is
// used exclusively for debugging user space XRT with HW xclbins
//
// The code is not beautified in any way or fashion, it is meant
// for quick and dirty validation of user space code changes.
#define XCL_DRIVER_DLL_EXPORT
#define XRT_CORE_PCIE_NOOP_SOURCE
#include "shim.h"                  // This file implements shim.h
#include "core/include/shim_int.h" // This file implements shim_int.h
#include "core/include/xrt/detail/ert.h"

#include "core/common/config_reader.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/task.h"
#include "core/common/thread.h"
#include "core/common/shim/buffer_handle.h"
#include "core/common/shim/hwctx_handle.h"

#include "core/common/api/hw_context_int.h"

#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>

namespace { // private implementation details

using device_index_type = unsigned int;

namespace buffer {

struct bo
{
  void* hbuf = nullptr;
  void* own = nullptr;
  void* dbuf = reinterpret_cast<void*>(0xdeadbeef);
  size_t size = 0;
  unsigned int flags = 0;

  bo(void* uptr, size_t bytes, unsigned int flgs)
    : hbuf(uptr), size(bytes), flags(flgs)
  {}

  bo(size_t bytes, unsigned int flgs)
    : hbuf(malloc(bytes)), own(hbuf), size(bytes), flags(flgs)
  {
    std::memset(hbuf, 0, size);
  }

  ~bo()
  { if (own) free(own); }
};

static std::mutex mutex;
static unsigned int handle = 0;
static std::map<unsigned int, std::unique_ptr<bo>> h2b;

auto
find(unsigned int handle)
{
  std::lock_guard<std::mutex> lk(mutex);
  auto itr = h2b.find(handle);
  if (itr == h2b.end())
    throw std::runtime_error("no such bo handle: " + std::to_string(handle));
  return itr;
}

bo*
get(unsigned int handle)
{
  auto itr = find(handle);
  return (*itr).second.get();
}

unsigned int
alloc(size_t size, unsigned int flags)
{
  std::lock_guard<std::mutex> lk(mutex);
  h2b.insert(std::make_pair(handle, std::make_unique<bo>(size, flags)));
  return handle++;
}

unsigned int
alloc(void* uptr, size_t size, unsigned int flags)
{
  std::lock_guard<std::mutex> lk(mutex);
  h2b.insert(std::make_pair(handle, std::make_unique<bo>(uptr, size, flags)));
  return handle++;
}

void*
map(unsigned int handle)
{
  auto bo = get(handle);
  return bo->hbuf;
}

void
free(unsigned int handle)
{
  auto itr = find(handle);
  h2b.erase(itr);
}

} // buffer

// Model the programming part of the device.
// Pretend a device that supports multiple xclbins, which can can be
// loaded by host application into different slots, where slots are
// chosen by the driver (this code).
namespace pl {


class device
{
  // model multiple partitions, but for simplicity slot is
  // used for context handle also.
  using slot_id = xrt_core::hwctx_handle::slot_id;

  // registered xclbins
  std::map<xrt::uuid, xrt::xclbin> m_xclbins;

  // mapped xclbins (assigned resources)
  std::map<slot_id, xrt::xclbin> m_slots;

  // capture cu data based on which slot it is associated with
  struct cu_data {
    std::string name;  // cu name
    slot_id slot = 0u; // slot in which this cu is opened
    uint32_t ctx = 0;  // how many contexts are opened on the cu
  };
  std::map<uint32_t, cu_data> m_idx2cu;  // idx -> cu_data

  // cu indices are per device from 0..127.
  // keep a stack of indices that can be used
  // once last context on cu is released, its index is recycled
  static constexpr uint32_t cu_max = 128;
  std::vector<uint32_t> m_free_cu_indices;  // push, back, pop

  // slot index for xclbin is a running incremented index
  uint32_t m_slot_index = 0; // running index

  // exclusive locking to prevent race
  std::mutex m_mutex;

public:
  // device ctor, initialize free cu indices
  device()
  {
    m_free_cu_indices.reserve(128);
    for (int i = 127; i >=0; --i)
      m_free_cu_indices.push_back(i);
  }

  void
  register_xclbin(const xrt::xclbin& xclbin)
  {
    std::lock_guard lk(m_mutex);
    m_xclbins[xclbin.get_uuid()] = xclbin;
  }

  slot_id
  create_hw_context(const xrt::uuid& xid)
  {
    std::lock_guard lk(m_mutex);
    auto itr = m_xclbins.find(xid);
    if (itr == m_xclbins.end())
      throw xrt_core::error("xclbin must be registered before hw context can be created");
    m_slots[++m_slot_index] = (*itr).second;
    return m_slot_index;  // for simplicity we use the slot_id as the context handle
  }

  void
  destroy_hw_context(slot_id slot)
  {
    std::lock_guard lk(m_mutex);
    m_slots.erase(slot);   // for simplicity context handle is same as slot
  }

  xrt_core::cuidx_type
  open_cu_context(slot_id slot, const xrt::uuid& xid, const std::string& cuname)
  {
    std::lock_guard lk(m_mutex);

    // this xclbin must have been registered
    const auto& xclbin = m_slots[slot];
    if (!xclbin)
      throw xrt_core::error("Slot xclbin mismatch, no such registered xclbin in slot: " + std::to_string(slot));

    if (xclbin.get_uuid() != xid)
      throw xrt_core::error("Slot xclbin uuid mismatch in slot: " + std::to_string(slot));

    // find cu in xclbin
    auto cu = xclbin.get_ip(cuname);
    if (!cu)
      throw xrt_core::error("No such cu: " + cuname);

    // Current user-level implementation only attempts opening of context
    // once per CU.  In other words, it is an error if this function is
    // called twice on same CU within same process and since this noop driver
    // is tied to a process we simply throw.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
    for (const auto& [idx, cud] : m_idx2cu)
      if (cud.name == cuname && cud.slot == slot)
        throw xrt_core::error("Context already opened on cu: " + cuname);
#pragma GCC diagnostic pop

    auto idx = m_free_cu_indices.back();
    auto& cudata = m_idx2cu[idx];
    cudata.name = cuname;
    cudata.slot = slot;
    cudata.ctx = 1;
    m_free_cu_indices.pop_back();

    return xrt_core::cuidx_type{idx};
  }

  // close the cu context
  void
  close_context(const xrt::uuid& xid, uint32_t cuidx)
  {
    std::lock_guard lk(m_mutex);

    auto cu_itr = m_idx2cu.find(cuidx);
    if (cu_itr == m_idx2cu.end())
      throw xrt_core::error("No such cu with index: " + std::to_string(cuidx));
    auto& cudata  = (*cu_itr).second;
    if (cudata.ctx < 1)
      throw xrt_core::error("No context aquired on cu: " + std::to_string(cuidx));

    // clean up if last context was released
    if (!--cudata.ctx) {
      auto nm = cudata.name;
      m_idx2cu.erase(cuidx);
      m_free_cu_indices.push_back(cuidx);
    }
  }

  xrt_core::query::kds_cu_info::result_type
  kds_cu_info()
  {
    xrt_core::query::kds_cu_info::result_type vec;
    for (const auto& [idx, cud] : m_idx2cu) {
      xrt_core::query::kds_cu_info::data data;
      data.slot_index = cud.slot;
      data.index = idx;
      data.name = cud.name;
      data.base_addr = 0xdeadbeef;
      data.status = 0;
      data.usages = 0;
      vec.push_back(std::move(data));
    }
    return vec;
  }

  xrt_core::query::xclbin_slots::result_type
  xclbin_slots()
  {
    xrt_core::query::xclbin_slots::result_type vec;
    uint32_t slotidx = 0;
    for (const auto& [uuid, xclbin] : m_xclbins) {
      if (uuid != xclbin.get_uuid())
        throw xrt_core::error("mismatched xclbin");
      xrt_core::query::xclbin_slots::slot_info data;
      data.slot = slotidx++;
      data.uuid = uuid.to_string();
      vec.push_back(std::move(data));
    }
    return vec;
  }

};

} // pl

static std::vector<std::shared_ptr<pl::device>> s_devices;


// Simulate asynchronous command completion.
//
// Command handles are added to a producer/consumer queue A worker
// thread pretends to run the command and marks it complete only if
// the command was enqueue some constant time before now.
namespace cmd {

static unsigned int completion_delay_us = 0;
static xrt_core::task::queue running_queue;
static std::thread completer;
static std::atomic<uint64_t> completion_count {0};

struct cmd_type
{
  xclBufferHandle handle;
  unsigned long queue_time;
  cmd_type(xclBufferHandle h)
    : handle(h), queue_time(xrt_core::time_ns())
  {}
};

static void
init()
{
  if ( (completion_delay_us = xrt_core::config::get_noop_completion_delay_us()) )
    completer = std::move(xrt_core::thread(xrt_core::task::worker, std::ref(running_queue)));
}

static void
stop()
{
  if (completion_delay_us) {
    running_queue.stop();
    completer.join();
  }
}

static void
wait()
{
  while (!completion_count) ;
  --completion_count;
}

static void
mark_cmd_handle_complete(xclBufferHandle handle)
{
  //XRT_PRINTF("handle(%d) is complete\n", handle);
  auto hbuf = buffer::map(handle);
  auto cmd = reinterpret_cast<ert_packet*>(hbuf);
  cmd->state = ERT_CMD_STATE_COMPLETED;
  ++completion_count;
}

static void
mark_cmd_complete(cmd_type ct)
{
  while (xrt_core::time_ns() - ct.queue_time < completion_delay_us * 1000);
  mark_cmd_handle_complete(ct.handle);
}

static void
add(xclBufferHandle handle)
{
  if (completion_delay_us)
    xrt_core::task::createF(running_queue, mark_cmd_complete, cmd_type(handle));
  else
    mark_cmd_handle_complete(handle);
}

struct X
{
  X() { init(); }
  ~X() { stop(); }
};

static X x;

} // cmd


struct shim
{
  using buffer_handle_type = xclBufferHandle; // xrt.h
  unsigned int m_devidx;
  bool m_locked = false;
  pl::device* m_pldev;
  std::shared_ptr<xrt_core::device> m_core_device;

  // Capture xclbins loaded using load_xclbin.
  // load_xclbin is legacy and creates a hw_context implicitly.  If an
  // xclbin is loaded with load_xclbin, an explicit hw_context cannot
  // be created for that xclbin.
  std::map<xrt::uuid, std::unique_ptr<xrt_core::hwctx_handle>> m_load_xclbin_slots;

  class buffer_object : public xrt_core::buffer_handle
  {
    shim* m_shim;
    int m_fd;   // fd
  public:
    buffer_object(shim* shim, int fd)
      : m_shim(shim)
      , m_fd(fd)
    {}

    ~buffer_object()
    {
      try {
        m_shim->free_bo(m_fd);
      }
      catch (...) {
      }
    }

    int
    get_fd() const
    {
      return m_fd;
    }

    // Detach and return export handle for legacy xclAPI use
    int
    detach_handle()
    {
      return std::exchange(m_fd, XRT_NULL_BO);
    }

    // Export buffer for use with another process or device
    // An exported buffer can be imported by another device
    // or hardware context.
    virtual std::unique_ptr<xrt_core::shared_handle>
    share() const override
    {
      throw xrt_core::error(std::errc::not_supported, __func__);
    }

    void*
    map(map_type) override
    {
      return m_shim->map_bo(m_fd, true);
    }

    void
    unmap(void* addr) override
    {
      m_shim->unmap_bo(m_fd, addr);
    }

    void
    sync(direction dir, size_t size, size_t offset) override
    {
      m_shim->sync_bo(m_fd, static_cast<xclBOSyncDirection>(dir), size, offset);
    }

    void
    copy(const buffer_handle*, size_t, size_t, size_t) override
    {
      throw xrt_core::error(std::errc::not_supported, __func__);
    }

    properties
    get_properties() const override
    {
      xclBOProperties xprop;
      m_shim->get_bo_properties(m_fd, &xprop);
      return {xprop.flags, xprop.size, xprop.paddr};
    }

    xclBufferHandle
    get_xcl_handle() const override
    {
      return static_cast<xclBufferHandle>(m_fd);
    }
  }; // buffer

  class hwcontext : public xrt_core::hwctx_handle
  {
    shim* m_shim;
    xrt::uuid m_uuid;
    slot_id m_slotidx;
    bool m_null = false;

public:
    hwcontext(shim* shim, slot_id slotidx, xrt::uuid uuid)
      : m_shim(shim)
      , m_uuid(std::move(uuid))
      , m_slotidx(slotidx)

    {}

    ~hwcontext()
    {
      m_shim->destroy_hw_context(m_slotidx);
    }

    slot_id
    get_slotidx() const override
    {
      return m_slotidx;
    }

    xrt::uuid
    get_xclbin_uuid() const
    {
      return m_uuid;
    }

    xrt_core::hwqueue_handle*
    get_hw_queue() override
    {
      return nullptr;
    }

    std::unique_ptr<xrt_core::buffer_handle>
    alloc_bo(void* userptr, size_t size, uint64_t flags) override
    {
      // The hwctx is embedded in the flags, use regular shim path
      return m_shim->alloc_userptr_bo(userptr, size, xcl_bo_flags{flags}.flags);
    }

    std::unique_ptr<xrt_core::buffer_handle>
    alloc_bo(size_t size, uint64_t flags) override
    {
      // The hwctx is embedded in the flags, use regular shim path
      return m_shim->alloc_bo(size, xcl_bo_flags{flags}.flags);
    }

    xrt_core::cuidx_type
    open_cu_context(const std::string& cuname) override
    {
      return m_shim->open_cu_context(this, cuname);
    }

    void
    close_cu_context(xrt_core::cuidx_type cuidx) override
    {
      m_shim->close_cu_context(this, cuidx);
    }

    void
    exec_buf(xrt_core::buffer_handle* cmd) override
    {
      m_shim->exec_buf(cmd->get_xcl_handle());
    }

    bool
    is_null() const
    {
      return m_null;
    }
  }; // class shim::hwccontext

  // create shim object, open the device, store the device handle
  shim(unsigned int devidx)
    : m_devidx(devidx), m_core_device(xrt_core::get_userpf_device(this, m_devidx))
  {
    if (s_devices.size() <= devidx)
      s_devices.resize(devidx + 1);

    if (!s_devices[devidx])
      s_devices[devidx] = std::make_shared<pl::device>();
    m_pldev = s_devices[devidx].get();
  }

  // destruct shim object, close the device
  ~shim()
  {}

  std::unique_ptr<xrt_core::buffer_handle>
  alloc_bo(size_t size, unsigned int flags)
  {
    return std::make_unique<buffer_object>(this, buffer::alloc(size, flags));
  }

  std::unique_ptr<xrt_core::buffer_handle>
  alloc_userptr_bo(void* userptr, size_t size, unsigned int flags)
  {
    return std::make_unique<buffer_object>(this, buffer::alloc(userptr, size, flags));
  }

  void*
  map_bo(buffer_handle_type handle, bool /*write*/)
  {
    return buffer::map(handle);
  }

  int
  unmap_bo(buffer_handle_type handle, void* addr)
  {
    return 0;
  }

  void
  free_bo(buffer_handle_type handle)
  {
    buffer::free(handle);
  }

  int
  sync_bo(buffer_handle_type, xclBOSyncDirection, size_t, size_t)
  {
    return 0;
  }

  xrt_core::cuidx_type
  open_cu_context(const hwcontext* hwctx, const std::string& cuname)
  {
    return m_pldev->open_cu_context(hwctx->get_slotidx(), hwctx->get_xclbin_uuid(), cuname);
  }

  void
  close_cu_context(const hwcontext* hwctx, xrt_core::cuidx_type cuidx)
  {
    return m_pldev->close_context(hwctx->get_xclbin_uuid(), cuidx.index);
  }

  int
  close_context(const xrt::uuid& xid, unsigned int idx)
  {
    m_pldev->close_context(xid, idx);
    return 0;
  }

  int
  exec_buf(buffer_handle_type handle)
  {
    cmd::add(handle);
    return 0;
  }

  int
  exec_wait(int msec)
  {
    cmd::wait();
    return 1;
  }

  int
  get_bo_properties(buffer_handle_type handle, struct xclBOProperties* properties)
  {
    auto bo = buffer::get(handle);
    properties->handle = handle;
    properties->flags = bo->flags;
    properties->size = bo->size;
    properties->paddr = reinterpret_cast<uint64_t>(bo->dbuf);

    return 0;
  }

  int
  load_xclbin(const struct axlf* top)
  {
    auto xclbin = m_core_device->get_xclbin(top->m_header.uuid);
    m_pldev->register_xclbin(xclbin);
    auto uuid = xclbin.get_uuid();
    m_load_xclbin_slots[uuid] = create_hw_context(uuid);
    return 0;
  }

  int
  write(enum xclAddressSpace, uint64_t, const void*, size_t)
  {
    throw std::runtime_error("not implemented");
  }

  int
  read(enum xclAddressSpace, uint64_t, void*, size_t)
  {
    throw std::runtime_error("not implemented");
  }

  ssize_t
  unmgd_pwrite(unsigned int, const void*, size_t, uint64_t)
  {
    throw std::runtime_error("not implemented");
  }

  ssize_t
  unmgd_pread(unsigned int, void*, size_t, uint64_t)
  {
    throw std::runtime_error("not implemented");
  }

  int
  write_bo(xclBufferHandle handle, const void *src, size_t size, size_t seek)
  {
    auto bo = buffer::get(handle);
    auto hbuf = reinterpret_cast<char*>(bo->hbuf) + seek;
    std::memcpy(hbuf, src, std::min(bo->size - seek, size));
    return 0;
  }

  int
  read_bo(xclBufferHandle handle, void *dst, size_t size, size_t skip)
  {
    auto bo = buffer::get(handle);
    auto hbuf = reinterpret_cast<char*>(bo->hbuf) + skip;
    std::memcpy(dst, hbuf, std::min(bo->size - skip, size));
    return 0;
  }

  std::unique_ptr<xrt_core::hwctx_handle>
  create_hw_context(const xrt::uuid& xclbin_uuid)
  {
    if (m_load_xclbin_slots.find(xclbin_uuid) != m_load_xclbin_slots.end())
      throw xrt_core::ishim::not_supported_error(__func__);

    auto slot = m_pldev->create_hw_context(xclbin_uuid);
    return std::make_unique<hwcontext>(this, slot, xclbin_uuid);
  }

  void
  destroy_hw_context(uint32_t slot)
  {
    m_pldev->destroy_hw_context(slot);
    for (const auto& [uuid, hwctx] : m_load_xclbin_slots) {
      if (slot != hwctx->get_slotidx())
        continue;

      m_load_xclbin_slots.erase(uuid);
      break;
    }
  }

  void
  register_xclbin(const xrt::xclbin& xclbin)
  {
    m_pldev->register_xclbin(xclbin);
  }


}; // struct shim

shim*
get_shim_object(xclDeviceHandle handle)
{
  // TODO: Do some sanity check
  return reinterpret_cast<shim*>(handle);
}

} // namespace

namespace userpf {

xrt_core::query::kds_cu_info::result_type
kds_cu_info(const xrt_core::device* device)
{
  auto id = device->get_device_id();
  if (s_devices.size() < id)
    throw xrt_core::error("Unknown device id: " + std::to_string(id));

  xrt_core::query::kds_cu_info::result_type vec;
  return s_devices[id]->kds_cu_info();
}

xrt_core::query::xclbin_slots::result_type
xclbin_slots(const xrt_core::device* device)
{
  auto id = device->get_device_id();
  if (s_devices.size() < id)
    throw xrt_core::error("Unknown device id: " + std::to_string(id));

  xrt_core::query::xclbin_slots::result_type vec;
  return s_devices[id]->xclbin_slots();
}

} // userpf

////////////////////////////////////////////////////////////////
// Implementation of internal SHIM APIs
////////////////////////////////////////////////////////////////
namespace xrt::shim_int {

std::unique_ptr<xrt_core::buffer_handle>
alloc_bo(xclDeviceHandle handle, size_t size, unsigned int flags)
{
  auto shim = get_shim_object(handle);
  return shim->alloc_bo(size, flags);
}

// alloc_userptr_bo()
std::unique_ptr<xrt_core::buffer_handle>
alloc_bo(xclDeviceHandle handle, void* userptr, size_t size, unsigned int flags)
{
  auto shim = get_shim_object(handle);
  return shim->alloc_userptr_bo(userptr, size, flags);
}

std::unique_ptr<xrt_core::hwctx_handle>
create_hw_context(xclDeviceHandle handle,
                  const xrt::uuid& xclbin_uuid,
                  const xrt::hw_context::cfg_param_type&,
                  xrt::hw_context::access_mode)
{
  auto shim = get_shim_object(handle);
  return shim->create_hw_context(xclbin_uuid);
}

void
register_xclbin(xclDeviceHandle handle, const xrt::xclbin& xclbin)
{
  auto shim = get_shim_object(handle);
  shim->register_xclbin(xclbin);
}

} // xrt::shim_int
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
// Implementation of user exposed SHIM APIs
// This are C level functions
////////////////////////////////////////////////////////////////
// Basic
unsigned int
xclProbe()
{
  return 1;
}

xclDeviceHandle
xclOpen(unsigned int deviceIndex, const char *logFileName, xclVerbosityLevel level)
{
  try {
    xrt_core::message::
      send(xrt_core::message::severity_level::debug, "XRT", "xclOpen()");
    return new shim(deviceIndex);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }

  return nullptr;
}

void
xclClose(xclDeviceHandle handle)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclClose()");
  auto shim = get_shim_object(handle);
  delete shim;
}


// XRT Buffer Management APIs
xclBufferHandle
xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned int flags)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclAllocBO()");
  auto shim = get_shim_object(handle);
  auto bo = shim->alloc_bo(size, flags);
  auto ptr = static_cast<shim::buffer_object*>(bo.get());
  return ptr->detach_handle();
}

xclBufferHandle
xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned int flags)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclAllocUserPtrBO()");
  auto shim = get_shim_object(handle);
  auto bo = shim->alloc_userptr_bo(userptr, size, flags);
  auto ptr = static_cast<shim::buffer_object*>(bo.get());
  return ptr->detach_handle();
}

void*
xclMapBO(xclDeviceHandle handle, xclBufferHandle boHandle, bool write)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclMapBO()");
  auto shim = get_shim_object(handle);
  return shim->map_bo(boHandle, write);
}

int
xclUnmapBO(xclDeviceHandle handle, xclBufferHandle boHandle, void* addr)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclUnmapBO()");
  auto shim = get_shim_object(handle);
  return shim->unmap_bo(boHandle, addr);
}

void
xclFreeBO(xclDeviceHandle handle, xclBufferHandle boHandle)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclFreeBO()");
  auto shim = get_shim_object(handle);
  return shim->free_bo(boHandle);
}

int
xclSyncBO(xclDeviceHandle handle, xclBufferHandle boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclSyncBO()");
  auto shim = get_shim_object(handle);
  return shim->sync_bo(boHandle, dir, size, offset);
}

int
xclCopyBO(xclDeviceHandle handle, xclBufferHandle dstBoHandle,
          xclBufferHandle srcBoHandle, size_t size, size_t dst_offset,
          size_t src_offset)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclCopyBO() NOT IMPLEMENTED");
  return ENOSYS;
}

int
xclReClock2(xclDeviceHandle handle, unsigned short region,
            const uint16_t* targetFreqMHz)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclReClock2() NOT IMPLEMENTED");
  return ENOSYS;
}

// Compute Unit Execution Management APIs
int
xclOpenContext(xclDeviceHandle handle, const xuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  return 0;
}

int
xclCloseContext(xclDeviceHandle handle, const xuid_t xclbinId, unsigned int ipIndex)
{
  return 0;
}

int
xclExecBuf(xclDeviceHandle handle, xclBufferHandle cmdBO)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclExecBuf()");
  auto shim = get_shim_object(handle);
  return shim->exec_buf(cmdBO);
}

int
xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclExecWait()");
  auto shim = get_shim_object(handle);
  return shim->exec_wait(timeoutMilliSec);
}

xclBufferExportHandle
xclExportBO(xclDeviceHandle handle, xclBufferHandle boHandle)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclExportBO() NOT IMPLEMENTED");
  return XRT_NULL_BO_EXPORT;
}

xclBufferHandle
xclImportBO(xclDeviceHandle handle, xclBufferExportHandle fd, unsigned flags)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclImportBO() NOT IMPLEMENTED");
  return XRT_NULL_BO_EXPORT;
}

int
xclCloseExportHandle(xclBufferExportHandle fd)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclCloseExportHandle() NOT IMPLEMENTED");
  return 0;
}

int
xclGetBOProperties(xclDeviceHandle handle, xclBufferHandle boHandle,
		   struct xclBOProperties *properties)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclGetBOProperties()");
  auto shim = get_shim_object(handle);
  return shim->get_bo_properties(boHandle,properties);
}

int
xclLoadXclBin(xclDeviceHandle handle, const struct axlf *buffer)
{
  try {
    xrt_core::message::
      send(xrt_core::message::severity_level::debug, "XRT", "xclLoadXclbin()");
    auto shim = get_shim_object(handle);
    if (auto ret = shim->load_xclbin(buffer))
      return ret;
    auto core_device = xrt_core::get_userpf_device(shim);
    core_device->register_axlf(buffer);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -EINVAL;
  }
}

unsigned int
xclVersion()
{
  return 2;
}

int
xclGetDeviceInfo2(xclDeviceHandle handle, struct xclDeviceInfo2 *info)
{
  std::memset(info, 0, sizeof(xclDeviceInfo2));
  info->mMagic = 0;
  info->mHALMajorVersion = XCLHAL_MAJOR_VER;
  info->mHALMinorVersion = XCLHAL_MINOR_VER;
  info->mMinTransferSize = 0;
  info->mDMAThreads = 2;
  info->mDataAlignment = 4096; // 4k

  return 0;
}

int
xclLockDevice(xclDeviceHandle handle)
{
  return 0;
}

int
xclUnlockDevice(xclDeviceHandle handle)
{
  return 0;
}

ssize_t
xclUnmgdPwrite(xclDeviceHandle handle, unsigned int flags, const void *buf, size_t count, uint64_t offset)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclUnmgdPwrite()");
  auto shim = get_shim_object(handle);
  return shim->unmgd_pwrite(flags, buf, count, offset) ? 0 : 1;
}

ssize_t
xclUnmgdPread(xclDeviceHandle handle, unsigned int flags, void *buf, size_t count, uint64_t offset)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclUnmgdPread()");
  auto shim = get_shim_object(handle);
  return shim->unmgd_pread(flags, buf, count, offset) ? 0 : 1;
}

size_t xclWriteBO(xclDeviceHandle handle, xclBufferHandle boHandle, const void *src, size_t size, size_t seek)
{
    xrt_core::message::
        send(xrt_core::message::severity_level::debug, "XRT", "xclWriteBO()");
    auto shim = get_shim_object(handle);
    return shim->write_bo(boHandle, src, size, seek);
}

size_t xclReadBO(xclDeviceHandle handle, xclBufferHandle boHandle, void *dst, size_t size, size_t skip)
{
    xrt_core::message::
        send(xrt_core::message::severity_level::debug, "XRT", "xclReadBO()");
    auto shim = get_shim_object(handle);
    return shim->read_bo(boHandle, dst, size, skip);
}

void
xclGetDebugIpLayout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret)
{
}

// Deprecated APIs
size_t
xclWrite(xclDeviceHandle handle, enum xclAddressSpace space, uint64_t offset, const void *hostbuf, size_t size)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclWrite()");
  auto shim = get_shim_object(handle);
  return shim->write(space,offset,hostbuf,size) ? 0 : size;
}

size_t
xclRead(xclDeviceHandle handle, enum xclAddressSpace space,
        uint64_t offset, void *hostbuf, size_t size)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::debug, "XRT", "xclRead()");
  auto shim = get_shim_object(handle);
  return shim->read(space,offset,hostbuf,size) ? 0 : size;
}

// Restricted read/write on IP register space
int
xclRegWrite(xclDeviceHandle handle, uint32_t ipidx, uint32_t offset, uint32_t data)
{
  return 1;
}

int
xclRegRead(xclDeviceHandle handle, uint32_t ipidx, uint32_t offset, uint32_t* datap)
{
  return 1;
}

int
xclGetTraceBufferInfo(xclDeviceHandle handle, uint32_t nSamples,
                      uint32_t& traceSamples, uint32_t& traceBufSz)
{
  return 0;
}

int
xclReadTraceData(xclDeviceHandle handle, void* traceBuf, uint32_t traceBufSz,
                 uint32_t numSamples, uint64_t ipBaseAddress,
                 uint32_t& wordsPerSample)
{
  return 0;
}

int
xclGetSubdevPath(xclDeviceHandle handle,  const char* subdev,
                 uint32_t idx, char* path, size_t size)
{
  return 0;
}

int
xclP2pEnable(xclDeviceHandle handle, bool enable, bool force)
{
  return 1; // -ENOSYS;
}

int
xclUpdateSchedulerStat(xclDeviceHandle handle)
{
  return 1; // -ENOSYS;
}

int
xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t force)
{
  return -ENOSYS;
}

int
xclInternalResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
  return 1; // -ENOSYS;
}
