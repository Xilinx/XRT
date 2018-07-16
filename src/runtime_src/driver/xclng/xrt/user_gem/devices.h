/**
 * Copyright (C) 2018 Xilinx, Inc
 * Author: Lizhi Hou
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

#ifndef _XCL_GLOBAL_DEVICES_H_
#define _XCL_GLOBAL_DEVICES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "../../include/devices.h"

struct xocl_board_info *get_mgmt_devinfo(uint16_t ven, uint16_t dev, uint16_t subsysid);
struct xocl_board_info *get_user_devinfo(uint16_t ven, uint16_t dev, uint16_t subsysid);

#ifdef __cplusplus
}
#endif

#endif
