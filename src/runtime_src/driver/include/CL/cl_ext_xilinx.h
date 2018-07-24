/*******************************************************************************
 * Copyright (c) 2008-2015 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * MODIFICATIONS TO THIS FILE MAY MEAN IT NO LONGER ACCURATELY REFLECTS
 * KHRONOS STANDARDS. THE UNMODIFIED, NORMATIVE VERSIONS OF KHRONOS
 * SPECIFICATIONS AND HEADER INFORMATION ARE LOCATED AT
 *    https://www.khronos.org/registry/
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 ******************************************************************************/

/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 */

#ifndef __CL_EXT_XILINX_H
#define __CL_EXT_XILINX_H

#include <CL/cl_ext.h>

#ifdef __cplusplus
extern "C" {
#endif

/**************************
* Xilinx vendor extensions*
**************************/

#define CL_XILINX_UNIMPLEMENTED  -20

/* New flags for cl_queue */
#define CL_QUEUE_DPDK                               (1 << 31)

#define CL_MEM_REGISTER_MAP                         (1 << 27)
#ifdef PMD_OCL
# define CL_REGISTER_MAP CL_MEM_REGISTER_MAP
#endif
/* Delay device side buffer allocation for progvars */
#define CL_MEM_PROGVAR                              (1 << 28)
/* New cl_mem flags for DPDK Buffer integration */
#define CL_MEM_RTE_MBUF_READ_ONLY                   (1 << 29)
#define CL_MEM_RTE_MBUF_WRITE_ONLY                  (1 << 30)

#define CL_PIPE_ATTRIBUTE_DPDK_ID                   (1 << 31)

/* Additional cl_device_partition_property */
#define CL_DEVICE_PARTITION_BY_CONNECTIVITY         (1 << 31)


/**
 * Aquire the device address associated with a cl_mem buffer on
 * a specific device.
 *
 * CL_INVALID_MEM_OBJECT: if mem is not a valid buffer object,
 *                        or if mem is not associated with device.
 * CL_INVALID_DEVICE    : if device is not a valid device.
 * CL_INVALID_VALUE     : if address is nullptr
 * CL_INVALID_VALUE     : if sz is different from sizeof(uintptr_tr)
 */
extern cl_int
xclGetMemObjDeviceAddress(cl_mem mem,
                          cl_device_id device,
                          size_t sz,
                          void* address);

/**
 * Acquire FD associated with a cl_mem buffer from an exporting device.
 *
 * CL_INVALID_MEM_OBJECT: if mem is not a valid buffer object,
 *                        or if mem is not associated with any device,
 *                        or if unable to obtain FD from exporting device
 * CL_INVALID_VALUE     : if fd is nullptr
 */
extern cl_int
xclGetMemObjectFd(cl_mem mem,
                  int* fd); /* returned fd */


/**
 * Acquire cl_mem buffer object in this context on the importing device associated
 * with a FD from an exporting device.
 *
 * CL_INVALID_MEM_OBJECT: if unable to obtain MemObject handle from exporting device
 * CL_INVALID_DEVICE    : if device is not a valid device,
 *                        if device is not in this context.
 * CL_INVALID_VALUE     : if fd is nullptr,
 *                        if context is nullptr,
 *                        if mem address of variable for cl_mem pointer is nullptr.
 */
extern cl_int
xclGetMemObjectFromFd(cl_context context,
                      cl_device_id deviceid,
                      cl_mem_flags flags,
                      int fd,
                      cl_mem* mem);



extern cl_int
xclEnqueuePeerToPeerCopyBuffer(cl_command_queue    command_queue,
                     cl_mem              src_buffer,
                     cl_mem              dst_buffer,
                     size_t              src_offset,
                     size_t              dst_offset,
                     size_t              size,
                     cl_uint             num_events_in_wait_list,
                     const cl_event *    event_wait_list,
                     cl_event *          event_parameter);

// -- Work in progress - new QDMA APIs
//
//struct rte_mbuf;
typedef struct _cl_mem * rte_mbuf;
typedef struct _cl_pipe * cl_pipe;


/* New flag RTE_MBUF_READ_ONLY or RTE_MBUF_WRITE_ONLY
 * OpenCL runtime will use rte_eth_rx_queue_setup to create DPDK RX Ring. The API will return cl_pipe object.
 * OpenCL runtime will use rte_eth_tx_queue_setup to create DPDK TX Ring. The API will return cl_pipe object.
 */
extern CL_API_ENTRY cl_pipe CL_API_CALL
    clCreateHostPipe(cl_device_id device,
	    cl_mem_flags flags,
	    cl_uint packet_size,
	    cl_uint max_packets,
	    const cl_pipe_attributes *attributes, //TODO: properties?
	    cl_int *errcode_ret) CL_API_SUFFIX__VERSION_1_0;

/* OpenCL runtime will use rte_eth_tx_burst to send the buffers to TX queue.
 * The API will return count of buffers successfully sent. The API would bind the buffers
 * to descriptors in the TX Ring
 */
extern CL_API_ENTRY cl_uint CL_API_CALL
    clWritePipeBuffers(cl_command_queue command_queue,
	    cl_pipe pipe,
	    rte_mbuf** buf,
	    cl_uint count,
	    cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;

/* OpenCL runtime will use rte_eth_rx_burst to receive buffers from RX queue.
 * The API will return count of buffers received. The API would unbind the buffers
 * from the descriptors in the RX Ring.
 */
extern CL_API_ENTRY cl_uint CL_API_CALL
    clReadPipeBuffers(cl_command_queue command_queue,
	    cl_pipe pipe,
	    rte_mbuf** buf,
	    cl_uint count,
	    cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;

/*Use rte_pktmbuf_alloc to allocate a buffer from the same mempool used by the pipe.
 * This buffer is not yet bound to any descriptor in the RX/TX queue referred to by the pipe.
 */
extern CL_API_ENTRY rte_mbuf* CL_API_CALL
    clAcquirePipeBuffer(cl_command_queue command_queue,
	    cl_pipe pipe,
	    cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;


/*Use rte_pktmbuf_free to return a buffer to the same mempool used by the pipe.
 * This buffer should not be bound to any descriptor in the RX/TX queue referred to by the pipe.
 */
extern CL_API_ENTRY cl_int CL_API_CALL
    clReleasePipeBuffer(cl_command_queue command_queue,
	    cl_pipe pipe,
	    rte_mbuf* buf) CL_API_SUFFIX__VERSION_1_0;

//End work-in-progress QDMA APIs

/*
  Host Accessible Program Scope Globals
*/

//cl_mem_flags bitfield
//accepted by the <flags> parameter of clCreateBuffer

#define CL_MEM_EXT_PTR_XILINX                       (1 << 31)

typedef struct{
  unsigned flags; //Top 8 bits reserved.
  void *obj;
  void *param;
} cl_mem_ext_ptr_t;

//valid flags in above
#define XCL_MEM_DDR_BANK0               (1<<0)
#define XCL_MEM_DDR_BANK1               (1<<1)
#define XCL_MEM_DDR_BANK2               (1<<2)
#define XCL_MEM_DDR_BANK3               (1<<3)

//-- 8 reserved bits of flags in cl_mem_ext_ptr_t
#define XCL_MEM_LEGACY                  0x0
#define XCL_MEM_TOPOLOGY                (1<<31)
#define XCL_MEM_EXT_P2P_BUFFER          (1<<30)

//cl_program_info
//accepted by the <flags> paramete of clGetProrgamInfo
#define CL_PROGRAM_BUFFERS_XILINX       0x1180

// cl_kernel_info
#define CL_KERNEL_COMPUTE_UNIT_COUNT    0x1300
#define CL_KERNEL_INSTANCE_BASE_ADDRESS 0x1301

// cl_program_build_info (CR962714)
#define CL_PROGRAM_TARGET_TYPE          0x1190

// valid target types (CR962714)
typedef cl_uint cl_program_target_type;
#define CL_PROGRAM_TARGET_TYPE_NONE     0x0
#define CL_PROGRAM_TARGET_TYPE_HW       0x1
#define CL_PROGRAM_TARGET_TYPE_SW_EMU   0x2
#define CL_PROGRAM_TARGET_TYPE_HW_EMU   0x4

#ifdef __cplusplus
}
#endif

#endif
