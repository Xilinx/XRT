/**
 * Copyright (C) 2019 Xilinx, Inc
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

#include "shim.h"

namespace { // private implementation details

}

namespace xocl {  // shared implementation

}

// Basic
unsigned int
xclProbe()
{
  
}

xclDeviceHandle
xclOpen(unsigned int deviceIndex, const char *logFileName, xclVerbosityLevel level)
{
  return nullptr;
}

void
xclClose(xclDeviceHandle handle)
{
}


// XRT Buffer Management APIs
unsigned int
xclAllocBO(xclDeviceHandle handle, size_t size, enum xclBOKind domain, unsigned int flags)
{
  return 0;
}

unsigned int
xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned int flags)
{
  return 0;
}

void*
xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
  return nullptr;
}

void
xclFreeBO(xclDeviceHandle handle, unsigned int boHandle)
{
}

int
xclSyncBO(xclDeviceHandle handle, unsigned int boHandle, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  return 0;
}

// Compute Unit Execution Management APIs
int
xclOpenContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned int ipIndex,bool shared)
{
  return 0;
}

int xclCloseContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned int ipIndex)
{
  return 0;
}

int
xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
  return 0;
}

int
xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  return 0;
}
  
