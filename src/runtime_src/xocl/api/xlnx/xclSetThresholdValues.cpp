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

// Copyright 2019 Xilinx, Inc. All rights reserved.

#include "xocl/core/device.h"

namespace xocl {

static cl_int
clSetThresholdValues(cl_device_id device,
                uint16_t power,
                uint16_t temperature)
{
        int ret;
        auto xdevice  = xocl(device);

        ret = xdevice->get_xrt_device()->setThresholdValues(power, temperature);
        if (!ret)
                return CL_SUCCESS;
        return ret;
}

} // Namespace xocl END

namespace xlnx {

cl_int
clSetThresholdValues(cl_device_id device,
                uint16_t power,
                uint16_t temperature)
{
        try {
                return xocl::clSetThresholdValues(device, power, temperature);
        }
        catch (const xrt::error& ex) {
                xocl::send_exception_message(ex.what());
                return ex.get_code();
        }
        catch (const std::exception& ex) {
                xocl::send_exception_message(ex.what());
                return CL_INVALID_OPERATION;
        }
}

} // xlnx


cl_int
xclSetThresholdValues(cl_device_id device,
                uint16_t power,
                uint16_t temperature)
{
        return xlnx::clSetThresholdValues(device, power, temperature);
}
