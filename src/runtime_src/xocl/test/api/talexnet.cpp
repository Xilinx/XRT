/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include <boost/test/unit_test.hpp>
#include "setup.h"

#include <CL/opencl.h>
#include <vector>
#include <bitset>
#include <array>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <limits>
#include <atomic>
#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdarg>

#ifdef RDIPF_x8664
# define NULLPTR_INITIALIZER {nullptr}
#else
# define NULLPTR_INITIALIZER
#endif 

// Emulate hardware command queue and result register
#define EMULATE_HARDWARE

#ifdef EMULATE_HARDWARE
# define EMULATION_NAMESPACE emulate
#else
# define EMULATION_NAMESPACE
#endif

#define ALEXNET alexnet

//#define VERBOSE

#ifdef VERBOSE
namespace { namespace debug {

static std::mutex mutex;
void
logf(const char* format, ...)
{
  std::lock_guard<std::mutex> lk(mutex);
  va_list args;
  va_start(args,format);
  vfprintf(stdout,format,args);
  fflush(stdout);
  va_end(args);
}

}}

# define LOG(format,...) ::debug::logf(format, ##__VA_ARGS__)
#else
# define LOG(...)
#endif  // VERBOSE

////////////////////////////////////////////////////////////////
// Some literals
////////////////////////////////////////////////////////////////
constexpr std::size_t 
operator""_kb(unsigned long long v)  { return 1024u * v; }
constexpr std::size_t 
operator""_mb(unsigned long long v)  { return 1024u * 1024u * v; }
constexpr std::size_t 
operator""_gb(unsigned long long v)  { return 1024u * 1024u * 1024u * v; }
constexpr std::chrono::microseconds 
operator""_us(unsigned long long us) { return std::chrono::microseconds(us); }
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
// User contract, move to header
////////////////////////////////////////////////////////////////
const auto input_reserve = 1_gb;
const auto output_reserve = 1_mb;
const auto input_size = 4_mb;
const auto output_size = 4_kb;
const auto image_status_throttle = 1000_us;
const uintptr_t image_status_register_address = 0;
struct input_type  { char data[input_size]; };
struct output_type { char data[output_size]; };

namespace ALEXNET {

/**
 * Add an image to the network
 *
 * Blocks if the network is saturated.  When function
 * returns, the image has successfully been added.
 */
void 
add(const input_type& in);
 
/**
 * Get processed image
 *
 * Blocks until an image is avaiable. 
 */
void
get(output_type& out);

void
setup();

void
stop();


} // alexnet
////////////////////////////////////////////////////////////////

#ifdef EMULATE_HARDWARE

////////////////////////////////////////////////////////////////
// Stub out ocl extension that requires require readback of
// hardware register to get address of processed image.  
// The stubbed out function simply returns the ddr address of the
// last image transferred from host->ddr.
////////////////////////////////////////////////////////////////
namespace EMULATION_NAMESPACE {

static void
host2ddr_done(cl_event,cl_int,void* data);

static cl_int
clEnqueueWriteBuffer(cl_command_queue   command_queue, 
                     cl_mem             buffer, 
                     cl_bool            blocking, 
                     size_t             offset, 
                     size_t             size,  
                     const void *       ptr, 
                     cl_uint            num_events_in_wait_list , 
                     const cl_event *   event_wait_list , 
                     cl_event *         event_parameter);

static cl_int
clEnqueueReadBuffer(cl_command_queue   command_queue, 
                    cl_mem             buffer, 
                    cl_bool            blocking, 
                    size_t             offset, 
                    size_t             size,  
                    void *             ptr, 
                    cl_uint            num_events_in_wait_list , 
                    const cl_event *   event_wait_list , 
                    cl_event *         event_parameter);

} // emulate

#endif // EMULATE_HARDWARE


