/*
 * Copyright (C) 2015-2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel HAL userspace driver APIs
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

#include <CL/cl_ext.h>

#ifndef __CL_EXT_XILINX_STREAM_H
#define __CL_EXT_XILINX_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cl_stream_flags. Type of the stream , eg set to CL_STREAM_READ_ONLY for
 * read only. Used in clCreateStream()
 */
typedef cl_bitfield         cl_stream_flags;
#define CL_STREAM_READ_ONLY			    (1 << 0)
#define CL_STREAM_WRITE_ONLY                        (1 << 1)

/**
 * cl_stream_attributes. eg set it to CL_STREAM for stream mode. Used
 * in clCreateStream()
 */
typedef cl_uint             cl_stream_attributes;
#define CL_STREAM                                   (1 << 0)
#define CL_PACKET                                   (1 << 1)

/**
 * cl_stream_attributes.
 * eg set it to CL_STREAM_CDH for Customer Defined Header.
 * Used in clReadStream() and clWriteStream()
 */
typedef cl_uint             cl_stream_xfer_req_type;
#define CL_STREAM_EOT                               (1 << 0)
#define CL_STREAM_CDH                               (1 << 1)
#define CL_STREAM_NONBLOCKING                       (1 << 2)
#define CL_STREAM_SILENT                            (1 << 3)

/**
 * cl_stream_xfer_req.
 * For each read or write request, this extra data needs to be sent.
 */

typedef struct cl_stream_xfer_req {
    cl_stream_xfer_req_type flags;
    char*                   cdh;
    cl_uint                 cdh_len;
    void*		    priv_data;
    cl_uint                 timeout; //in ms
    char                    reserved[64];
} cl_stream_xfer_req;

/**
 * struct cl_streams_poll_req_completions 
 * For each poll completion provide one of this struct.
 * Keep this in sync with xclReqCompletion in xclhal2.h
 */
typedef struct cl_streams_poll_req_completions {
    char			resv[64]; /* reserved for meta data */
    void			*priv_data;
    size_t			nbytes;
    int				err_code;
} cl_streams_poll_req_completions;

typedef struct _cl_stream *      cl_stream;
typedef struct _cl_stream_mem *  cl_stream_mem;

#ifdef __cplusplus
}
#endif

#endif
