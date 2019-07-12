/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lib/xmahw_private.h"

extern XmaHwInterface hw_if;

int xma_hw_probe(XmaHwCfg *hwcfg)
{
    return hw_if.probe(hwcfg);
}

//bool xma_hw_is_compatible(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg)
bool xma_hw_is_compatible(XmaHwCfg *hwcfg, XmaXclbinParameter *devXclbins, int32_t num_parms)
{
    return hw_if.is_compatible(hwcfg, devXclbins, num_parms);
    //return hw_if.is_compatible(hwcfg, systemcfg);
}

//bool xma_hw_configure(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg, bool hw_cfg_status)
bool xma_hw_configure(XmaHwCfg *hwcfg, XmaXclbinParameter *devXclbins, int32_t num_parms)
{
    return hw_if.configure(hwcfg, devXclbins, num_parms);
    //return hw_if.configure(hwcfg, systemcfg, hw_cfg_status);
}
