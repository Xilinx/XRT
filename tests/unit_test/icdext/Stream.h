#ifndef _XCL_STREAM_HPP_
#define _XCL_STREAM_HPP_

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

#include <CL/cl2.hpp>
#include <CL/cl_ext.h>

#include <iostream>
#include <string>

namespace cl {

    class Stream {
        Device device_;
        cl_stream stream_;
        static decltype(&clCreateStream) openStm_;
        static decltype(&clReleaseStream) closeStm_;
        static decltype(&clReadStream) readStm_;
        static decltype(&clWriteStream) writeStm_;
    public:
        static void init(cl::Platform platform) {
            void *bar = clGetExtensionFunctionAddressForPlatform(platform(), "clCreateStream");
            openStm_ = (decltype(&clCreateStream))bar;
            std::cout << "clCreateStream(0x" << bar << ")\n";
            bar = clGetExtensionFunctionAddressForPlatform(platform(), "clReleaseStream");
            closeStm_ = (decltype(&clReleaseStream))bar;
            std::cout << "clReleaseStream(0x" << bar << ")\n";
            bar = clGetExtensionFunctionAddressForPlatform(platform(), "clReadStream");
            readStm_ = (decltype(&clReadStream))bar;
            std::cout << "clReadStream(0x" << bar << ")\n";
            bar = clGetExtensionFunctionAddressForPlatform(platform(), "clWriteStream");
            writeStm_ = (decltype(&clWriteStream))bar;
            std::cout << "clWriteStream(0x" << bar << ")\n";
        }
        Stream(Device device, cl_stream_flags flags,
	       cl_stream_attributes attr,
	       cl_mem_ext_ptr_t* ext) : device_(device) {
            int res = 0;
            openStm_(device_(), flags, attr, ext, &res);
        }

        ~Stream() {
            closeStm_(stream_);
        }

        int read(void* buf, size_t offset, size_t size,
                 cl_stream_xfer_req* attr) {
            int res = 0;
            readStm_(device_(), stream_, buf, offset, size, attr, &res);
            return res;
        }

        int write(void* buf, size_t offset, size_t size,
                  cl_stream_xfer_req* attr) {
            int res = 0;
            writeStm_(device_(), stream_, buf, offset, size, attr, &res);
            return res;
        }
    };
}

#endif
