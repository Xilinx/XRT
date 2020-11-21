/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef xocl_core_device_h_
#define xocl_core_device_h_
#include "xocl/config.h"
#include "xocl/core/object.h"
#include "xocl/core/refcount.h"
#include "xocl/core/error.h"
#include "xocl/core/compute_unit.h"
#include "xocl/xclbin/xclbin.h"
#include "xrt/device/device.h"
#include "xrt/scheduler/command.h"
#include "core/common/unistd.h"
#include "core/common/scope_guard.h"

#include <cassert>

namespace xocl {

class device : public refcount, public _cl_device_id
{
public:
  using buffer_object_handle = xrt_xocl::device::buffer_object_handle;
  using memidx_bitmask_type = xclbin::memidx_bitmask_type;
  using compute_unit_type = std::shared_ptr<compute_unit>;
  using compute_unit_vector_type = std::vector<compute_unit_type>;
  using compute_unit_range = compute_unit_vector_type;
  using compute_unit_iterator = compute_unit_vector_type::const_iterator;
  using cmd_type = std::shared_ptr<xrt_xocl::command>;
  using memidx_type = xclbin::memidx_type;
  using connidx_type = xclbin::connidx_type;

  /**
   * Construct an xocl::device.
   *
   * @param platform
   *   The platform associated with this device
   * @param xdevice
   *   The underlying xrt device managed by the platform
   */
  device(platform* pltf, xrt_xocl::device* xdevice);

  /**
   * Sub device constructor
   *
   * A sub device clones the parent device but explicitly limits the
   * compute units as specified.  The sub-device can be used like a
   * regular device.  Note that platform tracks physical devices only
   * hence knows nothing about subdevices
   *
   * Limitations (xilinx):
   *  - A subdevice can only be constructed after parent device has
   *    loaded a program.  The act of loading a program creates CUs
   *    and a subdevice uses one or more of these CUs
   *  - A program cannot be loaded on a subdevice; per above it is
   *    implicitly loaded when the subdevice is constructed
   *  - If a program is unloaded from a root device, then subdevices
   *    of root device also implicitly unloads the program and are no
   *    longer valid.
   *  - A subdevice cannot be further subdeviced.
   *
   * @param parent
   *   Parent device to clone
   * @param cus
   *   List of parent cus this sub device can use
   */
  device(device* parent, const compute_unit_vector_type& cus);

  /**
   * Delegating ctor (temp)
   */
  device()
    : device(nullptr,nullptr)
  {}

  virtual ~device();

  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  device*
  get_parent_device() const
  {
    return m_parent.get();
  }

  const device*
  get_root_device() const
  {
    return m_parent.get() ? m_parent->get_root_device() : this;
  }

  bool is_sub_device() const
  {
    return m_parent.get() != nullptr;
  }

  xrt_xocl::device*
  get_xdevice() const
  {
    return  m_xdevice;
  }

  platform*
  get_platform() const
  {
    return m_platform;
  }

  std::string
  get_name() const
  {
    return m_xdevice ? m_xdevice->getName() : "fpga0";
  }

  std::string
  get_unique_name() const
  {
    return get_name() + "-" + std::to_string(m_uid);
  }

  /**
   * Get the BDF of the device
   *
   * Throws on error
   */
  std::string
  get_bdf() const;

  /**
   * Check if this device is a NODMA device
   */
  bool
  is_nodma() const;

  /**
   * Get underlying driver device handle
   */
  void*
  get_handle() const;

  /**
   * Get the number of DDR memory banks on the current device
   *
   * @return
   *  Number of banks on device
   */
  unsigned int
  get_ddr_bank_count() const
  {
    return m_xdevice->getBankCount();
  }

  /**
   * Get the size of the DDR memory on the current device
   *
   * @return
   *  Size of DDR on device
   */
  size_t
  get_ddr_size() const
  {
    return m_xdevice->getDdrSize();
  }

  /**
   * Get max clock frequency for this device.
   *
   * The max clock frequency is whatever frequency the device
   * is currently set to.  It really isn't the max, since the
   * xclbin can reclock the device at a higher rate.
   *
   * @return
   *   Current device clock frequency.
   */
  unsigned short
  get_max_clock_frequency() const;

public:
  /**
   * @return Minimum buffer alignment in bytes
   */
  size_t
  get_alignment() const
  {
    return m_xdevice ? m_xdevice->getAlignment() : xrt_core::getpagesize();
  }

  /**
   * Check if memory is aligned per device requirement.
   *
   * Default is page size if no backing xrt device
   *
   * @return
   *   true if ptr is aligned, false otherwise
   */
  bool
  is_aligned_ptr(void* p) const
  {
    return p && (reinterpret_cast<uintptr_t>(p) % get_alignment())==0;
  }

