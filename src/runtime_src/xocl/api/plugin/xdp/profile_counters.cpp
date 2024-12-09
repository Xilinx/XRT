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

#include <functional>

#include "plugin/xdp/profile_counters.h"

#include "core/common/module_loader.h"
#include "core/common/utils.h"
#include "core/common/dlfcn.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xocl/core/command_queue.h"
#include "xocl/core/program.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"

namespace xocl {

  namespace profile {

    // All of the function pointers that will be dynamically linked to
    //  callback functions on the XDP plugin side
    std::function<void (const char*,
                        unsigned long long int,
                        bool)> counter_function_start_cb ;
    std::function<void (const char*)> counter_function_end_cb ;
    std::function<void (const char*,
                        bool,
                        unsigned long long int,
                        unsigned long long int,
                        unsigned long long int,
                        const char*,
                        const char*,
                        const char*,
                        const char**,
                        unsigned long long int)> counter_kernel_execution_cb ;
    std::function<void (const char*,
                        const char*,
                        const char*,
                        const char*,
                        bool)> counter_cu_execution_cb ;
    std::function<void (unsigned long long int,
                        unsigned long long int,
                        const char*,
                        unsigned long long int,
                        unsigned long long int,
                        bool,
                        bool,
                        unsigned long long int,
                        unsigned long long int)> counter_action_read_cb ;
    std::function<void (unsigned long long int,
                        const char*,
                        unsigned long long int,
                        unsigned long long int,
                        bool,
                        bool,
                        unsigned long long int,
                        unsigned long long int)> counter_action_write_cb ;
    std::function<void ()> counter_mark_objects_released_cb ;

    void register_opencl_counters_functions(void* handle)
    {
      using start_type       = void (*)(const char*,            // Function name
                                        unsigned long long int, // Queue address
                                        bool);                  // isOOO

      using end_type         = void (*)(const char*);           // Function name

      using kernel_exec_type = void (*)(const char*,            // Kernel name
                                        bool,                   // isStart
                                        unsigned long long int, // Inst address
                                        unsigned long long int, // Context ID
                                        unsigned long long int, // Command Q ID
                                        const char*,            // Device name
                                        const char*,            // Global WS
                                        const char*,            // Local WS
                                        const char**,           // Buffers
                                        unsigned long long int);// numBuffers

      using cu_exec_type     = void (*)(const char*,            // CU name
                                        const char*,            // Kernel name
                                        const char*,            // Local WG
                                        const char*,            // Global WG
                                        bool);                  // isStart

      using read_type        = void (*)(unsigned long long int, // Context ID
                                        unsigned long long int, // Num Devices
                                        const char*,            // Device name
                                        unsigned long long int, // Event ID
                                        unsigned long long int, // Size
                                        bool,                   // isStart
                                        bool,                   // isP2P
                                        unsigned long long int, // Address
                                        unsigned long long int);// Command Q ID

      using write_type       = void (*)(unsigned long long int, // Context ID
                                        const char*,            // Device name
                                        unsigned long long int, // Event ID
                                        unsigned long long int, // Size
                                        bool,                   // isStart
                                        bool,                   // isP2P
                                        unsigned long long int, // Address
                                        unsigned long long int);// Command Q ID

      using release_type    = void (*)() ;

      counter_function_start_cb = reinterpret_cast<start_type>(xrt_core::dlsym(handle, "log_function_call_start")) ;
      if (xrt_core::dlerror() != NULL) counter_function_start_cb = nullptr ;
      
      counter_function_end_cb = reinterpret_cast<end_type>(xrt_core::dlsym(handle, "log_function_call_end")) ;
      if (xrt_core::dlerror() != NULL) counter_function_start_cb = nullptr ;
      
      counter_kernel_execution_cb = reinterpret_cast<kernel_exec_type>(xrt_core::dlsym(handle, "log_kernel_execution")) ;
      if (xrt_core::dlerror() != NULL) counter_kernel_execution_cb = nullptr ;
      
      counter_cu_execution_cb = reinterpret_cast<cu_exec_type>(xrt_core::dlsym(handle, "log_compute_unit_execution")) ;
      if (xrt_core::dlerror() != NULL) counter_cu_execution_cb = nullptr ;
      
      counter_action_read_cb = reinterpret_cast<read_type>(xrt_core::dlsym(handle, "counter_action_read")) ;
      if (xrt_core::dlerror() != NULL) counter_action_read_cb = nullptr ;
      
      counter_action_write_cb = reinterpret_cast<write_type>(xrt_core::dlsym(handle, "counter_action_write")) ;
      if (xrt_core::dlerror() != NULL) counter_action_write_cb = nullptr ;

      counter_mark_objects_released_cb = reinterpret_cast<release_type>(xrt_core::dlsym(handle, "counter_mark_objects_released")) ;
      if (xrt_core::dlerror() != NULL) counter_mark_objects_released_cb = nullptr ;

      // For logging counter information for compute unit executions
      xocl::add_command_start_callback(xocl::profile::log_cu_start) ;
      xocl::add_command_done_callback(xocl::profile::log_cu_end) ;
    }
    
