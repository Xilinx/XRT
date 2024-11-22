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

/**
 * This file contains the API for adapting the mixed xcl/xocl
 * data structures to the enqueuing infrastructure.
 *
 * Once xcl has been eliminated, this file should move to xocl/api
 * Temporarily, the file abuses the xocl namespace, but it cannot
 * currently be moved since enqueue.cpp has xcl dependencies that
 * are strictly forbidden in xocl.
 */

#include "enqueue.h"
#include "xocl/core/event.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/device.h"
#include "xocl/core/kernel.h"

namespace {

// Exception pointer for device exceptions during enqueue tasks.  The
// pointer is set with the exception thrown by the task.
static std::exception_ptr s_exception_ptr;

// This function is called only when an exception is in play
// Set the static global exception pointer to the current exception
static void
handle_device_exception(xocl::event* event, const std::exception& ex)
{
  static std::mutex m_mutex;
  std::lock_guard<std::mutex> lk(m_mutex);
  xocl::send_exception_message(ex.what());
  if (!s_exception_ptr)
    s_exception_ptr = std::current_exception();

  // Abort the event and any event dependencies.  Indicate
  // fatal error to forcefully abort from submitted state
  // in command queue.
  event->abort(-1,true/*fatal*/);

  // Re-throw the current exception
  throw;
}

// All enqueue actions are guarded against any earlier error
// from the device.  This function throws an xocl::error which
// will be caught by a clEnqueue API call.
inline void
throw_if_error()
{
  // Potential race condition on s_exception_ptr, but this
  // is on the exceptional code path, either s_exception_ptr
  // is not set, or set, or it is in a race to be set.  I don't
  // really care that it is half set and may not work.
  // Its more important to keep this function fast in the good case.
  try {
    if (s_exception_ptr)
      std::rethrow_exception(s_exception_ptr);
  }
  catch (const std::exception& ex) {
    throw xocl::error(CL_OUT_OF_RESOURCES,std::string("Operation failed due to earlier error '") + ex.what() + "'");
  }
}

using async_type = xrt_xocl::device::queue_type;

auto event_completer = [](xocl::event* ev)
{
  ev->set_status(CL_COMPLETE);
};

using unique_event_completer = std::unique_ptr<xocl::event,decltype(event_completer)>;
using shared_event_completer = std::shared_ptr<xocl::event>;

inline shared_event_completer
make_shared_event_completer(xocl::event* event)
{
  return shared_event_completer(event,event_completer);
}
inline unique_event_completer
make_unique_event_completer(xocl::event* event)
{
  return unique_event_completer(event,event_completer);
}

static void
fill_buffer(xocl::event* event,xocl::device* device
            ,cl_mem buffer,const void* pattern,size_t pattern_size,size_t offset,size_t size)
{
  try {
    event->set_status(CL_RUNNING);
    device->fill_buffer(xocl::xocl(buffer),pattern,pattern_size,offset,size);
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}

static void
copy_buffer(xocl::event* event,xocl::device* device
            ,cl_mem src_buffer,cl_mem dst_buffer,size_t src_offset,size_t dst_offset,size_t size)
{
  try {
    event->set_status(CL_RUNNING);
    device->copy_buffer(xocl::xocl(src_buffer),xocl::xocl(dst_buffer),src_offset,dst_offset,size);
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}

static void
map_buffer(xocl::event* event,xocl::device* device
           ,cl_mem buffer,cl_map_flags map_flags, size_t offset,size_t size,void* userptr)
{
  try {
    event->set_status(CL_RUNNING);
    device->map_buffer(xocl::xocl(buffer),map_flags,offset,size,userptr);
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}

static void
map_svm_buffer(xocl::event* event,xocl::device* device
               ,cl_map_flags map_flags,void* svm_ptr,size_t size)
{
  try {
    event->set_status(CL_RUNNING);
    // For MPSoC SVM, we don't need to sync device memory and host memory. Not sure what will happend in PCIe board.
    // Do nothing
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}

static void
read_buffer(xocl::event* event,xocl::device* device
            ,cl_mem buffer,size_t offset,size_t size,void* ptr)
{
  try {
    event->set_status(CL_RUNNING);
    device->read_buffer(xocl::xocl(buffer),offset,size,ptr);
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}

static void
write_buffer(xocl::event* event,xocl::device* device
             ,cl_mem buffer,size_t offset,size_t size,const void* ptr)
{
  try {
    event->set_status(CL_RUNNING);
    device->write_buffer(xocl::xocl(buffer),offset,size,ptr);
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}

static void
unmap_buffer(xocl::event* event,xocl::device* device
             ,cl_mem buffer, void* mapped_ptr)
{
  try {
    event->set_status(CL_RUNNING);
    device->unmap_buffer(xocl::xocl(buffer),mapped_ptr);
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}

static void
unmap_svm_buffer(xocl::event* event,xocl::device* device
                 ,void* svm_ptr)
{
  try {
    event->set_status(CL_RUNNING);
    // As map_svm_buffer, do nothing
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}

static void
migrate_buffer(shared_event_completer sec,xocl::device* device
               ,cl_mem buffer,cl_mem_migration_flags flags)
{
  // The time recorded for CL_RUNNING is from first mem object
  // starts migration.  If multiple buffers are migrated the
  // recorded CL_COMPLTE time (per shared_event_completer) is
  // after the last buffer is migrated. This is not accurate,
  // but is the best supported by OpenCL.
  try {
    sec->set_status(CL_RUNNING);
    device->migrate_buffer(xocl::xocl(buffer),flags);
  }
  catch (const std::exception& ex) {
    handle_device_exception(sec.get(),ex);
  }
}

static void
read_image(xocl::event* event,xocl::device* device,cl_mem image,
	const size_t* origin,const size_t* region, size_t row_pitch,size_t slice_pitch,
	void* ptr)
{
  try {
    event->set_status(CL_RUNNING);
    device->read_image(xocl::xocl(image),origin,region,row_pitch,slice_pitch,ptr);
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}

static void
write_image(xocl::event* event,xocl::device* device,cl_mem image,
	const size_t* origin,const size_t* region, size_t row_pitch,size_t slice_pitch,
	const void* ptr)
{
  try {
    event->set_status(CL_RUNNING);
    device->write_image(xocl::xocl(image),origin,region,row_pitch,slice_pitch,ptr);
    event->set_status(CL_COMPLETE);
  }
  catch (const std::exception& ex) {
    handle_device_exception(event,ex);
  }
}


} // namespace

namespace xocl { namespace enqueue {

xocl::event::action_enqueue_type
action_fill_buffer(cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size)
{
  throw_if_error();
  return [=](xocl::event* event) {
    auto command_queue = event->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    xdevice->schedule(fill_buffer,async_type::misc,event,device,buffer, pattern, pattern_size, offset, size);
  };
}

xocl::event::action_enqueue_type
action_copy_buffer(cl_mem src_buffer,cl_mem dst_buffer,size_t src_offset,size_t dst_offset,size_t size)
{
  throw_if_error();
  return [=](xocl::event* ev) {
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    copy_buffer(ev,device,src_buffer,dst_buffer,src_offset,dst_offset,size);
  };
}

xocl::event::action_enqueue_type
action_ndrange_migrate(cl_event event,cl_kernel kernel)
{
  throw_if_error();
  //Allocate all global/constant args onto target device
  auto command_queue = xocl::xocl(event)->get_command_queue();
  auto device = command_queue->get_device();

  // Create buffer objects for all arguments
  std::vector<xocl::memory*> kernel_args;
  for (auto& arg : xocl::xocl(kernel)->get_xargument_range()) {
    if (auto mem = arg->get_memory_object()) {
      if (mem->is_resident(device))
        continue;
      mem->get_buffer_object(device);
      kernel_args.push_back(mem);
    }
  }

  // Avoid complicated enqueue action if nothing to do
  if (kernel_args.empty())
    return [](xocl::event* ev) { ev->set_status(CL_COMPLETE); };

  return [kernel_args{std::move(kernel_args)}](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching ndrange migrate DMA event(",ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    auto ec = make_shared_event_completer(ev);

    for (auto mem : kernel_args) {
      // do not migrate if argument is write only, but trick the code
      // into assuming that the argument is resident
      if ((mem->get_flags() & CL_MEM_WRITE_ONLY) || (xocl(mem)->no_host_memory())) {
        mem->set_resident(device);
        continue;
      }

      // only migrate if not already resident on device
      if (!mem->is_resident(device)) {
        xdevice->schedule(migrate_buffer,async_type::write,ec,device,mem,0);
      }
    }
  };
}

xocl::event::action_enqueue_type
action_read_buffer(cl_mem buffer,size_t offset, size_t size, const void* ptr)
{
  throw_if_error();
  return [=](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching read buffer DMA event(",ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    xdevice->schedule(read_buffer,async_type::read,ev,device,buffer,offset,size,const_cast<void*>(ptr));
  };
}

xocl::event::action_enqueue_type
action_map_buffer(cl_event event,cl_mem buffer,cl_map_flags map_flags,size_t offset,size_t size,void** hostbase)
{
  throw_if_error();

  // Compute mapped host pointer in host thread
  auto command_queue = xocl::xocl(event)->get_command_queue();
  auto device = command_queue->get_device();
  auto userptr = device->map_buffer(xocl(buffer),map_flags,offset,size,nullptr,true/*nosync*/);
  *hostbase = userptr;

  // Event scheduler schedules the actual map copy through this lambda
  // stored as an event action.   We pass in the ptr computed for user
  // as a sanity check to ensure device->enqueueMapBuffer computes the
  // same address
  return [buffer,map_flags,offset,size,userptr](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching map buffer DMA event(",ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    xdevice->schedule(map_buffer,async_type::read,ev,device,buffer,map_flags,offset,size,userptr);
  };
}

xocl::event::action_enqueue_type
action_map_svm_buffer(cl_event event,cl_map_flags map_flags,void* svm_ptr,size_t size)
{
  throw_if_error();
  return [map_flags,svm_ptr,size](xocl::event* ev) {
    XOCL_DEBUG(std::cout, "launching map svm buffer event(", ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    xdevice->schedule(map_svm_buffer,async_type::read,ev,device,map_flags,svm_ptr,size);
  };
}

xocl::event::action_enqueue_type
action_write_buffer(cl_mem buffer,size_t offset, size_t size, const void* ptr)
{
  throw_if_error();
  return [=](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching write buffer DMA event(",ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    xdevice->schedule(write_buffer,async_type::write,ev,device,buffer,offset,size,ptr);
  };
}

xocl::event::action_enqueue_type
action_unmap_buffer(cl_mem memobj,void* mapped_ptr)
{
  throw_if_error();
  return [=](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching unmap DMA event(",ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    xdevice->schedule(unmap_buffer,async_type::write,ev,device,memobj,mapped_ptr);
  };
}

xocl::event::action_enqueue_type
action_unmap_svm_buffer(void* svm_ptr)
{
  throw_if_error();
  return [=](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching unmap svm buffer event(",ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    xdevice->schedule(unmap_svm_buffer,async_type::write,ev,device,svm_ptr);
  };
}

xocl::event::action_enqueue_type
action_read_image(cl_mem image,const size_t* origin,const size_t* region, size_t row_pitch,size_t slice_pitch,const void* ptr)
{
  throw_if_error();
  return [=](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching read image DMA event(",ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    xdevice->schedule(read_image,async_type::read,ev,device,image,origin,region,row_pitch,slice_pitch,const_cast<void*>(ptr));
  };
}

xocl::event::action_enqueue_type
action_write_image(cl_mem image,const size_t* origin,const size_t* region,size_t row_pitch,size_t slice_pitch,const void* ptr)
{
  throw_if_error();
  return [=](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching write image DMA event(",ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    xdevice->schedule(write_image,async_type::write,ev,device,image,origin,region,row_pitch,slice_pitch,ptr);
  };
}

xocl::event::action_enqueue_type
action_migrate_memobjects(size_t num, const cl_mem* memobjs, cl_mem_migration_flags flags)
{
  throw_if_error();
  std::vector<cl_mem> mo(memobjs,memobjs+num);

  return [mo = std::move(mo), flags](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching migrate DMA event(",ev->get_uid(),")\n");
    auto command_queue = ev->get_command_queue();
    auto device = command_queue->get_device();
    auto xdevice = device->get_xdevice();
    auto ec = make_shared_event_completer(ev);
    for (auto mem : mo) {
      // do not migrate if argument is CL_MIGRATE_MEM_OBJECT_CONTENT_UNDERFINED
      // but trick code into assuming that the argument is resident
      if (flags & CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED) {
        // at least allocate buffer on device if necessary
        xocl::xocl(mem)->get_buffer_object(device);
        xocl::xocl(mem)->set_resident(device);
        continue;
      }

      auto at = (flags & CL_MIGRATE_MEM_OBJECT_HOST) ? async_type::read : async_type::write;
      xdevice->schedule(migrate_buffer,at,ec,device,mem,flags);
    }
  };
}

xocl::event::action_enqueue_type
action_ndrange_execute()
{
  return [](xocl::event* ev) {
    XOCL_DEBUG(std::cout,"launching ndrange execute CU event(",ev->get_uid(),")\n");
    ev->get_execution_context()->execute();
  };
}


}}