  /**
   * Import a buffer object from exporting device to this device
   *
   * This function assumes correct XARE device connections, e.g.
   * no mix of XARE and non-XARE. It is undefined behavior to call
   * this function if a buffer object already exists for current
   * device and argument bo.
   *
   * @param src_device
   *   Device that that exports to buffer object
   * @param src_boh
   *  Buffer object to import, must belong to exporting_device
   * @return
   *  Imported buffer object associated with this device
   */
  buffer_object_handle
  import_buffer_object(const device* src_device, const  buffer_object_handle& src_boh);

  /**
   * Allocate and return buffer object for argument cl_mem
   *
   * This function allocates the buffer in memory bank identified by
   * argument memidx.
   *
   * @param mem
   *   The cl_mem object to allocate a buffer object from.
   * @param memidx
   *   The memory bank to allocate buffer in.
   * @return
   *   The buffer object that was created.
   */
  buffer_object_handle
  allocate_buffer_object(memory* mem, memidx_type memidx);

  /**
   * Special interface to allocate a buffer object undconditionally
   *
   * Used by clCreateProgramWithBinary.  Not a great interface, but
   * has to be exposed here to ensure proper locking
   */
  buffer_object_handle
  allocate_buffer_object(memory* mem, xrt_xocl::device::memoryDomain domain, uint64_t memoryIndex, void* user_ptr);

  /**
   * Free memory object on this device
   *
   * Throws runtime_error if mem not allocated on this device
   *
   * @param mem
   *   Memory object to free
   */
  void
  free(const memory* mem);

  /**
   * Check if buffer is imported to this device
   *
   * If the buffer is allocated on this device, then check if the
   * corresponding buffer object is an imported buffer object
   *
   * @return: true if imported, false otherwise
   */
  bool
  is_imported(const memory* mem) const;

  /**
   * Get memory addr for the given boh
   *
   * @return
   *   return the address of the buffer object
   */
  uint64_t
  get_boh_addr(const buffer_object_handle& boh) const;

  /**
   * Get indicies of matching memory banks on which mem is allocated
   *
   * The memory indicies are returned as a bitmask because a given ddr
   * address can be access through multiple banks
   *
   * @return
   *   Memory indeces identifying bank or -1 if not allocated
   */
  memidx_bitmask_type
  get_boh_memidx(const buffer_object_handle& boh) const;

  /**
   * Get the banktag of the bank on which mem is allocated
   *
   * In cases where an address can be accessed through multiple banks,
   * the banktag returned is that of first matching bank.
   *
   *
   * @return
   *   Banktag or "Unknown" if no match
   */
  std::string
  get_boh_banktag(const buffer_object_handle& boh) const;

  /**
   * Get the memory index of the bank for all CUs in this device
   *
   * @return Memory index for DDR bank if all CUs are uniquely connected
   *  to same DDR bank for all arguments, -1 otherwise
   */
  memidx_type
  get_cu_memidx() const;

  /**
   * Map buffer (clEnqueueMapBuffer) implementation
   */
  void*
  map_buffer(memory* mem, cl_map_flags map_flags, size_t offset, size_t size, void* assert_result, bool nosync=false);

  /**
   * Unmap buffer (clEnqueueUnmapMemObjects) implementation
   */
  void
  unmap_buffer(memory* mem, void* mapped_ptr);

  /**
   * Migrate buffer to this device (clEnqueueMigrateMemObjects)
   *
   * After this call the buffer is resident on this device
   */
  void
  migrate_buffer(memory* buffer,cl_mem_migration_flags flags);

  /**
   * Write data size bytes to buffer at specified offset
   *
   * @param buffer
   *  Buffer to write to.  The underlying buffer object will
   *  receive the data.  The data will be synced to device if
   *  and only of the buffer is currently resident on the device
   * @param offset
   *  The offset in buffer to write to
   * @param size
   *  Number of bytes to write
   * @param data
   *  The data to write from
   */
  void
  write_buffer(memory* buffer, size_t offset, size_t size, const void* data);

  /**
   * Read data size bytes from buffer at specified offset
   *
   * @param buffer
   *  Buffer read from.  The buffer will synced from device first if
   *  and only of the buffer is currently resident on the device.
   * @param offset
   *  The offset in buffer read from
   * @param size
   *  Number of bytes to read
   * @param ptr
   *  The ptr write to
   */
  void
  read_buffer(memory* buffer, size_t offset, size_t size, void* data);

  /**
   * Copy size data from from src buffer to dst buffer at specified offsets
   *
   * @param src_buffer
   *  Buffer read from.  The buffer will synced from device first if
   *  and only of the buffer is currently resident on the device.
   * @param dst_buffer
   *  Buffer write to.  The buffer will synced to device after write if
   *  and only of the buffer is currently resident on the device.
   * @param src_offset
   *  The offset in buffer read from
   * @param dst_offset
   *  The offset in buffer read from
   * @param size
   *  Number of bytes to copy
   * @param cmd
   *  Copy command buffer to be scheduled for execution
   */
  void
  copy_buffer(memory* src_buffer, memory* dst_buffer, size_t src_offset, size_t dst_offset, size_t size, const cmd_type& cmd);

