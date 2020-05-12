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

#include "app/xmalogger.h"


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






XmaSingleton *g_xma_singleton;

int test_logger_init()
{
    int rc1;
    int i;
    int rc = 0;

    g_xma_singleton = (XmaSingleton*)malloc(sizeof(XmaSingleton)); 
    memset(g_xma_singleton, 0x0, sizeof(XmaSingleton));
    XmaLogger *logger = &g_xma_singleton->logger;

    logger->use_stdout = false;
    logger->use_fileout = true;
    strcpy(logger->filename, "./logger_test.log");
    logger->log_level = XMA_DEBUG_LOG;  

    rc1 = xma_logger_init(logger);
    rc |= ck_assert_int_eq(rc1, 0);

    for (i = 0; i < 2048; i++)
        xma_logmsg(XMA_DEBUG_LOG, "check_xmalogger", "This is my message %d\n", i);

    rc1 = xma_logger_close(logger);
    rc |= ck_assert_int_eq(rc1, 0);

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


    rc = test_logger_init();
    if (rc != 0) {
      number_failed++;
    }

    
   if (number_failed == 0) {
     printf("XMA check_xmalogger test completed successfully\n");
     return EXIT_SUCCESS;
    } else {
     printf("ERROR: XMA check_xmalogger test failed\n");
     return EXIT_FAILURE;
    }
    //return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
