/*
 * Copyright 2020 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __XRT_UTILS_H__
#define __XRT_UTILS_H__

/* Update of this file by the user is not encouraged */
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>


//#include "xclhal2.h"
#include <uuid/uuid.h>
#include "xrt.h"
#include "ert.h"
#define __COUNT                 (1024)
#define DATA_SIZE               (__COUNT * sizeof(int))


typedef struct _xrt_buffer {
  unsigned int bo;
  void* user_ptr;
  uint64_t phy_addr;
  unsigned int size;
} xrt_buffer;


int alloc_xrt_buffer (xclDeviceHandle handle, unsigned int size, enum xclBOKind bo_kind, unsigned flags, xrt_buffer *buffer);
void free_xrt_buffer (xclDeviceHandle handle, xrt_buffer *buffer);
int download_xclbin ( const char *bit, unsigned deviceIndex, int* cu_index, xclDeviceHandle *handle, uuid_t *xclbinId);
int send_softkernel_command (xclDeviceHandle handle, xrt_buffer *sk_buf, unsigned int *payload, unsigned int num_idx, unsigned int cu_mask, int timeout);


#endif
