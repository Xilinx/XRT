/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) APIs
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

#ifndef _XCL_XRT_DEPRECATED_H_
#define _XCL_XRT_DEPRECATED_H_

/* This header file is included from include.xrt.h, it is not
 * Not a stand-alone header file */

#ifdef __GNUC__
# define XRT_DEPRECATED __attribute__ ((deprecated))
#else
# define XRT_DEPRECATED
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Use xbutil to reset device */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclResetDevice(xclDeviceHandle handle, enum xclResetKind kind);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclLockDevice(xclDeviceHandle handle);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclUnlockDevice(xclDeviceHandle handle);

/* Use xbmgmt to flash device */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclUpgradeFirmware(xclDeviceHandle handle, const char *fileName);

/* Use xbmgmt to flash device */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclUpgradeFirmware2(xclDeviceHandle handle, const char *file1, const char* file2);

/* Use xbmgmt to flash device */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclUpgradeFirmwareXSpi(xclDeviceHandle handle, const char *fileName, int index);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclBootFPGA(xclDeviceHandle handle);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclRemoveAndScanFPGA();

/* Use xclGetBOProperties */
XRT_DEPRECATED
static inline size_t
xclGetBOSize(xclDeviceHandle handle, xclBufferHandle boHandle)
{
    struct xclBOProperties p;
    return !xclGetBOProperties(handle, boHandle, &p) ? (size_t)p.size : (size_t)-1;
}

/* Use xclGetBOProperties */
XRT_DEPRECATED
static inline uint64_t
xclGetDeviceAddr(xclDeviceHandle handle, xclBufferHandle boHandle)
{
    struct xclBOProperties p;
    return !xclGetBOProperties(handle, boHandle, &p) ? p.paddr : (uint64_t)-1;
}

/* Use xclRegWrite */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
size_t
xclWrite(xclDeviceHandle handle, enum xclAddressSpace space, uint64_t offset,
         const void *hostBuf, size_t size);

/* Use xclRegRead */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
size_t
xclRead(xclDeviceHandle handle, enum xclAddressSpace space, uint64_t offset,
        void *hostbuf, size_t size);

/* Not supported */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclRegisterInterruptNotify(xclDeviceHandle handle, unsigned int userInterrupt,
                           int fd);

/**
 * DOC: XRT Stream Queue APIs
 *
 * NOTE: ALL STREAMING APIs ARE DEPRECATED!!!!
 * THESE WILL BE REMOVED IN A FUTURE RELEASE
 * These functions are used for next generation DMA Engine, QDMA. QDMA
 * provides not only memory mapped DMA which moves data between host
 * memory and board memory, but also stream DMA which moves data
 * between host memory and kernel directly. XDMA memory mapped DMA
 * APIs are also supported on QDMA. New stream APIs are provided here
 * for preview and may be revised in a future release. These can only
 * be used with platforms with QDMA engine under the hood. The higher
 * level OpenCL based streaming APIs offer more refined interfaces and
 * compatibility between releases and each stream maps to a QDMA queue
 * underneath.
 */

enum xclStreamContextFlags {
	/* Enum for xclQueueContext.flags */
	XRT_QUEUE_FLAG_POLLING		= (1 << 2),
};

/*
 * struct xclQueueContext - structure to describe a Queue
 */
struct xclQueueContext {
    uint32_t	type;	   /* stream or packet Queue, read or write Queue*/
    uint32_t	state;	   /* initialized, running */
    uint64_t	route;	   /* route id from xclbin */
    uint64_t	flow;	   /* flow id from xclbin */
    uint32_t	qsize;	   /* number of descriptors */
    uint32_t	desc_size; /* this might imply max inline msg size */
    uint64_t	flags;	   /* isr en, wb en, etc */
};

