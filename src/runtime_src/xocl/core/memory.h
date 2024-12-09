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

#ifndef xocl_core_memory_h_
#define xocl_core_memory_h_

#include "xocl/core/object.h"
#include "xocl/core/refcount.h"
#include "xocl/core/property.h"
#include "xocl/xclbin/xclbin.h"
#include "xrt/device/device.h"

#include "core/common/memalign.h"
#include "core/common/unistd.h"
#include "core/common/api/bo.h"
#include "core/include/xrt/xrt_bo.h"

#include <map>

#ifdef _WIN32
#pragma warning( push )
#pragma warning ( disable : 4245 )
#endif

namespace xocl {

class kernel;

class memory : public refcount, public _cl_mem
{
  using memory_flags_type  = property_object<cl_mem_flags>;
  using memory_extension_flags_type = property_object<unsigned int>;
  using memidx_bitmask_type = xclbin::memidx_bitmask_type;
  using memidx_type = xclbin::memidx_type;

protected:
  using buffer_object_handle = xrt_xocl::device::buffer_object_handle;
  using buffer_object_map_type = std::map<const device*,buffer_object_handle>;
  using bomap_type = std::map<const device*,buffer_object_handle>;
  using bomap_value_type = bomap_type::value_type;
  using bomap_iterator_type = bomap_type::iterator;
public:
  using memory_callback_type = std::function<void (memory*)>;
  using memory_callback_list = std::vector<memory_callback_type>;

  memory(context* cxt, cl_mem_flags flags);
  virtual ~memory();

  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  const memory_flags_type
  get_flags() const
  {
    return m_flags;
  }

  void
  add_flags(cl_mem_flags flags)
  {
    m_flags |= flags;
  }

  const memory_extension_flags_type
  get_ext_flags() const
  {
    return m_ext_flags;
  }

  void
  set_ext_flags(memory_extension_flags_type flags)
  {
    m_ext_flags = flags;
  }

  /**
   * @return
   *  The memory bank index used by this buffer, or -1 if unassigned.
   */
  memidx_type
  get_memidx() const
  {
    return m_memidx;
  }

  memidx_type
  get_ext_memidx(const xclbin& xclbin) const;

  /**
   * Record that this buffer is used as argument to kernel at argidx
   *
   * @return
   *   true if the {kernel,argidx} was not previously recorded,
   *   false otherwise
   */
  bool
  set_kernel_argidx(const kernel* kernel, unsigned int argidx);

  void
  set_ext_kernel(const kernel* kernel)
  {
    m_ext_kernel = kernel;
  }

  context*
  get_context() const
  {
    return m_context.get();
  }

  bool
  is_sub_buffer() const
  {
    return get_sub_buffer_parent() != nullptr;
  }

  bool
  is_device_memory_only() const
  {
    return m_flags & CL_MEM_HOST_NO_ACCESS;
  }

  bool
  is_device_memory_only_p2p() const
  {
    return m_ext_flags & XCL_MEM_EXT_P2P_BUFFER;
  }

  bool
  is_host_only() const
  {
    return m_ext_flags & XCL_MEM_EXT_HOST_ONLY;
  }

  bool
  no_host_memory() const
  {
    return is_device_memory_only() || is_device_memory_only_p2p();
  }

  // Derived classes accessors
  // May be structured differently when _xcl_mem is eliminated
  virtual size_t
  get_size() const
  {
    throw std::runtime_error("get_size on bad object");
  }


  /**
   * Get the address and DDR bank where this memory object is allocated
   * if owning context has one device only.
   *
   * @return through reference arguments address and bank
   */
  virtual void
  try_get_address_bank(uint64_t& addr, std::string& bank) const;

  virtual void*
  get_host_ptr() const
  {
    throw std::runtime_error("get_host_ptr called on bad object");
  }

  virtual bool
  is_aligned() const
  {
    throw std::runtime_error("is_aligned called on bad object");
  }

  virtual bool
  need_extra_sync() const
  {
    throw std::runtime_error("need_extra_sync called on bad object");
  }

  virtual void
  set_extra_sync()
  {
    throw std::runtime_error("set_extra_sync called on bad object");
  }

  virtual cl_mem_object_type
  get_type() const = 0;