////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////
// Hardware assumptions:
// =====================
//  - An input queue with fixed number of entries where each entry can
//    be written by software with the ddr address of where an image is
//    located and the ddr address of where the result should be placed
//  - A result register with same number of bits as entries in the
//    input queue, where a bit at some location indicates whether (1) or
//    not (0) the corresponding image in the input queue is done.  This
//    register is clear on read
//
// User software:
// ==============
//  - alexnet::add(const input_type&):  adds an image to be processed
//  - alexnet::get(output_type&): returns a processed image
//  - alexnet::setup(): initializes and starts implementation threads
//  - alexnet::stop(): brings down the implementation
//
// Implementation software:
// ========================
//  - Network is initialized (setup())
//    * Initializes OpenCL
//    * Reserves device DDR space for images
//    * Carves out CL sub_buffers for individual images
//    * Starts implementation threads
//
//  - thread 1 (host2ddr::image2ddr())
//    * Waits for image to be added by user
//    * Enqueues migration of the image at reserved available ddr address.
//    * Upon successful migration, receives an event callback that in turn
//      triggers insertion into the hardware input queue
//
//  - thread 2 (ddr2host::check_hardware())
//    * Checks hardware result register for images that are done
//    * Sleeps for specified microseconds if no image is done
//    * Adds image to set of ready images to be transferred from
//      ddr to host
//
//  - thread 3 (ddr2host::image2host())
//    * Waits for ready images per check_hardware() thread
//    * Enqueue read back of ready image at ddr address
//    * Upon transfer complete, receives an event callback that in turn
//      inserts the transferred image into a list that can be consumed 
//      by user.
////////////////////////////////////////////////////////////////

namespace {

/**
 * @return
 *   nanoseconds since first call
 */
unsigned long
time_ns()
{
  static auto zero = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto integral_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now-zero).count();
  return integral_duration;
}

////////////////////////////////////////////////////////////////
// An image pool is a set of pre allocated image buffers.
//
// An image buffer encapsulates a cl_mem object that is transferred to
// DDR on device when application receives an image input
//
// An image buffer encapsulates the device DDR address of where the image
// will reside on the DDR.  This address is passed to the hardware 
// write kernel to indicate a ready-to-be-processed image on device.
//
// The idea here is that an image buffer is stored permanently in the
// pool and that the pool keeps track of which buffer is currently
// ununsed.  Buffers are recycled for reuse when no longer in use.
////////////////////////////////////////////////////////////////
class image_pool
{
public:
  struct buffer
  {
    const size_t idx; // this buffers location in pre-allocated set

    cl_mem    w_mem;         // cl_mem sub_buffer object for host->ddr
    void*     w_hbuf;        // hbuf of underlying wmem boh [hbuf,dbuf]
    uintptr_t w_dbuf;        // dbuf of underlying wmem boh [hbuf,dbuf]

    cl_mem    r_mem;         // cl_mem sub_buffer object for ddr->host
    void*     r_hbuf;        // hbuf of underlying wmem boh [hbuf,dbuf]
    uintptr_t r_dbuf;        // dbuf of underlying wmem boh [hbuf,dbuf]
  };

  // Get some currently unused buffer
  const buffer* 
  get()
  {
    std::unique_lock<std::mutex> lk(mutex);
    while (m_unused_buffers.empty())
      wait_for_buffer.wait(lk);

    LOG("#unused image buffers: %d\n",m_unused_buffers.size());
    auto index = m_unused_buffers.back();
    m_unused_buffers.pop_back();
    return &m_buffers[index];
  }

  // Recycle a buffer, that is, add to list of unused buffers
  void
  recycle(const buffer* image)
  {
    std::lock_guard<std::mutex> lk(mutex);
    m_unused_buffers.push_back(image->idx);
    wait_for_buffer.notify_one();
  }