    void opencl_counters_warning_function()
    {
    }
    
    void load_xdp_opencl_counters()
    {
      static xrt_core::module_loader
        opencl_trace_loader("xdp_opencl_counters_plugin",
                            register_opencl_counters_functions,
                            opencl_counters_warning_function) ;
    }
    
  } // end namespace profile
  
} // end namespace xocl

// This anonymous namespace is for helper functions used in this file
namespace {

  // Index of CU used to execute command. This is not necessarily the
  // proper CU since a command may have the option to execute on
  // multiple CUs and only scheduler knows which one was picked
  static unsigned int get_cu_index(const xrt::run& run)
  {
    auto& cumask = xrt_core::kernel_int::get_cumask(run) ;
    for (size_t bit = 0; bit < cumask.size(); ++bit)
      if (cumask.test(bit))
        return static_cast<unsigned int>(bit);
    return 0 ;
  }

  static uint64_t get_memory_address(cl_mem buffer)
  {
    uint64_t address = 0 ;
    std::string bank = "Unknown" ;
    try {
      if (auto xmem = xocl::xocl(buffer))
        xmem->try_get_address_bank(address, bank) ;
    }
    catch (const xocl::error&)
    {
    }
    return address ;
  }

} // end anonymous namespace

namespace xocl {
  namespace profile {

    // ******** OpenCL Counter/Guidance Callbacks *********

    // These are the functions on the XOCL side that gets called when
    //  execution contexts start and stop
    void log_cu_start(const xocl::execution_context* ctx,
                      const xrt::run& run)
    {
      if (!counter_cu_execution_cb) return ;

      // Check for software emulation logging of compute unit starts as well
      unsigned int cuIndex = get_cu_index(run) ;
      auto cu = ctx->get_event()->get_command_queue()->get_device()->get_compute_unit(cuIndex) ;
      if (cu)
      {
        auto localDim = ctx->get_local_work_size() ;
        auto globalDim = ctx->get_global_work_size() ;
        
        std::string localWorkgroupSize = 
          std::to_string(localDim[0]) + ":" +
          std::to_string(localDim[1]) + ":" +
          std::to_string(localDim[2]) ;

        std::string globalWorkgroupSize = 
          std::to_string(globalDim[0]) + ":" +
          std::to_string(globalDim[1]) + ":" +
          std::to_string(globalDim[2]) ;

        counter_cu_execution_cb(cu->get_name().c_str(),
                                cu->get_kernel_name().c_str(),
                                localWorkgroupSize.c_str(),
                                globalWorkgroupSize.c_str(),
                                true) ;
      }
    }
    
