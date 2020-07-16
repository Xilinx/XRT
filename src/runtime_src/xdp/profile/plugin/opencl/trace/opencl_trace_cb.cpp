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

#include "xdp/profile/plugin/opencl/trace/opencl_trace_cb.h"
#include "xdp/profile/plugin/opencl/trace/opencl_trace_plugin.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/opencl_api_calls.h"
#include "xdp/profile/database/events/opencl_host_events.h"
#include "core/common/time.h"

namespace xdp {

  static OpenCLTraceProfilingPlugin openclPluginInstance ;

  static void log_function_start(const char* functionName,
				 uint64_t queueAddress,
				 uint64_t functionID)
  {
    double timestamp = xrt_core::time_ns() ;
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    if (queueAddress != 0) 
      (db->getStaticInfo()).addCommandQueueAddress(queueAddress) ;

    VTFEvent* event = new OpenCLAPICall(0,
					timestamp,
					functionID,
					(db->getDynamicInfo()).addString(functionName),
					queueAddress
					) ;
    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).markStart(functionID, event->getEventId()) ;
  }

  static void log_function_end(const char* functionName,
			       uint64_t queueAddress,
			       uint64_t functionID)
  {
    double timestamp = xrt_core::time_ns() ;
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    uint64_t start = (db->getDynamicInfo()).matchingStart(functionID) ;

    VTFEvent* event = new OpenCLAPICall(start,
					timestamp,
					functionID,
					(db->getDynamicInfo()).addString(functionName),
					queueAddress) ;
    (db->getDynamicInfo()).addEvent(event) ;
  }

  /*
  static void action_read(unsigned int id,
			  bool isStart,
			  unsigned long long int deviceAddress,
			  const char* memoryResource,
			  size_t bufferSize,
			  bool isP2P,
			  unsigned long int* dependencies,
			  unsigned int numDependencies)
  {
    double timestamp = xrt_core::time_ns() ;
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    uint64_t start = 0 ;
    
    if (!isStart) start = (db->getDynamicInfo()).matchingStart(id) ;

    VTFEvent* event = 
      new OpenCLBufferTransfer(start,
			       timestamp,
			       (isP2P ? READ_BUFFER_P2P : READ_BUFFER),
			       deviceAddress,
			       (db->getDynamicInfo()).addString(memoryResource),
			       bufferSize) ;

    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).addOpenCLMapping(id, event->getEventId()) ;

    if (dependencies != nullptr && numDependencies > 0)
    {
      std::vector<uint64_t> dependentEvents ;
      for (uint32_t i = 0 ; i < numDependencies ; ++i)
      {
	dependentEvents.push_back(dependencies[i]) ;
      }
      (db->getDynamicInfo()).addDependencies(event->getEventId(), 
					     dependentEvents);
    }
  }

  static void action_write(unsigned int id,
			   bool isStart,
			   unsigned long long int deviceAddress,
			   const char* memoryResource,
			   size_t bufferSize,
			   bool isP2P,
			   unsigned long int* dependencies,
			   unsigned int numDependencies)
  {
    double timestamp = xrt_core::time_ns() ;
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    uint64_t start = 0 ;
    
    if (!isStart) start = (db->getDynamicInfo()).matchingStart(id) ;

    VTFEvent* event = 
      new OpenCLBufferTransfer(start,
			       timestamp,
			       (isP2P ? WRITE_BUFFER_P2P : WRITE_BUFFER),
			       deviceAddress,
			       (db->getDynamicInfo()).addString(memoryResource),
			       bufferSize) ;

    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).addOpenCLMapping(id, event->getEventId()) ;

    if (dependencies != nullptr && numDependencies > 0)
    {
      std::vector<uint64_t> dependentEvents ;
      for (uint32_t i = 0 ; i < numDependencies ; ++i)
      {
	dependentEvents.push_back(dependencies[i]) ;
      }
      (db->getDynamicInfo()).addDependencies(event->getEventId(), 
					     dependentEvents);
    }
  }

  static void action_copy(unsigned int id,
			  bool isStart,
			  unsigned long long int deviceAddress,
			  const char* memoryResource,
			  size_t bufferSize,
			  bool isP2P,
			  unsigned long int* dependencies,
			  unsigned int numDependencies)
  {
    double timestamp = xrt_core::time_ns() ;
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    uint64_t start = 0 ;
    
    if (!isStart) start = (db->getDynamicInfo()).matchingStart(id) ;

    VTFEvent* event = 
      new OpenCLBufferTransfer(start,
			       timestamp,
			       (isP2P ? COPY_BUFFER_P2P : COPY_BUFFER),
			       deviceAddress,
			       (db->getDynamicInfo()).addString(memoryResource),
			       bufferSize) ;

    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).addOpenCLMapping(id, event->getEventId()) ;

    if (dependencies != nullptr && numDependencies > 0)
    {
      std::vector<uint64_t> dependentEvents ;
      for (uint32_t i = 0 ; i < numDependencies ; ++i)
      {
	dependentEvents.push_back(dependencies[i]) ;
      }
      (db->getDynamicInfo()).addDependencies(event->getEventId(), 
					     dependentEvents);
    }
  }

  static void action_ndrange(unsigned int id, bool isStart)
  {
    
  }
  */

} // end namespace xdp

extern "C"
void function_start(const char* functionName, 
		    unsigned long long int queueAddress,
		    unsigned long long int functionID)
{
  xdp::log_function_start(functionName, queueAddress, functionID) ;
}

extern "C"
void function_end(const char* functionName, 
		  unsigned long long int queueAddress,
		  unsigned long long int functionID)
{
  xdp::log_function_end(functionName, queueAddress, functionID) ;
}

/*
extern "C"
void action_read(unsigned int id,
		 bool isStart,
		 unsigned long long int deviceAddress,
		 const char* memoryResource,
		 size_t bufferSize,
		 bool isP2P,
		 unsigned long int* dependencies,
		 unsigned int numDependencies)
{
  xdp::action_read(id, isStart, deviceAddress, memoryResource, bufferSize, 
		   isP2P, dependencies, numDependencies) ;
}

extern "C"
void action_write(unsigned int id,
		  bool isStart,
		  unsigned long long int deviceAddress,
		  const char* memoryResource,
		  size_t bufferSize,
		  bool isP2P,
		  unsigned long int* dependencies,
		  unsigned int numDependencies)
{
  xdp::action_write(id, isStart, deviceAddress, memoryResource, bufferSize, 
		    isP2P, dependencies, numDependencies) ;
}

extern "C"
void action_copy(unsigned int id,
		 bool isStart,
		 unsigned long long int deviceAddress,
		 const char* memoryResource,
		 size_t bufferSize,
		 bool isP2P,
		 unsigned long int* dependencies,
		 unsigned int numDependencies)
{
  xdp::action_copy(id, isStart, deviceAddress, memoryResource, bufferSize,
		   isP2P, dependencies, numDependencies) ;
}

extern "C"
void action_ndrange(unsigned int id, bool isStart)
{
  xdp::action_ndrange(id, isStart) ;
}
*/