  // Construct a new buffer and add to list of unused buffers
  void
  add(cl_mem w_buf, uintptr_t w_hbuf, uintptr_t w_dbuf, cl_mem r_buf, uintptr_t r_hbuf, uintptr_t r_dbuf)
  {
    auto idx = m_buffers.size();
    m_buffers.emplace_back(buffer{idx
          ,w_buf,reinterpret_cast<void*>(w_hbuf),w_dbuf
          ,r_buf,reinterpret_cast<void*>(r_hbuf),r_dbuf});
    m_unused_buffers.push_back(idx);
  }

  // Reserve space for some number of buffers
  void
  reserve(size_t sz)
  {
    m_buffers.reserve(sz);
    m_unused_buffers.reserve(sz);
  }

  void
  release()
  {
    for (auto& ibuf : m_buffers) {
      clReleaseMemObject(ibuf.w_mem);
      clReleaseMemObject(ibuf.r_mem);
    }
    m_buffers.clear();
    m_unused_buffers.clear();
  }


private:
  using buffer_vec   = std::vector<buffer>;
  using buffer_idx   = std::vector<buffer>::size_type;
  using buffer_stack = std::vector<buffer_idx>;

  // pre-allocated image buffers 
  buffer_vec m_buffers;

  // indeces (into m_buffers) of currently unused image buffers
  buffer_stack m_unused_buffers;
  std::mutex mutex;
  std::condition_variable wait_for_buffer;
};


// Convenience typedef
using image_buffer = image_pool::buffer;

// OCL variables initialized at setup
static cl_platform_id platform = nullptr;
static cl_device_id device = nullptr;
static cl_context context = nullptr;
static cl_command_queue queue = nullptr;

// Handle to reserved memory objects for ddr
static cl_mem w_ddr = nullptr;
static cl_mem r_ddr = nullptr;

// Pool of pre-allocated image buffers
static image_pool buffers;

// Get an available image buffer from pool 
inline const image_buffer*
get_unused_buffer()
{
  return buffers.get();
}

// Return an unused image buffer
inline void
recycle_buffer(const image_buffer* buffer)
{
  buffers.recycle(buffer);
}

static void
init_hardware();

static void
init_ocl()
{
  cl_int err = clGetPlatformIDs(1,&platform,nullptr);
  assert(err==CL_SUCCESS);

  err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ACCELERATOR, 1, &device, nullptr);
  assert(err==CL_SUCCESS);

  context=clCreateContext(0, 1, &device, nullptr, nullptr, &err);
  assert(err==CL_SUCCESS);

  queue = clCreateCommandQueue(context,device,0,&err);
  assert(err==CL_SUCCESS);
}

