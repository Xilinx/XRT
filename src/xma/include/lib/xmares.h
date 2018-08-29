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

/**
 * @file xmares.h
 * @brief header for internal use within libxmaapi.  Functions for obtaining
 * and freeing shared memory resources for managing access to devices and
 * kernels.
*/
#ifndef _XMA_RES_
#define _XMA_RES_

#include "xma.h"
#include "lib/xmacfg.h"

#ifndef XMA_RES_TEST
#define XMA_SHM_FILE "/tmp/xma_shm_db"
#define XMA_SHM_FILE_SIG "/tmp/xma_shm_db_ready"
#else
extern char* XMA_SHM_FILE;
extern char* XMA_SHM_FILE_SIG;
#endif
typedef struct XmaSession XmaSession;

/**
 * @brief opaque object representing data stored in shared memory
*/
typedef void * XmaResources;

/**
 * @brief opaque object representing an encoder kernel
*/
typedef void * XmaKernelRes;


/**
 * @brief create and init a decoder kernel request object
 *
 * @param type Type of decoder plugin
 * @param vendor Vendor string of decoder plugin
 * @param dev_excl Flag to indicate device exclusivity
 *
 * returns 0 on success with session object updated with kernel resource data;
 * error code otherwise.
*/
int32_t xma_res_alloc_dec_kernel(XmaResources shm_cfg, XmaDecoderType type,
                                 const char *vendor, XmaSession *session,
                                 bool dev_excl);

/**
 * @brief create and init a filter kernel request object
 *
 * @param type Type of filter plugin
 * @param vendor Vendor string of filter plugin
 * @param dev_excl Flag to indicate device exclusivity
 *
 * returns 0 on success with session object updated with kernel resource data;
 * error code otherwise.
*/
int32_t xma_res_alloc_filter_kernel(XmaResources shm_cfg, XmaFilterType type,
                                    const char *vendor, XmaSession *session,
                                    bool dev_excl);

/**
 * @brief create and init a generic kernel request object
 *
 * @param type Type of kernel plugin
 * @param vendor Vendor string of kernel plugin
 * @param dev_excl Flag to indicate device exclusivity
 *
 * returns 0 on success with session object updated with kernel resource data;
 * error code otherwise.
*/
int32_t xma_res_alloc_kernel_kernel(XmaResources   shm_cfg,
                                    XmaKernelType  type,
                                    const char    *vendor,
                                    XmaSession    *session,
                                    bool           dev_excl);

/**
 * @brief create and init an encoder kernel request object
 *
 * @param type Type of encoder plugin
 * @param vendor Vendor string of encoder plugin
 * @param dev_excl Flag to indicate device exclusivity
 *
 * returns 0 on success with session object updated with kernel resource data;
 * error code otherwise.
*/
int32_t xma_res_alloc_enc_kernel(XmaResources shm_cfg, XmaEncoderType type,
                                 const char *vendor, XmaSession *session,
                                 bool dev_excl);

/**
 * @brief create and init an scaler kernel request object
 *
 * @param type Type of scaler plugin
 * @param vendor Vendor string of scaler plugin
 * @param dev_excl Flag to indicate device exclusivity
 *
 * returns 0 on success with session object updated with kernel resource data;
 * error code otherwise.
*/
int32_t xma_res_alloc_scal_kernel(XmaResources shm_cfg, XmaScalerType type,
                                  const char *vendor, XmaSession *session,
                                  bool dev_excl);

/**
 * @brief allocates an entire device as a resource
 *
 * @param shm_cfg shared memory pointer
 * @param excl indicates whether device is for process exclusive use
 *
 * @returns device id or -1 on error
*/
int32_t xma_res_alloc_dev(XmaResources shm_cfg, bool excl);

/**
 * @brief allocates next available device starting after dev_handle
 *
 * @param shm_cfg shared memory pointer
 * @param dev_handle device id from which to start search (inclusive)
 * @param excl indicates whether device is for process exclusive use
 *
 * @returns device id or -1 on error
*/
int32_t xma_res_alloc_next_dev(XmaResources shm_cfg, int dev_handle, bool excl);

/**
 * @brief frees an entire device as a resource
 *
 * @param shm_cfg shared memory pointer
 * @param dev_handle Device id of device to free
 *
 * @returns 0 or -1 on error
*/
int32_t xma_res_free_dev(XmaResources shm_cfg, int32_t dev_handle);

/**
 * @brief frees kernel resource
 *
 * @param shm_cfg shared memory pointer
 * @param kern_res opaque kernel resource object
 *
 * @returns 0 or -1 on error
*/
int32_t xma_res_free_kernel(XmaResources shm_cfg,
                            XmaKernelRes kern_res);

/**
 * @brief retrieve the dev handle associated with this resource
 *
 * @param kern_res Previously allocated kernel resource
 *
 * returns device handle id or -1 on error
*/
int32_t xma_res_dev_handle_get(XmaKernelRes *kern_res);

/**
 * @brief retrieve the plugin handle associated with this resource
 *
 * @param kern_res Previously allocated kernel resource
 *
 * returns plugin handle id or -1 on error
*/
int32_t xma_res_plugin_handle_get(XmaKernelRes *kern_res);

/**
 * @brief retrieve the kern handle associated with this resource
 *
 * @param kern_res Previously allocated kernel resource
 *
 * returns kern handle id or -1 on error
*/
int32_t xma_res_kern_handle_get(XmaKernelRes *kern_res);

/**
 * @brief retrieve the session object associated with this resource
 *
 * @param kern_res Previously allocated kernel resource
 *
 * returns session pointer or NULL on error
*/
XmaSession *xma_res_session_get(XmaKernelRes *kern_res);

/**
 * @brief retrieve the channel id associated with this resource
 *
 * @param kern_res Previously allocated kernel resource
 *
 * returns kern handle id or -1 on error
*/
int32_t xma_res_kern_chan_id_get(XmaKernelRes *kern_res);

/**
 * @brief obtain pointer to shared memory resource management database
 *
 * @param config Reference to parsed YAML configuration file
 * @return Reference to shared memory or NULL if an error occured
*/
XmaResources xma_res_shm_map(struct XmaSystemCfg *config);

/**
 * @brief unmaps shared memory and reduces ref count
 *
 * Processes should call close to release the shared memory lest the
 * shared memory object persist when no process is left to use it
 * @param shm_cfg shared memory pointer
 * @returns none
*/
void xma_res_shm_unmap(XmaResources shm_cfg);


/**
 * @brief Mark shared memory and hwconfig steps as complete to other processes
 *
 * The process that is able to proceed and update the devices with new
 * xclbin images should call this routine after completion.  Other processes
 * might be waiting until the process is complete and will be polling
 * for the existence of a file that will only be created when this routine is
 * called.
 * @param shm_cfg shared memory pointer
 * @returns none
*/
void xma_res_mark_xma_ready(XmaResources shm_cfg);


/**
 * @brief Before attempting hw config, verify that hw init hasn't already
 * completed
 *
 * @returns true if init is already complete, false otherwise
*/
bool xma_res_xma_init_completed(void);
#endif
