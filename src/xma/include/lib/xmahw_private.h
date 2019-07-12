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
#ifndef _XMA_HW_PRIVATE_H_
#define _XMA_HW_PRIVATE_H_

#include "lib/xmahw_lib.h"
//#include "lib/xmacfg.h"
#include "app/xmaparam.h"

/*Sarab: Remove yaml system cfg stuff
typedef struct XmaHwInterface
{
    int32_t (*probe)(XmaHwCfg *hwcfg);
    bool    (*is_compatible)(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg);
    bool    (*configure)(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg,
                         bool hw_cfg_status);
} XmaHwInterface;
*/
typedef struct XmaHwInterface
{
    int32_t (*probe)(XmaHwCfg *hwcfg);
    bool    (*is_compatible)(XmaHwCfg *hwcfg, XmaXclbinParameter *devXclbins, int32_t num_parms);
    bool    (*configure)(XmaHwCfg *hwcfg, XmaXclbinParameter *devXclbins, int32_t num_parms);
} XmaHwInterface;

#endif