static void
init_pool()
{
  // Allocate ddr reserved memory
  cl_int err = CL_SUCCESS;
  w_ddr = clCreateBuffer(context,CL_MEM_WRITE_ONLY,input_reserve,nullptr,&err);
  assert(err==CL_SUCCESS);
  r_ddr = clCreateBuffer(context,CL_MEM_READ_ONLY,output_reserve,nullptr,&err);
  assert(err==CL_SUCCESS);

  // Get the ddr buffer object host ptr
  auto w_hbuf = clEnqueueMapBuffer(queue,w_ddr,CL_TRUE,CL_MAP_WRITE_INVALIDATE_REGION,0,input_size,0,0,nullptr,&err);
  assert(err==CL_SUCCESS);
  clEnqueueUnmapMemObject(queue,w_ddr,w_hbuf,0,nullptr,nullptr);
  auto r_hbuf = clEnqueueMapBuffer(queue,r_ddr,CL_TRUE,CL_MAP_WRITE_INVALIDATE_REGION,0,output_size,0,0,nullptr,&err);
  assert(err==CL_SUCCESS);
  clEnqueueUnmapMemObject(queue,r_ddr,r_hbuf,0,nullptr,nullptr);

  // Get the ddr buffer object device addr
  uintptr_t w_dbuf = 0;
  err = xclGetMemObjDeviceAddress(w_ddr,device,sizeof(uintptr_t),&w_dbuf);
  assert(err==CL_SUCCESS);
  uintptr_t r_dbuf = 0;
  err = xclGetMemObjDeviceAddress(r_ddr,device,sizeof(uintptr_t),&r_dbuf);
  assert(err==CL_SUCCESS);

  // Make the ddr memory object device resident, but do not DMA
  // undefined content
  cl_event migrate_event = nullptr;
  cl_mem ddr[2] = {w_ddr,r_ddr};
  err = clEnqueueMigrateMemObjects(queue,2,ddr,CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED,0,nullptr,&migrate_event);
  assert(err==CL_SUCCESS);
  err = clWaitForEvents(1,&migrate_event);
  assert(err==CL_SUCCESS);
  err = clReleaseEvent(migrate_event);
  assert(err==CL_SUCCESS);

  // Carve out sub buffers, create and deposit image buffers in the pool.
  // The sub buffers are automatically device resident.
  assert((input_reserve % input_size)==0);
  assert((output_reserve % output_size)==0);
  assert((input_reserve/input_size)==(output_reserve/output_size));
  buffers.reserve(input_reserve/input_size);
  for (size_t woffset=0,roffset=0; woffset<input_reserve; woffset+=input_size, roffset+=output_size) {
    cl_buffer_region wr { woffset, input_size };
    auto wmem = clCreateSubBuffer(w_ddr,CL_MEM_WRITE_ONLY,CL_BUFFER_CREATE_TYPE_REGION,&wr,&err);
    assert(err==CL_SUCCESS);
    cl_buffer_region rr { roffset, output_size };
    auto rmem = clCreateSubBuffer(r_ddr,CL_MEM_READ_ONLY,CL_BUFFER_CREATE_TYPE_REGION,&rr,&err);
    assert(err==CL_SUCCESS);
    
    auto whbuf = reinterpret_cast<uintptr_t>(w_hbuf) + woffset;
    auto wdbuf = w_dbuf + woffset;
    auto rhbuf = reinterpret_cast<uintptr_t>(r_hbuf) + roffset;
    auto rdbuf = r_dbuf + roffset;

    buffers.add(wmem,whbuf,wdbuf,rmem,rhbuf,rdbuf);
  }
}

static void 
setup()
{
  init_ocl();
  init_hardware();
  init_pool();
}

static void
setdown()
{
  buffers.release();
  clReleaseMemObject(w_ddr);
  clReleaseMemObject(r_ddr);
  clReleaseCommandQueue(queue);
  clReleaseContext(context);
  clReleaseDevice(device);
}

////////////////////////////////////////////////////////////////
// Hardware interface
//
// This represents the hardware assumptions
//   - Notify hardware that an image is transferred and
//     available at ddr address. 
//   - Inquire hardware about images that are processed and
//     ready to be transferred back to host.
//
// The hardware maintains an input queue of 256 entries and an
// output register with 256 bits.
//   - Entries in the input queue are populated with address of
//     where an input images is located and the address where the
//     corresponding processed output should be placed.
//   - The output queue is a 256 bit register where bits are
//     set to '1' to indicate that the image that was placed 
//     at the corresponding location in the input queue is now
//     done and ready to be tranferred back to host.  The output
//     register is clear on read.
//
// The software side maintains a bitset<256> for used entries in
// the corresponding hardware input queue. A '0' in the used bitset
// indicates that the corresponding hardware input queue entry is
// available, a '1' represents that the entry is currently in use.
// 
// The software maintains a bitset<256> for completed entries. This 
// bitset<256> is reset with new entries from the hardware output 
// register when this register is read.  
//
// The hardware interface is simple, it supports starting an image
// and retrieving a result.
////////////////////////////////////////////////////////////////
namespace hardware {

/**
 * Representation of hardware queue for pushing new images to be 
 * processed, and popping off completed images.
 */
class queue_type
{
public:
  /**
   * Queue an image. Notify hardware input queue has room.
   *
   * The image has already been transferred to the DDR on the device.
   *
   * If hardware is full, the image remains queued until completed
   * images are retrieved from hardware
   */
  void
  write(const image_buffer* ibuf)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_queued.push_back(ibuf);
    start_images();
  }

