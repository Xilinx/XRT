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

// Xilinx implements deprecated APIs
// Examples use deprecated APIs
// Turn off deprecation warnings
#define CL_USE_DEPRECATED_OPENCL_1_0_APIS
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

// Do *not* include cl_ext.h from this directory
#include_next <CL/cl_ext.h>

#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************
* Xilinx vendor extensions*
**************************/

/**
 * struct cl_mem_ext_ptr: Xilinx specific memory extensions
 *
 * Control bank allocation of buffer object.
 *
 * @flags:    Legacy bank flag when @kernel is nullptr
 * @argidx:   Same as @flags, argument index associated with valid @kernel
 * @host_ptr: Host pointer when buffer is created with CL_MEM_USE_HOST_PTR
 * @obj:      Same as @obj
 * @param:    Same as @kernel
 * @kernel:   Kernel associated with @argidx
 *
 * The default legacy layout has overloaded use of flags and param, which
 * is redefined/aliased in interpreted layouts.
 *
 * Two usages are supported:
 *  (1) Specify bank assignment for buffer with XCL_MEM mask. Optionally
 *      use host_ptr for reusing host side buffer per CL_MEM_USE_HOST_PTR.
 *  (2) Specify that buffer is for argument @argidx of kernel @kernel.
 *      Optionally use host_ptr as for (1).
 *
 * To use cl_mem_ext_ptr_t, simply pass the struct object as the host_ptr
 * clCreateBuffer and make sure cl_mem_flags specifies CL_MEM_EXT_PTR_XILINX
 */
typedef struct cl_mem_ext_ptr_t {
  union {
    struct { // legacy layout
      unsigned int flags;   // Top 8 bits reserved for XCL_MEM_EXT flags
      void *obj;
      void *param;
    };
    struct { // interpreted legcy bank assignment
      unsigned int banks;   // Top 8 bits reserved for XCL_MEM_EXT flags
      void *host_ptr;
      void *unused1;        // nullptr required
    };
    struct { // interpreted kernel arg assignment
      unsigned int argidx;  // Top 8 bits reserved for XCL_MEM_EXT flags
      void *host_ptr_;      // use as host_ptr
      cl_kernel kernel;
    };
  };
} cl_mem_ext_ptr_t;

/* Make clCreateBuffer to interpret host_ptr argument as cl_mem_ext_ptr_t */
#define CL_MEM_EXT_PTR_XILINX                       (1 << 31)

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

#ifdef CL_VERSION_1_0
extern cl_int
clSetCommandQueueProperty(cl_command_queue command_queue,
                          cl_command_queue_properties properties,
                          cl_bool enable,
                          cl_command_queue_properties *old_properties);
#endif

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

/*----
 *
 * DOC: OpenCL Stream APIs
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * These structs and functions are used for the new DMA engine QDMA.
 */

/**
 * cl_stream_flags. Type of the stream , eg set to CL_STREAM_READ_ONLY for
 * read only. Used in clCreateStream()
 */
typedef uint64_t cl_stream_flags;
#define CL_STREAM_READ_ONLY			    (1 << 0)
#define CL_STREAM_WRITE_ONLY                        (1 << 1)
#define CL_STREAM_POLLING                           (1 << 2)

/**
 * cl_stream_attributes. eg set it to CL_STREAM for stream mode. Used
 * in clCreateStream()
 */
typedef uint32_t cl_stream_attributes;
#define CL_STREAM                                   (1 << 0)
#define CL_PACKET                                   (1 << 1)

/**
 * cl_stream_attributes.
 * eg set it to CL_STREAM_CDH for Customer Defined Header.
 * Used in clReadStream() and clWriteStream()
 */

#define CL_STREAM_EOT                               (1 << 0)
#define CL_STREAM_CDH                               (1 << 1)
#define CL_STREAM_NONBLOCKING                       (1 << 2)
#define CL_STREAM_SILENT                            (1 << 3)

typedef stream_xfer_req_type         cl_stream_xfer_req_type;
typedef streams_poll_req_completions cl_streams_poll_req_completions;
typedef stream_xfer_req              cl_stream_xfer_req;
typedef struct _cl_stream *          cl_stream;
typedef struct _cl_stream_mem *      cl_stream_mem;

/**
 * clCreateStream - create the stream for reading or writing.
 * @device_id   : The device handle on which stream is to be created.
 * @flags       : The cl_stream_flags
 * @attributes  : The attributes of the requested stream.
 * @ext         : The extension for kernel and argument matching.
 * @errcode_ret : The return value eg CL_SUCCESS
 */
