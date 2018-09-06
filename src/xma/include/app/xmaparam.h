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
#ifndef _XMAAPP_PARAM_H_
#define _XMAAPP_PARAM_H_

/**
 * @ingroup xma_app_intf
 * @file app/xmaparam.h
 * Generalized TLV parameters to support custom kernel properties or arguments
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @ingroup xma
 *  @addtogroup xmadec xmadecoder.h
 *  @{
*/

/**
 * @typedef XmaDataType
 * Type of data represented by XmaParameter::value
 *
 * @typedef XmaParameter
 * Type-Length-Value data structure used for passing custom arguments to
 * XmaKernel type plugin and/or for custom properties for any given kernel
*/

/**
 * @enum XmaDataType
 * Type of data represented by XmaParameter::value
*/
typedef enum {
    XMA_STRING = 1, /**< 1 */
    XMA_INT32,      /**< 2 */
    XMA_UINT32,     /**< 3 */
    XMA_INT64,      /**< 4 */
    XMA_UINT64      /**< 5 */
} XmaDataType;

/**
 * @struct XmaParameter
 * Type-Length-Value data structure used for passing custom arguments to
 * XmaKernel type plugin and/or for custom properties for any given kernel
*/
typedef struct XmaParameter {
    char        *name; /**< name of parameter*/
    /** integer id of parameter; for use as a customer-specific id as needed */
    int32_t      user_type;
    XmaDataType  type; /**< data type of data */
    size_t       length; /**< size of data in value */
    void        *value; /**< pointer to buffer holding data */
} XmaParameter;
/** @} */

#ifdef __cplusplus
}
#endif
#endif