  /**
   * Get result from hardware.
   *
   * Upon successful retrieval, the function attempts to start a
   * pending queued image if any.
   *
   * @return
   *   image buffer reprensenting the image that is done processing
   */
  const image_buffer*
  get()
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto idx = get_complete_idx();
    if (idx != noidx) {
      auto ibuf = m_running[idx];
      m_running[idx] = nullptr;
      // room for at least one image now so start if any pending
      start_images();
      return ibuf;
    }
    return nullptr;
  }
  
  queue_type()
  {}
 
  ~queue_type()
  {
    if (m_read)
      clReleaseMemObject(m_read);
    if (m_write)
      clReleaseMemObject(m_write);
  }

  void
  init()
  {
    cl_int err = 0;
    m_read = clCreateBuffer(context,CL_MEM_REGISTER_MAP,sizeof(cl_int),nullptr,&err);
    assert(err==CL_SUCCESS);
    m_write = clCreateBuffer(context,CL_MEM_REGISTER_MAP,sizeof(cl_int),nullptr,&err);
    assert(err==CL_SUCCESS);
  }

private:
  const size_t noidx   = std::numeric_limits<size_t>::max();

  void
  start_images()
  {
    if (m_queued.empty())
      return;

    auto slots = m_used.size() - m_used.count();
    while (slots-- && !m_queued.empty()) {
      auto ibuf = m_queued.back();
      m_queued.pop_back();
      auto idx = acquire_queue_index();
      assert(idx!=noidx);
      assert(m_used.test(idx));
      assert(!m_running[idx]);
      assert(idx<256);
      const auto size = 4 + sizeof(uintptr_t) * 2;
      uint8_t data[size] = {0};
      std::memcpy(data,&idx,4);
      std::memcpy(data+4,&ibuf->w_dbuf,sizeof(uintptr_t));
      std::memcpy(data+4+sizeof(uintptr_t),&ibuf->r_dbuf,sizeof(uintptr_t));
      // to check in debugger: gdb> p *((size_t*)(data+12))
      EMULATION_NAMESPACE::clEnqueueWriteBuffer(::queue,m_write,CL_TRUE,m_write_offset,size,data,0,nullptr,nullptr);
      m_running[idx] = ibuf;
    }
  }

  // Get first unused queue index
  // Mark as used before returning to caller
  size_t 
  acquire_queue_index()
  {
    if (m_used.all())
      return noidx;

    size_t idx = 0;
    while (idx < m_used.size() && m_used.test(idx)) 
      ++idx;
    assert(idx<m_used.size());
    m_used.set(idx,true);
    LOG("#used: %d\n",m_used.count());
    return idx;
  }

  // Get index of a complete image
  // Check hardware if no remaining complete images
  size_t
  get_complete_idx()
  {
    if (m_complete.none()) {
      const size_t bytes = 256/8;
      uint8_t data[bytes] = {0};
      EMULATION_NAMESPACE::clEnqueueReadBuffer(::queue,m_read,CL_TRUE,m_read_offset,bytes,data,0,nullptr,nullptr);
      
      size_t idx = 0;
      for (auto byte : data)
        for (size_t b=0; byte && b<8; ++b)
          m_complete.set(idx++,byte >> b & 1);
    }

    LOG("#complete: %d\n",m_complete.count());

    if (m_complete.none())
      return noidx;

    size_t idx = 0; // first complete index
    while (idx<m_complete.size() && !m_complete.test(idx) && ++idx) ;
    assert(idx<m_complete.size());
    assert(m_used.test(idx));
    m_complete.set(idx,false);
    m_used.set(idx,false);
    return idx;
  }

#ifdef EMULATE_HARDWARE
public:
  std::bitset<256> get_used() const { return m_used; }
#endif

