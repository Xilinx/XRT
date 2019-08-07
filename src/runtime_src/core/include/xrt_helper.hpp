/*
 * Copyright (C) 2019, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#ifndef __XRT_HELPER_H__
#define __XRT_HELPER_H__
#include "xrt.h"
#include <vector>

namespace xrt_util {

class helper {
    static helper* mInstance;
	std::vector<xclDeviceHandle> mHandleList;
	helper() {
		xclDeviceInfo2 deviceInfo;
		int ndevice = xclProbe();;
    		if(ndevice  == 0) {
			throw std::runtime_error("No device found");
		}

		for(int i=0;i<ndevice; i++) {
			xclDeviceHandle handle;
			handle = xclOpen(i, NULL, XCL_INFO);
			mHandleList.push_back(handle);
		}
	}	
public:
    static helper* getinstance(unsigned deviceIndex=0) {
		if(mInstance == nullptr) {
			mInstance = new helper();
		}
		return mInstance;
	}

    void* allocate(int _num,unsigned deviceIndex=0) {
		return xclAllocHostPtr(mHandleList[deviceIndex], _num,XCL_BO_FLAGS_CACHEABLE);	
	}

   	void deallocate(void* _ptr,unsigned deviceIndex=0) {
		xclFreeHostPtr(mHandleList[deviceIndex],_ptr);		
	}

	~helper() {
		for (auto it : mHandleList) {
			xclClose(it);
		}
	}

};

helper* helper::mInstance = nullptr;
}
#endif