  void
  copy_p2p_buffer(memory* src_buffer, memory* dst_buffer, size_t src_offset, size_t dst_offset, size_t size);


  /**
   * Fill size bytes of buffer at offset with specified pattern
   *
   * @param buffer
   *  Buffer to fill with pattern.  The buffer will synced to device
   *  after being filled if and only if the buffer is currently
   *  resident on the device.
   * @param pattern
   *  The pattern to fill the buffer with.
   * @param pattern_size
   *  The size of the pattern buffer.
   * @param offset
   *  The offset at which to start writing the pattern.
   * @param size
   *  The number of bytes to fill with pattern.
   */
  void
  fill_buffer(memory* buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size);

  void
  write_image(memory* image,const size_t* origin,const size_t* region,size_t row_pitch,size_t slice_pitch,const void *ptr);

  void
  read_image(memory* image,const size_t* origin,const size_t* region,size_t row_pitch,size_t slice_pitch,void *ptr);

  int
  get_stream(xrt_xocl::device::stream_flags flags, xrt_xocl::device::stream_attrs attrs, const cl_mem_ext_ptr_t* ext, xrt_xocl::device::stream_handle* stream, int32_t& m_conn);

  int
  close_stream(xrt_xocl::device::stream_handle stream, int connidx);

  ssize_t
  write_stream(xrt_xocl::device::stream_handle stream, const void* ptr, size_t size, xrt_xocl::device::stream_xfer_req* req);

  ssize_t
  read_stream(xrt_xocl::device::stream_handle stream, void* ptr, size_t size, xrt_xocl::device::stream_xfer_req* req);

  xrt_xocl::device::stream_buf
  alloc_stream_buf(size_t size, xrt_xocl::device::stream_buf_handle* handle);

  int
  free_stream_buf(xrt_xocl::device::stream_buf_handle handle);

  int
  set_stream_opt(xrt_xocl::device::stream_handle stream, int type, uint32_t val);

  int
  poll_stream(xrt_xocl::device::stream_handle stream, xrt_xocl::device::stream_xfer_completions* comps, int min, int max, int* actual, int timeout);

  int
  poll_streams(xrt_xocl::device::stream_xfer_completions* comps, int min, int max, int* actual, int timeout);

  /**
   * Read a device register at specified offset
   *
   * @param mem
   *   Place holder mem object that must have CL_MEM_REGISTER_MAP or CL_REGISTER_MAP
   *   flags set.
   * @param offset
   *   The offset of the register in the device register map??
   * @param ptr
   *   The ptr to write the register value to
   * @param size
   *   The size of the ptr data storage and the number of bytes to read
   */
  void
  read_register(memory* mem, size_t offset,void* ptr, size_t size);

  /**
   * Write a device register at specified offset
   *
   * @param mem
   *   Place holder mem object that must have CL_MEM_REGISTER_MAP or CL_REGISTER_MAP
   *   flags set.
   * @param offset
   *   The offset of the register in the device register map??
   * @param ptr
   *   The ptr with the data to write to the register
   * @param size
   *   The size of the ptr data storage and the number of bytes to write
   */
  void
  write_register(memory* mem, size_t offset,const void* ptr, size_t size);

  /**
   * Load a program binary
   *
   * @param program
   *  Program to load
   */
  void
  load_program(program* program);

  /**
   * Unload the program if any
   */
  void
  unload_program(const program* program);

  /**
   * Get current loaded program
   *
   * @return
   *   Program that is currently loaded or nullptr if none
   */
  program*
  get_program() const
  {
    return m_active;
  }

  /**
   * @return
   *  Current loaded xclbin
   */
  XRT_XOCL_EXPORT
  xclbin
  get_xclbin() const;

  /**
   * @return
   *  AXLF top
   */
  const axlf*
  get_axlf() const;

  /**
   * @return
   *   axlf section, or nullptr if not present
   */
  XRT_XOCL_EXPORT
  std::pair<const char*, size_t>
  get_axlf_section(axlf_section_kind kind) const;

  /**
   * @return
   *   axlf section, or nullptr if not present
   */
  template <typename SectionType>
  SectionType
  get_axlf_section(axlf_section_kind kind) const
  {
    return reinterpret_cast<SectionType>(get_axlf_section(kind).first);
  }

  /**
   * Check if this device is active, meaning it is programmed
   */
  bool
  is_active() const { return m_active!=nullptr; }