private:
  cl_mem m_read = nullptr;
  cl_mem m_write = nullptr;
  const size_t m_write_offset = 0;          // input ddr address
  const size_t m_read_offset = 0;           // result ddr address

  std::mutex m_mutex;
  std::bitset<256> m_used;
  std::bitset<256> m_complete;
  std::array<const image_buffer*,256> m_running NULLPTR_INITIALIZER;
  std::vector<const image_buffer*> m_queued;
};

static queue_type queue;

} // hardware

static void
init_hardware()
{
  hardware::queue.init();
}

namespace host2ddr {

namespace detail {

static std::vector<const image_buffer*> images;
static std::mutex mutex;
static std::condition_variable wait_for_image;

static bool stop = false;
static std::vector<std::thread> threads;

static std::atomic<unsigned int> active_transfers{0};

static void
image2ddr_done(cl_event,cl_int,void* data)
{
  auto t = --active_transfers;
  auto ibuf = reinterpret_cast<const image_buffer*>(data);
  LOG("host2ddr done for image #%d.  Active transfers: %d\n",ibuf->idx,t);

  // Notify hardware
  hardware::queue.write(ibuf);
}

// thread
static void 
image2ddr()
{
  while (!stop) {

    const image_buffer* ibuf = nullptr;

    {
      // critical section, blocks add_image
      std::unique_lock<std::mutex> lk(mutex);
      while (!stop && images.empty())
        wait_for_image.wait(lk);

      if (stop)
        break;

      ibuf = images.back();
      images.pop_back();
    }

    // sync ibuf's mem object from host->device
    ++active_transfers;
    cl_event migrate_event = nullptr;
    cl_int err = clEnqueueMigrateMemObjects(queue,1,&ibuf->w_mem,0,0,nullptr,&migrate_event);
    assert(err==CL_SUCCESS);
    err = clSetEventCallback(migrate_event,CL_COMPLETE,image2ddr_done,const_cast<image_buffer*>(ibuf));
    err = clReleaseEvent(migrate_event);
    assert(err==CL_SUCCESS);

  } // while
}

} // detail

static void
stop()
{
  {
    std::lock_guard<std::mutex> lk(detail::mutex);
    detail::stop = true;

    detail::wait_for_image.notify_all();
  }

  for (auto& t : detail::threads)
    t.join();

  detail::threads.clear();
  detail::images.clear();
}

static void
start()
{
  std::lock_guard<std::mutex> lk(detail::mutex);
  detail::stop = false;
  detail::threads.emplace_back(std::thread(detail::image2ddr));
}

static void
add(const image_buffer* ibuf)
{
  // Add an image buffer into the list of images to send to ddr
  std::lock_guard<std::mutex> lk(detail::mutex);
  detail::images.emplace_back(ibuf);
  LOG("#pending images: %d\n",detail::images.size());
  detail::wait_for_image.notify_one();
}

} // host2ddr

