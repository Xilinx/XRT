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
#ifndef _XMA_CFG_H_
#define _XMA_CFG_H_

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include "lib/xmalimits.h"

#if !defined (PATH_MAX) || !defined (NAME_MAX)
#include <linux/limits.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @file
 */
#define XMA_CFG_FUNC_NM_DEC "decoder"
#define XMA_CFG_FUNC_NM_ENC "encoder"
#define XMA_CFG_FUNC_NM_SCALE "scaler"
#define XMA_CFG_FUNC_NM_FILTER "filter"
#define XMA_CFG_FUNC_NM_KERNEL "kernel"

/**
 * @addtogroup xmacfg
 * @{
 */
typedef struct XmaKernelCfg
{
    int32_t  instances;
    char     function[MAX_FUNCTION_NAME];
    char     plugin[MAX_PLUGIN_NAME];
    char     vendor[MAX_VENDOR_NAME];
    char     name[MAX_KERNEL_NAME];
    int32_t  ddr_map[MAX_DDR_MAP];
} XmaKernelCfg;

typedef struct XmaImageCfg
{
    char         xclbin[NAME_MAX];
    bool         zerocopy;
    int32_t      num_devices;
    int32_t      device_id_map[MAX_XILINX_DEVICES];
    int32_t      num_kernelcfg_entries;
    XmaKernelCfg kernelcfg[MAX_KERNEL_CONFIGS];
} XmaImageCfg;

typedef struct XmaSystemCfg
{
    char        dsa[MAX_DSA_NAME];
    bool        logger_initialized;
    char        logfile[PATH_MAX];
    int32_t     loglevel;
    char        pluginpath[PATH_MAX];
    char        xclbinpath[PATH_MAX];
    int32_t     num_images;
    XmaImageCfg imagecfg[MAX_IMAGE_CONFIGS];
} XmaSystemCfg;

/**
 *  @brief Parse the XMA configuration file
 *
 *  This function parses the XMA System configuration file
 *  containing information such as the plugin and xclbin paths
 *  image configuration for the devices in the system as well
 *  as kernel configuration information.  The results of the
 *  parsing are populated in the XmaSysteCfg data structure.
 *
 *  This function should be called once during application
 *  initialization and is typically called by the @ref xma_init()
 *  function.
 *
 *  @param file      Pointer to a C-style string containing the name of
 *                   the XMA System configuration file
 *  @param systemcfg Pointer to an XmaSystemCfg structure that will
 *                   hold the results of the parsing upon success.
 *                   On failure, the contents of this structure
 *                   are undefined.
 *
 *  @return          0 on success
 *                  -1 on failure
 */
int xma_cfg_parse(char *file, XmaSystemCfg *systemcfg);

/**
 * @brief Obtain count of images from parsed system cfg file
 *
 * This accessor function will provide a total count of image instances
 * from within the parsed YAML configuration file.  Used internally
 * for various looping constructs that must know this and accesses
 * this from the global singleton object.
 *
 * @returns number of images instances or -1 upon failure
*/
int32_t xma_cfg_img_cnt_get(void);

/**
 * @brief Obtain count of devices from parsed system cfg file
 *
 * This accessor function will provide a total count of device instances
 * from within the parsed YAML configuration file.  Used internally
 * for various looping constructs that must know this and accesses
 * this from the global singleton object.
 *
 * @returns number of devices instances or -1 upon failure
*/
int32_t xma_cfg_dev_cnt_get(void);

/**
 * @brief Obtain device ids present within system cfg file
 *
 * This accessor function will provide a list of device ids.
 *
 * @param [output] dev_ids Caller allocated array of size MAX_XILINX_DEVICES
 *
 * @returns array populated with device ids present in cfg file
*/
void xma_cfg_dev_ids_get(uint32_t dev_ids[]);
/**
 *  @}
 */

#ifdef __cplusplus
}
#endif

#endif