/**
 * xclCreateWriteQueue - Create Write Queue(DEPRECATED)
 *
 * @handle:        Device handle
 * @q_ctx:         Queue Context
 * @q_hdl:         Queue handle
 * Return:         0 or appropriate error number
 *
 * Create write queue based on information provided in Queue
 * context. Queue handle is generated if creation successes.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclCreateWriteQueue(xclDeviceHandle handle, struct xclQueueContext *q_ctx,
                    uint64_t *q_hdl);

/**
 * xclCreateReadQueue - Create Read Queue(DEPRECATED)
 *
 * @handle:        Device handle
 * @q_ctx:         Queue Context
 * @q_hdl:         Queue handle
 * Return:         0 or appropriate error number
 *
 * Create read queue based on information provided in Queue
 * context. Queue handle is generated if creation successes.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclCreateReadQueue(xclDeviceHandle handle, struct xclQueueContext *q_ctx,
                   uint64_t *q_hdl);

/**
 * xclDestroyQueue - Destroy Queue(DEPRECATED)
 *
 * @handle:        Device handle
 * @q_hdl:         Queue handle
 * Return:         0 or appropriate error number
 *
 * Destroy read or write queue and release all queue resources.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclDestroyQueue(xclDeviceHandle handle, uint64_t q_hdl);

/**
 * xclAllocQDMABuf - Allocate DMA buffer(DEPRECATED)
 *
 * @handle:        Device handle
 * @size:          Buffer size
 * @buf_hdl:       Buffer handle
 * Return:         0 or appropriate error number
 *
 * Allocate DMA buffer which is used for queue read and write.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
void*
xclAllocQDMABuf(xclDeviceHandle handle, size_t size, uint64_t *buf_hdl);

/**
 * xclFreeQDMABuf - Free DMA buffer(DEPRECATED)
 *
 * @handle:        Device handle
 * @buf_hdl:       Buffer handle
 * Return:         0 or appropriate error number
 *
 * Free DMA buffer allocated by xclAllocQDMABuf.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclFreeQDMABuf(xclDeviceHandle handle, uint64_t buf_hdl);

/*
 * xclModifyQueue - Modify Queue(DEPRECATED)
 *
 * @handle:		Device handle
 * @q_hdl:		Queue handle
 *
 * This function modifies Queue context on the fly. Modifying rid implies
 * to program hardware traffic manager to connect Queue to the kernel pipe.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclModifyQueue(xclDeviceHandle handle, uint64_t q_hdl);

/*
 * xclStartQueue - set Queue to running state(DEPRECATED)
 * @handle:             Device handle
 * @q_hdl:              Queue handle
 *
 * This function set xclStartQueue to running state. xclStartQueue
 * starts to process Read and Write requests.
 * TODO: remove this
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclStartQueue(xclDeviceHandle handle, uint64_t q_hdl);

/*
 * xclStopQueue - set Queue to init state(DEPRECATED)
 * @handle:             Device handle
 * @q_hdl:              Queue handle
 *
 * This function set Queue to init state. all pending read and write
 * requests will be flushed.  wr_complete and rd_complete will be
 * called with error wbe for flushed requests.
 * TODO: remove this
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclStopQueue(xclDeviceHandle handle, uint64_t q_hdl);

/*
 * struct xclWRBuffer
 */
struct xclReqBuffer {
    union {
	char*    buf;    // ptr or,
	uint64_t va;	 // offset
    };
    uint64_t  len;
    uint64_t  buf_hdl;   // NULL when first field is buffer pointer
};

/*
 * enum xclQueueRequestKind - request type.
 */
enum xclQueueRequestKind {
    XCL_QUEUE_WRITE = 0,
    XCL_QUEUE_READ  = 1,
    //More, in-line etc.
};

/*
 * enum xclQueueRequestFlag - flags associated with the request.
 */
/* this has to be the same with Xfer flags defined in opencl CL_STREAM* */
enum xclQueueRequestFlag {
    XCL_QUEUE_REQ_EOT			= 1 << 0,
    XCL_QUEUE_REQ_CDH			= 1 << 1,
    XCL_QUEUE_REQ_NONBLOCKING		= 1 << 2,
    XCL_QUEUE_REQ_SILENT		= 1 << 3, /* not supp. not generate event for non-blocking req */
};

/*
 * struct xclQueueRequest - read and write request
 */
struct xclQueueRequest {
    enum xclQueueRequestKind op_code;
    struct xclReqBuffer*       bufs;
    uint32_t	        buf_num;
    char*               cdh;
    uint32_t	        cdh_len;
    uint32_t		flag;
    void*		priv_data;
    uint32_t            timeout;
};

