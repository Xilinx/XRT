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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include <memory.h>
//#include <strings.h
#include <string>
#include <iostream>
#include "xma.h"
#include "xma_test_plg.h"
#include "lib/xmahw.h"
#include "lib/xmahw_private.h"
#include "lib/xmaapi.h"
#include "lib/xmares.h"



int ck_assert_int_lt(int rc1, int rc2) {
  if (rc1 >= rc2) {
    return -1;
  } else {
    return 0;
  }
}

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


static XmaHwHAL hw_hal;
static XmaHwCfg hw_cfg;

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

static void tst_setup(void);
static void tst_teardown_check(void);

int test_scaler_session_create()
{
    extern XmaSingleton *g_xma_singleton;
    XmaScalerProperties scaler_props;
    XmaScalerSession *sess;
    
    g_xma_singleton->hwcfg = hw_cfg;

    memset(&scaler_props, 0, sizeof(XmaScalerProperties));
    scaler_props.num_outputs = 1;
    scaler_props.max_dest_cnt = 1;
    scaler_props.hwscaler_type = XMA_POLYPHASE_SCALER_TYPE;
    strncpy(scaler_props.hwvendor_string, "Xilinx", (MAX_VENDOR_NAME - 1));

    sess = xma_scaler_session_create(&scaler_props);
    return ck_assert(sess != NULL);
}

int neg_test_scaler_session_create()
{
    XmaScalerSession *sess1, *sess2, *sess3, *sess4, *sess5;
    extern XmaSingleton *g_xma_singleton;
    XmaScalerProperties scaler_props;
    int rc = 0;
    
    g_xma_singleton->hwcfg = hw_cfg;

    memset(&scaler_props, 0, sizeof(XmaScalerProperties));
    scaler_props.num_outputs = 1;
    scaler_props.max_dest_cnt = 1;
    scaler_props.hwscaler_type = XMA_POLYPHASE_SCALER_TYPE;
    strncpy(scaler_props.hwvendor_string, "Xilinx", (MAX_VENDOR_NAME - 1));

    sess1 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess1 != NULL);
    sess2 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess2 != NULL);
    sess3 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess3 != NULL);
    sess4 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess4 != NULL);

    sess5 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess5 == NULL);
    
    return rc;
}

int test_scaler_session_create_destroy_create()
{
    extern XmaSingleton *g_xma_singleton;
    XmaScalerProperties scaler_props;
    XmaScalerSession *sess1, *sess2, *sess3, *sess4, *sess5;
    int32_t rc1;
    int rc = 0;
    
    g_xma_singleton->hwcfg = hw_cfg;

    memset(&scaler_props, 0, sizeof(XmaScalerProperties));
    scaler_props.num_outputs = 1;
    scaler_props.max_dest_cnt = 1;
    scaler_props.hwscaler_type = XMA_POLYPHASE_SCALER_TYPE;
    strncpy(scaler_props.hwvendor_string, "Xilinx", (MAX_VENDOR_NAME - 1));

    sess1 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess1 != NULL);
    sess2 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess2 != NULL);
    sess3 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess3 != NULL);
    sess4 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess4 != NULL);

    sess5 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess5 == NULL);

    rc1 = xma_scaler_session_destroy(sess4);
    rc |= ck_assert_int_eq(rc1, 0);

    sess5 = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess5 != NULL);
    
    return rc;
}

int test_scaler_session_send()
{
    extern XmaSingleton *g_xma_singleton;
    XmaScalerProperties scaler_props;
    XmaScalerSession *sess;
    XmaFrame *dummy = (XmaFrame*)malloc(sizeof(XmaFrame));
    //int32_t data_used = 0;
    int32_t rc1;
    int rc = 0;
    
    g_xma_singleton->hwcfg = hw_cfg;

    memset(&scaler_props, 0, sizeof(XmaScalerProperties));
    scaler_props.num_outputs = 1;
    scaler_props.max_dest_cnt = 1;
    scaler_props.hwscaler_type = XMA_POLYPHASE_SCALER_TYPE;
    strncpy(scaler_props.hwvendor_string, "Xilinx", (MAX_VENDOR_NAME - 1));

    sess = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess != NULL);

    rc1 = xma_scaler_session_send_frame(sess, dummy);
    
    rc |= ck_assert(rc1 & XMA_PLG_SCAL);
    rc |= ck_assert(rc1 & XMA_PLG_SEND);
    
    return rc;
}

int test_scaler_session_recv()
{
    extern XmaSingleton *g_xma_singleton;
    XmaScalerProperties scaler_props;
    XmaScalerSession *sess;
    XmaFrame **dummy = (XmaFrame**)malloc(sizeof(XmaFrame*));
    int32_t rc1;
    int rc = 0;
    
    g_xma_singleton->hwcfg = hw_cfg;

    memset(&scaler_props, 0, sizeof(XmaScalerProperties));
    scaler_props.num_outputs = 1;
    scaler_props.max_dest_cnt = 1;
    scaler_props.hwscaler_type = XMA_POLYPHASE_SCALER_TYPE;
    strncpy(scaler_props.hwvendor_string, "Xilinx", (MAX_VENDOR_NAME - 1));

    sess = xma_scaler_session_create(&scaler_props);
    rc |= ck_assert(sess != NULL);

    rc1 = xma_scaler_session_recv_frame_list(sess, dummy);
    
    rc |= ck_assert(rc1 & XMA_PLG_SCAL);
    rc |= ck_assert(rc1 & XMA_PLG_RECV);
    
    return rc;
}