  virtual memory*
  get_sub_buffer_parent() const
  {
    //throw std::runtime_error("get_sub_buffer_parent called on bad object");
    return nullptr;
  }

  virtual size_t
  get_sub_buffer_offset() const
  {
    throw std::runtime_error("get_sub_buffer_offset called on bad object");
  }

  virtual cl_image_format
  get_image_format()
  {
    throw std::runtime_error("get_image_format called on bad object");
  }

  virtual size_t
  get_image_data_offset() const
  {
    throw std::runtime_error("get_image_offset called on bad object");
  }

  virtual size_t
  get_image_width() const
  {
    throw std::runtime_error("get_image_width called on bad object");
  }

  virtual size_t
  get_image_height() const
  {
    throw std::runtime_error("get_image_height called on bad object");
  }

  virtual size_t
  get_image_depth() const
  {
    throw std::runtime_error("get_image_depth called on bad object");
  }

  virtual size_t
  get_image_bytes_per_pixel() const
  {
    throw std::runtime_error("get_bytes_per_pixel called on bad object");
  }

  virtual size_t
  get_image_row_pitch() const
  {
    throw std::runtime_error("get_image_row_pitch called on bad object");
  }

  virtual size_t
  get_image_slice_pitch() const
  {
    throw std::runtime_error("get_image_slice_pitch called on bad object");
  }

  virtual void
  set_image_row_pitch(size_t pitch)
  {
    throw std::runtime_error("set_image_row_pitch called on bad object");
  }

  virtual void
  set_image_slice_pitch(size_t pitch)
  {
    throw std::runtime_error("set_image_slice_pitch called on bad object");
  }

  virtual cl_uint
  get_pipe_packet_size() const
  {
    throw std::runtime_error("get_pipe_packet_size called on bad object");
  }

  virtual cl_uint
  get_pipe_max_packets() const
  {
    throw std::runtime_error("get_pipe_max_packets called on bad object");
  }

  /****************************************************************
   * Mapping from memory object to device buffer object.  The mapping
   * is maintained in this class (as opposed to device class).  The
   * memory overhead for a std::map with few entries is small in
   * comparison to the runtime overhead of accessing a single std::map
   * with many entries.  If stored in device the mapping would be from
   * mem->boh with the requirement that all access be syncrhonized.
   * If stored stored here in this class the mapping is from
   * device->boh and locking is per memory object.
   ****************************************************************/

  /**
   * Update Device to BufferObject map of the cl_mem buffer.
   * Note: get_buffer_object functions require this map.
   *
   * This function return true on success or throws run time error.
   * If BO is not mapped to buffer then it is added to the map.
   * Otherwise it results in runtime error.
   *
   * @param device
   *   The device that created the cl_mem buffer object
   *   The BufferObject handle from the device
   * @return
   *   true or throws runtime error
   */
  virtual void
  update_buffer_object_map(const device* device, buffer_object_handle boh);


  /**
   * Get or create the device buffer object associated with arg device
   *
   * This function return the buffer object that is associated with
   * the argument device.  If a buffer object does not exist it is
   * created and associated with device.
   *
   * @param device
   *   The device object that creates the buffer object
   * @param subidx
   *   The memory bank index required by sub-buffer
   * @return
   *   The buffer object
   *
   * The sub-buffer index is used when allocation of this boh is
   * originating from a sub-buffer allocation.  It means that the
   * sub-buffer is used as a kernel argument with the 'subidx'
   * conectitivy.  The parent buffer is the one that is physically
   * allocated and it must be allocated in the bank indicated by 
   * the sub buffer.
   */
  virtual buffer_object_handle
  get_buffer_object(device* device, memidx_type subidx=-1);

  /**
   * Get the buffer object on argument device or error out if none
   * exists.
   *
   * This function return the buffer object that is associated with
   * argument device.  If a buffer object does not exist the function
   * throws std::runtime_error.
   *
   * @param device
   *   The device from which to get a buffer object.
   * @return
   *   The buffer object associated with the device, or
   *   std::runtime_error if no buffer object exists.
   */
  buffer_object_handle
  get_buffer_object_or_error(const device* device) const;

