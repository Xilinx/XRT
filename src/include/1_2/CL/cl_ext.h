/*******************************************************************************
 * Copyright (c) 2008-2010 The Khronos Group Inc.
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
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 ******************************************************************************/

/* $Revision: 11928 $ on $Date: 2010-07-13 09:04:56 -0700 (Tue, 13 Jul 2010) $ */

/* cl_ext.h contains OpenCL extensions which don't have external */
/* (OpenGL, D3D) dependencies.                                   */

#ifndef __CL_EXT_H
#define __CL_EXT_H

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __APPLE__
	#include <OpenCL/cl.h>
    #include <AvailabilityMacros.h>
#else
	#include <CL/cl.h>
#endif

/* cl_khr_fp64 extension - no extension #define since it has no functions  */
#define CL_DEVICE_DOUBLE_FP_CONFIG                  0x1032

/* cl_khr_fp16 extension - no extension #define since it has no functions  */
#define CL_DEVICE_HALF_FP_CONFIG                    0x1033

/* Memory object destruction
 *
 * Apple extension for use to manage externally allocated buffers used with cl_mem objects with CL_MEM_USE_HOST_PTR
 *
 * Registers a user callback function that will be called when the memory object is deleted and its resources
 * freed. Each call to clSetMemObjectCallbackFn registers the specified user callback function on a callback
 * stack associated with memobj. The registered user callback functions are called in the reverse order in
 * which they were registered. The user callback functions are called and then the memory object is deleted
 * and its resources freed. This provides a mechanism for the application (and libraries) using memobj to be
 * notified when the memory referenced by host_ptr, specified when the memory object is created and used as
 * the storage bits for the memory object, can be reused or freed.
 *
 * The application may not call CL api's with the cl_mem object passed to the pfn_notify.
 *
 * Please check for the "cl_APPLE_SetMemObjectDestructor" extension using clGetDeviceInfo(CL_DEVICE_EXTENSIONS)
 * before using.
 */
#define cl_APPLE_SetMemObjectDestructor 1
cl_int	CL_API_ENTRY clSetMemObjectDestructorAPPLE(  cl_mem /* memobj */,
                                        void (* /*pfn_notify*/)( cl_mem /* memobj */, void* /*user_data*/),
                                        void * /*user_data */ )             CL_EXT_SUFFIX__VERSION_1_0;


/* Context Logging Functions
 *
 * The next three convenience functions are intended to be used as the pfn_notify parameter to clCreateContext().
 * Please check for the "cl_APPLE_ContextLoggingFunctions" extension using clGetDeviceInfo(CL_DEVICE_EXTENSIONS)
 * before using.
 *
 * clLogMessagesToSystemLog fowards on all log messages to the Apple System Logger
 */
#define cl_APPLE_ContextLoggingFunctions 1
extern void CL_API_ENTRY clLogMessagesToSystemLogAPPLE(  const char * /* errstr */,
                                            const void * /* private_info */,
                                            size_t       /* cb */,
                                            void *       /* user_data */ )  CL_EXT_SUFFIX__VERSION_1_0;

/* clLogMessagesToStdout sends all log messages to the file descriptor stdout */
extern void CL_API_ENTRY clLogMessagesToStdoutAPPLE(   const char * /* errstr */,
                                          const void * /* private_info */,
                                          size_t       /* cb */,
                                          void *       /* user_data */ )    CL_EXT_SUFFIX__VERSION_1_0;

/* clLogMessagesToStderr sends all log messages to the file descriptor stderr */
extern void CL_API_ENTRY clLogMessagesToStderrAPPLE(   const char * /* errstr */,
                                          const void * /* private_info */,
                                          size_t       /* cb */,
                                          void *       /* user_data */ )    CL_EXT_SUFFIX__VERSION_1_0;


/************************
* cl_khr_icd extension *
************************/
#define cl_khr_icd 1

/* cl_platform_info                                                        */
#define CL_PLATFORM_ICD_SUFFIX_KHR                  0x0920

/* Additional Error Codes                                                  */
#define CL_PLATFORM_NOT_FOUND_KHR                   -1001

extern CL_API_ENTRY cl_int CL_API_CALL
clIcdGetPlatformIDsKHR(cl_uint          /* num_entries */,
                       cl_platform_id * /* platforms */,
                       cl_uint *        /* num_platforms */);

typedef CL_API_ENTRY cl_int (CL_API_CALL *clIcdGetPlatformIDsKHR_fn)(
    cl_uint          /* num_entries */,
    cl_platform_id * /* platforms */,
    cl_uint *        /* num_platforms */);