    void log_cu_end(const xocl::execution_context* ctx,
                    const xrt::run& run)
    {
      if (!counter_cu_execution_cb) return ;

      // Check for software emulation logging of compute unit ends as well
      unsigned int cuIndex = get_cu_index(run) ;
      auto cu = ctx->get_event()->get_command_queue()->get_device()->get_compute_unit(cuIndex) ;
      if (cu)
      {
        auto localDim = ctx->get_local_work_size() ;
        auto globalDim = ctx->get_global_work_size() ;

        std::string localWorkgroupSize = 
          std::to_string(localDim[0]) + ":" +
          std::to_string(localDim[1]) + ":" +
          std::to_string(localDim[2]) ;

        std::string globalWorkgroupSize = 
          std::to_string(globalDim[0]) + ":" +
          std::to_string(globalDim[1]) + ":" +
          std::to_string(globalDim[2]) ;

        counter_cu_execution_cb(cu->get_name().c_str(),
                                cu->get_kernel_name().c_str(),
                                localWorkgroupSize.c_str(),
                                globalWorkgroupSize.c_str(),
                                false) ;
      }
    }

    void mark_objects_released()
    {
      if (counter_mark_objects_released_cb)
        counter_mark_objects_released_cb() ;
    }

    std::function<void (xocl::event*, cl_int)>
    counter_action_read(cl_mem buffer)
    {
      return [buffer](xocl::event* e, cl_int status)
             {
               if (!counter_action_read_cb) return ;
               if (status != CL_RUNNING && status != CL_COMPLETE) return ;

               auto queue = e->get_command_queue() ;
               uint64_t contextId = e->get_context()->get_uid() ;
               uint64_t eventId = e->get_uid() ;
               uint64_t numDevices = e->get_context()->num_devices() ;
               std::string deviceName = queue->get_device()->get_name() ;

               auto xmem = xocl::xocl(buffer) ;
               auto ext_flags = xmem->get_ext_flags() ;
               bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

               uint64_t address = get_memory_address(buffer) ;

               // I'm logging numDevices here because it has to be logged
               //  somewhere, although it really doesn't make much sense here

               if (status == CL_RUNNING)
               {
                 counter_action_read_cb(contextId,
                                        numDevices,
                                        deviceName.c_str(),
                                        eventId,
                                        0,
                                        true,
                                        isP2P,
                                        address,
                                        static_cast<uint64_t>(queue->get_uid()));
               }
               else if (status == CL_COMPLETE)
               {
                 counter_action_read_cb(contextId,
                                        numDevices,
                                        deviceName.c_str(),
                                        eventId,
                                        xmem->get_size(),
                                        false,
                                        isP2P,
                                        address,
                                        static_cast<uint64_t>(queue->get_uid())) ;
               }
             } ;
    }

    std::function<void (xocl::event*, cl_int)>
    counter_action_write(cl_mem buffer)
    {
      return [buffer](xocl::event* e, cl_int status) 
             {
               if (!counter_action_write_cb) return ;
               if (status != CL_RUNNING && status != CL_COMPLETE) return ;

               auto queue = e->get_command_queue() ;
               uint64_t contextId = e->get_context()->get_uid() ;
               uint64_t eventId = e->get_uid() ;
               std::string deviceName = queue->get_device()->get_name() ;

               auto xmem = xocl::xocl(buffer) ;
               auto ext_flags = xmem->get_ext_flags() ;
               bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

               uint64_t address = get_memory_address(buffer) ;

               if (status == CL_RUNNING)
               {
                 counter_action_write_cb(contextId,
                                         deviceName.c_str(),
                                         eventId,
                                         0,
                                         true,
                                         isP2P,
                                         address,
                                         static_cast<uint64_t>(queue->get_uid()));
               }
               else if (status == CL_COMPLETE)
               {
                 counter_action_write_cb(contextId,
                                         deviceName.c_str(),
                                         eventId,
                                         xmem->get_size(),
                                         false,
                                         isP2P,
                                         address,
                                         static_cast<uint64_t>(queue->get_uid())) ;
               }
             } ;
    }