extern CL_API_ENTRY cl_stream CL_API_CALL
clCreateStream(cl_device_id                /* device_id */,
	       cl_stream_flags             /* flags */,
	       cl_stream_attributes        /* attributes*/,
	       cl_mem_ext_ptr_t*              /* ext */,
	       cl_int* /*errcode_ret*/) CL_API_SUFFIX__VERSION_1_0;

/**
 * clReleaseStream - Once done with the stream, release it and its associated
 * objects
 * @stream: The stream to be released.
 * Return a cl_int
 */
extern CL_API_ENTRY cl_int CL_API_CALL
clReleaseStream(cl_stream /*stream*/) CL_API_SUFFIX__VERSION_1_0;

/**
 * clWriteStream - write data to stream
 * @device_id : The device
 * @stream    : The stream
 * @ptr       : The ptr to write from.
 * @offset    : The offset in the ptr to write from
 * @size      : The number of bytes to write.
 * @req_type  : The write request type.
 * errcode_ret: The return value eg CL_SUCCESS
 * Return a cl_int
 */
extern CL_API_ENTRY cl_int CL_API_CALL
clWriteStream(cl_device_id    /* device_id*/,
	cl_stream             /* stream*/,
	const void *          /* ptr */,
	size_t                /* offset */,
	size_t                /* size */,
	cl_stream_xfer_req*   /* attributes */,
	cl_int*               /* errcode_ret*/) CL_API_SUFFIX__VERSION_1_0;

/**
 * clReadStream - write data to stream
 * @device_id : The device
 * @stream    : The stream
 * @ptr       : The ptr to write from.
 * @offset    : The offset in the ptr to write from
 * @size      : The number of bytes to write.
 * @req_type  : The read request type.
 * errcode_ret: The return value eg CL_SUCCESS
 * Return a cl_int.
 */
extern CL_API_ENTRY cl_int CL_API_CALL
clReadStream(cl_device_id     /* device_id*/,
	     cl_stream             /* stream*/,
	     void *                /* ptr */,
	     size_t                /* offset */,
	     size_t                /* size */,
	     cl_stream_xfer_req*   /* attributes */,
	     cl_int*               /* errcode_ret*/) CL_API_SUFFIX__VERSION_1_0;


/* clCreateStreamBuffer - Alloc buffer used for read and write.
 * @size       : The size of the buffer
 * errcode_ret : The return value, eg CL_SUCCESS
 * Returns cl_stream_mem
 */
extern CL_API_ENTRY cl_stream_mem CL_API_CALL
clCreateStreamBuffer(cl_device_id device,
	size_t                /* size*/,
	cl_int *              /* errcode_ret*/) CL_API_SUFFIX__VERSION_1_0;

/* clReleaseStreamBuffer - Release the buffer created.
 * @cl_stream_mem : The stream memory to be released.
 * Return a cl_int
 */
extern CL_API_ENTRY cl_int CL_API_CALL
clReleaseStreamBuffer(cl_stream_mem /*stream memobj */) CL_API_SUFFIX__VERSION_1_0;

/* clPollStreams - Poll streams on a device for completion.
 * @device_id             : The device
 * @completions           : Completions array
 * @min_num_completions   : Minimum number of completions requested
 * @max_num_completions   : Maximum number of completions requested
 * @actual_num_completions: Actual number of completions returned.
 * @timeout               : Timeout in milliseconds (ms)
 * @errcode_ret :         : The return value eg CL_SUCCESS
 * Return a cl_int.
 */
extern CL_API_ENTRY cl_int CL_API_CALL
clPollStreams(cl_device_id /*device*/,
       	cl_streams_poll_req_completions* /*completions*/,
	cl_int  /*min_num_completion*/,
	cl_int  /*max_num_completion*/,
	cl_int* /*actual num_completion*/,
	cl_int /*timeout in ms*/,
	cl_int * /*errcode_ret*/) CL_API_SUFFIX__VERSION_1_0;

//End QDMA APIs

typedef struct _cl_mem * rte_mbuf;
typedef struct _cl_pipe * cl_pipe;

// incorrectly placed in cl.h
typedef cl_uint cl_pipe_attributes;
typedef struct _cl_image_filler_xilinx {
    cl_uint                 t0;
    cl_uint                 t1;
    cl_uint                 t2;
    cl_uint                 t3;
} cl_image_fillier_xilinx;

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


/*
  Host Accessible Program Scope Globals
*/

//cl_mem_flags bitfield
//accepted by the <flags> parameter of clCreateBuffer


//valid flags in cl_mem_ext_ptr_t
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

// cl_mem_info
#define CL_MEM_BANK                     0x1109

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
