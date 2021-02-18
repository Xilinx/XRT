/*
 *  Copyright (C) 2021, Xilinx Inc
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage:
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.  This program is
 *  distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 *  License for more details.  You should have received a copy of the
 *  GNU General Public License along with this program; if not, write
 *  to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA
 *
 */

#ifndef _SHIM_INT_H_
#define _SHIM_INT_H_


/* This file defines internal shim APIs, which is not end user visible.
 * You cannot include this file without include xrt.h.
 * This header file should not be published to xrt release include/ folder.
 */
#ifdef _XCL_XRT_CORE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * xclOpenByBDF() - Open a device and obtain its handle by PCI BDF
 *
 * @bdf:           Deice PCE BDF
 * @logFileName:   Log file to use for optional logging
 * @level:         Severity level of messages to log
 *
 * Return:         Device handle
 */
XCL_DRIVER_DLLESPEC
xclDeviceHandle
xclOpenByBDF(const char *bdf, const char *logFileName,
        enum xclVerbosityLevel level);

#ifdef __cplusplus
}
#endif

#else
#error "Must include xrt.h before include this file"
#endif

#endif