  /**
   * Lock current device.
   *
   * If the device is already locked by this process, then the
   * lock count is incremented and returned.
   *
   * If the device is not currently locked, then this function
   * queries hardware to check if the device is free in which
   * case it is opened and locked.
   *
   * May throw cl error code if device could not be locked by probing
   * hardware.
   *
   * @return
   *  Number of locks on this device by current process.
   */
  unsigned int
  lock();

  /**
   * Unlock current device.
   *
   * If the device is not currently locked by this process, then this
   * function is a no-op.
   *
   * If the device is currently locked, then this function
   * decrements the lock count.  If the lock count reaches 0,
   * the hardware device is unlocked (closed).
   *
   * May throw cl error code if device could not be unlocked by
   * probing hardware.
   *
   * @return
   *  Number of locks on this device by current process after
   *  releasing one lock.
   */
  unsigned int
  unlock();

  /**
   * Return a scoped lock guard managing a lock on the device.
   *
   * When the scope goes out of scope, the aquired lock is released
   * automatically.
   */
  xrt_core::scope_guard<std::function<void()>>
  lock_guard()
  {
    lock();
    auto unlocker = [](device* d) { d->unlock(); };
    return {std::bind(unlocker, this)};
  }

  /**
   * Check is this device is available for use by this process.
   *
   * This function goes hand in hand with lock_device.  Once a device
   * is successfully locked, it is available.   If a device has not been
   * locked, then it is not available.
   *
   * Used by clGetDeviceInfo.
   */
  bool
  is_available() const { return m_locks>0; }

  /**
   * Check if an address is currently mapped from memory
   * maintained by this device.
   *
   * clEnqueueMapBuffer maps the host ptr of a buffer object
   * and the resulting mapped ptr is maintained by this device
   * object
   *
   * @return
   *   true if argument was mapped from a device buffer object
   */
  bool
  is_mapped(const void* mapped_ptr) const
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    return (m_mapped.find(mapped_ptr)!=m_mapped.end());
  }

  /**
   * Get a range of compute units this device can use
   */
  const compute_unit_vector_type&
  get_cus() const
  {
    return m_computeunits;
  }

  const compute_unit_range&
  get_cu_range() const
  {
    return get_cus();
  }

  compute_unit_vector_type::size_type
  get_num_cus() const
  {
    return m_computeunits.size();
  }

  /**
   * Acquire a context for a given compute unit on this device.
   *
   * By default the context is acquired as shared.
   * Throws exception if context cannot be acquired on device.
   *
   * @return
   *   @true on success, @false if no program loaded.
   */
  bool
  acquire_context(const compute_unit* cu) const;

  /**
   * Release a context for a given compute unit on this device
   *
   * Throws exception if context cannot be release properly.
   *
   * @return
   *   @true on success, @false if no program loaded.
   */
  bool
  release_context(const compute_unit* cu) const;

  /**
   * @return
   *   Number of CDMA copy kernels available
   */
  size_t
  get_num_cdmas() const;

  void
  clear_connection(connidx_type conn);

private:

  /**
   * Add a cu this device can use
   *
   * CUs are added by load_program
   */
  void
  add_cu(compute_unit_type cu)
  {
    m_computeunits.emplace_back(std::move(cu));
  }

  void
  clear_cus();

  /**
   * Allocate device side buffer buffer object on specified bank
   *
   * @param mem
   *  Memory object for which to allocated device side buffer
   * @param bank
   *  DDR bank ID specifying which virtual bank to allocated buffer on
   * @return
   *  Buffer object handle for allocated memory
   */
  buffer_object_handle
  alloc(memory* mem, memidx_type memidx);

private:
  struct mapinfo {
    cl_map_flags flags = 0; // mapflags
    size_t offset = 0;      // boh:hbuf offset
    size_t size = 0;        // max size mapped
  };

  unsigned int m_uid = 0;
  program* m_active = nullptr;   // program loaded on to this device
  xclbin m_metadata;             // cache xclbin that came from program
  unsigned int m_locks = 0;      // number of locks on this device

  platform* m_platform = nullptr;
  xrt_xocl::device* m_xdevice = nullptr;

  // Set for sub-device only
  ptr<device> m_parent = nullptr;

  // Mutual exclusive access to this device
  mutable std::mutex m_mutex;

  // Track how a region of a buffer object is mapped.  There is
  // no tracking of matching map and unmap.  Last map of a region
  // is what is stored and first unmap of a region erases the content.
  std::map<const void*,mapinfo> m_mapped;

  // Track memory objects allocated on this device
  std::set<const memory*> m_memobjs;

  // CUs populated during load_program or by sub device contructor.
  compute_unit_vector_type m_computeunits;

  // Caching.  Purely implementation detail (-2 => not initialized)
  mutable memidx_type m_cu_memidx = -2;
};

} // xocl

#endif
