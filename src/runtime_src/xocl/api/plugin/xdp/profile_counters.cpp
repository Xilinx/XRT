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
			const char*)> counter_kernel_execution_cb ;
    std::function<void (const char*,
			const char*,
			const char*,
			bool)> counter_cu_execution_cb ;
    std::function<void (unsigned long long int,
			unsigned long long int,
			const char*,
			unsigned long long int,
			bool,
			bool,
			unsigned long long int,
			unsigned long long int)> counter_action_read_cb ;
    std::function<void (unsigned long long int,
			const char*,
			unsigned long long int,
			bool,
			bool,
			unsigned long long int,
			unsigned long long int)> counter_action_write_cb ;
    std::function<void ()> counter_mark_objects_released_cb ;

    void register_opencl_counters_functions(void* handle)
    {
      typedef void (*stype)(const char*, unsigned long long int, bool) ;
      counter_function_start_cb = 
	(stype)(xrt_core::dlsym(handle, "log_function_call_start")) ;
      if (xrt_core::dlerror() != NULL) counter_function_start_cb = nullptr ;
      
      typedef void (*etype)(const char*) ;
      counter_function_end_cb = 
	(etype)(xrt_core::dlsym(handle, "log_function_call_end")) ;
      if (xrt_core::dlerror() != NULL) counter_function_start_cb = nullptr ;
      
      typedef void (*ktype)(const char*, bool, unsigned long long int, unsigned long long int, unsigned long long int, const char* ,const char*, const char*) ;
      counter_kernel_execution_cb =
	(ktype)(xrt_core::dlsym(handle, "log_kernel_execution")) ;
      if (xrt_core::dlerror() != NULL) counter_kernel_execution_cb = nullptr ;
      
      typedef void (*cuetype)(const char*, const char*, const char*, bool) ;
      counter_cu_execution_cb =
	(cuetype)(xrt_core::dlsym(handle, "log_compute_unit_execution")) ;
      if (xrt_core::dlerror() != NULL) counter_cu_execution_cb = nullptr ;
      
      typedef void (*atype)(unsigned long long int, unsigned long long int, const char*, unsigned long long int, bool, bool, unsigned long long int, unsigned long long int) ;
      counter_action_read_cb =
	(atype)(xrt_core::dlsym(handle, "counter_action_read")) ;
      if (xrt_core::dlerror() != NULL) counter_action_read_cb = nullptr ;
      
      typedef void (*wtype)(unsigned long long int, const char*, unsigned long long int, bool, bool, unsigned long long int, unsigned long long int) ;
      counter_action_write_cb =
	(wtype)(xrt_core::dlsym(handle, "counter_action_write")) ;
      if (xrt_core::dlerror() != NULL) counter_action_write_cb = nullptr ;

      typedef void (*vtype)() ;
      counter_mark_objects_released_cb =
	(vtype)(xrt_core::dlsym(handle, "counter_mark_objects_released")) ;
      if (xrt_core::dlerror() != NULL) counter_mark_objects_released_cb = nullptr ;

      // For logging counter information for kernel executions
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

  static uint32_t get_num_cu_masks(uint32_t header)
  {
    return ((1 + (header >> 10)) & 0x3) ;
  }

  static uint32_t get_cu_index_mask(uint32_t cumask)
  {
    uint32_t cu_idx = 0 ;
    for (; (cumask & 0x1) == 0 ; ++cu_idx, cumask >>= 1) ;
    return cu_idx ;
  }

  static unsigned int get_cu_index(const xrt::command* cmd)
  {
    auto& packet = cmd->get_packet() ;
    auto masks = get_num_cu_masks(packet[0]) ;

    for (unsigned int i = 0 ; i < masks ; ++i)
    {
      if (auto cumask = packet[i+1])
	return get_cu_index_mask(cumask) + 32 * i ;
    }
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
    void log_cu_start(const xrt::command* cmd, 
		      const xocl::execution_context* ctx)
    {
      if (!counter_cu_execution_cb) return ;

      // Check for software emulation logging of compute unit starts as well
      unsigned int cuIndex = get_cu_index(cmd) ;
      auto cu = ctx->get_compute_unit(cuIndex) ;
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
				localWorkgroupSize.c_str(),
				globalWorkgroupSize.c_str(),
				true) ;
      }
    }
    
    void log_cu_end(const xrt::command* cmd,
			const xocl::execution_context* ctx)
    {
      // Check for software emulation logging of compute unit ends as well
      if (counter_cu_execution_cb)
      {
	unsigned int cuIndex = get_cu_index(cmd) ;
	auto cu = ctx->get_compute_unit(cuIndex) ;
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
				  localWorkgroupSize.c_str(),
				  globalWorkgroupSize.c_str(),
				  false) ;
	}
      }
    }

    void mark_objects_released()
    {
      if (counter_mark_objects_released_cb)
	counter_mark_objects_released_cb() ;
    }

    std::function<void (xocl::event*, cl_int, const std::string&)>
    counter_action_read(cl_mem buffer)
    {
      return [buffer](xocl::event* e, cl_int status, const std::string&) 
	     {
	       if (!counter_action_read_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;

	       auto queue = e->get_command_queue() ;
	       uint64_t contextId = e->get_context()->get_uid() ;
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
					0,
					true,
					isP2P,
					address,
					reinterpret_cast<uint64_t>(queue));
	       }
	       else if (status == CL_COMPLETE)
	       {
		 counter_action_read_cb(contextId,
					numDevices,
					deviceName.c_str(),
					xmem->get_size(),
					false,
					isP2P,
					address,
					reinterpret_cast<uint64_t>(queue)) ;
	       }
	     } ;
    }

    std::function<void (xocl::event*, cl_int, const std::string&)>
    counter_action_write(cl_mem buffer)
    {
      return [buffer](xocl::event* e, cl_int status, const std::string&) 
	     {
	       if (!counter_action_write_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;

	       auto queue = e->get_command_queue() ;
	       uint64_t contextId = e->get_context()->get_uid() ;
	       std::string deviceName = queue->get_device()->get_name() ;

	       auto xmem = xocl::xocl(buffer) ;
	       auto ext_flags = xmem->get_ext_flags() ;
	       bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

	       uint64_t address = get_memory_address(buffer) ;

	       if (status == CL_RUNNING)
	       {
		 counter_action_write_cb(contextId,
					 deviceName.c_str(),
					 0,
					 true,
					 isP2P,
					 address,
					 reinterpret_cast<uint64_t>(queue));
	       }
	       else if (status == CL_COMPLETE)
	       {
		 counter_action_write_cb(contextId,
					 deviceName.c_str(),
					 xmem->get_size(),
					 false,
					 isP2P,
					 address,
					 reinterpret_cast<uint64_t>(queue)) ;
	       }
	     } ;
    }

    std::function<void (xocl::event*, cl_int, const std::string&)>
    counter_action_migrate(cl_mem buffer, cl_mem_migration_flags flags)
    {
      // Don't do anything for this migration
      if (flags & CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED)
      {
	return [](xocl::event*, cl_int, const std::string&) { } ;
      }

      // Migrate actions could be either a read or a write.
      if (flags & CL_MIGRATE_MEM_OBJECT_HOST)
      {
	// Read
	return [buffer](xocl::event* e, cl_int status, const std::string&)
	       {
		 if (!counter_action_read_cb) return ;
		 if (status != CL_RUNNING && status != CL_COMPLETE) return ;

		 auto queue = e->get_command_queue() ;
		 uint64_t contextId = e->get_context()->get_uid() ;
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
					  0,
					  true,
					  isP2P,
					  address,
					  reinterpret_cast<uint64_t>(queue));
		 }
		 else if (status == CL_COMPLETE)
		 {
		   counter_action_read_cb(contextId,
					  numDevices,
					  deviceName.c_str(),
					  xmem->get_size(),
					  false,
					  isP2P,
					  address,
					  reinterpret_cast<uint64_t>(queue)) ;
		 }
	       } ;
      }
      else
      {
	// Write
	return [buffer](xocl::event* e, cl_int status, const std::string&)
	       {
		 if (!counter_action_write_cb) return ;
		 if (status != CL_RUNNING && status != CL_COMPLETE) return ;

		 auto queue = e->get_command_queue() ;
		 uint64_t contextId = e->get_context()->get_uid() ;
		 std::string deviceName = queue->get_device()->get_name() ;

		 auto xmem = xocl::xocl(buffer) ;
		 auto ext_flags = xmem->get_ext_flags() ;
		 bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

		 uint64_t address = get_memory_address(buffer) ;

		 if (status == CL_RUNNING)
		 {
		   counter_action_write_cb(contextId,
					   deviceName.c_str(),
					   0,
					   true,
					   isP2P,
					   address,
					   reinterpret_cast<uint64_t>(queue));
		 }
		 else if (status == CL_COMPLETE)
		 {
		   counter_action_write_cb(contextId,
					   deviceName.c_str(),
					   xmem->get_size(),
					   false,
					   isP2P,
					   address,
					   reinterpret_cast<uint64_t>(queue)) ;
		 }
	       } ;
      }
    }

    std::function<void (xocl::event*, cl_int, const std::string&)>
    counter_action_ndrange(cl_kernel kernel)
    {
      return [kernel](xocl::event* e, cl_int status, const std::string&)
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
					     "") ;
	       }
	       else if (status == CL_COMPLETE)
	       {
		 auto xevent = xocl::xocl(e) ;
		 auto queue = xevent->get_command_queue() ;
		 uint64_t contextId = e->get_context()->get_uid() ;
		 std::string deviceName = queue->get_device()->get_name() ;

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

		 counter_kernel_execution_cb(kernelName.c_str(), false,
					     reinterpret_cast<uint64_t>(kernel),
					     contextId,
					     static_cast<uint64_t>(queue->get_uid()),
					     deviceName.c_str(),
					     globalWorkgroupSize.c_str(),
					     localWorkgroupSize.c_str()) ;
	       }
	     } ;
    }

    std::function<void (xocl::event*, cl_int, const std::string&)>
    counter_action_ndrange_migrate(cl_event event, cl_kernel kernel)
    {
      auto xevent = xocl::xocl(event) ;
      auto xkernel = xocl::xocl(kernel) ;

      auto queue = xevent->get_command_queue() ;
      auto device = queue->get_device() ;

      cl_mem mem0 = nullptr ;

      // See how many of the arguments will be migrated and mark them
      for (auto& arg : xkernel->get_argument_range())
      {
	if (auto mem = arg->get_memory_object())
	{
	  if (arg->is_progvar() && 
	      arg->get_address_qualifier() == CL_KERNEL_ARG_ADDRESS_GLOBAL)
	    continue ;
	  else if (mem->is_resident(device))
	    continue ;
	  else if (!(mem->get_flags() & 
		     (CL_MEM_WRITE_ONLY|CL_MEM_HOST_NO_ACCESS)))
	  {
	    mem0 = mem ;
	  }

	}
      }
      
      if (mem0 == nullptr)
      {
	return [](xocl::event*, cl_int, const std::string&)
	       {
	       } ;
      }

      return [mem0](xocl::event* e, cl_int status, const std::string&) 
	     {
	       if (!counter_action_write_cb) return ;
	       if (status != CL_RUNNING && status != CL_COMPLETE) return ;

	       auto queue = e->get_command_queue() ;
	       uint64_t contextId = e->get_context()->get_uid() ;
	       std::string deviceName = queue->get_device()->get_name() ;

	       auto xmem = xocl::xocl(mem0) ;
	       auto ext_flags = xmem->get_ext_flags() ;
	       bool isP2P = ((ext_flags & XCL_MEM_EXT_P2P_BUFFER) != 0) ;

	       uint64_t address = get_memory_address(mem0) ;

	       if (status == CL_RUNNING)
	       {
		 counter_action_write_cb(contextId,
					 deviceName.c_str(),
					 0,
					 true,
					 isP2P,
					 address,
					 reinterpret_cast<uint64_t>(queue));
	       }
	       else if (status == CL_COMPLETE)
	       {
		 counter_action_write_cb(contextId,
					 deviceName.c_str(),
					 xmem->get_size(),
					 false,
					 isP2P,
					 address,
					 reinterpret_cast<uint64_t>(queue)) ;
	       }
	     } ;
    }
  } // end namespace profile
} // end namespace xocl
