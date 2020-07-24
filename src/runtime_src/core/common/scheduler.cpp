/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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

#include "scheduler.h"
#include "system.h"
#include "device.h"
#include "config_reader.h"
#include "xclbin_parser.h"
#include "message.h"
#include "ert.h"
#include "types.h"

#include <memory>
#include <string>
#include <stdexcept>
#include <cstring>
#include <iostream>

#ifdef __GNUC__
#include <sys/mman.h>
#endif

#ifdef _WIN32
# pragma warning( disable : 4244 4245 4267 4996)
#endif

// This is interim, must be consolidated with runtime_src/xrt/scheduler
// when XRT C++ code is refactored.

namespace {

/**
 * struct buffer_object - wrapper for a buffer object
 *
 * @bo: buffer object handle
 * @data: mapped kernel space data accessible in userspace
 * @size: size of buffer object
 * @dev: device handle associated with this buffer object
 * @prop: buffer object properties
 */
struct buffer_object
{
  xclBufferHandle bo;
  void* data;
  size_t size;
  xclDeviceHandle dev;
  xclBOProperties prop;
};

using buffer = std::shared_ptr<buffer_object>;

/**
 * create_exec_bo() - create a buffer object command execution
 *
 * @device: Device to associated with the buffer object should be allocated
 * @sz: Size of the buffer object
 * Return: Shared pointer to the allocated and mapped buffer object
 */
static buffer
create_exec_bo(xclDeviceHandle handle, size_t sz)
{
  auto delBO = [](buffer_object* bo) {
    xclUnmapBO(bo->dev, bo->bo, bo->data);
    xclFreeBO(bo->dev,bo->bo);
    delete bo;
  };

  auto ubo = std::make_unique<buffer_object>();
  ubo->dev = handle;
  ubo->bo = xclAllocBO(ubo->dev,sz,0,XCL_BO_FLAGS_EXECBUF);
  ubo->data = xclMapBO(ubo->dev,ubo->bo,true /*write*/);
  ubo->size = sz;
  std::memset(reinterpret_cast<ert_packet*>(ubo->data),0,sz);
  return buffer(ubo.release(),delBO);
}

/**
 * create_data_bo() - create a buffer object for data
 *
 * @device: Device to associated with the buffer object should be allocated
 * @sz: Size of the buffer object
 * @flags: Flags for allocating buffer
 * Return: Shared pointer to the allocated and mapped buffer object
 */
static buffer
create_data_bo(xclDeviceHandle handle, size_t sz, uint32_t flags)
{
  auto delBO = [](buffer_object* bo) {
    xclUnmapBO(bo->dev, bo->bo, bo->data);
    xclFreeBO(bo->dev, bo->bo);
    delete bo;
  };

  auto ubo = std::make_unique<buffer_object>();
  ubo->dev = handle;
  ubo->bo = xclAllocBO(ubo->dev, sz, 0, flags);
  ubo->data = xclMapBO(ubo->dev, ubo->bo, true /*write*/);

  if (xclGetBOProperties(ubo->dev, ubo->bo, &ubo->prop))
    throw std::runtime_error("Failed to get BO properties");

  ubo->size = sz;
  std::memset(reinterpret_cast<ert_packet*>(ubo->data), 0, sz);
  return buffer(ubo.release(), delBO);
}

} // unnamed

namespace xrt_core { namespace scheduler {

/**
 * init() - Initialize scheduler
 *
 * Initialize the scheduler
 * Gather, number of CUs, max regmap size (for number of slots)
 * Check sdaccel.ini for default overrides.
 *
 * If there are soft kernels in XCLBIN, config soft kernels as
 * well.
 */
int
init(xclDeviceHandle handle, const axlf* top)
{
  auto execbo = create_exec_bo(handle,0x1000);
  auto ecmd = reinterpret_cast<ert_configure_cmd*>(execbo->data);
  ecmd->state = ERT_CMD_STATE_NEW;
  ecmd->opcode = ERT_CONFIGURE;
  ecmd->type = ERT_CTRL;

  auto device = xrt_core::get_userpf_device(handle);
  ecmd->slot_size = device->get_ert_slots().second;

  auto cus = xclbin::get_cus(top, true);
  ecmd->num_cus = cus.size();
  ecmd->cu_shift = 16;
  ecmd->cu_base_addr = xclbin::get_cu_base_offset(top);
  ecmd->ert = config::get_ert();
  ecmd->polling = xrt_core::config::get_ert_polling();
  ecmd->cu_dma  = xrt_core::config::get_ert_cudma();
  ecmd->cu_isr  = xrt_core::config::get_ert_cuisr() && xclbin::get_cuisr(top);
  ecmd->cq_int  = xrt_core::config::get_ert_cqint();
  ecmd->dataflow = xclbin::get_dataflow(top) || xrt_core::config::get_feature_toggle("Runtime.dataflow");

  // cu addr map
  std::copy(cus.begin(), cus.end(), ecmd->data);
  ecmd->count = 5 + cus.size();

  xuid_t uuid = {0};
  uuid_copy(uuid, top->m_header.uuid);
  if (xclOpenContext(handle,uuid,std::numeric_limits<unsigned int>::max(),true))
    throw std::runtime_error("unable to reserve virtual CU");

  if (xclExecBuf(handle,execbo->bo))
    throw std::runtime_error("unable to issue xclExecBuf");

  // wait for command to complete
  while (ecmd->state < ERT_CMD_STATE_COMPLETED)
    while (xclExecWait(handle,1000)==0) ;

  auto sks = xclbin::get_softkernels(top);

  if (!sks.empty()) {
    // config soft kernel
    auto flags = xclbin::get_first_used_mem(top);
    if (flags < 0)
      throw std::runtime_error("unable to get available memory bank");

    ert_configure_sk_cmd* scmd;
    scmd = reinterpret_cast<ert_configure_sk_cmd*>(execbo->data);

    uint32_t start_cuidx = 0;
    for (const auto& sk:sks) {
      auto skbo = create_data_bo(handle, sk.size, flags);

      std::memset(scmd, 0, 0x1000);
      scmd->state = ERT_CMD_STATE_NEW;
      scmd->opcode = ERT_SK_CONFIG;
      ecmd->type = ERT_CTRL;
      scmd->count = sizeof (ert_configure_sk_cmd) / 4 - 1;
      scmd->start_cuidx = start_cuidx;
      scmd->num_cus = sk.ninst;
      sk.symbol_name.copy(reinterpret_cast<char*>(scmd->sk_name), 31);
      scmd->sk_addr = skbo->prop.paddr;
      scmd->sk_size = skbo->prop.size;
      std::memcpy(skbo->data, sk.sk_buf, sk.size);
      if (xclSyncBO(handle, skbo->bo, XCL_BO_SYNC_BO_TO_DEVICE, sk.size, 0))
	    throw std::runtime_error("unable to synch BO to device");

      if (xclExecBuf(handle,execbo->bo))
        throw std::runtime_error("unable to issue xclExecBuf");

      // wait for command to complete
      while (scmd->state < ERT_CMD_STATE_COMPLETED)
        while (xclExecWait(handle,1000)==0) ;

      start_cuidx += sk.ninst;
    }
  }

  xclCloseContext(handle,uuid,std::numeric_limits<unsigned int>::max());

  return 0;
}


}} // scheduler, xrt_core
