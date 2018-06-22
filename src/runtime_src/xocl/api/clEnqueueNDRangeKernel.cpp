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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include <CL/opencl.h>
#include "xocl/config.h"
#include "xocl/core/debug.h"
#include "xocl/core/time.h"
#include "xocl/core/range.h"
#include "xocl/core/device.h"
#include "xocl/core/program.h"
#include "xocl/core/context.h"
#include "xocl/core/execution_context.h"

#include "detail/command_queue.h"
#include "detail/kernel.h"
#include "detail/event.h"

#include "enqueue.h"
#include "profile.h"
#include "appdebug.h"
#include "api.h"

#include "xrt/util/memory.h"

#include "xdp/debug/rt_printf.h"

#include <sstream>

namespace {

// Forward declration of helper functions defined at end of file
static void
cb_BufferInitialized(cl_event event, cl_int status, void *data);

static void 
cb_BufferReturned(cl_event event, cl_int status, void *data);

static xocl::ptr<xocl::memory>
createPrintfBuffer(cl_context context, cl_kernel kernel,
                   const std::vector<size_t>& gsz, const std::vector<size_t>& lsz);

static cl_event 
enqueueInitializePrintfBuffer(cl_kernel kernel, cl_command_queue queue,cl_mem mem);

static cl_int 
enqueueReadPrintfBuffer(cl_kernel kernel, cl_command_queue queue,
                        cl_mem mem, cl_event waitEvent,
                        cl_event* event_param);

static cl_uint
getDeviceAddressBits(cl_device_id device)
{
  static cl_uint bits = 0;
  if (bits)
    return bits;
  xocl::api::clGetDeviceInfo(device,CL_DEVICE_ADDRESS_BITS,sizeof(cl_uint),&bits,nullptr);
  return bits;
}

static bool
is_sw_emulation()
{
// TODO check for only sw_emu. Some github examples are using "true", Remove this check once all github examples are updated 
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? (std::strcmp(xem,"sw_emu")==0) : false;
  return swem;
} 

static size_t
getDeviceMaxWorkGroupSize(cl_device_id device) 
{
  static size_t size = 0;
  if (size)
    return size;
  xocl::api::clGetDeviceInfo(device,CL_DEVICE_MAX_WORK_GROUP_SIZE,sizeof(size_t),&size,nullptr);
  return size;
}

static size_t*
getDeviceMaxWorkItemSizes(cl_device_id device) 
{
  static size_t sizes[3] = {0,0,0};
  if (sizes[0])
    return sizes;
  xocl::api::clGetDeviceInfo(device,CL_DEVICE_MAX_WORK_ITEM_SIZES,sizeof(size_t)*3,&sizes,nullptr);
  return sizes;
}

}

namespace xocl {

static void
validOrError(cl_command_queue command_queue,
             cl_kernel        kernel,
             cl_uint          work_dim,
             const size_t *   global_work_offset,
             const size_t *   global_work_size,
             const size_t *   local_work_size,
             cl_uint          num_events_in_wait_list,
             const cl_event * event_wait_list,
             cl_event *       event_parameter)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host
  // command-queue.
  detail::command_queue::validOrError(command_queue);

  auto xdevice = xocl(command_queue)->get_device();
  auto xkernel = xocl(kernel);

  // CL_INVALID_PROGRAM_EXECUTABLE if there is no successfully built
  // program executable available for device associated with
  // command_queue
  if (!xdevice->is_active())
    throw error(CL_INVALID_PROGRAM_EXECUTABLE,"No program executable for device");

  // CL_INVALID_KERNEL if kernel is not a valid kernel object
  detail::kernel::validOrError(kernel);

  // CL_INVALID_CONTEXT if context associated with command_queue and
  // kernel are not the same or if the context associated with
  // command_queue and events in event_wait_list are not the same.
  detail::event::validOrError(command_queue,num_events_in_wait_list,event_wait_list);

  // CL_INVALID_KERNEL_ARGS if the kernel argument values have not
  // been specified or if a kernel argument declared to be a pointer
  // to a type does not point to a named address space.
  detail::kernel::validArgsOrError(kernel);