    std::function<void (xocl::event*, cl_int)>
    counter_action_migrate(cl_uint num_mem_objects, const cl_mem* mem_objects, cl_mem_migration_flags flags)
    {
      // Don't do anything for this migration
      if (flags & CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED)
      {
        return [](xocl::event*, cl_int) { } ;
      }

      cl_mem buffer = num_mem_objects > 0 ? mem_objects[0] : nullptr ;
      size_t totalSize = 0 ;
      for (auto mem : xocl::get_range(mem_objects, mem_objects+num_mem_objects))
      {
        totalSize += xocl::xocl(mem)->get_size() ;
      }

      // Migrate actions could be either a read or a write.
      if (flags & CL_MIGRATE_MEM_OBJECT_HOST)
      {
        // Read
        return [buffer, totalSize](xocl::event* e, cl_int status)
               {
                 if (!counter_action_read_cb) return ;
                 if (status != CL_RUNNING && status != CL_COMPLETE) return ;

                 auto queue = e->get_command_queue() ;
                 uint64_t contextId = e->get_context()->get_uid() ;
                 uint64_t eventId = e->get_uid() ;
                 uint64_t numDevices = e->get_context()->num_devices() ;
                 std::string deviceName = queue->get_device()->get_name() ;

                 auto xmem = xocl::xocl(buffer) ;
                 auto ext_flags = xmem->get_ext_flags() ;
                 bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

                 uint64_t address = get_memory_address(buffer) ;

                 if (status == CL_RUNNING)
                 {
                   counter_action_read_cb(contextId,
                                          numDevices,
                                          deviceName.c_str(),
                                          eventId,
                                          0,
                                          true,
                                          isP2P,
                                          address,
                                          static_cast<uint64_t>(queue->get_uid()));
                 }
                 else if (status == CL_COMPLETE)
                 {
                   counter_action_read_cb(contextId,
                                          numDevices,
                                          deviceName.c_str(),
                                          eventId,
                                          totalSize,
                                          false,
                                          isP2P,
                                          address,
                                          static_cast<uint64_t>(queue->get_uid())) ;
                 }
               } ;
      }
      else
      {
        // Write
        return [buffer, totalSize](xocl::event* e, cl_int status)
               {
                 if (!counter_action_write_cb) return ;
                 if (status != CL_RUNNING && status != CL_COMPLETE) return ;

                 auto queue = e->get_command_queue() ;
                 uint64_t contextId = e->get_context()->get_uid() ;
                 uint64_t eventId = e->get_uid() ;
                 std::string deviceName = queue->get_device()->get_name() ;

                 auto xmem = xocl::xocl(buffer) ;
                 auto ext_flags = xmem->get_ext_flags() ;
                 bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

                 uint64_t address = get_memory_address(buffer) ;

                 if (status == CL_RUNNING)
                 {
                   counter_action_write_cb(contextId,
                                           deviceName.c_str(),
                                           eventId,
                                           0,
                                           true,
                                           isP2P,
                                           address,
                                           static_cast<uint64_t>(queue->get_uid()));
                 }
                 else if (status == CL_COMPLETE)
                 {
                   counter_action_write_cb(contextId,
                                           deviceName.c_str(),
                                           eventId,
                                           totalSize,
                                           false,
                                           isP2P,
                                           address,
                                           static_cast<uint64_t>(queue->get_uid())) ;
                 }
               } ;
      }
    }