  /**
   * Get the buffer object on argument device or nullptr if none
   *
   * This function return the buffer object that is associated
   * argument device. If a buffer object does not exist the
   * function returns nullptr;
   *
   * @param device
   *   The device from which to get a buffer object.
   * @return
   *   The buffer object associated with the device, or nullptr if
   *   none.
   */
  buffer_object_handle
  get_buffer_object_or_null(const device* device) const;

  /**
   * Try-lock to get the buffer object on argument device or nullptr if lock cannot be acquired onone
   *
   * This function return the buffer object that is associated
   * argument device. If a buffer object does not exist the
   * function returns nullptr. If a lock cannot be acquired it returns nullptr;
   *
   * @param device
   *   The device from which to get a buffer object.
   * @return
   *   The buffer object associated with the device, or nullptr if
   *   none.
   */
  buffer_object_handle
  try_get_buffer_object_or_error(const device* device) const;

  /**
   * Check if buffer is resident on any device
   *
   * Return: %true if this buffer is resident on a device, %false otherwise.
   */
  virtual bool
  is_resident() const
  {
    std::lock_guard<std::mutex> lk(m_boh_mutex);
    return m_resident.size();
  }

  /**
   * Check if buffer is resident on device
   */
  virtual bool
  is_resident(const device* device) const
  {
    std::lock_guard<std::mutex> lk(m_boh_mutex);
    return (std::find(m_resident.begin(),m_resident.end(),device) != m_resident.end());
  }

  /**
   * Get resident device if exactly one
   */
  virtual const device*
  get_resident_device() const
  {
    std::lock_guard<std::mutex> lk(m_boh_mutex);
    auto sz = m_resident.size();
    if (!sz || sz>1)
      return nullptr;
    return m_resident[0];
  }

  /**
   * Set device resident
   */
  void
  set_resident(const device* device)
  {
    std::lock_guard<std::mutex> lk(m_boh_mutex);
    if (std::find(m_resident.begin(),m_resident.end(),device) == m_resident.end())
      m_resident.push_back(device);
  }

  /**
   * Clear resident devices
   */
  void
  clear_resident()
  {
    std::lock_guard<std::mutex> lk(m_boh_mutex);
    m_resident.clear();
  }

  /**
   * Add a dtor callback
   */
  void
  add_dtor_notify(std::function<void()> fcn);

  /**
   * Register callback function for memory construction
   *
   * Callbacks are called in arbitrary order
   */
  XRT_XOCL_EXPORT
  static void
  register_constructor_callbacks(memory_callback_type&& aCallback);

  /**
   * Register callback function for memory destruction
   *
   * Callbacks are called in arbitrary order
   */
  XRT_XOCL_EXPORT
  static void
  register_destructor_callbacks(memory_callback_type&& aCallback);

private:
  memidx_type
  get_memidx_nolock(const device* d, memidx_type subidx=-1) const;

  memidx_type
  get_ext_memidx_nolock(const xclbin& xclbin) const;

  memidx_type
  update_memidx_nolock(const device* device, const buffer_object_handle& boh);

private:
  unsigned int m_uid = 0;
  ptr<context> m_context;

  memory_flags_type m_flags {0};

  // cl_mem_ext_ptr_t data.  move to buffer derived class
  memory_extension_flags_type m_ext_flags {0};
  const kernel* m_ext_kernel {nullptr};

  // Record that this buffer is used as argument to kernel,argidx
  std::vector<std::pair<const kernel*,unsigned int>> m_karg;

  // Assigned memory bank index for this object.  Affects behavior of
  // device side buffer allocation.
  mutable memidx_type m_memidx = -1;

  // List of dtor callback functions. On heap to avoid
  // allocation unless needed.
  std::unique_ptr<std::vector<std::function<void()>>> m_dtor_notify;

  mutable std::mutex m_boh_mutex;
  bomap_type m_bomap;
  std::vector<const device*> m_resident;
};

class buffer : public memory
{
public:
  buffer(context* ctx,cl_mem_flags flags, size_t sz, void* host_ptr)
    : memory(ctx,flags) ,m_size(sz), m_host_ptr(host_ptr)
  {
    // device is unknown so alignment requirement has to be hardwired
    const size_t alignment = xrt_core::bo::alignment();

    if (flags & (CL_MEM_COPY_HOST_PTR | CL_MEM_ALLOC_HOST_PTR))
      // allocate sufficiently aligned memory and reassign m_host_ptr
      if (xrt_core::posix_memalign(&m_host_ptr,alignment,sz))
        throw error(CL_MEM_OBJECT_ALLOCATION_FAILURE,"Could not allocate host ptr");
    if (flags & CL_MEM_COPY_HOST_PTR && host_ptr)
      std::memcpy(m_host_ptr,host_ptr,sz);

    m_aligned = (reinterpret_cast<uintptr_t>(m_host_ptr) % alignment)==0;
  }