  // CL_INVALID_WORK_DIMENSION if work_dim is not a valid value
  // (i.e. a value between 1 and 3).
  if (work_dim<1 || work_dim>3)
    throw error(CL_INVALID_WORK_DIMENSION,"Invalid work dimension '" + std::to_string(work_dim) + "'");

  // CL_INVALID_GLOBAL_WORK_SIZE if global_work_size is NULL, or if
  // any of the values specified in global_work_size[0],
  // ...global_work_size [work_dim - 1] are 0
  if (!global_work_size)
    throw error(CL_INVALID_GLOBAL_WORK_SIZE,"global_work_size is nullptr");
  if (std::any_of(global_work_size,global_work_size+work_dim,[](size_t sz){return sz==0;}))
    throw error(CL_INVALID_GLOBAL_WORK_SIZE,"global_work_size[?] is zero");

  // CL_INVALID_GLOBAL_WORK_SIZE if any of the values specified in
  // global_work_size[0], ...global_work_size [work_dim - 1] exceed
  // the range given by the sizeof(size_t) for the device on which the
  // kernel execution will be enqueued.
  cl_uint cl_device_address_bits=getDeviceAddressBits(xdevice);
  if (sizeof(size_t) > (cl_device_address_bits * 8)) {
    // device size_t smaller than host
    size_t devicemax = (((size_t)1) << (cl_device_address_bits))-1;    //8 -> (0x100-1) = 0xFF
    if (std::any_of(global_work_size,global_work_size+work_dim,[devicemax](size_t sz){return sz>devicemax;}))
      throw error(CL_INVALID_GLOBAL_WORK_SIZE, "global_work_size[?] > devicemax (" + std::to_string(devicemax) + ")");
  }

  // CL_INVALID_GLOBAL_OFFSET if the value specified in
  // global_work_size + the corresponding values in global_work_offset
  // for any dimensions is greater than the sizeof(size_t) for the
  // device on which the kernel execution will be enqueued.

  // CL_INVALID_WORK_GROUP_SIZE if local_work_size is specified and
  // does not match the required work-group size for kernel in the
  // program source.
  //
  // CL_INVALID_WORK_GROUP_SIZE if local_work_size is specified and is
  // not consistent with the required number of sub-groups for kernel
  // in the program source.
  //
  // CL_INVALID_WORK_GROUP_SIZE if local_work_size is specified and
  // the total number of work-items in the work-group computed as
  // local_work_size[0] * ... local_work_size[work_dim – 1] is greater
  // than the value specified by CL_KERNEL_WORK_GROUP_SIZE in table
  // 5.21.
  //
  // CL_INVALID_WORK_GROUP_SIZE if the program was compiled with
  // –cl-uniform-work-group-size and the number of work-items
  // specified by global_work_size is not evenly divisible by size of
  // work-group given by local_work_size or by the required work-
  // group size specified in the kernel source.
  auto compile_wgs_range = xocl::xocl(kernel)->get_compile_wg_size_range();
  bool reqd_work_group_size_set = 
    std::any_of(compile_wgs_range.begin(),compile_wgs_range.end(),[](size_t sz) { return sz!=0; });
  for (cl_uint work_dim_it=0; work_dim_it < work_dim; ++work_dim_it) {
    if (local_work_size && !local_work_size[work_dim_it])
      throw xocl::error(CL_INVALID_WORK_GROUP_SIZE,"ClEnqueueNDRangeKernel: CL_INVALID_WORK_GROUP_SIZE case 0");

    if(local_work_size && ((global_work_size[work_dim_it] % local_work_size[work_dim_it]) !=0))
      throw xocl::error(CL_INVALID_WORK_GROUP_SIZE,"ClEnqueueNDRangeKernel: CL_INVALID_WORK_GROUP_SIZE case 1");

    if(reqd_work_group_size_set &&
       (!local_work_size || local_work_size[work_dim_it] != compile_wgs_range[work_dim_it]))
      throw xocl::error(CL_INVALID_WORK_GROUP_SIZE,"ClEnqueueNDRangeKernel : CL_INVALID_WORK_GROUP_SIZE case 2");
  }

