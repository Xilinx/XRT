/*
 * A GEM style device manager for Xilinx Profiling IP
 *
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:	Anurag Dubey <hackwad@xilinx.com>
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
#define	AIM_IOC_MAGIC		0x28
#define	ASM_IOC_MAGIC		0x29
#define	AM_IOC_MAGIC		0x2a
#define TR_FIFO_MAGIC		0x2b
#define TR_FUNNEL_MAGIC		0x2c
#define	TR_S2MM_MAGIC		0x2d

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
	uint64_t outstanding_cnt;
	uint64_t wr_last_address;
	uint64_t wr_last_data;
	uint64_t rd_last_address;
	uint64_t rd_last_data;
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
	AM_STOP_TRACE = 5,
	AM_CONFIG_DFLOW = 6
};

#define	AM_IOC_RESET		_IO(AM_IOC_MAGIC, AM_RESET)
#define	AM_IOC_STARTCNT		_IO(AM_IOC_MAGIC, AM_START_COUNTERS)
#define	AM_IOC_READCNT		_IOR(AM_IOC_MAGIC, AM_READ_COUNTERS,\
		struct am_counters)
#define	AM_IOC_STOPCNT		_IO(AM_IOC_MAGIC, AM_STOP_COUNTERS)
#define	AM_IOC_STARTTRACE	_IOW(AM_IOC_MAGIC, AM_START_TRACE, uint32_t)
#define	AM_IOC_STOPTRACE	_IO(AM_IOC_MAGIC, AM_STOP_TRACE)
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

/*
 * Trace FIFO
 */

enum TR_FIFO_COMMANDS
{
	TR_FIFO_RESET = 0,
	TR_FIFO_GET_NUMBYTES = 1
};

#define	TR_FIFO_IOC_RESET			_IO(TR_FIFO_MAGIC, TR_FIFO_RESET)
#define	TR_FIFO_IOC_GET_NUMBYTES	_IOR(TR_FIFO_MAGIC, TR_FIFO_GET_NUMBYTES,\
		uint32_t)

/*
 * Trace Funnel
 */

enum TR_FUNNEL_COMMANDS
{
	TR_FUNNEL_RESET = 0,
	TR_FUNNEL_TRAINCLK = 1
};

#define	TR_FUNNEL_IOC_RESET		_IO(TR_FUNNEL_MAGIC, TR_FUNNEL_RESET)
#define	TR_FUNNEL_IOC_TRAINCLK	_IOW(TR_FUNNEL_MAGIC, TR_FUNNEL_TRAINCLK, uint64_t)

/*
 * Trace S2MM
 */

struct ts2mm_config {
	uint64_t buf_size;
	uint64_t buf_addr;
	bool circular_buffer;
};

enum TR_S2MM_COMMANDS
{
	TR_S2MM_RESET = 0,
	TR_S2MM_START = 1,
	TR_S2MM_GET_WORDCNT = 2
};

#define	TR_S2MM_IOC_RESET		_IO(TR_S2MM_MAGIC, TR_S2MM_RESET)
#define	TR_S2MM_IOC_START		_IOW(TR_S2MM_MAGIC, TR_S2MM_START,\
		struct ts2mm_config)
#define	TR_S2MM_IOC_GET_WORDCNT	_IOR(TR_S2MM_MAGIC, TR_S2MM_GET_WORDCNT,\
		uint64_t)

/*
 * LAPC
 */

struct lapc_status {
	uint32_t overall_status;
	uint32_t cumulative_status_0;
	uint32_t cumulative_status_1;
	uint32_t cumulative_status_2;
	uint32_t cumulative_status_3;
	uint32_t snapshot_status_0;
	uint32_t snapshot_status_1;
	uint32_t snapshot_status_2;
	uint32_t snapshot_status_3;
};

/*
 * SPC
 */

struct spc_status {
	uint32_t pc_asserted;
	uint32_t current_pc;
	uint32_t snapshot_pc;
};


#endif