/******************************************
* cl_nv_device_attribute_query extension *
******************************************/
/* cl_nv_device_attribute_query extension - no extension #define since it has no functions */
#define CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV       0x4000
#define CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV       0x4001
#define CL_DEVICE_REGISTERS_PER_BLOCK_NV            0x4002
#define CL_DEVICE_WARP_SIZE_NV                      0x4003
#define CL_DEVICE_GPU_OVERLAP_NV                    0x4004
#define CL_DEVICE_KERNEL_EXEC_TIMEOUT_NV            0x4005
#define CL_DEVICE_INTEGRATED_MEMORY_NV              0x4006


/*********************************
* cl_amd_device_attribute_query *
*********************************/
#define CL_DEVICE_PROFILING_TIMER_OFFSET_AMD        0x4036


#ifdef CL_VERSION_1_1
   /***********************************
    * cl_ext_device_fission extension *
    ***********************************/
    #define cl_ext_device_fission   1

    extern CL_API_ENTRY cl_int CL_API_CALL
    clReleaseDeviceEXT( cl_device_id /*device*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL *clReleaseDeviceEXT_fn)( cl_device_id /*device*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    extern CL_API_ENTRY cl_int CL_API_CALL
    clRetainDeviceEXT( cl_device_id /*device*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    typedef CL_API_ENTRY cl_int
    (CL_API_CALL *clRetainDeviceEXT_fn)( cl_device_id /*device*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    typedef cl_ulong  cl_device_partition_property_ext;
    extern CL_API_ENTRY cl_int CL_API_CALL
    clCreateSubDevicesEXT(  cl_device_id /*in_device*/,
                            const cl_device_partition_property_ext * /* properties */,
                            cl_uint /*num_entries*/,
                            cl_device_id * /*out_devices*/,
                            cl_uint * /*num_devices*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    typedef CL_API_ENTRY cl_int
    ( CL_API_CALL * clCreateSubDevicesEXT_fn)(  cl_device_id /*in_device*/,
                                                const cl_device_partition_property_ext * /* properties */,
                                                cl_uint /*num_entries*/,
                                                cl_device_id * /*out_devices*/,
                                                cl_uint * /*num_devices*/ ) CL_EXT_SUFFIX__VERSION_1_1;

    /* cl_device_partition_property_ext */
    #define CL_DEVICE_PARTITION_EQUALLY_EXT             0x4050
    #define CL_DEVICE_PARTITION_BY_COUNTS_EXT           0x4051
    #define CL_DEVICE_PARTITION_BY_NAMES_EXT            0x4052
    #define CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN_EXT  0x4053

    /* clDeviceGetInfo selectors */
    #define CL_DEVICE_PARENT_DEVICE_EXT                 0x4054
    #define CL_DEVICE_PARTITION_TYPES_EXT               0x4055
    #define CL_DEVICE_AFFINITY_DOMAINS_EXT              0x4056
    #define CL_DEVICE_REFERENCE_COUNT_EXT               0x4057
    #define CL_DEVICE_PARTITION_STYLE_EXT               0x4058

    /* error codes */
    #define CL_DEVICE_PARTITION_FAILED_EXT              -1057
    #define CL_INVALID_PARTITION_COUNT_EXT              -1058
    #define CL_INVALID_PARTITION_NAME_EXT               -1059

    /* CL_AFFINITY_DOMAINs */
    #define CL_AFFINITY_DOMAIN_L1_CACHE_EXT             0x1
    #define CL_AFFINITY_DOMAIN_L2_CACHE_EXT             0x2
    #define CL_AFFINITY_DOMAIN_L3_CACHE_EXT             0x3
    #define CL_AFFINITY_DOMAIN_L4_CACHE_EXT             0x4
    #define CL_AFFINITY_DOMAIN_NUMA_EXT                 0x10
    #define CL_AFFINITY_DOMAIN_NEXT_FISSIONABLE_EXT     0x100

    /* cl_device_partition_property_ext list terminators */
    #define CL_PROPERTIES_LIST_END_EXT                  ((cl_device_partition_property_ext) 0)
    #define CL_PARTITION_BY_COUNTS_LIST_END_EXT         ((cl_device_partition_property_ext) 0)
    #define CL_PARTITION_BY_NAMES_LIST_END_EXT          ((cl_device_partition_property_ext) 0 - 1)



#endif /* CL_VERSION_1_1 */

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
#define XCL_MEM_TOPOLOGY                (1<<24)
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


#endif /* __CL_EXT_H */

// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