    std::function<void (xocl::event*, cl_int)>
    counter_action_ndrange(cl_kernel kernel)
    {
      return [kernel](xocl::event* e, cl_int status)
             {
               if (!counter_kernel_execution_cb) return ;
               if (status != CL_RUNNING && status != CL_COMPLETE) return ;

               auto xkernel = xocl::xocl(kernel) ;
               std::string kernelName = xkernel->get_name() ;

               if (status == CL_RUNNING)
               {
                 // The extra information is only used when an end event
                 //  happens, so don't spend the overhead in this branch
                 counter_kernel_execution_cb(kernelName.c_str(), true,
                                             0,
                                             0,
                                             0,
                                             "",
                                             "",
                                             "",
                                             nullptr,
                                             0) ;
               }
               else if (status == CL_COMPLETE)
               {
                 auto xevent = xocl::xocl(e) ;
                 auto queue = xevent->get_command_queue() ;
                 uint64_t contextId = e->get_context()->get_uid() ;
                 std::string deviceName = queue->get_device()->get_unique_name() ;

                 auto localDim = e->get_execution_context()->get_local_work_size() ;
                 auto globalDim = e->get_execution_context()->get_global_work_size() ;
        
                 std::string localWorkgroupSize = 
                   std::to_string(localDim[0]) + ":" +
                   std::to_string(localDim[1]) + ":" +
                   std::to_string(localDim[2]) ;

                 std::string globalWorkgroupSize = 
                   std::to_string(globalDim[0]) + ":" +
                   std::to_string(globalDim[1]) + ":" +
                   std::to_string(globalDim[2]) ;

                 // For buffer guidance, create strings for each
                 //  argument mapped to memory
                 std::vector<std::string> memoryInfo ;
                 auto device = queue->get_device() ;

                 for (auto& arg : xkernel->get_xargument_range()) {
                   try {
                     xocl::memory* mem = arg->get_memory_object() ;
                     if (!mem) continue ;

                     auto mem_id = mem->get_memidx() ;
                     std::string mem_tag =
                       device->get_xclbin().memidx_to_banktag(mem_id) ;
                     if (mem_tag.rfind("bank", 0) == 0)
                       mem_tag = "DDR[" + mem_tag.substr(4,4) + "]" ;

                     std::string bufferInfo = "" ;
                     bufferInfo += kernelName ;
                     bufferInfo += "|" ;
                     bufferInfo += arg->get_name() ;
                     bufferInfo += "|" ;
                     bufferInfo += mem_tag ;
                     bufferInfo += "|" ;
                     bufferInfo += std::to_string(mem->is_aligned()) ;
                     bufferInfo += "," ;
                     bufferInfo += std::to_string(mem->get_size()) ;
                     
                     memoryInfo.push_back(bufferInfo) ;
                   } catch (...) {
                     continue ;
                   }
                 }

                 // Convert to C-style so we can pass to the plugin cleanly
                 const char** buffers = nullptr ;
                 if (memoryInfo.size() != 0) {
                   buffers = new const char*[memoryInfo.size()] ;
                   for (uint64_t i = 0 ; i < memoryInfo.size() ; ++i) {
                     buffers[i] = memoryInfo[i].c_str() ;
                   }
                 }

                 counter_kernel_execution_cb(kernelName.c_str(), false,
                                             reinterpret_cast<uint64_t>(kernel),
                                             contextId,
                                             static_cast<uint64_t>(queue->get_uid()),
                                             deviceName.c_str(),
                                             globalWorkgroupSize.c_str(),
                                             localWorkgroupSize.c_str(),
                                             buffers,
                                             memoryInfo.size()) ;

                 if (buffers != nullptr) delete [] buffers ;
               }
             } ;
    }

    std::function<void (xocl::event*, cl_int)>
    counter_action_ndrange_migrate(cl_event event, cl_kernel kernel)
    {
      auto xevent = xocl::xocl(event) ;
      auto xkernel = xocl::xocl(kernel) ;

      auto queue = xevent->get_command_queue() ;
      auto device = queue->get_device() ;

      cl_mem mem0 = nullptr ;
      size_t totalSize = 0 ;

      // See how many of the arguments will be migrated and mark them
      for (auto& arg : xkernel->get_xargument_range())
      {
        if (auto mem = arg->get_memory_object())
        {
          /*
          if (arg->is_progvar() && 
              arg->get_address_qualifier() == CL_KERNEL_ARG_ADDRESS_GLOBAL)
            continue ;
          else 
          */
          if (mem->is_resident(device))
            continue ;
          else if (!(mem->get_flags() & 
                     (CL_MEM_WRITE_ONLY|CL_MEM_HOST_NO_ACCESS)))
          {
            mem0 = mem ;
            totalSize += xocl::xocl(mem)->get_size() ;
          }
        }
      }
      
      if (mem0 == nullptr)
      {
        return [](xocl::event*, cl_int)
               {
               } ;
      }

      return [mem0, totalSize](xocl::event* e, cl_int status)
             {
               if (!counter_action_write_cb) return ;
               if (status != CL_RUNNING && status != CL_COMPLETE) return ;

               auto queue = e->get_command_queue() ;
               uint64_t contextId = e->get_context()->get_uid() ;
               uint64_t eventId = e->get_uid() ;
               std::string deviceName = queue->get_device()->get_name() ;

               auto xmem = xocl::xocl(mem0) ;
               auto ext_flags = xmem->get_ext_flags() ;
               bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

               uint64_t address = get_memory_address(mem0) ;

               if (status == CL_RUNNING)
               {
                 counter_action_write_cb(contextId,
                                         deviceName.c_str(),
                                         eventId,
                                         0,
                                         true,
                                         isP2P,
                                         address,
                                         static_cast<uint64_t>(queue->get_uid()));
               }
               else if (status == CL_COMPLETE)
               {
                 counter_action_write_cb(contextId,
                                         deviceName.c_str(),
                                         eventId,
                                         totalSize,
                                         false,
                                         isP2P,
                                         address,
                                         static_cast<uint64_t>(queue->get_uid())) ;
               }
             } ;
    }

