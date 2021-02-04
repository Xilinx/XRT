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
#if 0

#ifndef HAL_DEVICE_TRACE_WRITER_DOT_H
#define HAL_DEVICE_TRACE_WRITER_DOT_H

#include <string>

#include "xdp/profile/writer/vp_base/vp_trace_writer.h"
#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/database/database.h"

namespace xdp {

  class HALDeviceTraceWriter : public VPTraceWriter
  {
  private:
    HALDeviceTraceWriter() = delete ;

    // Specific header information
    std::string xrtVersion;
    std::string toolVersion;

    std::map<int32_t, uint32_t>  cuBucketIdMap;
    std::map<uint32_t, uint32_t> aimBucketIdMap;
    std::map<uint32_t, uint32_t> asmBucketIdMap;

    uint64_t deviceId;

  protected:
    virtual void writeHeader() ;
    virtual void writeStructure() ;
    virtual void writeStringTable() ;
    virtual void writeTraceEvents() ;
    virtual void writeDependencies() ;

  public:
    HALDeviceTraceWriter(const char* filename, uint64_t deviceId, const std::string& version,
			 const std::string& creationTime,
			 const std::string& xrtV,
			 const std::string& toolV);

    ~HALDeviceTraceWriter() ;

    virtual void write(bool openNewFile) ;
    virtual bool isDevice() { return true ; } 
    virtual bool isSameDevice(void* /*handle*/) 
    { // return dev->getAbstractDevice()->getRawDevice() == handle ;
      return true ; }
  } ;

} // end namespace xdp

#endif

#endif
