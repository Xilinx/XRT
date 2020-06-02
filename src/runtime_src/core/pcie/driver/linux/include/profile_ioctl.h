/*
 * A GEM style device manager for PCIe nifd_based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef	_XCL_PROFILE_IOCTL_H_
#define	_XCL_PROFILE_IOCTL_H_

/*
 * Magic Number Definitions
 */
#define	AIM_IOC_MAGIC	0x28
#define	ASM_IOC_MAGIC	0x29
#define	AM_IOC_MAGIC	0x2a
#define TRACE_FIFO_LITE	0x2b
#define TRACE_FUNNEL	0x2c
#define	TRACE_S2MM		0x2d

/*
 * Axi Interface Monitor (AIM)
 */

struct aim_counters {
	uint64_t wr_bytes;
	uint64_t wr_tranx;
	uint64_t wr_latency;
	uint64_t wr_busy_cycles;
	uint64_t rd_bytes;
	uint64_t rd_tranx;
	uint64_t rd_latency;
	uint64_t rd_busy_cycles;
};

enum AIM_COMMANDS
{
	AIM_RESET = 0,
	AIM_START_COUNTERS = 1,
	AIM_READ_COUNTERS = 2,
	AIM_STOP_COUNTERS = 3,
	AIM_START_TRACE = 4
};

#define AIM_IOC_RESET		_IO(AIM_IOC_MAGIC, AIM_RESET)
#define AIM_IOC_STARTCNT	_IO(AIM_IOC_MAGIC, AIM_START_COUNTERS)
#define AIM_IOC_READCNT		_IOR(AIM_IOC_MAGIC, AIM_READ_COUNTERS,\
		struct aim_counters)
#define	AIM_IOC_STOPCNT		_IO(AIM_IOC_MAGIC, AIM_STOP_COUNTERS)
#define	AIM_IOC_STARTTRACE	_IOW(AIM_IOC_MAGIC, AIM_START_TRACE, uint32_t)

/*
 * Accelerator Monitor (AM)
 */

struct am_counters {
	/* execution count is end count*/
	uint64_t end_count;
	uint64_t start_count;
	uint64_t exec_cycles;
	uint64_t stall_int_cycles;
	uint64_t stall_str_cycles;
	uint64_t stall_ext_cycles;
	uint64_t busy_cycles;
	uint64_t max_parallel_iterations;
	uint64_t max_exec_cycles;
	uint64_t min_exec_cycles;
};

enum AM_COMMANDS
{
	AM_RESET = 0,
	AM_START_COUNTERS = 1,
	AM_READ_COUNTERS = 2,
	AM_STOP_COUNTERS = 3,
	AM_START_TRACE = 4,
	AM_CONFIG_DFLOW = 5
};

#define	AM_IOC_RESET		_IO(AM_IOC_MAGIC, AM_RESET)
#define	AM_IOC_STARTCNT		_IO(AM_IOC_MAGIC, AM_START_COUNTERS)
#define	AM_IOC_READCNT		_IOR(AM_IOC_MAGIC, AM_READ_COUNTERS,\
		struct am_counters)
#define	AM_IOC_STOPCNT		_IO(AM_IOC_MAGIC, AM_STOP_COUNTERS)
#define	AM_IOC_STARTTRACE	_IOW(AM_IOC_MAGIC, AM_START_TRACE, uint32_t)
#define	AM_IOC_CONFIGDFLOW	_IOW(AM_IOC_MAGIC, AM_CONFIG_DFLOW, uint32_t)

/*
 * Axi Stream Monitor (ASM)
 */

struct asm_counters {
	uint64_t num_tranx;
	uint64_t data_bytes;
	uint64_t busy_cycles;
	uint64_t stall_cycles;
	uint64_t starve_cycles;
};

enum ASM_COMMANDS
{
	ASM_RESET = 0,
	ASM_START_COUNTERS = 1,
	ASM_READ_COUNTERS = 2,
	ASM_STOP_COUNTERS = 3,
	ASM_START_TRACE = 4,
};

#define	ASM_IOC_RESET		_IO(ASM_IOC_MAGIC, ASM_RESET)
#define	ASM_IOC_STARTCNT	_IO(ASM_IOC_MAGIC, ASM_START_COUNTERS)
#define	ASM_IOC_READCNT		_IOR(ASM_IOC_MAGIC, ASM_READ_COUNTERS,\
		struct asm_counters)
#define	ASM_IOC_STOPCNT		_IO(ASM_IOC_MAGIC, ASM_STOP_COUNTERS)
#define	ASM_IOC_STARTTRACE	_IOW(ASM_IOC_MAGIC, ASM_START_TRACE, uint32_t)

#endif