/*
 * struct xclReqCompletion - read/write completion
 * keep this in sync with cl_streams_poll_req_completions
 * in core/include/stream.h
 */
struct xclReqCompletion {
    char			resv[64]; /* reserved for meta data */
    void			*priv_data;
    size_t			nbytes;
    int				err_code;
};

/**
 * xclWriteQueue - write data to queue(DEPRECATED)
 * @handle:        Device handle
 * @q_hdl:         Queue handle
 * @wr_req:        Queue request
 * Return:         Number of bytes been written or appropriate error number
 *
 * Move data from host memory to board. The destination is determined
 * by flow id and route id which are provided to
 * xclCreateWriteQueue. it returns number of bytes been moved or error
 * code.  By default, this function returns only when the entire buf
 * has been written, or error. If XCL_QUEUE_REQ_NONBLOCKING flag is
 * used, it returns immediately and xclPollCompletion needs to be used
 * to determine if the data transmission is completed.  If
 * XCL_QUEUE_REQ_EOT flag is used, end of transmit signal will be
 * added at the end of this tranmission.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
ssize_t
xclWriteQueue(xclDeviceHandle handle, uint64_t q_hdl, struct xclQueueRequest *wr_req);

/**
 * xclReadQueue - read data from queue(DEPRECATED)
 * @handle:        Device handle
 * @q_hdl:         Queue handle
 * @rd_req:        read request
 * Return:         Number of bytes been read or appropriate error number
 *
 * Move data from board to host memory. The source is determined by
 * flow id and route id which are provided to xclCreateReadQueue. It
 * returns number of bytes been moved or error code.  This function
 * returns until all the requested bytes is read or error happens. If
 * XCL_QUEUE_REQ_NONBLOCKING flag is used, it returns immediately and
 * xclPollCompletion needs to be used to determine if the data
 * trasmission is completed.  If XCL_QUEUE_REQ_EOT flag is used, data
 * transmission for the current read request completes immediatly once
 * end of transmit signal is received.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
ssize_t
xclReadQueue(xclDeviceHandle handle, uint64_t q_hdl, struct xclQueueRequest *rd_req);

/**
 * xclPollQueue - poll a single read/write queue completion(DEPRECATED)
 * @handle:        Device handle
 * @q_hdl:         Queue handle
 * @min_compl:     Unblock only when receiving min_compl completions
 * @max_compl:     Max number of completion with one poll
 * @comps:         Completed request array
 * @actual_compl:  Number of requests been completed
 * @timeout:       Timeout
 * Return:         Number of events or appropriate error number
 *
 * Poll completion events of non-blocking read/write requests. Once
 * this function returns, an array of completed requests is returned.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclPollQueue(xclDeviceHandle handle, uint64_t q_hdl, int min_compl,
		   int max_compl, struct xclReqCompletion *comps,
		   int* actual_compl, int timeout);

/**
 * xclSetQueueOpt - Set a single read/write queue's option(DEPRECATED)
 * @handle:        Device handle
 * @q_hdl:         Queue handle
 * @type:          option type
 * @val:           option value
 * Return:         Number of events or appropriate error number
 *
 * Set option of a read or write queue.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclSetQueueOpt(xclDeviceHandle handle, uint64_t q_hdl, int type, uint32_t val);


/**
 * xclPollCompletion - poll read/write queue completion(DEPRECATED)
 * @handle:        Device handle
 * @min_compl:     Unblock only when receiving min_compl completions
 * @max_compl:     Max number of completion with one poll
 * @comps:         Completed request array
 * @actual_compl:  Number of requests been completed
 * @timeout:       Timeout
 * Return:         Number of events or appropriate error number
 *
 * Poll completion events of non-blocking read/write requests. Once
 * this function returns, an array of completed requests is returned.
 */
XRT_DEPRECATED
XCL_DRIVER_DLLESPEC
int
xclPollCompletion(xclDeviceHandle handle, int min_compl, int max_compl,
                  struct xclReqCompletion *comps, int* actual_compl, int timeout);

#ifdef __cplusplus
}
#endif

#endif
