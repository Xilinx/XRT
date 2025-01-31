/**
 *  Copyright (C) 2015-2021, Xilinx Inc
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
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by the Free Software Foundation;
 *  either version 2 of the License, or (at your option) any later version.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/**
 * DOC: Device last / latest error status related structs and defines
 * This file is used by both userspace and kernel driver.
 * This file is used by xbutil, xocl, zocl and xbmgmt.
 */

#ifndef XCLERR_INT_H_
#define XCLERR_INT_H_

#include "xrt_error_code.h"

#define	XCL_ERROR_CAPACITY	32

/**
 * struct xclErrorLast - Container for all last(latest) error records
 * Only one entry in error array per error class xrtErrorClass
 * A xrtErrorModule may produce multiple classes of errors
 * xrtErrorCode (64 bits) = ErrorNum + Driver + Severity + Module + Class
 */
typedef struct xclErrorLast {
	xrtErrorCode	 err_code;	/* 64 bits; XRT error code */
	xrtErrorTime	 ts;		/* 64 bits; timestamp */
	unsigned       pid;            /* 32 bits; pid associated with error, if available */
  xrtExErrorCode ex_error_code; /* 64 bits; XRT extra error code*/
} xclErrorLast;

typedef struct xcl_errors {
	int		num_err;	/* number of errors recorded */
	struct xclErrorLast errors[XCL_ERROR_CAPACITY];	/* error array pointer */
} xcl_errors;

#endif /* XCLERR_INT_H_ */