namespace ddr2host {

namespace detail {

// List of image buffers ready to be transferred from ddr->host
static std::vector<const image_buffer*> ddr_images;
static std::mutex transfer_mutex;
static std::condition_variable wait_for_ddr_image;

// Images that have been transferred from ddr->host 
// and are ready to be consumed by user
static std::vector<const image_buffer*> ready_images;
static std::mutex ready_mutex;
static std::condition_variable wait_for_ready_image;

static bool stop = false;
static std::vector<std::thread> threads;

static std::atomic<unsigned int> active_transfers{0};

// Event callback for ddr -> host transfer
// When called, it implies that the image is ready for 
// external consumption.
static void
transfer_done(cl_event,cl_int,void* data)
{
  auto t = --active_transfers;
  auto ibuf = reinterpret_cast<const image_buffer*>(data);
  LOG("ddr2host done for image #%d.  Active transfers: %d\n",ibuf->idx,t);
  
  std::lock_guard<std::mutex> lk(ready_mutex);
  ready_images.push_back(ibuf);
  wait_for_ready_image.notify_one();
}

// Thread routine for checking status of hardware image processing
static void
check_hardware()
{
  while (!stop) {

    // read hardware register for ddr address of ready image
    if (auto ibuf = hardware::queue.get()) {
      std::lock_guard<std::mutex> lk(transfer_mutex);
      ddr_images.push_back(ibuf);
      wait_for_ddr_image.notify_one();
    }
    else {
      std::this_thread::sleep_for(image_status_throttle);
    }

  } // while
}

// Thread routine for reading back images from ddr
static void
image2host()
{
  while (!stop) {

    const image_buffer* ibuf = nullptr;
    size_t addr = 0;
    {
      std::unique_lock<std::mutex> lk(transfer_mutex);
      while (!stop && ddr_images.empty())
        wait_for_ddr_image.wait(lk);

      if (stop)
        break;

      ibuf = ddr_images.back();
      ddr_images.pop_back();
    }

    // sync ibuf's mem object from device->host
    // when migration is complete, call transfer_done
    ++active_transfers;
    auto mem = ibuf->r_mem;
    cl_event migrate_event = nullptr;
    cl_int err = clEnqueueMigrateMemObjects(queue,1,&mem,CL_MIGRATE_MEM_OBJECT_HOST,0,nullptr,&migrate_event);
    assert(err==CL_SUCCESS);
    err = clSetEventCallback(migrate_event,CL_COMPLETE,transfer_done,const_cast<image_buffer*>(ibuf));
    assert(err==CL_SUCCESS);
    clReleaseEvent(migrate_event);
  } // while
}

} // detail

static void
stop()
{
  {
    std::lock_guard<std::mutex> lk(detail::transfer_mutex);
    detail::stop = true;

    detail::wait_for_ddr_image.notify_all();
    detail::wait_for_ready_image.notify_all();
  }

  for (auto& t : detail::threads)
    t.join();

  detail::threads.clear();
  detail::ddr_images.clear();
  detail::ready_images.clear();
}

static void
start()
{
  std::lock_guard<std::mutex> lk(detail::transfer_mutex);
  detail::stop = false;
  detail::threads.emplace_back(std::thread(detail::check_hardware));
  detail::threads.emplace_back(std::thread(detail::image2host));
}

static const image_buffer*
get()
{
  std::unique_lock<std::mutex> lk(detail::ready_mutex);
  while (!detail::stop && detail::ready_images.empty())
    detail::wait_for_ready_image.wait(lk);
  
  if (detail::stop)
    return nullptr;

  LOG("#ready images: %d\n",detail::ready_images.size());
  
  auto ibuf = detail::ready_images.back();
  detail::ready_images.pop_back();
  return ibuf;
}

} // ddr2host

} // namespace


namespace ALEXNET {

void 
add(const input_type& in)
{
  auto ibuf = get_unused_buffer();

  // memcpy image to mem objects hbuf, which is cached in ibuf
  auto wptr = ibuf->w_hbuf;
  std::memcpy(wptr,in.data,input_size);

  host2ddr::add(ibuf);
}

void
get(output_type& out) 
{
  if (auto ibuf = ddr2host::get()) {
    std::memcpy(out.data,ibuf->r_hbuf,output_size);
    recycle_buffer(ibuf);
  }
}

void
setup()
{
  ::setup();
  ::host2ddr::start();
  ::ddr2host::start();
}

void
stop()
{
  ::setdown();
  ::host2ddr::stop();
  ::ddr2host::stop();
}

}