  // CL_INVALID_WORK_ITEM_SIZE if the number of work-items specified
  // in any of local_work_size[0], ... local_work_size[work_dim - 1]
  // is greater than the corresponding values specified by
  // CL_DEVICE_MAX_WORK_ITEM_SIZES[0],
  // .... CL_DEVICE_MAX_WORK_ITEM_SIZES[work_dim - 1].

  // CL_MISALIGNED_SUB_BUFFER_OFFSET if a sub-buffer object is
  // specified as the value for an argument that is a buffer object
  // and the offset specified when the sub-buffer object is created is
  // not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device
  // associated with queue.

  // CL_INVALID_IMAGE_SIZE if an image object is specified as an
  // argument value and the image dimensions (image width, height,
  // specified or compute row and/or slice pitch) are not supported by
  // device associated with queue.

  // CL_IMAGE_FORMAT_NOT_SUPPORTED if an image object is specified as
  // an argument value and the image format (image channel order and
  // data type) is not supported by device associated with queue.

  // CL_OUT_OF_RESOURCES if there is a failure to queue the execution
  // instance of kernel on the command-queue because of insufficient
  // resources needed to execute the kernel. For example, the
  // explicitly specified local_work_size causes a failure to execute
  // the kernel because of insufficient resources such as registers or
  // local memory. Another example would be the number of read-only
  // image args used in kernel exceed the
  // CL_DEVICE_MAX_READ_IMAGE_ARGS value for device or the number of
  // write-only image args used in kernel exceed the
  // CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS value for device or the
  // number of samplers used in kernel exceed CL_DEVICE_MAX_SAMPLERS
  // for device.

  // CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to
  // allocate memory for data store associated with image or buffer
  // objects specified as arguments to kernel.
  // XLNX: Check if kernel argument ddr match with cu(s) ddr connection
  size_t argidx = 0;
  for (auto& arg : xocl::xocl(kernel)->get_indexed_argument_range()) {
    if (auto mem = arg->get_memory_object()) {
      mem->get_buffer_object(xdevice); // make sure buffer is allocated on device
      auto mem_memidx_mask = mem->get_memidx(xdevice);
      for (auto& cu : xdevice->get_cu_range()) {
        if (cu->get_symbol()->uid!=xkernel->get_symbol_uid())
          continue;
        auto cu_memidx_mask = cu->get_memidx(argidx);
        if ((cu_memidx_mask & mem_memidx_mask).none()) {
          std::stringstream ostr;
          ostr << "Memory bank specified for kernel instance \""
               << cu->get_name() 
               << "\" of kernel \""
               << xkernel->get_name()
               << "\" for argument \"" << arg->get_name() << "\" "
               << "does not match the physical connectivity from the binary.\n"
               << "Memory bank mask specified for argument ";
          if (mem_memidx_mask.any()) 
            ostr << "is \"" << mem_memidx_mask << "\"";
          else 
            ostr << "does not exist";
          ostr << " while memory bank mask in binary is \"" << cu_memidx_mask << "\".";
          XOCL_DEBUG(std::cout,ostr.str(),"\n");
          if (!is_sw_emulation()) // pr Amit
            throw xocl::error(CL_MEM_OBJECT_ALLOCATION_FAILURE,ostr.str());
        }
      }
    }
    ++argidx;
  }
  

  // CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and
  // num_events_in_wait_list > 0, or event_wait_list is not NULL and
  // num_events_in_wait_list is 0, or if event objects in
  // event_wait_list are not valid events.

