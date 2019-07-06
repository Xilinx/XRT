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

#ifndef core_common_bo_cache_h_
#define core_common_bo_cache_h_

#include <stack>
#include <utility>

#include "xrt.h"
#include "ert.h"

namespace xrt_core {
    typedef std::pair<const unsigned, struct ert_start_copybo_cmd * const> cmd_bo;
    class bo_cache {
        xclDeviceHandle device_handle;
        unsigned cmd_bo_cache_max_size;
        std::stack<cmd_bo> cmd_bo_cache;
        std::mutex cmd_bo_cache_lock;

    public:
        bo_cache(xclDeviceHandle handle, unsigned max_size) : device_handle(handle),
                                                              cmd_bo_cache_max_size(max_size) {}
        ~bo_cache() {
            std::lock_guard<std::mutex> lock(cmd_bo_cache_lock);
            while (!cmd_bo_cache.empty()) {
                cmd_bo bo = cmd_bo_cache.top();
                destroy(bo);
                cmd_bo_cache.pop();
            }
        }

        cmd_bo alloc() {
            {
                std::lock_guard<std::mutex> lock(cmd_bo_cache_lock);
                if (!cmd_bo_cache.empty()) {
                    cmd_bo bo = cmd_bo_cache.top();
                    cmd_bo_cache.pop();
                    return bo;
                }
            }
            unsigned execHandle = xclAllocBO(device_handle, sizeof(ert_start_copybo_cmd),
                                             0, XCL_BO_FLAGS_EXECBUF);
            struct ert_start_copybo_cmd *execData =
                reinterpret_cast<struct ert_start_copybo_cmd *>(
                    xclMapBO(device_handle, execHandle, true));
            return std::make_pair(execHandle, execData);
        }
        void release(cmd_bo bo) {
            {
                std::lock_guard<std::mutex> lock(cmd_bo_cache_lock);
                if (cmd_bo_cache.size() < cmd_bo_cache_max_size)
                    cmd_bo_cache.push(bo);
            }
            destroy(bo);
        }

    private:
        void destroy(cmd_bo bo) {
            (void)munmap(bo.second, sizeof(ert_start_copybo_cmd));
            xclFreeBO(device_handle, bo.first);
        }
    };
}

#endif
