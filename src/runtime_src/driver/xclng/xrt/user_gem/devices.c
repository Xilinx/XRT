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

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "devices.h"

static struct xocl_board_info  mgmt_devices[] = {
    XOCL_MGMT_PCI_IDS,
    { 0, }
};

static struct xocl_board_info	user_devices[] = {
    XOCL_USER_XDMA_PCI_IDS,
    XOCL_USER_QDMA_PCI_IDS,
    { 0, }
};

struct xocl_board_info *get_mgmt_devinfo(uint16_t ven, uint16_t dev, uint16_t subsysid)
{
    int			i = 0;

    while (mgmt_devices[i].vendor != 0) {
        if (ven == mgmt_devices[i].vendor &&
            dev == mgmt_devices[i].device &&
            (mgmt_devices[i].subdevice == (uint16_t)PCI_ANY_ID ||
            subsysid == mgmt_devices[i].subdevice))
            return &mgmt_devices[i];

        i++;
    }

    return NULL;
}

struct xocl_board_info *get_user_devinfo(uint16_t ven, uint16_t dev, uint16_t subsysid)
{
    int			i = 0;

    while (user_devices[i].vendor != 0) {
        if (ven == user_devices[i].vendor &&
            dev == user_devices[i].device &&
            (user_devices[i].subdevice == (uint16_t)PCI_ANY_ID ||
            subsysid == user_devices[i].subdevice))
            return &user_devices[i];

        i++;
    }

    return NULL;
}
