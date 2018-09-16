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
#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include "xma.h"
#include "lib/xmares.h"
#include "lib/xmahw.h"
#include "lib/xmahw_private.h"


static inline bool check_xmaapi_is_compatible(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg) {
    return true;
}

/* TODO: JPM include basic yaml config with single kernel xclbin
 * so that hw configure could be executed with respect to populating
 * the XmaHwCfg data structure
*/
static inline bool check_xmaapi_hw_configure(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg, bool hw_cfg_status) {
    return true;
}

int main()
{
    int number_failed = 0;
    int32_t rc;
    extern XmaHwInterface hw_if;

    hw_if.is_compatible = check_xmaapi_is_compatible;
    hw_if.configure = check_xmaapi_hw_configure;

    rc = xma_initialize("../system_cfg/check_cfg.yaml");
    if (rc != 0) {
      number_failed++;
    }

    if (number_failed == 0) {
     printf("XMA check_xmaapi test completed successfully\n");
     return EXIT_SUCCESS;
    } else {
     printf("ERROR: XMA check_xmaapi test failed\n");
     return EXIT_FAILURE;
    }
    //return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