  ~buffer()
  {
    if (m_host_ptr && (get_flags() & (CL_MEM_COPY_HOST_PTR | CL_MEM_ALLOC_HOST_PTR)))
      free(m_host_ptr);
  }

  virtual cl_mem_object_type
  get_type() const
  {
    return CL_MEM_OBJECT_BUFFER;
  }

  virtual void*
  get_host_ptr() const
  {
    return m_host_ptr;
  }

  virtual bool
  is_aligned() const
  {
    return m_aligned;
  }

  virtual size_t
  get_size() const
  {
    return m_size;
  }

  virtual void
  set_extra_sync()
  {
    m_extra_sync = true;
  }

  virtual bool
  need_extra_sync() const
  {
    return m_extra_sync;
  }

private:
  bool m_extra_sync = false;
  bool m_aligned = false;
  size_t m_size = 0;
  void* m_host_ptr = nullptr;
};

class sub_buffer : public buffer
{
public:
  sub_buffer(memory* parent,cl_mem_flags flags,size_t offset, size_t sz)
  : buffer(parent->get_context(),flags,sz,
           parent->get_host_ptr()
           ? static_cast<char*>(parent->get_host_ptr())+offset
           : nullptr)
  , m_parent(parent),m_offset(offset)
  {
    // sub buffer inherits parent CL_MEM_ALLOC_HOST_PTR and creates
    // its own ubuf even though it shares the buffer hbuf with parent
    auto phbuf = static_cast<const char*>(m_parent->get_host_ptr());
    auto shbuf = static_cast<const char*>(get_host_ptr());
    if (shbuf && (phbuf + m_offset != shbuf))
      set_extra_sync();
  }

  virtual size_t
  get_sub_buffer_offset() const
  {
    return m_offset;
  }

  virtual memory*
  get_sub_buffer_parent() const
  {
    return m_parent.get();
  }

  virtual const device*
  get_resident_device() const
  {
    if (auto d = memory::get_resident_device())
      return d;
    return m_parent->get_resident_device();
  }

  virtual bool
  is_resident() const
  {
    // sub buffer is resident if it itself is resident on some device
    if (memory::is_resident())
      return true;

    // or if parent is resident on some device
    if (m_parent->is_resident())
      return true;

    return false;
  }

  virtual bool
  is_resident(const device* device) const
  {
    // sub buffer is resident if it itself is marked resident
    if (memory::is_resident(device))
      return true;

    // or, if parent is resident
    if (m_parent->is_resident(device)) {
      // make sub buffer explicit resident, logically const
      const_cast<sub_buffer*>(this)->make_resident(device);
      return true;
    }
    return false;
  }

private:
  void
  make_resident(const device* device)
  {
    memory::get_buffer_object(const_cast<xocl::device*>(device));
    memory::set_resident(device);
  }


private:
  ptr<memory> m_parent;
  size_t m_offset;
};

class image : public buffer
{
  struct image_info {
    cl_image_format fmt;
    cl_image_desc desc;
  };

public:
  image(context* ctx,cl_mem_flags flags, size_t sz,
        size_t w, size_t h, size_t d,
        size_t row_pitch, size_t slice_pitch,
        uint32_t bpp, cl_mem_object_type type,
        cl_image_format fmt, void* host_ptr)
    : buffer(ctx,flags,sz+sizeof(image_info), host_ptr)
    , m_width(w), m_height(h), m_depth(d)
    , m_row_pitch(row_pitch), m_slice_pitch(slice_pitch)
    , m_bpp(bpp), m_image_type(type), m_format(fmt)
  {
  }

  virtual cl_mem_object_type
  get_type() const override
  {
    return m_image_type;
  }

  virtual cl_image_format
  get_image_format() override
  {
    return m_format;
  }