  // CL_INVALID_OPERATION if SVM pointers are passed as arguments to a
  // kernel and the device does not support SVM or if system pointers
  // are passed as arguments to a kernel and/or stored inside SVM
  // allocations passed as kernel arguments and the device does not
  // support fine grain system SVM allocations.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clEnqueueNDRangeKernel(cl_command_queue command_queue,
                       cl_kernel        kernel,
                       cl_uint          work_dim,
                       const size_t *   global_work_offset,
                       const size_t *   global_work_size,
                       const size_t *   local_work_size,
                       cl_uint          num_events_in_wait_list,
                       const cl_event * event_wait_list,
                       cl_event *       event_parameter)
{
  validOrError(command_queue,kernel
               ,work_dim,global_work_offset,global_work_size,local_work_size
               ,num_events_in_wait_list,event_wait_list,event_parameter );

  cl_context context=xocl::xocl(kernel)->get_program()->get_context();

  // err_checking: this code is highly fragile and it was suggested that we make minimal changes to this section.
  // We are not really able to disable error checks completely because the error checks are heavily intertwined
  // with the functionality in this section. Here is a good trade-off.
  if(xocl::config::api_checks()) {
    //XCL_CONFORMANCECOLLECT mode
    //write out the kernel sources in clCreateKernel and fail quickly in clEnqueueNDRange
    //skip build in clBuildProgram
    if (getenv("XCL_CONFORMANCECOLLECT")) {
      if (event_parameter) {
        auto uevent = xocl::create_soft_event(0,-1);
        xocl::assign(event_parameter,uevent.get());
        uevent->set_status(CL_COMPLETE);
      }
      return CL_INVALID_KERNEL;
    }

  } // error checking end
    
  auto compile_wgs_range = xocl::xocl(kernel)->get_compile_wg_size_range();
  bool reqd_work_group_size_set = 
    std::any_of(compile_wgs_range.begin(),compile_wgs_range.end(),[](size_t sz) { return sz!=0; });
  
  auto max_wgs_range = xocl::xocl(kernel)->get_max_wg_size_range();
  bool xcl_max_work_group_size_set = 
    std::any_of(max_wgs_range.begin(),max_wgs_range.end(),[](size_t sz) { return sz!=0; });
  
  bool xcl_max_work_group_size_totalworkitemconstraint_set =
    (max_wgs_range[0]!=0 && max_wgs_range[1]==0 && max_wgs_range[2]==0);

  //MAXIMAL DIMENSION WORK DIMENSIONS
  std::vector<size_t> global_work_offset_3D = {0,0,0};
  std::vector<size_t> global_work_size_3D = {1,1,1};
  std::vector<size_t> local_work_size_3D = {1,1,1};
  for (cl_uint work_dim_it=0; work_dim_it < work_dim; ++work_dim_it) {
    if (global_work_offset)
      global_work_offset_3D[work_dim_it] = global_work_offset[work_dim_it];
    global_work_size_3D[work_dim_it] = global_work_size[work_dim_it];
    if (local_work_size)
      local_work_size_3D[work_dim_it] = local_work_size[work_dim_it];
  }


  // pick an local work size if the user does not provide one.
  if (!local_work_size) {
    size_t max_wg_size = std::numeric_limits<size_t>::max(); // no total work items constraint
    if(!xcl_max_work_group_size_set)
      max_wg_size=getDeviceMaxWorkGroupSize(xocl::xocl(command_queue)->get_device());
    else if (xcl_max_work_group_size_totalworkitemconstraint_set)
      max_wg_size=max_wgs_range[0];

    size_t best_wg_size = 1;
    size_t total_size = global_work_size_3D[0] * global_work_size_3D[1] * global_work_size_3D[2];
    size_t dim_max[3] = {1, 1, 1};
    for (cl_uint work_dim_it=0; work_dim_it < work_dim; ++work_dim_it) {
      size_t m = (xcl_max_work_group_size_set && (!xcl_max_work_group_size_totalworkitemconstraint_set))
        ? max_wgs_range[work_dim_it]
        : getDeviceMaxWorkItemSizes(xocl::xocl(command_queue)->get_device())[work_dim_it];
      dim_max[work_dim_it] = (std::min)(m, global_work_size_3D[work_dim_it]);
    }
    for (size_t z = 1; z <= dim_max[2]; ++z) {
      if (global_work_size_3D[2] % z) continue;
      for (size_t y = 1; y <= dim_max[1]; ++y) {
        if (global_work_size_3D[1] % y) continue;
        for (size_t x = 1; x <= dim_max[0]; ++x) {
          if (global_work_size_3D[0] % x) continue;
          if ( (x*y*z > best_wg_size) && (x*y*z <= max_wg_size) &&
               (x*y*z <= total_size) && !(total_size % x*y*z) ) {
            local_work_size_3D[0] = x;
            local_work_size_3D[1] = y;
            local_work_size_3D[2] = z;
            best_wg_size = x*y*z;
          }
        }
      }
    }
  }
  assert(local_work_size_3D[0] && local_work_size_3D[1] && local_work_size_3D[2]);

  // More api checks after computing sizes above
  if (config::api_checks()) {

    //opencl1.2-rev11.pdf P168
    //CL_INVALID_WORK_GROUP_SIZE if local_work_size is specified and the total number
    //of work-items in the work-group computed as local_work_size[0] *
    //local_work_size[work_dim-1] is greater than the value specified by
    //CL_DEVICE_MAX_WORK_GROUP_SIZE in table 4.3.
    //Not checked if xcl_max_work_group_size set
    if(!xcl_max_work_group_size_set && !reqd_work_group_size_set) {
      size_t num_workitems = local_work_size_3D[0] * local_work_size_3D[1] * local_work_size_3D[2];
      if (num_workitems>getDeviceMaxWorkGroupSize(xocl::xocl(command_queue)->get_device()))
        throw xocl::error(CL_INVALID_WORK_GROUP_SIZE,"ClEnqueueNDRangeKernel : CL_INVALID_WORK_GROUP_SIZE case 4");
    }

    //xcl_max_work_items
    //CL_INVALID_WORK_GROUP_SIZE if local_work_size is specified and
    //xcl_max_work_group_size(x) : check total work items <= x
    //xcl_max_work_group_size metadata = : check each dimension bound
    if(xcl_max_work_group_size_set) {
      if(xcl_max_work_group_size_totalworkitemconstraint_set) {
        size_t num_workitems = local_work_size_3D[0] * local_work_size_3D[1] * local_work_size_3D[2];
        if (num_workitems>max_wgs_range[0])
          throw xocl::error(CL_INVALID_WORK_GROUP_SIZE,"ClEnqueueNDRangeKernel : CL_INVALID_WORK_GROUP_SIZE case 5");
      }
      else {
        for (cl_uint work_dim_it=0; work_dim_it<3; work_dim_it++)
          if(local_work_size_3D[work_dim_it] > max_wgs_range[work_dim_it])
            throw xocl::error(CL_INVALID_WORK_GROUP_SIZE,"ClEnqueueNDRangeKernel : CL_INVALID_WORK_GROUP_SIZE case 6");
      }
    }

  } // api_checks

  // PRINTF - we need to allocate a buffer and do an initial memory transfer before kernel
  // execution starts to initialize the printf buffer to known values.
  auto printf_buffer_scoped = createPrintfBuffer(context, kernel, global_work_size_3D, local_work_size_3D);
  cl_mem printf_buffer = printf_buffer_scoped.get(); // cast to cl_mem is important befure passing as void*
  cl_event printf_init_event = nullptr;
  if (printf_buffer) {
    xocl(kernel)->set_printf_argument(sizeof(cl_mem),&printf_buffer);
    printf_init_event = enqueueInitializePrintfBuffer(kernel, command_queue, printf_buffer);
  }

  // Add printf buffer initialization to wait list to ensure this is forced to happen
  // before kernel execution starts in case we are running out of order.
  const cl_event* new_wait_list = event_wait_list;
  cl_uint new_wait_list_size = num_events_in_wait_list;
  std::vector<cl_event> printf_wait_list;
  if (printf_init_event) {
    std::copy(event_wait_list,event_wait_list+num_events_in_wait_list,std::back_inserter(printf_wait_list));
    printf_wait_list.push_back(printf_init_event);
    new_wait_list = printf_wait_list.data();
    new_wait_list_size = printf_wait_list.size();
  }

  // Event for kernel arg migration (todo: experiment with multiple events, one pr arg)
  auto umEvent = xocl::create_hard_event(command_queue,CL_COMMAND_MIGRATE_MEM_OBJECTS,new_wait_list_size,new_wait_list);
  cl_event mEvent = umEvent.get();

  if (printf_init_event)
    // The printf_init_event has been added to the event waitlist
    // The event is no longer neeeded.
    api::clReleaseEvent(printf_init_event);

  // Migration action and enqueing
  xocl::enqueue::set_event_action(umEvent.get(),xocl::enqueue::action_ndrange_migrate,mEvent,kernel);
  xocl::profile::set_event_action(umEvent.get(),xocl::profile::action_ndrange_migrate,mEvent,kernel);
  appdebug::set_event_action(umEvent.get(),appdebug::action_ndrange_migrate,mEvent,kernel);

  // Schedule migration
  umEvent->queue();

  // Event for kernel execution, must wait on migration
  auto ueEvent = xocl::create_hard_event(command_queue,CL_COMMAND_NDRANGE_KERNEL,1,&mEvent);
  cl_event eEvent = ueEvent.get();

  // execution context
  auto device = ueEvent->get_command_queue()->get_device();
    ueEvent->set_execution_context
      (xrt::make_unique<execution_context>
       (device,xocl(kernel),xocl(eEvent),work_dim,global_work_offset_3D.data(),global_work_size_3D.data(),local_work_size_3D.data()));
    xocl::enqueue::set_event_action(ueEvent.get(),xocl::enqueue::action_ndrange_execute);
  
  xocl::profile::set_event_action(ueEvent.get(),xocl::profile::action_ndrange,eEvent,kernel);
  appdebug::set_event_action(ueEvent.get(),appdebug::action_ndrange,eEvent,kernel);

  // Schedule execution
  ueEvent->queue();

  // Schdule the printf buffer retrieval to happen AFTER the kernel
  // execution completes (wait on ueEvent).  The execution event may
  // have already completed (it was queued above), but this function
  // has a reference to ueEvent so the event is alive and well.
  if (printf_buffer)
    enqueueReadPrintfBuffer(kernel,command_queue,printf_buffer,eEvent,nullptr);

  xocl::assign(event_parameter,ueEvent.get());
  XOCL_DEBUG(std::cout,"<-clEnqueneNDRange event(",ueEvent->get_uid(),") returns: ",xocl::time_ns()*1e-6,"\n");
  return CL_SUCCESS;
}

