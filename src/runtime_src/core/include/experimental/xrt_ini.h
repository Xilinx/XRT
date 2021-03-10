/*
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef _XRT_INI_H_
#define _XRT_INI_H_

#include "xrt.h"

#ifdef __cplusplus
# include <string>
#endif

#ifdef __cplusplus

/*!
 * @namespace xrt::ini
 *
 * APIs for XRT configuration control.
 *
 * XRT can be configured through a json xrt.ini file co-located with
 * the host executable.  If present, XRT uses configuration options 
 * from the ini file when a given option is first accessed. Without an
 * ini file, the configuration options take on default values.
 *
 * The APIs in this file allow host application to specify
 * configuration options for XRT programatically.  It is only possible
 * for the host application to change configuration options before a
 * given option is used by XRT the very first time.
 */
namespace xrt { namespace ini {

/*!
 * set() - Change xrt.ini string value for specified key
 *
 * @param key
 *  Key to change value for
 * @param value
 *  New value for key
 *
 * Throws if key value cannot be changed.
 */
XCL_DRIVER_DLLESPEC
void
set(const std::string& key, const std::string& value);

/*!
 * set() - Change xrt.ini string value for specified key
 *
 * @param key
 *  Key to change value for
 * @param value
 *  New value for key
 *
 * Throws if key value cannot be changed.
 */
void
set(const std::string& key, unsigned int value)
{
  set(key, std::to_string(value));
}


}} // ini, xrt

/// @cond
extern "C" {
#endif

/**
 * xrtIniSet() - Change xrt.ini string value for specified key
 * 
 * @key:    Key to change value for
 * @value:  New value for key
 * Return:  0 on success, error if key value cannot be changed
 */
XCL_DRIVER_DLLESPEC
int
xrtIniStringSet(const char* key, const char* value);

/**
 * xrtIniUintSet() - Change xrt.ini unsigned int value for specified key
 * 
 * @key:    Key to change value for
 * @value:  New value for key
 * Return:  0 on success, error if key value cannot be changed
 */
XCL_DRIVER_DLLESPEC
int
xrtIniUintSet(const char* key, unsigned int value);

/// @endcond  
#ifdef __cplusplus
}
#endif

#endif
