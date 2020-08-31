/*
 *  Copyright (C) 2020, Xilinx Inc
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

#ifndef _ERT_FA_H_
#define _ERT_FA_H_

#if defined(__KERNEL__)
# include <linux/types.h>
#else
# include <stdint.h>
#endif

#ifdef _WIN32
# pragma warning( push )
# pragma warning( disable : 4201 )
#endif

/**
 * ERT fast adapter error type
 *
 * @ERT_FA_DESC_FIFO_OVERRUN
 * @ERT_FA_DESC_DECERR
 * @ERT_FA_TASKCOUNT_DECERR
 */
typedef enum ert_fa_error_type
{
  ERT_FA_DESC_FIFO_OVERRUN = 0x1,
  ERT_FA_DESC_DECERR = 0x2,
  ERT_FA_TASKCOUNT_DECERR = 0x4
} ert_fa_error_t;

/**
 * ERT fast adapter status type
 *
 * @ERT_FA_UNDEFINED
 * @ERT_FA_ISSUED
 * @ERT_FA_COMPLETED
 */
typedef enum ert_fa_status_type
{
  ERT_FA_UNDEFINED = 0xFFFFFFFF,
  ERT_FA_ISSUED = 0x0,
  ERT_FA_COMPLETED = 0x1
} ert_fa_status_t;

/**
 * struct ert_fa_desc_entry - kernel input/output descriptor entry
 *
 * @arg_offset: offset within the acc aperture
 * @arg_size:   size of argument in bytes
 * @arg_value:  arg_size number of bytes containing arg value
 */
struct ert_fa_desc_entry {
  uint32_t arg_offset;
  uint32_t arg_size;
  uint32_t arg_value[1];
};

/**
 * struct ert_fa_descriptor - fast adapter kernel descriptor
 *
 * @status:             descriptor control synchronization word
 * @num_input_entries:  number of input arg entries
 * @input_entry_bytes:  total number of bytes for input args
 * @num_output_entries: number of output arg entries
 * @output_entry_bytes: total number of bytes for output args
 * @io_entries:         array of input entries and out entries
 * 
 * The io_entries is an array of input entries with num_input_entries
 * elements followed by an array of output entries with
 * num_output_entries elements starting at address io_entries +
 * input_ntry_bytes
 *
 * Kernel scheduling embeds the address of the descriptor as 
 * the payload of an ert_start_kernel_cmd.
 */
struct ert_fa_descriptor {
  ert_fa_status_t status;
  uint32_t num_input_entries;
  uint32_t input_entry_bytes;
  uint32_t num_output_entries;
  uint32_t output_entry_bytes;
  struct ert_fa_desc_entry io_entries[1];
};
  
#ifdef _WIN32
# pragma warning( pop )
#endif

#endif