  virtual size_t
  get_image_data_offset() const override
  {
    return sizeof(image_info);
  }

  virtual size_t
  get_image_width() const override
  {
    return m_width;
  }

  virtual size_t
  get_image_height() const override
  {
    return m_height;
  }

  virtual size_t
  get_image_depth() const override
  {
    return m_depth;
  }

  virtual size_t
  get_image_bytes_per_pixel() const override
  {
    return m_bpp;
  }

  virtual size_t
  get_image_row_pitch() const override
  {
    return m_row_pitch;
  }

  virtual size_t
  get_image_slice_pitch() const override
  {
    return m_slice_pitch;
  }

  virtual void
  set_image_row_pitch(size_t pitch) override
  {
    m_row_pitch = pitch;
  }

  virtual void
  set_image_slice_pitch(size_t pitch) override
  {
    m_slice_pitch = pitch;
  }

  virtual buffer_object_handle
  get_buffer_object(device* device);

private:

  void populate_image_info(image_info& info) {
    memset(&info, 0, sizeof(image_info));
    info.fmt = m_format;
    info.desc.image_type=m_image_type;
    info.desc.image_width=m_width;
    info.desc.image_height=m_height;
    info.desc.image_depth=m_depth;
    info.desc.image_array_size=0;
    info.desc.image_row_pitch=m_row_pitch;
    info.desc.image_slice_pitch=m_slice_pitch;
    info.desc.num_mip_levels=0;
    info.desc.num_samples=0;
  }

private:
  size_t m_width;
  size_t m_height;
  size_t m_depth;
  size_t m_row_pitch;
  size_t m_slice_pitch;
  uint32_t m_bpp; //bytes per pixel
  cl_mem_object_type m_image_type;
  cl_image_format m_format;
};

/**
 * Pipes are not accessible from host code.
 */
class pipe : public memory
{
public:
  pipe(context* ctx,cl_mem_flags flags, cl_uint packet_size, cl_uint max_packets)
    : memory(ctx,flags), m_packet_size(packet_size), m_max_packets(max_packets)
  {}

  void
  set_pipe_host_ptr(void* p)
  {
    m_host_ptr = p;
  }

  virtual cl_mem_object_type
  get_type() const
  {
    return CL_MEM_OBJECT_PIPE;
  }

  virtual cl_uint
  get_pipe_packet_size() const
  {
    return m_packet_size;
  }

  virtual cl_uint
  get_pipe_max_packets() const
  {
    return m_max_packets;
  }

private:
  cl_uint m_packet_size = 0;
  cl_uint m_max_packets = 0;
  void* m_host_ptr = nullptr;
};

inline const void*
get_host_ptr(cl_mem_flags flags, const void* host_ptr)
{
  return (flags & CL_MEM_EXT_PTR_XILINX)
    ? reinterpret_cast<const cl_mem_ext_ptr_t*>(host_ptr)->host_ptr
    : host_ptr;
}

inline void*
get_host_ptr(cl_mem_flags flags, void* host_ptr)
{
  return (flags & CL_MEM_EXT_PTR_XILINX)
    ? reinterpret_cast<cl_mem_ext_ptr_t*>(host_ptr)->host_ptr
    : host_ptr;
}

inline unsigned int
get_xlnx_ext_flags(cl_mem_flags flags, const void* host_ptr)
{
  return (flags & CL_MEM_EXT_PTR_XILINX)
    ? reinterpret_cast<const cl_mem_ext_ptr_t*>(host_ptr)->flags
    : 0;
}

inline cl_kernel
get_xlnx_ext_kernel(cl_mem_flags flags, const void* host_ptr)
{
  return (flags & CL_MEM_EXT_PTR_XILINX)
    ? reinterpret_cast<const cl_mem_ext_ptr_t*>(host_ptr)->kernel
    : 0;
}

inline unsigned int
get_xlnx_ext_argidx(cl_mem_flags flags, const void* host_ptr)
{
  return get_xlnx_ext_flags(flags,host_ptr) & 0xffffff;
}

inline unsigned int
get_ocl_flags(cl_mem_flags flags)
{
  return ( flags & ~(CL_MEM_EXT_PTR_XILINX) );
}

} // xocl

#ifdef _WIN32
#pragma warning( pop )
#endif

#endif
