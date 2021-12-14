/*
 * Copyright (C) 2021 Xilinx Inc - All rights reserved
 * Xilinx Debug & Profile (XDP) APIs
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

#ifndef XDP_PROFILE_DEVICE_IOCTL_ADD_H
#define XDP_PROFILE_DEVICE_IOCTL_ADD_H

#include "xdp/profile/device/add.h"

namespace xdp {

/** Specialization for IOCtlDeadlockDetector Profile IP with support for open+ioctl on device driver based IOCtlDeadlockDetector subdevice
 */

class IOCtlDeadlockDetector : public DeadlockDetector
{
public:
  IOCtlDeadlockDetector(Device*  handle  /* XDP Device Abstraction handle : xrt or HAL device handle */
            ,uint64_t index   /* Index of the IP in Debug_IP_Layout */
            ,debug_ip_data* data = nullptr);

  virtual ~IOCtlDeadlockDetector();

  virtual size_t reset();
  virtual uint32_t getDeadlockStatus(/* need type ?*/);

  virtual int read(uint64_t offset, size_t size, void* data);
  virtual int write(uint64_t offset, size_t size, void* data);

  virtual bool isOpened();

protected:
  int driver_FD = -1;
};

}

#endif
