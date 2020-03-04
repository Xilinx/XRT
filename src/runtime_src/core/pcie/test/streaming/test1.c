/**
 * Copyright (C) 2018 Xilinx, Inc
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

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <cassert>
#include "../../../include/xclhal2.h"

int main(int argc, char *argv[])
{
	xclDeviceHandle	dev_hdl;
	int	dev_idx=0;
	xclQueueContext q_ctx;
	uint64_t qhdl;
	char log[] = "/tmp/testlog";
	int ret = 0;

	if (argc == 2) {
		dev_idx = atoi(argv[1]);
	}

	xclProbe();
	dev_hdl = xclOpen(dev_idx, log, XCL_QUIET);
	if (dev_hdl < 0) {
		std::cerr << "Open device " << dev_idx << " failed" << std::endl;
		return -1;
	}

	ret = xclCreateWriteQueue(dev_hdl, &q_ctx, &qhdl);
	if (ret < 0) {
		std::cerr << "Create Queue failed ret=" << ret <<std::endl;
		goto failed;
	}
	std::cout << "Created Write Queue: " << qhdl << std::endl;

	ret = xclDestroyQueue(dev_hdl, qhdl);
	if (ret < 0) {
		std::cerr << "Destroy Queue failed ret=" << ret << std::endl;
	}
	std::cout << "Destroyed Write Queue: " << qhdl << std::endl;

failed:
	std::cout << "Close device" << std::endl;
	xclClose(dev_hdl);
	return 0;
}
