/*
 *  Copyright (C) 2015-2017, Xilinx Inc
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
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by the Free Software Foundation;
 *  either version 2 of the License, or (at your option) any later version.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/**
 * DOC: PCIe Kernel Driver for Managament Physical Function
 * Interfaces exposed by *xclmgmt* driver are defined in file, *mgmt-ioctl.h*.
 * Core functionality provided by *xclmgmt* driver is described in the following table:
 *
 * ==== ====================================== ============================== ==================================
 * #    Functionality                          ioctl request code             data format
 * ==== ====================================== ============================== ==================================
 * 1    FPGA image download                    XCLMGMT_IOCICAPDOWNLOAD_AXLF   xclmgmt_ioc_bitstream_axlf
 * 2    CL frequency scaling                   XCLMGMT_IOCFREQSCALE           xclmgmt_ioc_freqscaling
 * 3    PCIe hot reset                         XCLMGMT_IOCHOTRESET            NA
 * 4    CL reset                               XCLMGMT_IOCOCLRESET            NA
 * 5    Live boot FPGA from PROM               XCLMGMT_IOCREBOOT              NA
 * 6    Device sensors (current, voltage and   NA                             *hwmon* (xclmgmt_microblaze and
 *      temperature)                                                          xclmgmt_sysmon) interface on sysfs
 * 7    Querying device errors                 XCLMGMT_IOCERRINFO             xclErrorStatus
 * 8    SW Mailbox                             XCLMGMT_IOCSWMAILBOX           xclmgmt_ioc_sw_mailbox
 * ==== ====================================== ============================== ==================================
 *
 */

#ifndef _XCLMGMT_IOCALLS_POSIX_H_
#define _XCLMGMT_IOCALLS_POSIX_H_

#include <linux/ioctl.h>
#include "xclerr.h"

#define XCLMGMT_IOC_MAGIC	'X'
#define XCLMGMT_NUM_SUPPORTED_CLOCKS 4
#define XCLMGMT_NUM_ACTUAL_CLOCKS 2
#define XCLMGMT_NUM_FIREWALL_IPS 3
#define AWS_SHELL14             69605400

#define AXI_FIREWALL

enum XCLMGMT_IOC_TYPES {
	XCLMGMT_IOC_INFO,
	XCLMGMT_IOC_ICAP_DOWNLOAD,
	XCLMGMT_IOC_FREQ_SCALE,
	XCLMGMT_IOC_OCL_RESET,
	XCLMGMT_IOC_HOT_RESET,
	XCLMGMT_IOC_REBOOT,
	XCLMGMT_IOC_ICAP_DOWNLOAD_AXLF,
	XCLMGMT_IOC_ERR_INFO,
	XCLMGMT_IOC_SW_MAILBOX,
	XCLMGMT_IOC_MAX
};

/**
 * struct xclmgmt_ioc_info - Obtain information from the device
 * used with XCLMGMT_IOCINFO ioctl
 *
 * Note that this structure will be obsoleted in future and the same functionality will be exposed via sysfs nodes
 */
struct xclmgmt_ioc_info {
	unsigned short vendor;
	unsigned short device;
	unsigned short subsystem_vendor;
	unsigned short subsystem_device;
	unsigned driver_version;
	unsigned device_version;
	unsigned long long feature_id;
	unsigned long long time_stamp;
	unsigned short ddr_channel_num;
	unsigned short ddr_channel_size;
	unsigned short pcie_link_width;
	unsigned short pcie_link_speed;
	char vbnv[64];
	char fpga[64];
	unsigned short onchip_temp;
	unsigned short fan_temp;
	unsigned short fan_speed;
	unsigned short vcc_int;
	unsigned short vcc_aux;
	unsigned short vcc_bram;
	unsigned short ocl_frequency[XCLMGMT_NUM_SUPPORTED_CLOCKS];
	bool mig_calibration[4];
	unsigned short num_clocks;
	bool isXPR;
	unsigned pci_slot;
	unsigned long long xmc_version;
	unsigned short twelve_vol_pex;
	unsigned short twelve_vol_aux;
	unsigned long long pex_curr;
	unsigned long long aux_curr;
	unsigned short three_vol_three_pex;
	unsigned short three_vol_three_aux;
	unsigned short ddr_vpp_btm;
	unsigned short sys_5v5;
	unsigned short one_vol_two_top;
	unsigned short one_vol_eight_top;
	unsigned short zero_vol_eight;
	unsigned short ddr_vpp_top;
	unsigned short mgt0v9avcc;
	unsigned short twelve_vol_sw;
	unsigned short mgtavtt;
	unsigned short vcc1v2_btm;
	short se98_temp[4];
	short dimm_temp[4];
};