    std::function<void (xocl::event*, cl_int)>
    counter_action_map(cl_mem buffer, cl_map_flags flags)
    {
      return [buffer, flags](xocl::event* e, cl_int status)
        {
          if (!counter_action_read_cb) return ;
          if (status != CL_RUNNING && status != CL_COMPLETE) return ;

          // Ignore if mapping an invalidated region
          if (flags & CL_MAP_WRITE_INVALIDATE_REGION) return ;

          // If the buffer is not resident on device, don't record this
          auto xmem = xocl::xocl(buffer) ;
          auto queue = e->get_command_queue() ;
          auto device = queue->get_device() ;
          if (!(xmem->is_resident(device))) return ;

          uint64_t contextId = e->get_context()->get_uid() ;
          uint64_t eventId = e->get_uid() ;
          uint64_t numDevices = e->get_context()->num_devices() ;
          std::string deviceName = device->get_name() ;

          auto ext_flags = xmem->get_ext_flags() ;
          bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

          uint64_t address = get_memory_address(buffer) ;

          if (status == CL_RUNNING) {
            counter_action_read_cb(contextId,
                                   numDevices,
                                   deviceName.c_str(),
                                   eventId,
                                   0,
                                   true,
                                   isP2P,
                                   address,
                                   static_cast<uint64_t>(queue->get_uid()));
          }
          else if (status == CL_COMPLETE) {
            counter_action_read_cb(contextId,
                                   numDevices,
                                   deviceName.c_str(),
                                   eventId,
                                   xmem->get_size(),
                                   false,
                                   isP2P,
                                   address,
                                   static_cast<uint64_t>(queue->get_uid())) ;
          }
        } ;
    }

    std::function<void (xocl::event*, cl_int)>
    counter_action_unmap(cl_mem buffer)
    {
      return [buffer](xocl::event* e, cl_int status)
        {
          if (!counter_action_write_cb) return ;
          if (status != CL_RUNNING && status != CL_COMPLETE) return ;

          auto xmem = xocl::xocl(buffer) ;

          // If P2P transfer, don't record this transfer
          if (xmem->no_host_memory()) return ;

          // If the buffer is not resident on device, don't record this
          auto queue = e->get_command_queue() ;
          auto device = queue->get_device() ;
          if (!(xmem->is_resident(device))) return ;

          uint64_t contextId = e->get_context()->get_uid() ;
          uint64_t eventId = e->get_uid() ;
          std::string deviceName = device->get_name() ;
          auto ext_flags = xmem->get_ext_flags() ;
          bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

          uint64_t address = get_memory_address(buffer) ;

          if (status == CL_RUNNING) {
            counter_action_write_cb(contextId,
                                    deviceName.c_str(),
                                    eventId,
                                    0,
                                    true,
                                    isP2P,
                                    address,
                                    static_cast<uint64_t>(queue->get_uid()));
          }
          else if (status == CL_COMPLETE) {
            counter_action_write_cb(contextId,
                                    deviceName.c_str(),
                                    eventId,
                                    xmem->get_size(),
                                    false,
                                    isP2P,
                                    address,
                                    static_cast<uint64_t>(queue->get_uid())) ;
          }
        } ;
    }
  } // end namespace profile
} // end namespace xocl