namespace api {

cl_int
clEnqueueNDRangeKernel(cl_command_queue command_queue,
    cl_kernel        kernel,
    cl_uint          work_dim,
    const size_t *   global_work_offset,
    const size_t *   global_work_size,
    const size_t *   local_work_size,
    cl_uint          num_events_in_wait_list,
    const cl_event * event_wait_list,
    cl_event *       event_parameter)
{
  return ::xocl::clEnqueueNDRangeKernel
    ( command_queue,kernel
      ,work_dim,global_work_offset,global_work_size,local_work_size
      ,num_events_in_wait_list,event_wait_list,event_parameter );
}

}

} // namespace xocl

// --------------------------------------------------------------------------

namespace {

struct CallbackArgs {
  xocl::ptr<xocl::kernel> kernel;
  xocl::ptr<xocl::memory> mem;
  std::vector<uint8_t> buf;
};

void CL_CALLBACK cb_BufferInitialized(cl_event event, cl_int status, void *data)
{
  CallbackArgs *args = reinterpret_cast<CallbackArgs*>(data);
  delete args;
  if ( XCL::Printf::isPrintfDebugMode() ) {
    std::cout << "clEnqueueNDRangeKernel - printf buffer init callback\n";
  }
}


void CL_CALLBACK cb_BufferReturned(cl_event event, cl_int status, void *data)
{
  CallbackArgs *args = reinterpret_cast<CallbackArgs*>(data);
  cl_kernel kernel = args->kernel.get();
  XCL::Printf::PrintfManager printfManager;
  printfManager.enqueueBuffer(kernel, args->buf);
  delete args;
  if ( XCL::Printf::isPrintfDebugMode() ) {
    std::cout << "clEnqueueNDRangeKernel - printf buffer returned callback\n";
    printfManager.dbgDump();
  }
  printfManager.print();
  printfManager.clear();

  xocl::api::clReleaseEvent(event);
}

// Creates a device printf buffer but does not initialize
// Allocate device printf buffer if printf is needed for this workgroup.
xocl::ptr<xocl::memory> 
createPrintfBuffer(cl_context context, cl_kernel kernel
                   ,const std::vector<size_t>& gsz, const std::vector<size_t>& lsz)
{
  auto mem = XCL::Printf::kernelHasPrintf(kernel)
    ? clCreateBuffer(context, CL_MEM_READ_WRITE,XCL::Printf::getPrintfBufferSize(gsz,lsz),nullptr,nullptr)
    : nullptr;

  if (!mem)
    return nullptr;

  xocl::ptr<xocl::memory> retval(xocl::xocl(mem));
  assert(retval->count()==2);
  retval->release();
  return retval;
}

// Initialize the device printf buffer to known values. This must execute
// BEFORE the clEnqueueNDRangeKernel starts so the event is returned so it
// can be appended to the list of events the enqueue must wait for.
cl_event enqueueInitializePrintfBuffer(cl_kernel kernel, cl_command_queue queue,cl_mem mem)
{
  cl_event event = nullptr;
  if ( XCL::Printf::kernelHasPrintf(kernel) ) {
    std::unique_ptr<CallbackArgs> args = xrt::make_unique<CallbackArgs>();
    auto bufSize = xocl::xocl(mem)->get_size();
    args->kernel = xocl::xocl(kernel);
    args->mem = xocl::xocl(mem);
    args->buf.resize(bufSize);
    uint8_t *hostBuf = &args->buf[0];
    memset(hostBuf, 0xFF, bufSize);
    cl_int err = xocl::api::clEnqueueWriteBuffer
      (queue, mem, /*blocking_read*/CL_FALSE, 
       /*offset*/0, bufSize, hostBuf,
       /*num_events_in_wait_list*/0, 
       /*event_wait_list*/nullptr, 
       /*return event*/&event);
    if ( err != CL_SUCCESS )
      throw xocl::error(err,"enqueueInitializePrintfBuffer");
    err = xocl::api::clSetEventCallback(event, CL_COMPLETE, cb_BufferInitialized, args.get());
    if (err == CL_SUCCESS)
      args.release();
  }
  return event;
}

// Read device printf buffer back from the device. This must execute AFTER the 
// clEnqueueNDRangeKernel event completes. We pass an event wait list with the 
// enqueue event to ensure it happens in the correct order.
cl_int enqueueReadPrintfBuffer(cl_kernel kernel, cl_command_queue queue,
                               cl_mem mem, cl_event waitEvent, cl_event* event_param)
{
  cl_int err = CL_SUCCESS;
  if ( XCL::Printf::kernelHasPrintf(kernel) ) {
    std::unique_ptr<CallbackArgs> args = xrt::make_unique<CallbackArgs>();
    if ( !args ) {
      throw xocl::error(CL_OUT_OF_RESOURCES,"enqueueReadPrintfBuffer");
    }
    cl_event event = nullptr;
    auto bufSize = xocl::xocl(mem)->get_size();
    args->kernel = xocl::xocl(kernel);
    args->mem = xocl::xocl(mem);
    args->buf.resize(bufSize);
    uint8_t *hostBuf = &args->buf[0];
    err = xocl::api::clEnqueueReadBuffer
      (queue, mem,
       /*blocking_read*/CL_FALSE, 
       /*offset*/0, bufSize, hostBuf,
       /*num_events_in_wait_list*/1, /*event_wait_list*/&waitEvent, 
       /*return event*/&event);
    if (err != CL_SUCCESS)
      throw xocl::error(err,"enqueueReadPrintfBuffer");
    err = xocl::api::clSetEventCallback(event, CL_COMPLETE, cb_BufferReturned, args.get());
    if (err == CL_SUCCESS) {
      args.release();
    } 
  }
  return err;
}

} // namespace

cl_int
clEnqueueNDRangeKernel(cl_command_queue command_queue,
    cl_kernel        kernel,
    cl_uint          work_dim,
    const size_t *   global_work_offset,
    const size_t *   global_work_size,
    const size_t *   local_work_size,
    cl_uint          num_events_in_wait_list,
    const cl_event * event_wait_list,
    cl_event *       event_parameter)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueNDRangeKernel
      ( command_queue,kernel
       ,work_dim,global_work_offset,global_work_size,local_work_size
       ,num_events_in_wait_list,event_wait_list,event_parameter );
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}