struct xclmgmt_ioc_bitstream {
	//struct xclBin *xclbin;
};


/*
 * struct xclmgmt_err_info - Obtain Error information from the device
 * used with XCLMGMT_IOCERRINFO ioctl
 *
 * Note that this structure will be obsoleted in future and the same functionality will be exposed via sysfs nodes
 */
struct xclmgmt_err_info {
        unsigned mNumFirewalls;
        struct xclAXIErrorStatus mAXIErrorStatus[8];
        struct xclPCIErrorStatus mPCIErrorStatus;
};

/**
 * struct xclmgmt_ioc_bitstream_axlf - load xclbin (AXLF) device image
 * used with XCLMGMT_IOCICAPDOWNLOAD_AXLF ioctl
 *
 * @xclbin:	Pointer to user's xclbin structure in memory
 */
struct xclmgmt_ioc_bitstream_axlf {
	struct axlf *xclbin;
};

/**
 * struct xclmgmt_ioc_freqscaling - scale frequencies on the board using Xilinx clock wizard
 * used with XCLMGMT_IOCFREQSCALE ioctl
 *
 * @ocl_region:	        PR region (currently only 0 is supported)
 * @ocl_target_freq:	Array of requested frequencies, a value o zero in the array indicates leave untouched
 */
struct xclmgmt_ioc_freqscaling {
	unsigned ocl_region;
	unsigned short ocl_target_freq[XCLMGMT_NUM_SUPPORTED_CLOCKS];
};

/**
 * struct xclmgmt_ioc_sw_mailbox
 */
struct xclmgmt_ioc_sw_mailbox {
	uint64_t flags;
	uint32_t *data;
	bool isTx;
	size_t sz;
	uint64_t id;
};

#define XCLMGMT_IOCINFO		  _IOR (XCLMGMT_IOC_MAGIC,XCLMGMT_IOC_INFO,		 struct xclmgmt_ioc_info)
#define XCLMGMT_IOCICAPDOWNLOAD	  _IOW (XCLMGMT_IOC_MAGIC,XCLMGMT_IOC_ICAP_DOWNLOAD,	 struct xclmgmt_ioc_bitstream)
#define XCLMGMT_IOCICAPDOWNLOAD_AXLF	 _IOW(XCLMGMT_IOC_MAGIC,XCLMGMT_IOC_ICAP_DOWNLOAD_AXLF,	 struct xclmgmt_ioc_bitstream_axlf)
#define XCLMGMT_IOCFREQSCALE      _IOW (XCLMGMT_IOC_MAGIC,XCLMGMT_IOC_FREQ_SCALE,	 struct xclmgmt_ioc_freqscaling)
#define XCLMGMT_IOCHOTRESET       _IO  (XCLMGMT_IOC_MAGIC,XCLMGMT_IOC_HOT_RESET)
#define XCLMGMT_IOCOCLRESET       _IO  (XCLMGMT_IOC_MAGIC,XCLMGMT_IOC_OCL_RESET)
#define XCLMGMT_IOCREBOOT         _IO  (XCLMGMT_IOC_MAGIC,XCLMGMT_IOC_REBOOT)
#define XCLMGMT_IOCERRINFO	  _IOR (XCLMGMT_IOC_MAGIC,XCLMGMT_IOC_ERR_INFO, struct xclErrorStatus)
#define XCLMGMT_IOCSWMAILBOX     _IOWR (XCLMGMT_IOC_MAGIC,XCLMGMT_IOC_SW_MAILBOX, struct xclmgmt_ioc_sw_mailbox)

#define	XCLMGMT_MB_HWMON_NAME	    "xclmgmt_microblaze"
#define XCLMGMT_SYSMON_HWMON_NAME   "xclmgmt_sysmon"
#endif


