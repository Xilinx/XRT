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

#include <stdint.h>

#ifndef _XRT_STREAM_H
#define _XRT_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cl_stream_xfer_req.
 * For each read or write request, this extra data needs to be sent.
 */

typedef struct stream_xfer_req {
    stream_xfer_req_type    flags;
    char*                   cdh;
    uint32_t                cdh_len;
    void*		    priv_data;
    uint32_t                timeout; //in ms
    char                    reserved[64];
} stream_xfer_req;

/**
 * struct cl_streams_poll_req_completions
 * For each poll completion provide one of this struct.
 * Keep this in sync with xclReqCompletion in xclhal2.h
 */
typedef struct streams_poll_req_completions {
    char			resv[64]; /* reserved for meta data */
    void			*priv_data;
    size_t			nbytes;
    int				err_code;
} streams_poll_req_completions;

#ifdef __cplusplus
}
#endif

#endif