int main()
{
    int number_failed = 0;
    int32_t rc;
    extern XmaHwInterface hw_if;

    hw_if.is_compatible = check_xmaapi_is_compatible;
    hw_if.configure = check_xmaapi_hw_configure;
    hw_if.probe = check_xmaapi_probe;

    std::string kernel_name("bogus name");
    hw_hal.dev_handle = (void*)"bogus 0";
    kernel_name.copy(hw_hal.kernels[0].name, 20);
    hw_hal.kernels[0].base_address = 0x7000000000000000;
    hw_hal.kernels[0].ddr_bank = 0;
    kernel_name.copy(hw_hal.kernels[1].name, 20);
    hw_hal.kernels[1].base_address = 0x8000000000000000;
    hw_hal.kernels[1].ddr_bank = 0;

	
    hw_cfg.num_devices = 10;
    hw_cfg.devices[0].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[0].in_use = false;
    hw_cfg.devices[1].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[1].in_use = false;
    hw_cfg.devices[2].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[2].in_use = false;
    hw_cfg.devices[3].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[3].in_use = false;
    hw_cfg.devices[4].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[4].in_use = false;
    hw_cfg.devices[5].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[5].in_use = false;
    hw_cfg.devices[6].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[6].in_use = false;
    hw_cfg.devices[7].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[7].in_use = false;
    hw_cfg.devices[8].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[8].in_use = false;
    hw_cfg.devices[9].handle = (XmaHwDevice *)&hw_hal;
    hw_cfg.devices[9].in_use = false;

		
    tst_setup();
    rc = test_scaler_session_create();
    if (rc != 0) {
      number_failed++;
    }
    tst_teardown_check();
    tst_setup();
    rc = neg_test_scaler_session_create();
    if (rc != 0) {
      number_failed++;
    }
    tst_teardown_check();
    tst_setup();
    rc = test_scaler_session_create_destroy_create();
    if (rc != 0) {
      number_failed++;
    }
    tst_teardown_check();
    tst_setup();
    rc = test_scaler_session_send();
    if (rc != 0) {
      number_failed++;
    }
    tst_teardown_check();
    tst_setup();
    rc = test_scaler_session_recv();
    if (rc != 0) {
      number_failed++;
    }
    tst_teardown_check();


   if (number_failed == 0) {
     printf("XMA check_xmascaler test completed successfully\n");
     return EXIT_SUCCESS;
    } else {
     printf("ERROR: XMA check_xmascaler test failed\n");
     return EXIT_FAILURE;
    }
    //return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void tst_setup(void)
{
    extern XmaSingleton *g_xma_singleton;
    char *cfgfile = (char*) "../system_cfg/check_cfg.yaml";
    struct stat stat_buf;
    int rc;

    g_xma_singleton = (XmaSingleton*)malloc(sizeof(*g_xma_singleton));
    memset(g_xma_singleton, 0, sizeof(*g_xma_singleton));
    ck_assert(g_xma_singleton != NULL);

    rc = xma_cfg_parse(cfgfile, &g_xma_singleton->systemcfg);
    ck_assert_int_eq(rc, 0);

    rc = xma_logger_init(&g_xma_singleton->logger);
    ck_assert_int_eq(rc, 0);

    /* heuristic check to determine proper parsing of cfg */
    rc = strcmp(g_xma_singleton->systemcfg.dsa, "xilinx_vcu1525_dynamic_5_0");
    ck_assert_int_eq(rc, 0);

    /* Ensure no prior test file system pollution remains */
    unlink(XMA_SHM_FILE);
    unlink(XMA_SHM_FILE_SIG);

    g_xma_singleton->shm_res_cfg = xma_res_shm_map(&g_xma_singleton->systemcfg);
    ck_assert(g_xma_singleton->shm_res_cfg != NULL);
    xma_res_mark_xma_ready(g_xma_singleton->shm_res_cfg);

    rc = stat(XMA_SHM_FILE, &stat_buf);
    ck_assert_int_eq(rc, 0);

    rc = stat(XMA_SHM_FILE_SIG, &stat_buf);
    ck_assert_int_eq(rc, 0);

    rc = xma_enc_plugins_load(&g_xma_singleton->systemcfg,
                              g_xma_singleton->encodercfg);
    ck_assert_int_eq(rc, 0);

    rc = xma_scaler_plugins_load(&g_xma_singleton->systemcfg,
                                 g_xma_singleton->scalercfg);
    ck_assert_int_eq(rc, 0);
    rc = xma_dec_plugins_load(&g_xma_singleton->systemcfg,
                              g_xma_singleton->decodercfg);
    ck_assert_int_eq(rc, 0);

    rc = xma_filter_plugins_load(&g_xma_singleton->systemcfg,
                                 g_xma_singleton->filtercfg);
    ck_assert_int_eq(rc, 0);

    rc = xma_kernel_plugins_load(&g_xma_singleton->systemcfg,
                                 g_xma_singleton->kernelcfg);
    ck_assert_int_eq(rc, 0);

    return;
}

static void tst_teardown_check(void)
{
    extern XmaSingleton *g_xma_singleton;
    struct stat stat_buf;
    int rc;

    if (g_xma_singleton && g_xma_singleton->shm_res_cfg)
        xma_res_shm_unmap(g_xma_singleton->shm_res_cfg);

    rc = stat(XMA_SHM_FILE, &stat_buf);
    ck_assert_int_lt(rc, 0);
    rc = stat(XMA_SHM_FILE_SIG, &stat_buf);
    ck_assert_int_lt(rc, 0);

    if (g_xma_singleton)
        free(g_xma_singleton);

    return;
}