#ifdef EMULATE_HARDWARE
////////////////////////////////////////////////////////////////
// Emulate hardware.  This simplies simulates reading back of hardware
// status register and writing to the harware input queue
////////////////////////////////////////////////////////////////
namespace EMULATION_NAMESPACE {

static std::mutex mutex;
static std::vector<uintptr_t> ddr_addr;

cl_int
clEnqueueReadBuffer(cl_command_queue   command_queue, 
                    cl_mem             buffer, 
                    cl_bool            blocking, 
                    size_t             offset, 
                    size_t             size,  
                    void *             ptr, 
                    cl_uint            num_events_in_wait_list , 
                    const cl_event *   event_wait_list , 
                    cl_event *         event_parameter)
{
  // Artifical 10ms delay between reads
  static auto last = time_ns();
  if ( (time_ns() - last) < 10000000) {
    return CL_SUCCESS;
  }
  last = time_ns();

  assert(size==256/8);
  auto running = hardware::queue.get_used();

  auto idx = 0;
  auto data = reinterpret_cast<uint8_t*>(ptr);
  auto end = data + size;
  for (auto itr=data; itr!=end; ++itr) {
    auto& byte = (*itr);
    for (size_t b=0; b<8; ++b) {
      if (b)
        byte = byte >> 1;
      if (running.test(idx++))
        byte = byte | 128;
    }
  }
  return CL_SUCCESS;
}

static cl_int
clEnqueueWriteBuffer(cl_command_queue   command_queue, 
                     cl_mem             buffer, 
                     cl_bool            blocking, 
                     size_t             offset, 
                     size_t             size,  
                     const void *       ptr, 
                     cl_uint            num_events_in_wait_list , 
                     const cl_event *   event_wait_list , 
                     cl_event *         event_parameter)
{
  return CL_SUCCESS;
}

} // EMULATE_NAMESPACE
////////////////////////////////////////////////////////////////
#endif // EMULATE_HARDWARE

BOOST_AUTO_TEST_SUITE ( test_alexnet )

BOOST_AUTO_TEST_CASE( test_alexnet1 )
{
  ALEXNET::setup();
  ALEXNET::stop();
}

BOOST_AUTO_TEST_CASE( test_alexnet2 )
{
  ALEXNET::setup();

  std::unique_ptr<input_type> img_in(new input_type);
  ALEXNET::add(*img_in);
  std::unique_ptr<output_type> img_out(new output_type);
  ALEXNET::get(*img_out);

  ALEXNET::stop();
}

BOOST_AUTO_TEST_CASE( test_alexnet3 )
{
  auto seconds = 5;

  ALEXNET::setup();

  unsigned int count = 1000;
  unsigned int added = 0;
  unsigned int consumed = 0;
  unsigned long timeout = seconds * 1e9;

  auto now = time_ns();
  auto writer = [now,timeout,&added] () {
    std::unique_ptr<input_type> img_in(new input_type);
    while ((time_ns() - now) <timeout) {
      ALEXNET::add(*img_in);
      ++added;
    }
  };

  auto reader = [now,timeout,&consumed] () {
    std::unique_ptr<output_type> img_out(new output_type);
    while ((time_ns() - now) < timeout) {
      ALEXNET::get(*img_out);
      ++consumed;
    }
  };

  std::vector<std::thread> threads;
  threads.push_back(std::thread(writer));
  threads.push_back(std::thread(reader));
  
  for (auto& t: threads)
    t.join();

  auto backlog = added - consumed;

  std::unique_ptr<output_type> img_out(new output_type);
  while (consumed < added) {
    ALEXNET::get(*img_out);
    ++consumed;
  }

  auto runtime = time_ns() - now;

  BOOST_TEST_MESSAGE("Runtime: " << runtime * 1e-6 << " ms");
  BOOST_TEST_MESSAGE("Backlog: " << backlog);
  BOOST_TEST_MESSAGE("Added: " << added);
  BOOST_TEST_MESSAGE("Consumed: " << consumed);

  auto bytes_written = added * input_size;
  auto bytes_read = consumed * output_size;
  auto bytes_total = bytes_written + bytes_read;

  BOOST_TEST_MESSAGE("transfer rate (host<->ddr): " << bytes_total/(1_mb * runtime * 1e-9) << " MB/s");
    
  ALEXNET::stop();
}

BOOST_AUTO_TEST_SUITE_END()


