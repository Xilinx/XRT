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
#include <stdlib.h>

#include <memory.h>
//#include <strings.h>
#include <string>
#include "xma.h"
#include "lib/xmares.h"
#include "lib/xmahw.h"
#include "lib/xmahw_private.h"
#include "lib/xmacfg.h"

int ck_assert_int_eq(int rc1, int rc2) {
  if (rc1 != rc2) {
    return -1;
  } else {
    return 0;
  }
}

int ck_assert_str_eq(const char* str1, const char* str2) {
  if (std::string(str1) != std::string(str2)) {
    return -1;
  } else {
    return 0;
  }
}

int ck_assert(bool result) {
  if (!result) {
    return -1;
  } else {
    return 0;
  }
}

int test_simple_config()
{
    int rc;

    XmaSystemCfg systemcfg;
    memset(&systemcfg, 0x0, sizeof(systemcfg));

    rc = xma_cfg_parse((char*)"../system_cfg/simple_cfg.yaml", &systemcfg);

    rc |= ck_assert_int_eq(rc, 0);
    rc |= ck_assert_str_eq(systemcfg.pluginpath, "/plugin/path");
    rc |= ck_assert_str_eq(systemcfg.xclbinpath, "/xcl/path");
    rc |= ck_assert_int_eq(systemcfg.num_images, 1);
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].xclbin, "filename1.xclbin");

    rc |= ck_assert(systemcfg.imagecfg[0].zerocopy == false);

    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].num_devices, 3);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].device_id_map[0], 1);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].device_id_map[1], 2);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].device_id_map[2], 3);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].num_kernelcfg_entries, 1);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].instances, 6);
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[0].function, "encoder");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[0].plugin, 
                     "libtstencoderplg.so");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[0].vendor, 
                     "Xilinx");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[0].name, 
                     "virtual_encoder");
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].ddr_map[0], 0);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].ddr_map[1], 1);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].ddr_map[2], 2);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].ddr_map[3], 3);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].ddr_map[4], 0);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].ddr_map[5], 1);
    return rc;

}

int test_complex_config()
{
    int rc;

    XmaSystemCfg systemcfg;
    memset(&systemcfg, 0x0, sizeof(systemcfg));

    rc = xma_cfg_parse((char*)"../system_cfg/complex_cfg.yaml", &systemcfg);
    ck_assert_int_eq(rc, 0);

    /* System Config */
    rc |= ck_assert_str_eq(systemcfg.pluginpath, "/plugin/path");
    rc |= ck_assert_str_eq(systemcfg.xclbinpath, "/xcl/path");
    rc |= ck_assert_int_eq(systemcfg.num_images, 2);

    /* Image Config 0 */
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].xclbin, "filename1.xclbin");
    rc |= ck_assert(systemcfg.imagecfg[0].zerocopy == true);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].num_devices, 2);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].device_id_map[0], 0);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].device_id_map[1], 1);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].num_kernelcfg_entries, 2);

    /* Kernel Config 0 */
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].instances, 2);
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[0].function, "scaler");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[0].plugin, 
                     "libtstscalerplg.so");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[0].vendor, 
                     "Xilinx");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[0].name, 
                     "virtual_scaler");
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].ddr_map[0], 0);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[0].ddr_map[1], 0);

    /* Kernel Config 1 */
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[1].instances, 1);
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[1].function, "scaler");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[1].plugin, 
                     "libtstscalerplg.so");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[1].vendor, 
                     "Xilinx");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[0].kernelcfg[1].name, 
                     "virtual_scaler");
    rc |= ck_assert_int_eq(systemcfg.imagecfg[0].kernelcfg[1].ddr_map[0], 0);

    /* Image Config 1 */
    rc |= ck_assert_str_eq(systemcfg.imagecfg[1].xclbin, "filename2.xclbin");
    rc |= ck_assert(systemcfg.imagecfg[0].zerocopy == true);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[1].num_devices, 1);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[1].device_id_map[0], 2);
    rc |= ck_assert_int_eq(systemcfg.imagecfg[1].num_kernelcfg_entries, 1);

    /* Kernel Config 0 */
    rc |= ck_assert_int_eq(systemcfg.imagecfg[1].kernelcfg[0].instances, 1);
    rc |= ck_assert_str_eq(systemcfg.imagecfg[1].kernelcfg[0].function, "encoder");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[1].kernelcfg[0].plugin, 
                     "libtstencoderplg.so");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[1].kernelcfg[0].vendor, 
                     "Xilinx");
    rc |= ck_assert_str_eq(systemcfg.imagecfg[1].kernelcfg[0].name, 
                     "virtual_encoder");
    rc |= ck_assert_int_eq(systemcfg.imagecfg[1].kernelcfg[0].ddr_map[0], 0);
    return rc;
}

int test_error1_config()
{
    int rc;

    XmaSystemCfg systemcfg;
    memset(&systemcfg, 0x0, sizeof(systemcfg));

    rc = xma_cfg_parse((char*)"../system_cfg/error1_cfg.yaml", &systemcfg);

    rc = ck_assert_int_eq(rc, -1);
    return rc;
}

int test_error2_config()
{
    int rc;

    XmaSystemCfg systemcfg;
    memset(&systemcfg, 0x0, sizeof(systemcfg));

    rc = xma_cfg_parse((char*)"../system_cfg/error2_cfg.yaml", &systemcfg);

    rc = ck_assert_int_eq(rc, -1);
    return rc;
}

int test_error3_config()
{
    int rc;

    XmaSystemCfg systemcfg;
    memset(&systemcfg, 0x0, sizeof(systemcfg));

    rc = xma_cfg_parse((char*)"../system_cfg/error3_cfg.yaml", &systemcfg);

    rc = ck_assert_int_eq(rc, -1);
    return rc;
}

static inline int32_t check_xmaapi_probe(XmaHwCfg *hwcfg) {
    return 0;
}

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
    hw_if.probe = check_xmaapi_probe;

    rc = test_simple_config();
    if (rc != 0) {
      number_failed++;
    }

    rc = test_complex_config();
    if (rc != 0) {
      number_failed++;
    }

    rc = test_error1_config();
    if (rc != 0) {
      number_failed++;
    }

    rc = test_error2_config();
    if (rc != 0) {
      number_failed++;
    }

    rc = test_error3_config();
    if (rc != 0) {
      number_failed++;
    }


    if (number_failed == 0) {
     printf("XMA check_xmacfg test completed successfully\n");
     return EXIT_SUCCESS;
    } else {
     printf("ERROR: XMA check_xmacfg test failed\n");
     return EXIT_FAILURE;
    }

    //return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
