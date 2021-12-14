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

#ifndef _WIN32

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <chrono>

#include "ioctl_add.h"
#include "core/pcie/driver/linux/include/profile_ioctl.h"

namespace xdp {

IOCtlDeadlockDetector::IOCtlDeadlockDetector(Device* handle, uint64_t index, debug_ip_data* data)
                    : DeadlockDetector(handle, index, data)
{
  // Open DeadlockDetector Device Driver File
  std::string subDev("accel_deadlock");
  std::string driverFileName = getDevice()->getSubDevicePath(subDev, 0 /* a design can have atmost 1 DeadlockDetector*/);

  driver_FD = open(driverFileName.c_str(), O_RDWR);
  uint32_t tries = 0;
  const unsigned int maxTries = 5;
  while (driver_FD == -1 && tries < maxTries) {
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    driver_FD = open(driverFileName.c_str(), O_RDWR);
    tries++;
  }

  if (driver_FD == -1) {
    showWarning("Could not open device file.");
    return;
  }
}

IOCtlDeadlockDetector::~IOCtlDeadlockDetector()
{
  close(driver_FD);
}

bool IOCtlDeadlockDetector::isOpened()
{
  if (driver_FD == -1)
    return false;
  return true;
}

size_t IOCtlDeadlockDetector::reset()
{
  return 0;
}

uint32_t IOCtlDeadlockDetector::getDeadlockStatus()
{
  if (!isOpened())
    return 0;
 
  if (out_stream)
    (*out_stream) << " IOCtlDeadlockDetector::getDeadlockStatus " << std::endl;

  uint32_t status = 0;
  ioctl(driver_FD, ACCEL_DEADLOCK_DETECTOR_IOC_GET_STATUS, &status);

  return status;
} 

int IOCtlDeadlockDetector::read(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

int IOCtlDeadlockDetector::write(uint64_t /*offset*/, size_t size, void* /*data*/)
{
  // do nothing
  return size;
}

}
#endif
